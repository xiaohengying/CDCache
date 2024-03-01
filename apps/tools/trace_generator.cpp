// 根据trace和文件生成共给CDCache和Austere公平比较的数据集
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

//
// Created by xhy on 2022/12/1.
//
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "io_request.h"
#include "main_compressor.h"
#include "pair_serializer.h"
#include "trace_reader.h"
// 输入参数，为Austere和CDCache输出公平的trace
// 1994784768 32768 W 9d7b1b73375f7b3ba03429ad6a1fef28c93e5a18 2.115548

namespace {
    std::vector<std::string> parse_seq_files(const std::string& name) {
        auto it1 = name.find("{");
        auto it2 = name.find("}");
        if (it1 == std::string::npos || it2 == std::string::npos) return {name};
        auto quote = name.find(",", it1);
        if (quote == std::string::npos) return {name};
        auto n1 = name.substr(it1, quote - it1);
        auto n2 = name.substr(quote, it2 - quote);
        std::cout << "debug" << n1 << "|" << n2;

        return {name};
    }

}  // namespace

struct AustereTraceItem {
    size_t lba{};
    size_t block_size{};
    char io_type{};
    std::string block_hash;
    double compression_ratio{};
};

struct CDCacheTraceItem {
    size_t lba{};
    char io_type{};
    size_t unique_index{};
    size_t data_block_index{};
};

struct HashItem {
    size_t unique_index{0};  // 唯一数据块ID
    size_t offset{0};        // 数据块在文件中的偏移量，以block_size为单位
    std::string hash;        // 哈希
    size_t block_size{0};    // 块大小
    double comp_ratio{0};    // 压缩率
};

/*
 * 每一行是一个唯一的数据块

 */
std::vector<HashItem> read_hash_file(const std::string& path) {
    std::ifstream in(path);
    std::vector<HashItem> res;
    Assert(in.is_open(), "Can not open file %s", path.c_str());
    std::string line;
    while (std::getline(in, line)) {
        HashItem item;
        std::istringstream iss(line);
        if (!(iss >> item.unique_index >> item.offset >> item.block_size >> item.hash >> item.comp_ratio)) {
            Assert(false, "Can not read file");
        }
        res.emplace_back(item);
    }
    if (res.empty()) {
        ERROR("File %s is empty\n", path.c_str());
    } else {
        LOGGER("Read %zu hash items from %s successfully (%.3lf MBbytes)", res.size(), path.c_str(),
               res.size() * res[0].block_size * 1.0 / (1024 * 1024));
    }

    return res;
}

struct MiddleItem {
    addr_t lba{};
    IOType io_type{};
    std::string uid{};  // block_id
    size_t block_size{4096};
    // for output
    CDCacheTraceItem cd_item;
    AustereTraceItem austere_item;
};

void dump_trace(std::vector<MiddleItem>& traces, const std::string& name) {
    std::ofstream cds(name + ".cd.txt");
    std::ofstream aus(name + ".austere.txt");
    Assert(cds.is_open() && aus.is_open(), "Can not open file %s", name.c_str());

    for (auto& trace : traces) {
        auto& au = trace.austere_item;
        aus << au.lba << " " << au.block_size << " " << au.io_type << " " << au.block_hash << " "
            << au.compression_ratio << "\n";
        auto& cd = trace.cd_item;
        cds << cd.lba << " " << cd.io_type << " " << cd.data_block_index << "\n";
    }
    cds.close();
    aus.close();
    printf(" - %s\n", (name + ".cd.txt").c_str());
    printf(" - %s\n", (name + ".austere.txt").c_str());
}

void process(std::vector<MiddleItem>& middle_trace, const std::string& data_path, const std::string& output) {
    if (middle_trace.empty()) {
        ERROR("original traces is empty");
        return;
    }

    auto data_block_size = middle_trace.front().block_size;
    LOGGER("DataBlock size is %zu", block_size);

    size_t global_index_ctr = 0;

    std::unordered_map<std::string, size_t> data_block_mapper;
    std::unordered_set<addr_t> wss_space_ctr;
    uint64_t max_lba = 0;

    for (auto& item : middle_trace) {
        // 统计WSS
        wss_space_ctr.insert(item.lba);
        // 统计MAX LBA
        if (max_lba < item.lba) {
            max_lba = item.lba;
        }

        auto it = data_block_mapper.find(item.uid);
        // 在哈希文件中的第几个表项
        size_t idx = 0;
        if (it == data_block_mapper.end()) {  // 新的
            idx = global_index_ctr;
            global_index_ctr++;
            data_block_mapper[item.uid] = idx;
        } else {
            idx = it->second;
        }

        CDCacheTraceItem cd_item;
        AustereTraceItem aus_item;
        cd_item.lba = item.lba;
        cd_item.io_type = item.io_type == Read ? 'R' : 'W';
        aus_item.lba = item.lba;
        aus_item.io_type = cd_item.io_type;
        aus_item.block_size = item.block_size;
        // 这里的idx相当于是为uid重新编码了。就是数据块ID
        cd_item.unique_index = idx;

        item.cd_item = cd_item;
        item.austere_item = aus_item;
    }

    auto data_hashes = read_hash_file(data_path);
    Assert((!data_hashes.empty()) && data_hashes.front().block_size == data_block_size,
           "Trace and Dataset has different data_block size [%zu != %zu]", data_hashes.front().block_size,
           data_block_size);
    LOGGER("Total %zu unique data_blocks  (%.3lf Mbytes) needed in trace", hash_index.block_size(),
           hash_index.block_size() * data_block_size * 1.0 / (1024 * 1024));
    Assert(data_block_mapper.size() <= data_hashes.size(), "Too little data was provided!!");

    for (auto& trace : middle_trace) {
        auto idx = trace.cd_item.unique_index;
        // 重新设置hash和compression ratio
        trace.austere_item.compression_ratio = data_hashes[idx].comp_ratio;
        trace.austere_item.block_hash = data_hashes[idx].hash;
        // 读写位置
        trace.cd_item.data_block_index = data_hashes[idx].offset;
    }

    printf("Begin dump trace...\n");
    printf("Data set space: %.3lf MB\n", data_block_mapper.size() * data_block_size / (1024.0 * 1024.0));
    printf("Primary device size: %.3lf MB\n", max_lba / (1024.0 * 1024.0));
    printf("Working set size : %.3lf MB\n", wss_space_ctr.size() * data_block_size / (1024.0 * 1024.0));
    dump_trace(middle_trace, output);
    printf("Generate finished...\n");
}
/*
下面是转换模块，读取trace并转换为对应的Middle Item
*/

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

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("Use trace_generator <hash_file> <block_size> <input_type>  <output_file> [hash_files...]\n");
        return -1;
    }

    auto hash_file_name = std::string(argv[1]);
    auto block_size = strtol(argv[2], nullptr, 10);
    auto input_trace_type = std::string(argv[3]);
    auto output_file_name = std::string(argv[4]);
    std::vector<std::string> files;
    for (int i = 5; i < argc; i++) {
        files.emplace_back(argv[i]);
    }

    if (block_size <= 0 || (block_size & (block_size - 1)) != 0) {
        fprintf(stderr, "Error wrong block_size (must > 0 and pow of 2)\n");
        return -1;
    }

    printf("Hash file name: %s\n", hash_file_name.c_str());
    printf("Block size: %zu\n", block_size);
    printf("Input trace type: %s\n", input_trace_type.c_str());
    printf("Output file_name: %s\n", output_file_name.c_str());
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
        fprintf(stderr, "Invalid Trace type, please use <fiu> / <syn> / <systor17> / <msr>");
        return -1;
    }
    process(items, hash_file_name, output_file_name);
    return 0;
}