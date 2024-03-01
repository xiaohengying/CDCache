#ifndef CDCACHE_BASELINE_CACHE_H
#define CDCACHE_BASELINE_CACHE_H

#include <cstdio>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "abstract_cache.h"
#include "cache_policy.h"
#include "cd_cache.h"
#include "data_block.h"

/*
based cache LBA-based LRU
*/
class BaselineCache : public AbstractCache {
   public:
    explicit BaselineCache(size_t size_in_block) : AbstractCache(), size_in_block_(size_in_block) {
        this->policy_ = Env::policyInstance<addr_t>();
    }

    bool read(LogicalBlock& block) override;
    bool write(const LogicalBlock& block, bool read_miss) override;

    ~BaselineCache() override;

   private:
    std::unordered_set<addr_t> cache_;
    AbstractCachePolicy<addr_t>* policy_{nullptr};
    const size_t size_in_block_{0};
};

/*
based cache FP-based LRU
*/
class AdaptedBaselineCache : public AbstractCache {
   public:
    bool read(LogicalBlock& block) override;
    bool write(const LogicalBlock& block, bool read_miss) override;

    explicit AdaptedBaselineCache(size_t size_in_block) : AbstractCache(), size_in_block_(size_in_block) {
        this->policy_ = Env::policyInstance<addr_t>();
    }
    ~AdaptedBaselineCache() override { delete policy_; }

   private:
    std::unordered_map<addr_t, fp_t> lba_index_;              // LBA -> fp
    std::unordered_map<fp_t, std::vector<addr_t>> fp_index_;  // fp -> CA
    std::unordered_set<addr_t> cache_;
    const size_t size_in_block_ = 0;
    AbstractCachePolicy<addr_t>* policy_{nullptr};
};
/**
 *dedup cache
 */
class DedupCache : public AbstractCache {
   public:
    bool read(LogicalBlock& block) override;
    bool write(const LogicalBlock& block, bool read_miss) override;
    explicit DedupCache(size_t size_in_block) : AbstractCache(), size_in_block_(size_in_block) {
        LOGGER("Total %zu cacheline solots", size_in_block);
        this->policy_ = Env::policyInstance<addr_t>();
    }
    ~DedupCache() override { delete policy_; }

   private:
    std::unordered_map<addr_t, fp_t> lba_index_;
    std::unordered_map<fp_t, addr_t> fp_index_;
    std::unordered_set<addr_t> cache_;
    const size_t size_in_block_ = 0;
    AbstractCachePolicy<addr_t>* policy_{nullptr};
};

#endif  // CDCACHE_BASELINE_CACHE_H
