#include "lba_index.h"

bool SimpleLBAIndex::query(addr_t address, fp_t &fp) {
    auto it = this->table_.find(address);
    if (it == this->table_.end()) {
        return false;
    }
    fp = it->second;
    return true;
}

bool SimpleLBAIndex::insert(addr_t address, fp_t fp) {
    this->table_[address] = fp;
    return true;
}

bool SimpleLBAIndex::remove(addr_t address) {
    auto it = this->table_.find(address);
    if (it == this->table_.end()) {
        return false;
    }
    this->table_.erase(address);
    return true;
}
