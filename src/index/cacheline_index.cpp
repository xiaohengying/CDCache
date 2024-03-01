#include "cacheline_index.h"

#include <utility>

#include "config.h"

CachelineIndex::CachelineIndex() { this->policy_ = Env::policyInstance<cacheline_id_t>(); }
std::pair<cacheline_id_t, CachelineIndexData> CachelineIndex::fetchOldestCacheline() {
    auto oldests = this->policy_->oldests(5);

    int MIN_REF = 1000000;
    cacheline_id_t oldest{static_cast<cacheline_id_t>(-1)};
    for (auto id : oldests) {
        auto it = this->data_.find(id);
        if (it != this->data_.end() && MIN_REF > it->second.external_refs_.size()) {
            MIN_REF = it->second.external_refs_.size();
            oldest = id;
        }
    }

    auto it = this->data_.find(oldest);
    Assert(it != this->data_.end(), "Inconsistent data between cache line index and cache policy");
    globalEnv().s.evict_refs += it->second.external_refs_.size();

    return {oldest, it->second};
}

void CachelineIndex::remove(cacheline_id_t id) {
    this->policy_->evict(id);
    this->data_.erase(id);
}

bool CachelineIndex::query(cacheline_id_t id, CachelineIndexData &data, bool promote) {
    auto it = this->data_.find(id);
    if (it == this->data_.end()) {
        return false;
    } else {
        data = it->second;
        if (promote) {
            this->policy_->promote(id);
        }
        return true;
    }
}
void CachelineIndex::insert(cacheline_id_t id, const CachelineIndexData &data, bool promote) {
    this->data_[id] = data;
    this->policy_->insert(id);
    if (promote) {
        this->policy_->promote(id);
    }
}

void CachelineIndex::addRefToCacheline(cacheline_id_t id, cacheline_id_t ref, bool promote) {
    // CachelineIndexData idx;
    // this->query(id, idx, promote);
    // idx.external_refs_.insert(ref);
    // this->insert(id, idx, false);
    auto it = this->data_.find(id);
    if (it == this->data_.end()) return;
    it->second.external_refs_.insert(ref);
    this->policy_->promote(id);
}
