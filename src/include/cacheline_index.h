#ifndef CDCACHE_CACHELINE_INDEX_H
#define CDCACHE_CACHELINE_INDEX_H

#include <algorithm>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "abstract_counter.h"
#include "cache_policy.h"
#include "deletable_bloom_filter.h"
#include "fp_index.h"
#include "utils.h"

using cacheline_id_t = uint64_t;
struct CachelineIndexData {
    size_t time_stamp_ = 0;
    std::vector<addr_t> allocation_pages_;
    std::unordered_set<addr_t> external_refs_;
};

class CachelineIndex {
   public:
    CachelineIndex();
    std::pair<cacheline_id_t, CachelineIndexData> fetchOldestCacheline();
    void remove(cacheline_id_t id);
    bool query(cacheline_id_t id, CachelineIndexData &data, bool promote);
    void insert(cacheline_id_t id, const CachelineIndexData &data, bool promote);

    void addRefToCacheline(cacheline_id_t id, cacheline_id_t ref, bool promote);

    ~CachelineIndex() { delete this->policy_; }

   private:
    AbstractCachePolicy<cacheline_id_t> *policy_;
    std::unordered_map<cacheline_id_t, CachelineIndexData> data_;
};

#endif  // CDCACHE_CACHELINE_INDEX_H
