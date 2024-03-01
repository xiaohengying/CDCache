#include "fp_index.h"

bool SimpleFPIndex::query(fp_t fp, FPIndexData &data) {
    auto it = this->table_.find(fp);
    if (it == this->table_.end()) {
        return false;
    }
    data = it->second;
    return true;
}

bool SimpleFPIndex::insert(fp_t fp, FPIndexData data) {
    this->table_[fp] = data;
    return false;
}

bool SimpleFPIndex::remove(fp_t fp) {
    this->table_.erase(fp);
    return false;
}
size_t SimpleFPIndex::size() { return this->table_.size(); }
SimpleFPIndex::~SimpleFPIndex() = default;