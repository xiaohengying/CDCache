#include "page_manager.h"

bool StackedDiscretePageManager::allocate(size_t len, std::vector<addr_t> &blocks) {
    if (this->free_blocks_ < len) return false;
    for (int i = 0; i < len; i++) {
        addr_t addr;
        Assert(this->allocate_one(addr), "Allocation Failure");
        blocks.push_back(addr);
    }
    return true;
}
bool StackedDiscretePageManager::reclaim(addr_t addr) {
    Assert(addr < this->size_, "Invalid Block address %zu", addr);
    Assert(addr < begin_ || addr >= this->end_, "Try reclaim an un-allocated block");
    this->recycled_.push_back(addr);
    ++this->free_blocks_;
    return true;
}

bool StackedDiscretePageManager::allocate_one(addr_t &addr) {
    if (this->begin_ != this->end_) {
        addr = this->begin_;
        ++this->begin_;
        free_blocks_--;
        return true;
    } else if (!this->recycled_.empty()) {
        addr = this->recycled_.back();
        this->recycled_.pop_back();
        free_blocks_--;
        return true;
    }

    return false;
}

size_t StackedDiscretePageManager::free_blocks() { return this->free_blocks_; }