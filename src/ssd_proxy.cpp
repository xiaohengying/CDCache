#include "ssd_proxy.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "config.h"

namespace {
    size_t get_block_need(size_t bytes, size_t block_size) {
        auto res = bytes / block_size;
        return bytes % block_size == 0 ? res : res + 1;
    }
}  // namespace

// Assemble multiple data blocks into cacheline
Cacheline Cacheline::fromDataBlocks(const std::vector<DataBlock> &data_blocks) {
    Cacheline cacheline;
    size_t data_len = 0;
    for (const auto &ch : data_blocks) {
        CachelineDataLayout layout{};
        CachelineDataBlockInfo info{};
        info.raw_fp = ch.raw_fp();
        info.raw_len = ch.raw_len();
        info.comp_fp = ch.comp_fp();
        // info.len = ch.comp_data().size();
        info.type = !ch.comp_duplicated();
        if (!ch.comp_duplicated()) {  // store real data
            info.pos_index = cacheline.data_layout.size();
            info.external_address = -1;
            layout.len = ch.comp_data().size();
            layout.offset = data_len;
            cacheline.data_layout.push_back(layout);
            cacheline.data_blocks_data.push_back(ch.comp_data());
            data_len += ch.comp_data().size();
        } else {
            // metadata only
            info.external_address = ch.external_cacheline_addr();
            info.pos_index = -1;
        }
        cacheline.data_blocks_info.push_back(info);
    }

    const auto spaces = cacheline.data_blocks_info.size() - cacheline.data_layout.size();

    // fill all spaces in data layout
    for (int i = 0; i < spaces; i++) {
        cacheline.data_layout.push_back({static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)});
    }

    cacheline.header.header_len = sizeof(CachelineHeader);
    cacheline.header.data_blocks_info_len = sizeof(CachelineDataBlockInfo) * data_blocks.size();
    cacheline.header.data_layout_len = sizeof(CachelineDataLayout) * data_blocks.size();
    cacheline.header.data_blocks_data_len = data_len;
    cacheline.header.data_blocks_number = data_blocks.size();
    return cacheline;
}

bool SSDProxy::writeDataBlocks(std::vector<DataBlock> &data_blocks, CachelineIndexData &data) {
    auto cacheline = Cacheline::fromDataBlocks(data_blocks);
    LOGGER("[Cacheline space usage:] %.3lf", cacheline.get_utilization_rate());
    return this->writeCacheline(cacheline, data);
}

bool SSDProxy::writeCacheline(Cacheline &cacheline, CachelineIndexData &data) {
    std::stringstream s;
    cacheline.serialize(s);
    auto bytes = s.str();
    // Add padding zeros
    if (bytes.size() % this->PG_SZ != 0) {
        bytes.append(this->PG_SZ - bytes.size() % this->PG_SZ, 0);
    }
    if (!this->manager_->allocate(bytes.size() / this->PG_SZ, data.allocation_pages_)) {
        ERROR("[SSD write] Can not allocation enough free allocation blocks");
        return false;
    }

    LOGGER("[SSD Write] Write %zuB data to %zu blocks:[%s]", bytes.size(), data.allocation_page_.size(),
           vec2str(data.allocation_page_).c_str());

    Assert(bytes.size() == data.allocation_pages_.size() * this->PG_SZ, "[SSD write] Invalid bytes len");
    for (int i = 0; i < data.allocation_pages_.size(); i++) {
        this->write_allocated_page(data.allocation_pages_[i],
                                   reinterpret_cast<const byte_t *>(bytes.data() + this->PG_SZ * i));
    }
    return true;
}
bool SSDProxy::open_device(const std::string &name, size_t size) { return this->device_->open(name.c_str(), size); }

SSDProxy::SSDProxy(const std::string &name, size_t size) : PG_SZ(512 * globalEnv().c.page_granularity) {
    this->device_ = new MemBlockDevice();
    Assert(this->open_device(name, size), "Can not open SSD device %s", name.c_str());
    this->manager_ = new StackedDiscretePageManager(this->device_->size() / this->PG_SZ);
    Assert(this->manager_, "Can not init block allocation manager");
}

bool SSDProxy::write_allocated_page(addr_t address, const byte_t *data) {
    for (int i = 0; i < globalEnv().c.page_granularity; i++) {
        Assert(this->device_->write((address * globalEnv().c.page_granularity + i) * 512, data + 512 * i, 512) == 512,
               "Can not write data to SSD");
    }
    return true;
}

bool SSDProxy::read_allocated_page(addr_t address, std::vector<byte_t> &data) {
    data = std::vector<byte_t>(this->PG_SZ, 0);
    //    LOGGER("READ allocation block address: %d", address);
    for (int i = 0; i < globalEnv().c.page_granularity; i++) {
        Assert(this->device_->read((address * globalEnv().c.page_granularity + i) * 512, data.data() + 512 * i, 512) ==
                   512,
               "Can not read cacheline data from SSD");
    }

    return true;
}

// bool SSDProxy::readDataBlocks(std::vector<DataBlock> &data_blocks, const CachelineIndexData &data) {
//     //    Cacheline cacheline;
//     //    this->readCacheline(cacheline, data);
//     //    // 从cacheline中恢复列表
//     //    data_blocks = std::vector<DataBlock>(cacheline.header.data_layout_len, DataBlock());
//     //
//     //    // offset len
//     //    for (auto &ch : cacheline.data_blocks_info) {
//     //    }
//     //
//     //    //    int index = 0;
//     //    //    for (auto &ch : cacheline.data_blocks) {
//     //    //        data_blocks[index].setCompData(ch);
//     //    //        ++index;
//     //    //    }
//
//     return true;
// }
bool SSDProxy::readCacheline(Cacheline &cacheline, const CachelineIndexData &data) {
    std::stringstream s;
    auto &address = data.allocation_pages_;
    Assert(!address.empty(), "[SSD READ] Empty block address list when read cacheline");
    size_t sz = 0;
    for (auto addr : address) {
        std::vector<byte_t> temp_buffer;
        this->read_allocated_page(addr, temp_buffer);
        sz += temp_buffer.size();
        s.write(reinterpret_cast<const char *>(temp_buffer.data()), static_cast<std::streamsize>(temp_buffer.size()));
    }

    //  LOGGER("[SSD READ] blocks=[%s], %zu bytes data was read", vec2str(address).c_str(), sz);

    try {
        cacheline.deserialize(s, false);
        return true;
    } catch (std::exception &e) {
        ERROR("Can not deserialize cacheline, cause: %s", e.what());
        return false;
    }
}

bool SSDProxy::readMetadataOnly(Cacheline &cachline, const CachelineIndexData &data) {
    // TODO: Need optimization
    this->readCacheline(cachline, data);
    //    std::stringstream s;
    //    std::vector<byte_t> first_block;
    //    LOGGER("First address is %zu", data.allocation_page_[0]);
    //
    //    this->read_allocated_page(data.allocation_page_[0], first_block);
    //    s.write(reinterpret_cast<const char *>(first_block.data()), static_cast<std::streamsize>(first_block.size()));
    //    s.read(reinterpret_cast<char *>(&cachline.header), sizeof(CachelineHeader));
    //    // 先读取第一个block,获取metadata长度
    //    auto metadata_len = cachline.header.header_len + cachline.header.data_layout_len +
    //    cachline.header.data_blocks_info_len;
    //
    //    auto total_blocks_needed = metadata_len / (this->PG_SZ);
    //    if (metadata_len % this->PG_SZ != 0) total_blocks_needed += 1;
    //    LOGGER("metadata len is %zu //// blocks needed is %zu", metadata_len, total_blocks_needed);
    //    // 已经读取一个了，接下来还刷要total_blocks_needed - 1个
    //    for (int i = 0; i < total_blocks_needed - 1; i++) {
    //        std::vector<byte_t> temp_buffer;
    //        this->read_allocated_page(data.allocation_page_[i + 1], first_block);
    //        s.write(reinterpret_cast<const char *>(temp_buffer.data()),
    //        static_cast<std::streamsize>(temp_buffer.size()));
    //    }
    //    LOGGER("Total %zu bytes was read", s.tellp());
    //    cachline.deserialize(s, true);
    return true;
}

/**
 * dirty code below
 * @param cur Information about the cache line that is about to be deleted  <cacheline id -> cacheline metadata> (from
 * the cache line index)
 * @param refs All cache line metadata referencing the current cache line <cacheline id -> cacheline metadata>
 * @param moved Information about modified data blocks
            key: data block compress fingerprint
            value: The cache line ID where the data block is located after remove operation finished.
            If it is equal to the deleted cache line ID, it means that it has been actually deleted.
            If it is not equal to the deleted cache line ID, it means that it has been moved to another cache line.
 */
void SSDProxy::removeCacheline(std::pair<addr_t, CachelineIndexData> &cur, std::map<addr_t, CachelineIndexData> &refs,
                               std::map<fp_t, addr_t> &moved) {
    // read out curent cache line
    Cacheline cur_cacheline;
    LOGGER("Try remove cacheline %zu", cur.first);
    this->readCacheline(cur_cacheline, cur.second);
    // 要被逐出的cacheline的data_block表，key是comp_fp,value是它在data区域的位置，方便确定要逐出的cacheline真的引用了当前的cacheline
    // data block location in deleted cacheline
    std::unordered_map<fp_t, size_t> stored_data_blocks;
    for (auto &ch : cur_cacheline.data_blocks_info) {
        if (ch.type == 1) {
            Assert(ch.pos_index != -1, "Invalid Pos index");
            stored_data_blocks[ch.comp_fp] = ch.pos_index;
        }
        // Initialize `moved` table
        moved[ch.comp_fp] = cur.first;
    }

    // 如果需要删除的cacheline没有任何引用，直接移除即可
    // empty refs: return
    if (refs.empty()) {
        // reclaim all pages
        for (auto addr : cur.second.allocation_pages_) {
            this->manager_->reclaim(addr);
        }
        return;
    }

    //============================The following is the case when the reference is not empty=============================

    // Recycle the cache line contents (the cache line is already in memory)
    for (auto addr : cur.second.allocation_pages_) {
        this->manager_->reclaim(addr);
    }

    // 这里可能会出现各种相互引用和多个之间引用的情况，因此处理必须很小心
    // key ==> data_block指纹 value ==> 被引用的id
    // 这个表表示引用了data_block 哈希为key的所有cacheline的信息
    // 这个信息需要使用一个额外的东西来存，因为我们要选出一个来添加
    // 挑选的原则
    // 1. time stamp
    //  time stamp相同的话就用id

    struct ChooseInfo {
        size_t timestamp;
        addr_t cacheline_id;
        bool operator<(const ChooseInfo &rhs) const {
            if (timestamp < rhs.timestamp) return true;
            if (rhs.timestamp < timestamp) return false;
            return cacheline_id < rhs.cacheline_id;
        }
    };

    // key: 出现引用的data_block, 也就是说实际上不用移除的data_block表
    // Key: The data block ID where the reference appears
    // Value : Information about all cache lines that reference this data block
    std::unordered_map<fp_t, std::set<ChooseInfo>> ref_table;
    for (auto &ref : refs) {
        // 遍历每个引用当前cacheline的cacheline并读出引用的cacheline的metadata
        // Check the metadata of all cache lines that reference the deleted cache line
        Cacheline external_cacheline;
        this->readMetadataOnly(external_cacheline, ref.second);
        for (auto &ch : external_cacheline.data_blocks_info) {       // traverse all data blocks
            if (ch.type == 0 && ch.external_address == cur.first) {  // the data block refers to deleted cacheline
                Assert(stored_data_blocks.count(ch.comp_fp) > 0, "Can not find ref data_block when modify refs");
                // 添加到被引用表
                // ref_table[ch.comp_fp].push_back(ref.first);
                // add to reference table
                ChooseInfo info{ref.second.time_stamp_, ref.first};
                ref_table[ch.comp_fp].insert(info);
            }
        }
    }

    // Modifications required for each referenced cache line
    std::map<addr_t, std::vector<ModifyCommand>> commands_map;
    for (auto &kv : ref_table) {
        // for debug
        std::string s;
        for (auto &i : kv.second) {
            s += std::to_string(i.cacheline_id) + ", ";
        }
        LOGGER("cfp=%zu was used in cachelines cids = [%s]", kv.first, s.c_str());

        // 从cacheline中获取数据
        // 从kv.second找到一个最新的Cache,并把本来需要移除的data_block加入，让second中其他的引用这个cacheline即可(调用modify
        // cacheline) 由于上面已经做了排序了，直接让第一个作为被引用的，其他的作为引用的
        auto it = kv.second.begin();
        auto head_cacheline = *it;
        LOGGER("Choose cacheline cid=[%zu] to be restored", head_cacheline.cacheline_id);
        auto i = stored_data_blocks.find(kv.first);
        Assert(i != stored_data_blocks.end(), "Can not find data in cur cacheline");
        // Picks the first cache line from the reference table and appends the current data block to that cache line
        auto appendCmd = ModifyCommand::appendCmd(cur_cacheline.data_blocks_data[i->second], kv.first);

        // 更新指向的cacheline(更新后的表示kv.first实际上指向的data_block只是修改了位置，没有删除)
        // update data block location
        moved[kv.first] = head_cacheline.cacheline_id;
        commands_map[head_cacheline.cacheline_id].push_back(appendCmd);
        // 剩下的引用当前数据即可
        // For other cache lines that reference the current data block, just modify the reference.
        it++;
        while (it != kv.second.end()) {
            refs[head_cacheline.cacheline_id].external_refs_.insert(it->cacheline_id);
            auto modifyCmd = ModifyCommand::modifyCmd(kv.first, head_cacheline.cacheline_id);
            commands_map[it->cacheline_id].push_back(modifyCmd);
            ++it;
        }
    }

    // for debug
    for (auto &kv : commands_map) {
        LOGGER("commands for cacheline %zu", kv.first);
        for (auto &c : kv.second) {
            LOGGER("  - %s", c.toString().c_str());
        }
    }

    // apply commands to eache cacheline
    for (auto &kv : commands_map) {
        // 遍历每个命令并开始执行
        auto it = refs.find(kv.first);
        Assert(it != refs.end(), "Invalid Ref Cacheline id");
        // 修改data
        auto data = it->second;
        auto addr1 = data.allocation_pages_.front();
        LOGGER("Modify cacheline cid=%zu", it->first);

        this->modifyCacheline(kv.second, data);
        auto addr2 = data.allocation_pages_.front();
        // 因为这里会append一些block，会修改地址，因此cacheline index也要更新，这也是为什么const 不传引用的原因
        refs[kv.first] = data;
    }
}

/**
 * Apply modify command to cache line
 * @param commands command set for current cacheline
 * @param data Modified cache line metadata
 * @return
 */
bool SSDProxy::modifyCacheline(const std::vector<ModifyCommand> &commands, CachelineIndexData &data) {
    // TODO 根据commands内提供的信息修改一个cacheline
    Cacheline cacheline;
    this->readCacheline(cacheline, data);
    std::map<fp_t, std::vector<size_t>> infos;
    // data_blocks info里面是元数据位置
    // 这里可能有两个相同的data_block引用了同一个
    for (int i = 0; i < cacheline.data_blocks_info.size(); i++) {
        auto &info = cacheline.data_blocks_info[i];
        infos[info.comp_fp].push_back(i);
    }

    size_t layout_idx = 0;
    while (cacheline.data_layout[layout_idx].len != -1) ++layout_idx;
    LOGGER("Total %zu data_block are stored in this cacheline", layout_idx);
    for (auto &cmd : commands) {
        auto it = infos.find(cmd.fp);

        //        if (it == infos.end() || cacheline.data_blocks_info[it->second].type == 1) {
        //            LOGGER("DataBlock %zx is not in this cacheline or it's data was already stored", cmd.fp);
        //        }
        //

        //  auto &info = cacheline.data_blocks_info[it->second];
        auto target_idxes = it->second;
        if (cmd.action == ModifyCommand::ModifyRef) {
            // 修改外部引用
            for (auto idx : target_idxes) {
                cacheline.data_blocks_info[idx].external_address = cmd.new_external_addr;
            }

        } else {
            auto &info = cacheline.data_blocks_info[it->second.front()];
            // Assert(info.len = cmd.data.size(), "Error data length");
            cacheline.data_blocks_data.push_back(cmd.data);  // 追加数据
            info.pos_index = layout_idx;                     // 更新位置索引
            cacheline.data_layout[layout_idx] = {(uint32_t)cacheline.header.data_blocks_data_len,
                                                 (uint32_t)cmd.data.size()};  // 更新数据索引
            ++layout_idx;
            cacheline.header.data_blocks_data_len += cmd.data.size();
            info.external_address = -1;  // 修改外部地址
            info.type = 1;               // 修改类型
            // 修改添加新的data_block
            for (int i = 1; i < it->second.size(); i++) {
                auto &l_info = cacheline.data_blocks_info[it->second[i]];
                l_info.external_address = -1;
                l_info.pos_index = -1;
                l_info.type = 2;
            }
        }
    }

    // 回收之前的,然后重新写入整个cacheline,这里全部读出修改然后写入，其实可以继续优化（只读出一部分，但是过于麻烦1）
    for (auto addr : data.allocation_pages_) {
        this->manager_->reclaim(addr);
    }
    data.allocation_pages_.clear();
    LOGGER("After modify:");
    // cacheline.dumpToLogger();
    this->writeCacheline(cacheline, data);
    // LOGGER("Re alloc blocks for current cacheline with new size: %d", data.allocation_page_.size());
    return true;
}
SSDProxy::~SSDProxy() { delete this->manager_; }

/**
 * 对于modifyInfo内的每个kv修改其kv.first指向的data_block的external ref为kv.second
 * @param modifyInfo
 * @return
 */

void Cacheline::deserialize(std::istream &s, bool metadata_only) {
    if (!metadata_only) {
        s.read(reinterpret_cast<char *>(&this->header), sizeof(CachelineHeader));
        if (header.data_blocks_number != globalEnv().c.data_block_buffer_size) {
            LOGGER("Error header data_block len %d", header.data_blocks_number);
        }

        Assert(header.data_blocks_number == globalEnv().c.data_block_buffer_size,
               "[1] Error header data_block len (len = %d)", header.data_blocks_number);
    }

    Assert(header.data_blocks_number == globalEnv().c.data_block_buffer_size, "[2] Error header data_block len");
    this->data_blocks_info =
        std::vector<CachelineDataBlockInfo>(this->header.data_blocks_number, CachelineDataBlockInfo());
    this->data_layout = std::vector<CachelineDataLayout>(this->header.data_blocks_number, CachelineDataLayout());
    s.read(reinterpret_cast<char *>(this->data_blocks_info.data()),
           static_cast<std::streamsize>(sizeof(CachelineDataBlockInfo) * this->data_blocks_info.size()));
    s.read(reinterpret_cast<char *>(this->data_layout.data()),
           static_cast<std::streamsize>(sizeof(CachelineDataLayout) * this->data_layout.size()));
    if (!metadata_only) {
        for (auto &layout : this->data_layout) {
            if (layout.len == -1 && layout.offset == -1) {
                break;
            }
            std::vector<byte_t> bytes(layout.len, 0);
            s.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(layout.len));
            this->data_blocks_data.push_back(bytes);
        }
    }
}

size_t Cacheline::serialize(std::ostream &s) const {
    Assert(this->header.data_blocks_number == globalEnv().c.data_block_buffer_size,
           "Error data_block number with l = %zu", this->header.data_blocks_number);
    s.write(reinterpret_cast<const char *>(&this->header), sizeof(CachelineHeader));
    s.write(reinterpret_cast<const char *>(this->data_blocks_info.data()),
            static_cast<std::streamsize>(sizeof(CachelineDataBlockInfo) * this->data_blocks_info.size()));
    s.write(reinterpret_cast<const char *>(this->data_layout.data()),
            static_cast<std::streamsize>(sizeof(CachelineDataLayout) * this->data_layout.size()));
    for (auto &ch : this->data_blocks_data) {
        s.write(reinterpret_cast<const char *>(ch.data()), static_cast<std::streamsize>(ch.size()));
    }

    auto r = s.tellp();
    Assert(r == this->header.total_len(), "Unexpected serialize: tellp  =  %zu but total = %zu", r, header.total_len());
    return r;
}

size_t Cacheline::get_estimate_metadata_len() {
    return sizeof(CachelineHeader) +
           globalEnv().c.data_block_buffer_size * (sizeof(CachelineDataBlockInfo) + sizeof(CachelineDataLayout));
}

double Cacheline::get_utilization_rate() const {
    auto metadata_size = this->get_estimate_metadata_len();
    auto data_size = 0;
    for (auto &data : this->data_blocks_data) {
        data_size += data.size();
    }
    return data_size * 1.0 / (data_size + metadata_size);
}

void Cacheline::dumpToLogger() const {
    LOGGER("=========================================");
    LOGGER("Header");
    LOGGER(" - Header size:         %zu", header.header_len);
    LOGGER(" - DataBlock info size:     %zu", header.data_blocks_info_len);
    LOGGER(" - Data layout size:    %zu", header.data_layout_len);
    LOGGER(" - Data size:    %zu", header.data_blocks_data_len);
    LOGGER(" - DataBlocks number:       %zu", header.data_blocks_number);
    LOGGER("DataBlock info");
    for (auto &ch : this->data_blocks_info) {
        if (ch.external_address == -1) {
            // LOGGER("  - FP: %-24zu  len: %-5zu raw_len: %zu type: %d     position_index:    %d", ch.comp_fp, ch.len,
            //        ch.raw_len, ch.type, ch.pos_index);
        } else {
            // LOGGER("  - FP: %-24zu  len: %-5zu raw_len: %zu type: %d      external_address:   %d", ch.comp_fp,
            // ch.len,
            //        ch.raw_len, ch.type, ch.external_address);
        }
    }
    LOGGER("Data layout");
    int i = 0;
    for (auto &layout : this->data_layout) {
        if (layout.offset == -1 && layout.len == -1) {
            LOGGER(" - [%d] offset: -1  len:  -1");
        } else {
            LOGGER(" - [%d] offset: %zu  len:  %zu", i, layout.offset, layout.len);
        }
        i++;
    }

    i = 0;
    LOGGER("DataBlocks");
    for (auto &ch : this->data_blocks_data) {
        LOGGER(" - [%d] ==> %zu bytes", i, ch.size());
        i++;
    }
    LOGGER("=========================================");
}

// EQ
