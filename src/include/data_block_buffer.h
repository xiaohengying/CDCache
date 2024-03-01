#ifndef CDCACHE_DATA_BLOCK_BUFFER_H
#define CDCACHE_DATA_BLOCK_BUFFER_H

#include <cstdint>
#include <cstdlib>
#include <unordered_map>

#include "data_block.h"

class DataBlockBuffer {
   public:
    explicit DataBlockBuffer(size_t capacity) : capacity_(capacity) { LOGGER("capacity is %zu", this->capacity_); }

    DataBlockBuffer() = delete;

    inline bool empty() const { return this->data_.empty(); }

    inline bool isFull() const { return this->data_.size() >= this->capacity_; }

    void writeLogicalBlock(const LogicalBlock &b);

    void clear() { this->data_.clear(); }

    bool tryReadLogicalDataBlock(uint64_t address, LogicalBlock &block);

    std::vector<LogicalBlock> popAll();
    size_t size() const { return this->data_.size(); }

   private:
    const size_t capacity_;
    std::vector<LogicalBlock> data_;  // cached data
    std::unordered_map<addr_t, size_t> index_;
};

#endif  // CDCACHE_CHUNKBUFFER_H
