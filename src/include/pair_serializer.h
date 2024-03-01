#ifndef CDCACHE_PAIR_SERIALIZER_H
#define CDCACHE_PAIR_SERIALIZER_H

#include <vector>

#include "lz77_compressor.h"
#include "utils.h"

/**
simple algorithm for LZ77 <---> byte stream
*/
std::vector<byte_t> encodeLz77(const std::vector<LZ77Pair> &pairs);

std::vector<LZ77Pair> decodeLz77(const std::vector<byte_t> &pairs);

#endif  // CDCACHE_PAIR_SERIALIZER_H
