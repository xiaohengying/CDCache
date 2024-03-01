#ifndef CDCACHE_CONFIG_H
#define CDCACHE_CONFIG_H

#include <bits/types/FILE.h>
#include <bits/types/timer_t.h>
#include <sys/types.h>

#include <cstdarg>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "cache_policy.h"
#include "deletable_bloom_filter.h"
#include "nlohmann/json.hpp"

// constexpr uint32_t BLOCK_SIZE = 512;

struct Stat {
    // basic stat
    // runtime
    uint64_t write_io_ctr{0};    // in trace
    uint64_t write_back_ctr{0};  // caused by read miss
    uint64_t read_io{0};
    uint64_t read_hit{0};
    uint64_t block_evict_ctr{0};
    // for cdcache
    uint64_t read_lose_by_lba{0};
    uint64_t read_lose_by_fp{0};
    // compression_ratio
    uint64_t raw_data{0};  // The total size of data involved in compression = block size * write_ctr
    uint64_t compressed_data{0};
    // dedup ratio
    uint64_t write_logic_blocks{0};
    uint64_t write_unique_blocks{0};
    uint64_t page_write{0};  // The number of pages written to the cache block device (slightly larger than the total
                             // compressed data size due to metadata and padding)

    uint64_t evict_refs{0};  //
    // time break down
    uint64_t time_compression{0};
    uint64_t time_compression_lookup_table{0};
    uint64_t time_compression_deflate{0};
    uint64_t time_compression_encode{0};
    uint64_t time_compression_constrcutor{0};
    uint64_t time_process{0};

    uint64_t time_deduplication{0};
    uint64_t time_detection{0};
    uint64_t time_evict{0};
    uint64_t time_evict_remove_cacheline{0};
    uint64_t time_evict_update_index{0};
    uint64_t time_update_cache_line_index{0};

    // promote info
    uint64_t promote_cacheline{0};
    uint64_t not_promote_cacheline{0};

    nlohmann::json toJson();
};

struct CachePolicy {
    std::string policy{"lfu"};
    std::string promote_policy{"no"};
};

class Config {
   public:
    Config() = default;
    bool run = true;

    std::string primary_name;
    std::string cache_name;
    size_t primary_size{};
    size_t cache_size{};
    size_t data_block_buffer_size{8};
    std::string dataset_trace_path;
    std::string dataset_data_path;
    std::string cache_type;
    size_t dataset_block_size = 4096;
    std::string output_path;  // key info output path  (for debug)
    std::string log_path;     // log path
    std::string result_path;  // result path
    FILE *logger{stdout};     //
    FILE *output{nullptr};    //
    nlohmann::json result_cache;
    std::string compression_method;
    CachePolicy cache_policy;
    bool use_cache = true;        //
    bool use_huffman = false;     //
    size_t page_granularity = 1;  //(page size  = page_granularity * 512)

    nlohmann::json toJson();
    bool initFromFile(const std::string &fileName);
    void dump(FILE *fp = stdout) const;
};

struct Env;
Env &globalEnv();
struct Env {
    Config c;
    Stat s;
    time_t start_time{};
    time_t end_time{};
    bool init(const std::string &path);
    void startEvaluation();
    void finishEvaluation();

    // Global processing progress
    size_t index_{0};
    inline void update_index() { this->index_++; }
    [[nodiscard]] inline size_t index() const { return this->index_; }

    void write(const char *fmt, ...) const;

    Env() {}
    ~Env() {}

    template <typename T>
    static AbstractCachePolicy<T> *policyInstance() {
        return new LRUPolicy<T>();
        // const auto policy = globalEnv().c.cache_policy.policy;
        // if (policy == "lru") {
        //     return new LRUPolicy<T>();
        // } else if (policy == "lfu") {
        //     return new LFUPolicy<T>();
        // } else if (policy == "arc") {
        //     return new ARCPolicy<T>();
        // } else if (policy == "lirs") {
        //     return new LIRSPolicy<T>();
        // } else if (policy == "2q") {
        //     return new TwoQPolicy<T>();
        // } else {
        //     throw std::runtime_error("Unknown Cache policy type: " + std::string(policy));
        // }
    };
};
// Config &globalConfig();

#endif
