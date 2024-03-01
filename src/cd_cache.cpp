#include "cd_cache.h"

#include <cstddef>
#include <map>
#include <set>

#include "cacheline_index.h"
#include "config.h"
#include "main_compressor.h"

addr_t allocate_cacheline_id() {
    static addr_t id = 0;
    ++id;
    return id;
}

CDCache::CDCache() {  // NOLINT
    // Init index
    this->fp_index_ = new SimpleFPIndex();
    this->lba_index_ = new SimpleLBAIndex();
    this->detector_ = new BloomFilterDetector();
    Assert(fp_index_ && lba_index_, "Can't not create index instances");
}

/**
 * Evict the cache line from the underlying SSD device and write the referenced data blocks back
 * @param id  id of cacheline that is to be evicted
 * @param data metadata of the cacheline
 */

void CDCache::evictOne(cacheline_id_t id, const CachelineIndexData &data) {
    std::map<cacheline_id_t, CachelineIndexData>
        users;  // All cache lines (id->metadata) that reference the current cache line
    // collect users
    for (auto &used_by_cache_line_id : data.external_refs_) {
        CachelineIndexData ref_data;
        if (this->cacheline_index_.query(used_by_cache_line_id, ref_data, false)) {
            users[used_by_cache_line_id] = ref_data;
        }
    }

    // Collect the element data of the deleted cache line, which is the new metadata after being processed by the
    // `removeCacheline` function.
    std::pair<addr_t, CachelineIndexData> remove{id, data};

    std::map<fp_t, addr_t> deleted;

    LOGGER("[Evict] Users size is %zu, addresses size is %zu", users.size(), remove.second.allocation_page_.size());
    // the users will be modified
    PROF_TIMER(evict_remove_cacheline, {
        this->proxy_->removeCacheline(remove, users, deleted);  // 这个函数会更新users中的元数据，因为要重新修改指向
        this->cacheline_index_.remove(id);
    })
    // update the index of the cachelines that refer to deleted cacheline
    for (auto &ref : users) {
        this->cacheline_index_.insert(ref.first, ref.second, false);
    }

    PROF_TIMER(evict_update_index, {
        for (auto &ch : deleted) {
            FPIndexData fp_data{};
            if (!this->fp_index_->query(ch.first, fp_data)) {
                continue;
            }
            if (ch.second == id) {
                // same id evicted realy
                if (fp_data.cacheline_addr == id) {
                    this->fp_index_->remove(ch.first);
                    this->detector_->remove(fp_data.raw_fingerprint);
                    LOGGER("Remove Block [REAL] RFP = %zx, cfp =  %zx", fp_data.raw_fingerprint, ch.first);
                    globalEnv().write("EVICT %lx", fp_data.raw_fingerprint);
                    globalEnv().s.block_evict_ctr++;
                    // LOGGER("  - really remove data_block cfp=%zu with rfp=%zu", ch.first, fp_data.raw_fingerprint);
                } else {
                }
            } else {
                // moved to another cachelione
                LOGGER("Remove [FAKE] RFP = %zx", fp_data.raw_fingerprint);
                this->fp_index_->insert(ch.first, {ch.second});
            }
        }
    });

    globalEnv().s.time_evict_remove_cacheline += time_evict_remove_cacheline;
    globalEnv().s.time_evict_update_index += time_evict_update_index;
}

/**
 * @param read_miss If the current data block was written back due to a read miss
 */
bool CDCache::write(const DataBlock &data_block, bool read_miss) {
    if (!read_miss) {
        globalEnv().s.write_io_ctr++;
    } else {
        globalEnv().write("WRITE BACK %lx", data_block.raw_fp());
        globalEnv().s.write_back_ctr++;
    }

    this->data_block_buffer_.writeLogicalBlock(data_block);
    if (!this->data_block_buffer_.isFull()) return true;
    return this->flushBuffer();  // flush when full
}

bool CDCache::flushBuffer() {
    // Early eviction to prevent inconsistencies (complex reasons)
    auto &cfg = globalEnv().c;
    PROF_TIMER(
        evict,
        // evict
        auto raw_cacheline_size = cfg.dataset_block_size * cfg.data_block_buffer_size;
        raw_cacheline_size += Cacheline::get_estimate_metadata_len();
        auto estimated_block_need = (int)((double)(raw_cacheline_size)*1.5 / ((double)(cfg.page_granularity) * 512.0));
        // Evict until enough space
        size_t n = 0;
        while (estimated_block_need > this->proxy_->free_blocks()) {
            n++;
            auto idx = this->cacheline_index_.fetchOldestCacheline();
            this->evictOne(idx.first, idx.second);
        });

    auto flush_data_blocks = this->data_block_buffer_.popAll();
    PROF_TIMER(detection, { this->detecteBlockType(flush_data_blocks); });
    // compress
    MainCompressor::compress(flush_data_blocks, globalEnv().c.use_huffman);  // compression
    this->updateLBAIndex(flush_data_blocks);
    // deduplication
    PROF_TIMER(deduplication, { this->dedupBlocks(flush_data_blocks); });

    CachelineIndexData cachelineindexData;
    // wirte cache line to SSD
    Assert(this->proxy_->writeDataBlocks(flush_data_blocks, cachelineindexData),
           "Can not write DataBlocks flush data_blocks to SSD");
    globalEnv().s.page_write += cachelineindexData.allocation_pages_.size();

    auto cachelineId = allocate_cacheline_id();

    this->cacheline_index_.insert(cachelineId, cachelineindexData, false);

    // updat FP index / Cacheline  index reference
    for (auto &ch : flush_data_blocks) {
        // 对于Unique data_block
        if (!ch.comp_duplicated()) {
            this->fp_index_->insert(ch.comp_fp(), {cachelineId, ch.raw_fp()});  // NOLINT
        } else {
            this->cacheline_index_.addRefToCacheline(ch.external_cacheline_addr(), cachelineId, true);
        }
    }

    globalEnv().s.time_deduplication += time_deduplication;
    globalEnv().s.time_detection += time_detection;
    globalEnv().s.time_evict += time_evict;
    // globalEnv().s.time_update_cache_line_index += time_update_cache_line_index;
    return true;
}

void CDCache::open(const std::string &name, size_t size) { this->proxy_ = new SSDProxy(name, size); }

void CDCache::detecteBlockType(std::vector<DataBlock> &data_blocks) {
    for (auto &ch : data_blocks) {
        bool raw_duplicated = this->detector_->query(ch.raw_fp());
        ch.setRawDuplicated(raw_duplicated);
    }
    for (auto &ch : data_blocks) this->detector_->insert(ch.raw_fp());  // 插入
}

// return the total size of all unique data blocks
size_t CDCache::dedupBlocks(std::vector<DataBlock> &data_blocks) {
    size_t total_size = 0;
    for (auto &ch : data_blocks) {
        globalEnv().s.write_logic_blocks++;
        FPIndexData data{};
        data.cacheline_addr = -1;
        data.raw_fingerprint = ch.raw_fp();
        auto comp_duplicated = this->fp_index_->query(ch.comp_fp(), data);
        ch.setCompDuplicated(comp_duplicated);
        ch.set_external_cacheline_addr(data.cacheline_addr);
        if (!comp_duplicated) {
            globalEnv().s.write_unique_blocks++;
            total_size += ch.comp_data().size();
        }
    }
    return total_size;
}

void CDCache::updateLBAIndex(std::vector<DataBlock> &data_blocks) {
    for (auto &ch : data_blocks) {
        globalEnv().s.compressed_data += ch.comp_data().size();
        globalEnv().s.raw_data += ch.raw_data().size();
        this->lba_index_->insert(ch.address(), ch.comp_fp());
    }
}

// read
bool CDCache::read(LogicalBlock &block) {
    globalEnv().s.read_io++;
    if (this->data_block_buffer_.tryReadLogicalDataBlock(block.address(), block)) {
        globalEnv().s.read_hit++;
        return true;
    }

    fp_t comp_fp;
    if (!this->lba_index_->query(block.address(), comp_fp)) {
        globalEnv().s.read_lose_by_lba++;
        this->write(block, true);
        block.setRawData({});
        return false;
    }

    FPIndexData fp_index_data{};
    if (!this->fp_index_->query(comp_fp, fp_index_data)) {
        globalEnv().s.read_lose_by_fp++;
        this->write(block, true);  // write through, write back cache
        block.setRawData({});
        return false;
    }

    // LOGGER("cfp=%zu was stored in cacheline cid=%zu", comp_fp, fp_index_data.cacheline_addr);
    CachelineIndexData cachelineIndexData;

    Assert(this->cacheline_index_.query(fp_index_data.cacheline_addr, cachelineIndexData, true),
           "[READ]Can not find cfp=%zu 's cid=%zu in  cacheline index\n", comp_fp, fp_index_data.cacheline_addr);
    // decompress here
    globalEnv().s.read_hit++;
    return true;

    // Cacheline cacheline;
    // Assert(this->proxy_->readCacheline(cacheline, cachelineIndexData),
    //        "Can not read external cacheline cid=%zu from SSD", fp_index_data.cacheline_addr);

    // std::map<addr_t, Cacheline> external_cachelines;
    // for (auto &he : cacheline.data_blocks_info) {
    //     if (he.external_address != -1) {
    //         Cacheline c;
    //         CachelineIndexData external_data;
    //         // XXX TO QUIET
    //         Assert(this->cacheline_index_.quietQuery(he.external_address, external_data),
    //                "Can not find external cacheline cid=%zu in cacheline index, which is referenced by cid=%zu",
    //                he.external_address, fp_index_data.cacheline_addr);

    //         Assert(this->proxy_->readCacheline(c, external_data),
    //                "Can not read external cacheline cid=%zu from SSD,which is referenced by cid=%zu",
    //                he.external_address, fp_index_data.cacheline_addr);
    //         external_cachelines[he.external_address] = c;
    //     }
    // }
    // 下面的解压过程不影响命中率测试，因此这里暂时注释以提高测试效率

    //    std::vector<DataBlock> data_blocks;
    //    // 遍历当前cacheline的所有data_block
    //    for (auto &data_block_info : cacheline.data_blocks_info) {
    //        DataBlock c;
    //        c.set_raw_len(data_block_info.raw_len);
    //        c.set_comp_fp(data_block_info.comp_fp);
    //        c.set_raw_fp(data_block_info.raw_fp);
    //        if (data_block_info.type == 0) {  // 没有存到当前的cacheline中
    //            auto it = external_cachelines.find(data_block_info.external_address);
    //            Assert(it != external_cachelines.end(), "can not read external data_block");
    //            auto &ext = it->second;
    //            auto index = -1;
    //            for (int i = 0; i < ext.data_blocks_info.size(); i++) {
    //                if (data_block_info.comp_fp == ext.data_blocks_info[i].comp_fp) {
    //                    index = i;
    //                    break;
    //                }
    //            }
    //            Assert(index >= 0, "Can not find data_block info in external data_block when find data_block[%zu]",
    //            data_block_info.comp_fp); c.setCompData(ext.data_blocks_data[ext.data_blocks_info[index].pos_index]);
    //        } else {
    //            c.setCompData(cacheline.data_blocks_data[data_block_info.pos_index]);
    //        }
    //        data_blocks.push_back(c);
    //    }
    //
    //    MainCompressor::decompress(data_blocks, globalConfig().use_huffman);
    //
    //    for (auto &data_block : data_blocks) {
    //        if (comp_fp == data_block.comp_fp()) {
    //            block.setRawData(data_block.raw_data());
    //            LOGGER("[READ] read data_block cfp=%zu  success", comp_fp);
    //            stat.read_hit_ctr++;
    //            return true;
    //        }
    //    }
    //
    //    Assert(false, "Can not find data of cfp = %zu in cacheline", comp_fp);
    //    return false;
}

CDCache::~CDCache() {
    delete this->proxy_;
    delete this->fp_index_;
    delete this->lba_index_;
    // delete this->pv_;
}
