
#include "config.h"

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "nlohmann/json.hpp"
#include "utils.h"
#define GET_VALUE(T, name) this->name = j[#name].get<T>()
bool Config::initFromFile(const std::string &config_name) {
    using namespace nlohmann;

    try {
        std::ifstream f(config_name);
        if (!f.is_open()) {
            ERROR("Can not open file %s", config_name.c_str());
            return false;
        }

        json j;
        f >> j;

        auto random_name = std::to_string(get_timestamp());
        this->primary_name = j.value("primary_name", random_name + ".primary.dev");
        this->cache_name = j.value("cache_name", random_name + ".primary.dev");
        this->primary_size = j.value("primary_size", 128 * 1024);
        this->compression_method = j.value("compression_method", "lz77");
        this->cache_policy.policy = j.value("cache_policy", "lru");
        this->cache_policy.policy = j.value("promote_policy", "no");
        GET_VALUE(std::string, dataset_trace_path);
        GET_VALUE(std::string, dataset_data_path);
        GET_VALUE(size_t, dataset_block_size);
        GET_VALUE(std::string, output_path);
        GET_VALUE(size_t, cache_size);
        GET_VALUE(size_t, data_block_buffer_size);
        GET_VALUE(std::string, log_path);
        GET_VALUE(bool, run);
        GET_VALUE(std::string, result_path);
        GET_VALUE(std::string, cache_type);

        if (log_path != "stdout" && log_path != "~") {
            this->logger = fopen(this->log_path.c_str(), "w");
        }
        if (this->output_path != "~") {
            this->output = fopen(this->output_path.c_str(), "w");
        } else {
            this->output = nullptr;
        }
        return true;
    } catch (const std::exception &e) {
        ERROR("Can not create config: %s", e.what());
        return false;
    }
}
void Config::dump(FILE *fp) const {
    fprintf(fp, "Logger path :          %s \n", this->log_path.c_str());
    fprintf(fp, "Primary name:          %s\n", this->primary_name.c_str());
    fprintf(fp, "Primary size:          %zu KBytes\n", this->primary_size);
    fprintf(fp, "Cache name:            %s\n", this->cache_name.c_str());
    fprintf(fp, "Cache size:            %zu Bytes (%.2lf MiB)\n", this->cache_size,
            static_cast<double>(this->cache_size) / (1024.0 * 1024.0));
    fprintf(fp, "DataBlock buffer size:     %zu\n", this->data_block_buffer_size);
    fprintf(fp, "Cache policy:          %s\n", this->cache_policy.policy.c_str());
    fprintf(fp, "promote policy:         %s\n", this->cache_policy.promote_policy.c_str());
    fprintf(fp, "Cache type:            %s\n", this->cache_type.c_str());
    fprintf(fp, "Compression method:    %s\n", this->compression_method.c_str());
    printf("---------------------------------------------------\n");
    fprintf(fp, "Dataset Block size:    %zu Byte\n", this->dataset_block_size);
    fprintf(fp, "Dataset Trace path:    %s\n", this->dataset_trace_path.c_str());
    fprintf(fp, "Dataset Data path:     %s\n", this->dataset_data_path.c_str());
    fprintf(fp, "Output path:           %s\n", this->output_path.c_str());
    fprintf(fp, "Result path:           %s\n", this->result_path.c_str());
    printf("---------------------------------------------------\n");
}

nlohmann::json Config::toJson() {
    nlohmann::json j;
    j["log_path"] = this->log_path;
    j["primary_name"] = this->primary_name;
    j["primary_size"] = this->primary_size;
    j["cache_name"] = this->cache_name;
    j["cache_size"] = this->cache_size;
    j["data_block_buffer_size"] = this->data_block_buffer_size;
    j["compression_method"] = this->compression_method;
    j["data_block_size"] = this->dataset_block_size;
    j["trace_path"] = this->dataset_trace_path;
    j["data_path"] = this->dataset_data_path;
    return j;
}

bool Env::init(const std::string &path) { return this->c.initFromFile(path); }
void Env::startEvaluation() { start_time = get_timestamp(); }
void Env::finishEvaluation() {
    end_time = get_timestamp();
    nlohmann::json j;
    j["stat"] = this->s.toJson();
    j["config"] = this->c.toJson();
    j["start_time"] = this->start_time;
    j["end_time"] = this->end_time;
    j["stat"]["time"]["total"] = end_time - start_time;
    std::ofstream o(c.result_path);
    o << (j.dump(4));
    o.close();
}

nlohmann::json Stat::toJson() {
    nlohmann::json j;
    j["write_io_ctr"] = write_io_ctr;
    j["write_back_ctr"] = write_back_ctr;
    j["read_io"] = read_io;
    j["read_hit"] = read_hit;
    j["block_evict_ctr"] = block_evict_ctr;
    // for cdcache
    j["read_lose_by_lba"] = read_lose_by_lba;
    j["read_lose_by_fp"] = read_lose_by_fp;
    j["raw_data"] = raw_data;
    j["compressed_data"] = compressed_data;
    j["write_logic_blocks"] = write_logic_blocks;
    j["write_unique_blocks"] = write_unique_blocks;
    j["page_write"] = page_write;
    j["read_hit_ratio"] = static_cast<double>(read_hit) / static_cast<double>(read_io);

    j["promote"] = this->promote_cacheline;
    j["not_promote"] = this->not_promote_cacheline;
    j["evict_refs"] = this->evict_refs;

    auto logical_data_write = globalEnv().c.dataset_block_size * write_logic_blocks;
    auto actual_data_write = page_write * 512 * globalEnv().c.page_granularity;
    j["write_reduction_ratio"] = 1 - static_cast<double>(actual_data_write) / static_cast<double>(logical_data_write);
    j["compression_ratio"] = static_cast<double>(raw_data) / static_cast<double>(compressed_data);
    j["time"]["compression"] = time_compression / 1000000.0;
    j["time"]["compression_lookup_table"] = time_compression_lookup_table / 1000000.0;
    j["time"]["compression_deflate_data_blocks"] = time_compression_deflate / 1000000.0;
    j["time"]["compression_constructor"] = time_compression_constrcutor / 1000000.0;
    j["time"]["compression_encode"] = time_compression_encode / 1000000.0;
    j["time"]["deduplication"] = time_deduplication / 1000000.0;
    j["time"]["detection"] = time_detection / 1000000.0;
    j["time"]["evict"] = time_evict / 1000000.0;
    j["time"]["update_cache_line_index"] = time_update_cache_line_index / 1000000.0;
    j["time"]["evict_remove_cacheline"] = time_evict_remove_cacheline / 1000000.0;
    j["time"]["evict_update_index "] = time_evict_update_index / 1000000.0;

    // total
    j["time"]["total"] = time_process / 1000000.0;
    return j;
}

void Env::write(const char *fmt, ...) const {
    if (!c.output) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(c.output, fmt, args);
    fprintf(c.output, "\n");
    fflush(c.output);
}

Env &globalEnv() {
    static Env e;
    return e;
}
