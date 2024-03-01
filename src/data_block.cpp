#include "data_block.h"

#include <cstring>
#include <utility>

DataBlock::DataBlock(const byte_t *bytes, size_t len, addr_t addr) : address_(addr), raw_len_(len) {
    this->raw_data_ = std::vector<byte_t>(bytes, bytes + len);
    this->calRawFP();
}

DataBlock::DataBlock(std::vector<byte_t> bytes, addr_t addr) : raw_data_(std::move(bytes)), address_(addr) {
    this->calRawFP();
    this->raw_len_ = this->raw_data_.size();
}

void DataBlock::calRawFP() { this->raw_fp_ = XXH64(this->raw_data_.data(), this->raw_data_.size(), 0); }
void DataBlock::calCompFP() { this->comp_fp_ = XXH64(this->comp_data_.data(), this->comp_data_.size(), 0); }
