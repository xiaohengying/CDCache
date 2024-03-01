#ifndef CDCACHE_LZ77_COMPRESSOR_H
#define CDCACHE_LZ77_COMPRESSOR_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "data_block.h"
#include "utils.h"

// simple lz77 compressor
/**
 * dist match distabce
 * lit match length
 *
 * dist = 0  literal 0<=lit<=256
 * dist > 0  pair
 */
struct LZ77Pair {
    int dist = -1;
    int lit = -1;

    bool operator==(const LZ77Pair &rhs) const { return dist == rhs.dist && lit == rhs.lit; }

    [[nodiscard]] bool valid() const { return !(dist == -1 && lit == -1); }
};

static constexpr int HASH_BITS = 16;
static constexpr int MIN_MATCH = 3;
static constexpr int MAX_MATCH = 256;

struct LookupTable {
    inline void insert(uint32_t hash, int pos) {
        this->hash_list_[pos] = hash;
        tb[hash].push_back(pos);
    }

    [[nodiscard]] inline const std::vector<int> *findCandidates(size_t pos) const {
        // Assert(pos < this->hash_list_.size() && pos >= 0, "Invalid Pos %zu", pos);
        auto hash = this->hash_list_[pos];
        return tb[hash].empty() ? nullptr : &tb[hash];
    }

    inline void clear() {
        for (auto &t : tb) t.clear();
    }

    LookupTable() {
        this->tb = std::vector<std::vector<int>>((1 << 15), std::vector<int>());
        this->hash_list_ =
            std::vector<uint32_t>(globalEnv().c.data_block_buffer_size * globalEnv().c.dataset_block_size, 0);
    }

   public:
    std::vector<std::vector<int>> tb;
    std::vector<uint32_t> hash_list_;
};

struct DataBlockSlice {
    int begin{0};
    int end{-1};
    bool dup{false};
    size_t pair_size{0};
    std::vector<LZ77Pair> pairs;
    std::vector<byte_t> compressed_data;
    [[nodiscard]] inline int size() const { return end - begin + 1; }
};

class NewCompressor {
   public:
    static std::vector<DataBlockSlice> compressDataBlocks(std::vector<DataBlock> &data_blocks);
};

#endif
