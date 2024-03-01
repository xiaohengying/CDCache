//获取trace的基本信息
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "io_request.h"
#include "trace_reader.h"
#include "utils.h"

namespace {
    std::vector<std::string> parse_seq_files(const std::string& name) {
        return {name};
        // auto it1 = name.find("{");
        // auto it2 = name.find("}");
        // if (it1 == std::string::npos || it2 == std::string::npos) return {name};
        // auto quote = name.find("_", it1);
        // if (quote == std::string::npos) return {name};
        // auto n1 = name.substr(it1 + 1, quote - it1 - 1);
        // auto n2 = name.substr(quote + 1, it2 - quote - 1);
        // auto start = strtol(n1.c_str(), nullptr, 10);
        // auto end = strtol(n2.c_str(), nullptr, 10);
        // std::vector<std::string> res;

        // for (int i = start; i <= end; i++) {
        //     auto file = name.substr(0, it1) + std::to_string(i) + name.substr(it2 + 1);
        //     res.push_back(file);
        // }

        // return res;
    }

}  // namespace

struct MiddleItem {
    addr_t lba{};
    IOType io_type{};
    std::string uid{};  // block_id
    size_t block_size{4096};
};

std::vector<MiddleItem> readFromFIU(const std::vector<std::string>& files, size_t block_size) {
    std::vector<MiddleItem> items;
    for (auto& file : files) {
        auto trace = readFIUTraceFromFile(file, block_size);
        for (auto& record : trace) {
            MiddleItem item;
            item.block_size = record.block_size;
            item.io_type = record.io_type;
            item.lba = record.lba;
            item.uid = record.hash;
            items.push_back(item);
        }
    }
    return items;
}
std::vector<MiddleItem> readFromSYN(const std::vector<std::string>& files, size_t block_size) {
    std::vector<MiddleItem> items;
    for (auto& file : files) {
        auto trace = readSYNTraceFromFile(file, block_size);
        for (auto& record : trace) {
            MiddleItem item;
            item.block_size = record.block_size;
            item.io_type = record.io_type;
            item.lba = record.lba;
            item.uid = record.block_hash;
            items.push_back(item);
        }
    }

    return items;
}

std::vector<MiddleItem> readFromSYSTOR17(const std::vector<std::string>& files, size_t block_size) {
    std::vector<MiddleItem> items;
    for (auto& file : files) {
        auto trace = readSystorTraceFromFile(file, block_size);
        for (auto& record : trace) {
            MiddleItem item;
            item.block_size = record.block_size;
            item.io_type = record.io_type;
            item.lba = record.lba;
            //! 没有方块信息，只能这样了（或者重新整个映射方案？）
            item.uid = std::to_string(record.lba);
            items.push_back(item);
        }
    }
    return items;
}

std::vector<MiddleItem> readFromMSR(const std::vector<std::string>& files, size_t block_size) {
    std::vector<MiddleItem> items;
    for (auto& file : files) {
        auto trace = readMSRTraceFromFile(file, block_size);
        for (auto& record : trace) {
            MiddleItem item;
            item.block_size = record.block_size;
            item.io_type = record.io_type;
            item.lba = record.lba;
            //! 没有方块信息，只能这样了（或者重新整个映射方案？）
            item.uid = std::to_string(record.lba);
            items.push_back(item);
        }
    }
    return items;
}

template <typename KEY>
void evaluateDistribution(const std::unordered_map<KEY, int>& counter, int N) {
    using KVType = std::pair<KEY, int>;
    std::vector<KVType> array;
    int total = 0;
    for (auto& kv : counter) {
        array.push_back(kv);
        total += kv.second;
    }

    std::sort(array.begin(), array.end(), [](const KVType& p1, const KVType& p2) { return p1.second > p2.second; });
    const int part_size = array.size() / N;
    if (part_size == 0) {
        printf(" - Is N too large?");
        return;
    }

    int ctr = 0;
    int value = 0;
    std::vector<double> res;
    for (auto& kv : array) {
        ctr++;
        value += kv.second;
        if (ctr == part_size) {
            // printf("%.3lf\n", value * 1.0 / total);
            res.push_back(value * 1.0 / total);
            ctr = 0;

            value = 0;
        }
    }

    if (ctr > 0) {
        res.push_back(value * 1.0 / total);
        // printf("%.3lf\n", value * 1.0 / total);
    }
}

void analyze(const std::vector<MiddleItem>& traces) {
    if (traces.empty()) {
        ERROR("Empty trace, please check the input path");
        return;
    }

    uint64_t w = 0;
    uint64_t r = 0;
    uint64_t reduction_write = 0;
    uint64_t rhit = 0;

    // LBA and data block counter
    std::unordered_map<addr_t, int> lba_counter;
    std::unordered_map<std::string, int> block_counter;

    std::set<fp_t> cache;
    std::unordered_set<std::string> block_storage;

    auto block_size = 0ull;
    size_t invalid_lba_count{0};
    for (auto& t : traces) {
        if (t.lba % t.block_size != 0) {
            invalid_lba_count++;
        }

        block_size = t.block_size;
        lba_counter[t.lba]++;
        block_counter[t.uid]++;

        if (t.io_type == Write) {
            w++;
            cache.insert(t.lba);
            if (block_storage.count(t.uid)) {
                reduction_write++;
            } else {
                block_storage.insert(t.uid);
            }
        } else if (t.io_type == Read) {
            r++;
            if (cache.count(t.lba)) {
                rhit++;
            } else {
                // write back
                cache.insert(t.lba);
            }
        }
    }

    printf("BASIC INFO:\n");
    printf(" - Block size: %llu\n", block_size);
    printf(" - Read count: %zu ( %.3f %%)\n", r, r * 100.0 / (r + w));
    printf(" - Write count: %zu ( %.3f %%)\n", w, w * 100.0 / (r + w));
    printf(" - Reduction write count: %zu ( Write reduction ratio %.3f %%)\n", reduction_write,
           reduction_write * 100.0 / (w));
    printf(" - Data space: %.3lf MiB (%zu Bytes)\n", block_counter.size() * traces[0].block_size / (1024.0 * 1024.0),
           block_counter.size() * traces[0].block_size);
    printf(" - Max hit ratio: %.3lf %%\n", rhit * 100.0 / r);
    printf(" - Working set size: %.3lf MB (%llu byte)\n", lba_counter.size() * block_size * 1.0 / (1024 * 1024),
           lba_counter.size() * block_size);
    printf(" - Invalid LBA cont: %zu\n", invalid_lba_count);

    // printf("LBA ACCESS PATTERN:\n");
    evaluateDistribution<uint64_t>(lba_counter, 100);
    // printf("BLOCK ACCESS PATTERN:\n");
    evaluateDistribution<std::string>(block_counter, 100);
}

int main(int argc, const char* argv[]) {
    if (argc < 4) {
        printf("Use: ./trace_analyzer <block_size> <input_type> [trace_files]");
        return -1;
    }

    auto block_size = strtol(argv[1], nullptr, 10);
    auto input_trace_type = std::string(argv[2]);
    std::vector<std::string> files;
    for (int i = 3; i < argc; i++) {
        auto fs = parse_seq_files(argv[i]);
        files.insert(files.end(), fs.begin(), fs.end());
    }
    if (block_size <= 0 || (block_size & (block_size - 1)) != 0) {
        fprintf(stderr, "Error wrong block_size (must > 0 and pow of 2)\n");
        return -1;
    }

    printf("Block size: %zu\n", block_size);
    printf("Input trace type: %s\n", input_trace_type.c_str());
    printf("Here are the input traces:\n");
    for (auto& file : files) {
        printf(" - %s\n", file.c_str());
    }

    std::vector<MiddleItem> items;
    if (input_trace_type == "fiu") {
        items = readFromFIU(files, block_size);
    } else if (input_trace_type == "syn") {
        items = readFromSYN(files, block_size);
    } else if (input_trace_type == "systor17") {
        items = readFromSYSTOR17(files, block_size);
    } else if (input_trace_type == "msr") {
        items = readFromMSR(files, block_size);
    } else {
        fprintf(stderr, "Invalid Trace type, please use <fiu> / <syn> / <systor17>");
        return -1;
    }
    analyze(items);
    return 0;
}