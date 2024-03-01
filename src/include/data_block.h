#ifndef CDCACHE_DATA_BLOCK_H
#define CDCACHE_DATA_BLOCK_H

#include <memory>
#include <vector>

#include "utils.h"
#include "xxhash.h"

class DataBlock {
   public:
    [[nodiscard]] inline addr_t address() const { return this->address_; }

    [[nodiscard]] inline size_t raw_len() const { return this->raw_len_; }

    inline void set_raw_len(size_t len) { this->raw_len_ = len; }
    inline void set_raw_fp(fp_t fp) { this->raw_fp_ = fp; }
    inline void set_comp_fp(fp_t fp) { this->comp_fp_ = fp; }
    inline void set_address(addr_t address) { this->address_ = address; }
    [[nodiscard]] inline const std::vector<byte_t> &raw_data() const { return this->raw_data_; }

    [[nodiscard]] inline const std::vector<byte_t> &comp_data() const { return this->comp_data_; }

    inline void setRawDuplicated(bool duplicated) { this->raw_duplicated_ = duplicated; }

    [[nodiscard]] inline bool raw_duplicated() const { return this->raw_duplicated_; }

    inline void setCompDuplicated(bool duplicated) { this->comp_duplicated_ = duplicated; }

    [[nodiscard]] inline bool comp_duplicated() const { return this->comp_duplicated_; }

    [[nodiscard]] inline uint64_t raw_fp() const { return this->raw_fp_; }

    [[nodiscard]] inline uint64_t comp_fp() const { return this->comp_fp_; }
    DataBlock(const byte_t *bytes, size_t len, addr_t addr);

    DataBlock(std::vector<byte_t> bytes, addr_t addr);
    DataBlock() = default;

    void calRawFP();

    inline void setCompData(const std::vector<byte_t> &data) { this->comp_data_ = data; }
    inline void setRawData(const std::vector<byte_t> &data) { this->raw_data_ = data; }
    void calCompFP();

    void set_external_cacheline_addr(addr_t address) { this->external_cacheline_addr_ = address; }
    [[nodiscard]] inline addr_t external_cacheline_addr() const { return this->external_cacheline_addr_; }

   private:
    addr_t address_{0xffffffff};  // LBA

    // info of raw data block
    uint64_t raw_fp_{};               // Fingerprint of raw data_block
    bool raw_duplicated_ = false;     // Are raw data_block duplicated?
    std::vector<byte_t> raw_data_{};  // Raw data_block data
    size_t raw_len_ = 0;

    // info of compressed data block
    uint64_t comp_fp_{0xffffffff};     // Fingerprint of compressed data_block
    bool comp_duplicated_ = false;     // are compressed data_block duplicated?
    std::vector<byte_t> comp_data_{};  // Compressed data_block data;
    // Others info
    addr_t external_cacheline_addr_{0xffffffff};
};

typedef DataBlock LogicalBlock;

#endif  // CDCACHE_CHUNK_H
