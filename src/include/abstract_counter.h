#ifndef CDCACHE_ABSTRACE_COUNTER_H
#define CDCACHE_ABSTRACE_COUNTER_H

#include <cstdint>
#include <random>
#include <unordered_map>

#include "xxhash.h"

template <typename T>
class AbstractCounter {
   public:
    virtual ~AbstractCounter() = default;

    virtual void clear() = 0;
    virtual int get(const T& t) = 0;
    virtual void increase(const T& t) = 0;
};

template <typename T>
class MapCounter final : AbstractCounter<T> {
   public:
    void clear() override { ctr_.clear(); }
    int get(const T& t) override { return ctr_[t]; }
    void increase(const T& t) override { ++ctr_[t]; }
    ~MapCounter() override = default;

   private:
    std::unordered_map<T, size_t> ctr_;
};

template <typename T, int N, int K>
class CMSketch final : AbstractCounter<T> {
   public:
    explicit CMSketch(uint64_t seed) { this->initHashSeed(seed); }
    void clear() override {
        for (auto& array : table_) std::fill(array.begin(), array.end(), 0);
    }

    int get(const T& t) override {
        int res{INT32_MAX};
        for (int i = 0; i < K; i++) {
            auto v = XXH64(&t, sizeof(t), hash_seeds[i]) % N;
            if (res > table_[i][v]) res = table_[i][v];
        }
        return res;
    }

    void increase(const T& t) override {
        for (int i = 0; i < K; i++) {
            auto v = XXH64(&t, sizeof(t), hash_seeds[i]) % N;
            ++table_[i][v];
        }
    }
    ~CMSketch() override = default;

   private:
    void initHashSeed(int seed) {
        std::mt19937 gen(seed);
        std::uniform_int_distribution<> dis(1, N);
        for (int n = 0; n < K; ++n) {
            this->hash_seeds[n] = dis(gen);
        }
    }
    std::array<int, K> hash_seeds;
    std::array<std::array<uint32_t, N>, K> table_;
};

#endif  // CMSKETCH_H
