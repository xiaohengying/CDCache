//
// Created by xhy on 2022/12/1.
//

#include <openssl/sha.h>

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "main_compressor.h"
#include "utils.h"

std::vector<byte_t> read_trace_data(const std::string& path) {
    std::ifstream i(path, std::ios::binary | std::ios::ate);
    Assert(i.is_open(), "can not open file %s", path.c_str());
    auto trace_size = i.tellg();
    i.seekg(0, std::ios::beg);
    std::vector<byte_t> buffer(trace_size);
    if (!i.read((char*)buffer.data(), trace_size)) {
        ERROR("can not read data from file %s", path.c_str());
    }
    return buffer;
}

// [序号] [原始数据的序号] 大小 SHA1哈希 压缩率
void process(const std::string& file, size_t data_block_size) {
    auto data = read_trace_data(file);
    auto total_data_block_num = data.size() / data_block_size;

    std::unordered_set<std::string> hash_table;

    for (size_t i = 0; i < total_data_block_num; i++) {
        auto ch = std::vector<byte_t>(data.begin() + i * data_block_size, data.begin() + (i + 1) * data_block_size);
        std::string sha1(20, 0);
        SHA1(ch.data(), ch.size(), (unsigned char*)(sha1.data()));
        if (hash_table.count(sha1)) {  // duplcated data, noting todo
            continue;
        }

        size_t unique_data_index = hash_table.size();
        hash_table.insert(sha1);

        auto cmp_data = encodeLz77(MainCompressor::lz77CompressByteArray(ch));
        auto comp_ratio = static_cast<double>(ch.size()) * 1.0 / static_cast<double>(cmp_data.size());

        // 给当前的压缩算法擦屁股
        if (comp_ratio < 1.0) {
            comp_ratio = 1.0;
        }

        auto hash = str_to_hex(sha1);
        printf("%zu %zu %zu %s %.3lf\n", unique_data_index, i, data_block_size, hash.c_str(), comp_ratio);
    }
}

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        printf("        Data hash generator\n\nBuild time %s %s\n\n", __DATE__, __TIME__);
        printf(
            "This function is used to divide the file into blocks according to the given size,and\n"
            "calculate the SHA1 hash of each block and its actual compression ratio (using LZ77)\n\n");
        printf("\n\nUse ./hash_generator [file path] [data_block size]\n");
        return 1;
    }
    std::string file_path = argv[1];
    int data_block_size = static_cast<int>(strtol(argv[2], nullptr, 10));
    process(file_path, data_block_size);
    return 0;
}
