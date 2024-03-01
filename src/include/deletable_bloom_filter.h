#ifndef CDCACHE_DELETABLE_BLOOM_FILTER_H
#define CDCACHE_DELETABLE_BLOOM_FILTER_H

#include <array>
#include <bitset>
#include <random>

#include "xxhash.h"

template <int N, int K>
class DeletableBloomFilter {
   public:
    static_assert(N > 0 && (N & (N - 1)) == 0);

    explicit DeletableBloomFilter(uint64_t seed) { this->initHashSeed(seed); }

    DeletableBloomFilter() {
        std::random_device rd;
        this->initHashSeed(rd());
    }

    // for test
    const std::array<int, K> &getHashSeeds() { return this->hash_seeds; }

    void insert(uint64_t value) {
        for (int i = 0; i < K; i++) {
            auto v = XXH64(&value, sizeof(uint64_t), hash_seeds[i]) % N;
            if (this->slots_[v]) {
                this->collisions_.set(v);
            } else {
                this->slots_.set(v);
            }
        }
    }

    void remove(uint64_t value) {
        for (int i = 0; i < K; i++) {
            auto v = XXH64(&value, sizeof(uint64_t), hash_seeds[i]) % N;
            if (!this->collisions_[v]) {
                this->slots_.reset(v);
            }
        }
    }

    [[nodiscard]] bool query(uint64_t value) const {
        size_t num = 0;
        for (int i = 0; i < K; i++) {
            auto v = XXH64(&value, sizeof(uint64_t), hash_seeds[i]) % N;
            if (this->slots_[v]) ++num;
        }
        return num == K;
    }

   private:
    void initHashSeed(int seed) {
        std::mt19937 gen(seed);
        std::uniform_int_distribution<> dis(1, 4 * K);
        for (int n = 0; n < K; ++n) {
            this->hash_seeds[n] = dis(gen);
        }
    }

    std::array<int, K> hash_seeds;
    std::bitset<N> slots_;
    std::bitset<N> collisions_;
};

#endif  // CDCACHE_DELETABLE_BLOOM_FILTER_H
