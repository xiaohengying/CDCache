#include "pair_serializer.h"

#include <sstream>

#include "bitstream.h"

namespace simple_encoder {
    // Very simple Lz77 (de)serialization method,Huffman directly will cause the compression speed to be too slow
    std::vector<byte_t> bit_encodeLz77(const std::vector<LZ77Pair> &pairs) {
        std::vector<byte_t> buf(pairs.size() * 4, 0);
        BitstreamWriter w(buf.data(), buf.size());
        for (auto [d, l] : pairs) {
            // skip spaces
            if (d == -1 && l == -1) continue;
            Assert((d == 0 && l >= 0 && l <= 255) || (d > 0 && d < 32768 && l >= 1 && l <= 256),
                   "Invalid Lz77 Pair with distance = %d and literal = %d", d, l);
            if (d > 0) l -= 1;
            w.put<8>(l);              // lit
            w.put<1>(d > 0 ? 1 : 0);  // 0 -> lit / 1 -> pair
            if (d > 0) {
                w.put<16>(d);
            }
        }
        const auto len = w.mOffset % 8 == 0 ? w.mOffset / 8 : w.mOffset / 8 + 1;
        return {buf.begin(), buf.begin() + len};
    }

    std::vector<LZ77Pair> bit_decodeLz77(const std::vector<byte_t> &pairs) {
        BitstreamReader reader(pairs.data(), pairs.size());
        std::vector<LZ77Pair> res;
        const auto total = reader.size() * 8;
        while (total - reader.offset() >= 9) {
            LZ77Pair pair;
            pair.lit = reader.get<8>();
            if (const int flag = reader.get<1>(); flag != 0) {
                Assert(total - reader.offset() >= 16, "Invalid input");
                pair.dist = reader.get<16>();
                pair.lit++;
            } else {
                pair.dist = 0;
            }
            res.push_back(pair);
        }
        return res;
    }
}  // namespace simple_encoder
namespace lz4_encoder {
    // TODO
}

std::vector<byte_t> encodeLz77(const std::vector<LZ77Pair> &pairs) { return simple_encoder::bit_encodeLz77(pairs); }

std::vector<LZ77Pair> decodeLz77(const std::vector<byte_t> &bytes) { return simple_encoder::bit_decodeLz77(bytes); }
