#ifndef CDCACHE_SSD_PROXY_H
#define CDCACHE_SSD_PROXY_H

#include <cstdint>
#include <functional>
#include <map>

#include "cacheline_index.h"
#include "data_block.h"
#include "device.h"
#include "page_manager.h"
#include "utils.h"

struct ModifyCommand {
    enum Action { Append, ModifyRef };
    Action action;
    fp_t fp{};                   // 要修改的FP
    addr_t new_external_addr{};  // 新的引用cacheline id // modify ref时有效
    std::vector<byte_t> data;    // 要写入的数据//可能为空 //append时有效

    // for debug
    std::string toString() const {
        if (this->action == Action::Append) {
            return "Append cfp=" + std::to_string(fp) + " to current cacheline";
        } else {
            return "Modify cfp=" + std::to_string(fp) + " to new cacheline " + std::to_string(new_external_addr);
        }
    }

    static ModifyCommand appendCmd(const std::vector<byte_t> &data, fp_t fp) {
        ModifyCommand command;
        command.fp = fp;
        command.data = data;
        command.action = Action::Append;
        command.new_external_addr = -1;
        return command;
    }

    static ModifyCommand modifyCmd(fp_t fp, addr_t new_external_addr) {
        ModifyCommand command;
        command.fp = fp;
        command.action = Action::ModifyRef;
        command.new_external_addr = new_external_addr;
        return command;
    }
};

struct CachelineDataBlockInfo {
    fp_t comp_fp;
    fp_t raw_fp;
    uint32_t raw_len;
    uint8_t type;
    // uint32_t len;
    uint32_t pos_index;
    uint64_t external_address;
    // TODO: some other metadata;
    friend bool operator==(const CachelineDataBlockInfo &lhs, const CachelineDataBlockInfo &rhs) {
        return lhs.comp_fp == rhs.comp_fp && lhs.raw_fp == rhs.raw_fp &&
               lhs.type == rhs.type
               //  && lhs.len == rhs.len
               && lhs.pos_index == rhs.pos_index && lhs.external_address == rhs.external_address;
    }
};

/***

 */

struct CachelineDataLayout {
    uint32_t offset;
    uint32_t len;
    friend bool operator==(const CachelineDataLayout &lhs, const CachelineDataLayout &rhs) {
        return lhs.offset == rhs.offset && lhs.len == rhs.len;
    }
};

struct CachelineHeader {
    uint32_t header_len = 0;  // cacheline  header
    uint32_t data_blocks_info_len = 0;
    uint32_t data_layout_len = 0;
    uint32_t data_blocks_data_len = 0;
    uint8_t data_blocks_number = 0;
    [[nodiscard]] inline size_t total_len() const {
        return header_len + data_blocks_info_len + data_layout_len + data_blocks_data_len;
    }
    friend bool operator==(const CachelineHeader &lhs, const CachelineHeader &rhs) {
        return lhs.header_len == rhs.header_len && lhs.data_blocks_info_len == rhs.data_blocks_info_len &&
               lhs.data_layout_len == rhs.data_layout_len && lhs.data_blocks_data_len == rhs.data_blocks_data_len &&
               lhs.data_blocks_number == rhs.data_blocks_number;
    }
};

// cache line info
struct Cacheline {
    CachelineHeader header;
    std::vector<CachelineDataBlockInfo> data_blocks_info;
    std::vector<CachelineDataLayout> data_layout;
    std::vector<std::vector<byte_t>> data_blocks_data;

    static size_t get_estimate_metadata_len();

    static Cacheline fromDataBlocks(const std::vector<DataBlock> &data_blocks);

    size_t serialize(std::ostream &s) const;

    void deserialize(std::istream &s, bool metadata_only);

    [[nodiscard]] inline size_t size() const { return this->header.total_len(); }
    void dumpToLogger() const;

    double get_utilization_rate() const;

    friend bool operator==(const Cacheline &lhs, const Cacheline &rhs) {
        return lhs.header == rhs.header && lhs.data_blocks_info == rhs.data_blocks_info &&
               lhs.data_layout == rhs.data_layout && lhs.data_blocks_data == rhs.data_blocks_data;
    }
};

/**
 Organize and manage data layout inside SSD(bock device)
 */

class SSDProxy {
   public:
    SSDProxy(const std::string &name, size_t size);

    ~SSDProxy();

    // Write a batch of data blocks to the cache device and return whether the writing is successful.
    bool writeDataBlocks(std::vector<DataBlock> &data_blocks, CachelineIndexData &data);

    // Write a cache line to the cache device and return whether the write is successful.
    bool writeCacheline(Cacheline &cacheline, CachelineIndexData &data);

    // modify a cacheline in device (used when eviction)
    bool modifyCacheline(const std::vector<ModifyCommand> &commands, CachelineIndexData &data);

    // bool readDataBlocks(std::vector<DataBlock> &data_blocks, const CachelineIndexData &data);

    // Read cache line from cache device based on metadata
    bool readCacheline(Cacheline &cacheline, const CachelineIndexData &data);

    // Read cache line metadata from cache device based on metadata
    bool readMetadataOnly(Cacheline &cachline, const CachelineIndexData &data);

    inline AbstractPageManager *allocation_manager() { return manager_; }

    // Cache device free space
    size_t free_blocks() { return this->manager_->free_blocks(); }

    // remove a cacheline from device
    void removeCacheline(std::pair<addr_t, CachelineIndexData> &cur, std::map<addr_t, CachelineIndexData> &refs,
                         std::map<fp_t, addr_t> &moved);

   private:
    bool write_allocated_page(addr_t address, const byte_t *data);
    bool read_allocated_page(addr_t address, std::vector<byte_t> &data);

    bool open_device(const std::string &name, size_t size);

   private:
    AbstractPageManager *manager_{nullptr};  // page allocation
    AbstractBlockDevice *device_;            // device

    // The basic unit of page allocation
    const size_t PG_SZ{512};
};

#endif  // CDCACHE_SSD_PROXY_H
