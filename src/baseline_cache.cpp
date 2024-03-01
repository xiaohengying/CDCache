#include "baseline_cache.h"

#include <cassert>

#include "config.h"
#include "data_block.h"
#include "page_manager.h"

namespace {
    addr_t allocate_cacheline_id() {
        static addr_t id = 0;
        ++id;
        return id;
    }

}  // namespace

// Baseline
//
bool BaselineCache::read(LogicalBlock &block) {
    //   this->stat.read_ctr++;
    auto &stat = globalEnv().s;
    stat.read_io++;
    auto res = this->cache_.find(block.address());
    if (res != this->cache_.end()) {
        stat.read_hit++;
        this->policy_->promote(block.address());
    } else {
        this->write(block, true);
    }
    return true;
}

bool BaselineCache::write(const LogicalBlock &block, bool read_miss) {
    auto &stat = globalEnv().s;
    if (!read_miss) {
        stat.write_io_ctr++;
    } else {
        stat.write_back_ctr++;
    }

    while (this->size_in_block_ - this->cache_.size() <= 0) {
        addr_t ev = this->policy_->oldest();
        this->policy_->evict(ev);
        this->cache_.erase(ev);
    }
    // write and update LRU
    this->cache_.insert(block.address());
    this->policy_->insert(block.address());
    this->policy_->promote(block.address());
    return true;
}
BaselineCache::~BaselineCache() { delete this->policy_; }

bool DedupCache::read(LogicalBlock &block) {
    globalEnv().s.read_io++;
    if (auto it = this->lba_index_.find(block.address()); it != this->lba_index_.end()) {
        if (auto it2 = this->fp_index_.find(it->second); it2 != this->fp_index_.end()) {
            assert(this->cache_.count(it2->second));
            this->policy_->promote(it->second);
            globalEnv().s.read_hit++;
        } else {
            this->write(block, true);
            globalEnv().s.read_lose_by_fp++;
        }
    } else {
        this->write(block, true);
        globalEnv().s.read_lose_by_lba++;
    }
    return true;
}

bool DedupCache::write(const LogicalBlock &block, bool read_miss) {
    if (!read_miss) {
        globalEnv().s.write_io_ctr++;
    } else {
        globalEnv().s.write_back_ctr++;
    }

    globalEnv().s.write_logic_blocks++;
    // check FP index
    auto it = this->fp_index_.find(block.raw_fp());
    if (it != this->fp_index_.end()) {
        // update index directly
        this->lba_index_[block.address()] = block.raw_fp();
        this->policy_->promote(block.raw_fp());
        return true;
    } else {  //
              // write data block
        //  this->dedup_stat_.unique_write_++;
        while ((int)this->size_in_block_ - (int)this->cache_.size() <= 0) {
            //
            auto raw_fp = this->policy_->oldest();
            this->policy_->evict(raw_fp);
            assert(this->fp_index_.count(raw_fp));
            this->cache_.erase(this->fp_index_[raw_fp]);  // remove
            this->fp_index_.erase(raw_fp);
            globalEnv().s.block_evict_ctr++;
        }
        //
        auto random_address = allocate_cacheline_id();
        this->lba_index_[block.address()] = block.raw_fp();
        this->fp_index_[block.raw_fp()] = random_address;
        this->cache_.insert(random_address);
        this->policy_->insert(block.raw_fp());
        this->policy_->promote(block.raw_fp());
        globalEnv().s.write_unique_blocks++;
        globalEnv().s.page_write += globalEnv().c.dataset_block_size / (globalEnv().c.page_granularity * 512);
        return true;
    }
}

bool AdaptedBaselineCache::read(LogicalBlock &block) {
    globalEnv().s.read_io++;
    if (auto it = this->lba_index_.find(block.address()); it != this->lba_index_.end()) {
        auto fp = it->second;
        // auto address = it->second;
        if (auto it2 = this->fp_index_.find(fp); it2 != this->fp_index_.end()) {
            Assert(!it2->second.empty(), "Empty Address vertor");
            this->policy_->promote(it->second);
            globalEnv().s.read_hit++;
        } else {
            this->write(block, true);
            globalEnv().s.read_lose_by_fp++;
        }
    } else {
        this->write(block, true);
        globalEnv().s.read_lose_by_lba++;
    }
    return true;
}

bool AdaptedBaselineCache::write(const LogicalBlock &block, bool read_miss) {
    if (!read_miss) {
        globalEnv().s.write_io_ctr++;
    } else {
        globalEnv().s.write_back_ctr++;
    }
    globalEnv().s.write_logic_blocks++;
    while (this->size_in_block_ - this->cache_.size() <= 0) {
        fp_t raw_fp = this->policy_->oldest();
        this->policy_->evict(raw_fp);
        auto it = this->fp_index_.find(raw_fp);
        assert(it != this->fp_index_.end());
        assert(this->cache_.count(it->second.back()));
        this->cache_.erase(it->second.back());  //
        it->second.pop_back();                  //
        if (it->second.empty()) {
            this->fp_index_.erase(it);
        }

        globalEnv().s.block_evict_ctr++;
    }
    // write
    auto random_address = allocate_cacheline_id();
    this->lba_index_[block.address()] = block.raw_fp();
    this->fp_index_[block.raw_fp()].push_back(random_address);
    this->cache_.insert(random_address);
    this->policy_->insert(block.raw_fp());
    this->policy_->promote(block.raw_fp());
    globalEnv().s.write_unique_blocks++;
    globalEnv().s.page_write += globalEnv().c.dataset_block_size / (globalEnv().c.page_granularity * 512);
    return true;
}
