#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <ostream>
#include <system_error>
#include <thread>
#include <vector>

#include "config.h"
#include "data_block.h"
#include "lz77_compressor.h"
#include "pair_serializer.h"
#include "utils.h"

namespace {

    //=====================Multi thread==========================

    constexpr auto THREAD_NUM = 16;

    std::vector<std::pair<int, int>> createSlice(int begin, int end, int thread_num) {
        std::vector<std::pair<int, int>> res;
        const auto range = (end - begin);
        auto part_size = range / thread_num;
        Assert(part_size >= 2, "The spilt part is too small");
        if (range % thread_num != 0) part_size++;
        for (int i = 0; i < thread_num; i++) {
            int b = part_size * i;
            int e = part_size * (i + 1);
            if (e > end) e = end;
            res.emplace_back(b, e);
        }
        return res;
    }

    // dictionary instance
    LookupTable &LOOKUP_TABLE() {
        static LookupTable table;
        return table;
    }

    uint32_t hasher(const byte_t *byte) {
        auto a = static_cast<uint32_t>(byte[0]) & 0x1f;
        auto b = static_cast<uint32_t>(byte[1]) & 0x1f;
        auto c = static_cast<uint32_t>(byte[2]) & 0x1f;
        return (a << 10) | (b << 5) | (c);
    }

    void _initCompressor(const byte_t *data, size_t size) {
        LOOKUP_TABLE().clear();
        for (int j = 0; j < size - 2; j++) {
            auto hash = hasher(data + j);
            LOOKUP_TABLE().insert(hash, j);
        }
    }

    bool _longestMatch(const byte_t *data, const DataBlockSlice &cs, int start, int &ref, int &len, int limit) {
        len = -1;
        if (start < cs.begin || start > cs.end - 2) return false;  // 超出当前数据块
        auto *matches = LOOKUP_TABLE().findCandidates(start);
        // no candidate matches
        if (!matches) return false;
        // find one
        const auto it = std::upper_bound(matches->rbegin(), matches->rend(), start, std::greater<>());
        if (it == matches->rend()) return false;
        int idx = matches->rend() - it - 1;
        const int FIRST = idx;
        while (idx >= 0) {
            const auto candidate = matches->operator[](idx);
            if (start - candidate >= 32767) break;
            if (candidate < limit) break;
            if (FIRST - idx > 32 || len >= 32) break;  // Prevent wasting too much time
            auto m_src_pos = start;
            auto m_ref_pos = candidate;
            int match_len = 0;
            while (m_src_pos <= cs.end && data[m_src_pos] == data[m_ref_pos] && match_len < (MAX_MATCH - MIN_MATCH)) {
                ++m_src_pos;
                ++m_ref_pos;
                ++match_len;
            }

            if (match_len >= 3 && len < match_len) {
                len = match_len;
                ref = candidate;
            }
            idx--;
        }
        return len != -1;
    }

    int _getOneMatch(const byte_t *data, const DataBlockSlice &cs, int pos, int &ref_pos, int &start_pos, pos_t limit) {
        int current_len{-1}, current_ref{-1}, current_pos{pos};
        if (!_longestMatch(data, cs, current_pos, current_ref, current_len, limit)) return -1;
        int last_len{-1}, last_ref{-1}, last_pos{-1};
        do {
            last_len = current_len;
            last_ref = current_ref;
            last_pos = current_pos;
            current_pos++;
            _longestMatch(data, cs, current_pos, current_ref, current_len, limit);
            if (current_len <= last_len || current_len >= 32 /*Prevent wasting too much time*/) {
                ref_pos = last_ref;
                start_pos = last_pos;
                return last_len;
            }

        } while (true);
    }

    void _compressOneDataBlock(const byte_t *data, DataBlockSlice &cs) {
        int cur = cs.begin;
        while (cur <= cs.end) {
            auto pos = cur;
            pos_t ref = -1;
            pos_t start = cur;
            const auto limit = cs.dup ? cs.begin : cur - 32767;  // 32K
            const auto len = _getOneMatch(data, cs, cur, ref, start, limit);
            // handle match result
            if (len == -1) {  // literal
                const LZ77Pair pair = {0, static_cast<int>(data[cur])};
                cs.pairs[cs.pair_size] = pair;
                cs.pair_size++;
                cur = cur + 1;
            } else {
                // sequence of literals + match
                for (int i = cur; i < start; i++) {
                    const LZ77Pair pair = {0, static_cast<int>(data[i])};
                    cs.pairs[cs.pair_size] = pair;
                    cs.pair_size++;
                }

                const LZ77Pair pair{start - ref, len};
                cs.pairs[cs.pair_size] = pair;
                cs.pair_size++;
                cur = start + len;
            }
        }
    }

    void buildDataBlockSlice(const std::vector<DataBlock> &data_blocks, std::vector<DataBlockSlice> &data_block_slice,
                             std::vector<byte_t> &data) {
        int acc = 0;
        for (auto &ch : data_blocks) {
            DataBlockSlice m;
            m.begin = acc;
            m.end = acc + ch.raw_data().size() - 1;
            m.dup = ch.raw_duplicated();
            m.pairs.resize(ch.raw_data().size());
            data_block_slice.push_back(m);
            data.insert(data.end(), ch.raw_data().begin(), ch.raw_data().end());
            acc += ch.raw_data().size();
        }
    }

}  // namespace

std::vector<DataBlockSlice> NewCompressor::compressDataBlocks(std::vector<DataBlock> &data_blocks) {
    std::vector<byte_t> data;
    std::vector<DataBlockSlice> slices;
    buildDataBlockSlice(data_blocks, slices, data);
    PROF_TIMER(generate_table, { _initCompressor(data.data(), data.size()); });
    PROF_TIMER(deflate_data_block, {
        std::vector<std::future<void>> fs;
        fs.reserve(globalEnv().c.data_block_buffer_size);
        for (int i = 0; i < globalEnv().c.data_block_buffer_size; i++) {
            // multi thread (Ineffective)
            //  fs.emplace_back(THREAD_POOL().submit([i, &data, &slices]() {
            //  lz77
            _compressOneDataBlock(data.data(), std::ref(slices[i]));
            slices[i].pairs.resize(slices[i].pair_size);
            // encode
            slices[i].compressed_data = encodeLz77(slices[i].pairs);
            // }));
        }
        for (auto &f : fs) f.get();
    });

    globalEnv().s.time_compression_deflate += time_deflate_data_block;
    globalEnv().s.time_compression_lookup_table += time_generate_table;
    return slices;
}
