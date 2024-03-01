#include "data_block_buffer.h"

void DataBlockBuffer::writeLogicalBlock(const LogicalBlock &b) {
    this->index_[b.address()] = this->data_.size();
    this->data_.push_back(b);
}

bool DataBlockBuffer::tryReadLogicalDataBlock(uint64_t address, LogicalBlock &block) {
    if (this->data_.empty()) return false;
    const auto it = this->index_.find(address);
    if (it == this->index_.end()) return false;
    block.setRawData(this->data_[it->second].raw_data());
    return true;
}

std::vector<LogicalBlock> DataBlockBuffer::popAll() {
    // avoid memory copy
    auto res = std::move(this->data_);
    this->data_ = std::vector<DataBlock>();
    this->index_.clear();
    return res;
}
