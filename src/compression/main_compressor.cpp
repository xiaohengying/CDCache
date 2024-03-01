#include "main_compressor.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "config.h"
#include "data_block.h"
#include "lz77_compressor.h"
#include "pair_serializer.h"
#include "utils.h"

namespace {

    std::vector<LZ77Pair> lz77DataBlockCompression(std::vector<DataBlock> &rawDataBlocks) {
        const auto c = NewCompressor::compressDataBlocks(rawDataBlocks);
        for (int i = 0; i < rawDataBlocks.size(); i++) {
            rawDataBlocks[i].setCompData(c[i].compressed_data);
            rawDataBlocks[i].calCompFP();
        }
        return {};
    }
}  // namespace

// lz77 string compression (TODO: if other compression algorithm is used, this needs to be modified)
std::vector<LZ77Pair> MainCompressor::lz77CompressByteArray(const std::vector<byte_t> &bytes) {
    std::vector<DataBlock> data_blocks(1, DataBlock{});
    data_blocks[0].setRawData(bytes);
    auto slice = NewCompressor::compressDataBlocks(data_blocks);
    return slice[0].pairs;
}

std::vector<byte_t> MainCompressor::lz77DecompressByteArray(const std::vector<LZ77Pair> &pairs) {
    std::vector<byte_t> bytes;
    for (auto &p : pairs) {
        if (!p.valid()) continue;
        if (p.dist == 0) {
            bytes.push_back(static_cast<byte_t>(p.lit));
        } else {
            const int start = static_cast<int>(bytes.size()) - p.dist;
            Assert(start >= 0, "Invalid Lz77 Pairs with byte size %zu, dist = %d", bytes.size(), p.dist);
            for (int i = 0; i < p.lit; i++) {
                bytes.push_back(bytes[start + i]);
            }
        }
    }
    return bytes;
}

void MainCompressor::compress(std::vector<DataBlock> &rawDataBlocks, bool useHuffman) {
    if (globalEnv().c.compression_method == "lz77") {
        PROF_TIMER(compression, { lz77DataBlockCompression(rawDataBlocks); });
        globalEnv().s.time_compression += time_compression;
    } else if (globalEnv().c.compression_method == "lz4") {
        Assert(false, "The lz4 algorithm has not been implemented yet", globalEnv().c.compression_method.c_str());
    } else {
        Assert(false, "Wrong compression %s", globalEnv().c.compression_method.c_str());
    }
    if (useHuffman) {
        // TODO: Huffman coding
    }
}
