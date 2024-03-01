#ifndef CDCACHE_MAIN_COMPRESSOR_H
#define CDCACHE_MAIN_COMPRESSOR_H
#include <vector>

#include "data_block.h"
#include "deletable_bloom_filter.h"
#include "pair_serializer.h"

// compression algorithm export interface
class MainCompressor {
   public:
    MainCompressor() = delete;
    // compress a bathc of data blocks
    static void compress(std::vector<DataBlock> &dataBlocks, bool useHuffman);

    static void decompress(std::vector<DataBlock> &dataBlocks, bool useHuffman);

    // compress byte array
    static std::vector<LZ77Pair> lz77CompressByteArray(const std::vector<byte_t> &bytes);
    static std::vector<byte_t> lz77DecompressByteArray(const std::vector<LZ77Pair> &pairs);
};

#endif  // CDCACHE_MAIN_COMPRESSOR_H
