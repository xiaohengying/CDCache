#ifndef CDCACHE_BLOCKDETECTOR_H
#define CDCACHE_BLOCKDETECTOR_H
#include <cstdint>
#include <fstream>
#include <unordered_set>

#include "deletable_bloom_filter.h"

// Data block detector, used to determine the type of a data block in advance
class AbstractBlockDetector {
   public:
    virtual void insert(uint64_t value) = 0;
    virtual void remove(uint64_t value) = 0;
    virtual bool query(uint64_t value) = 0;
    virtual void dumpInfo(){};
};

class BloomFilterDetector : public AbstractBlockDetector {
   public:
    void insert(uint64_t value) override;
    void remove(uint64_t value) override;
    bool query(uint64_t value) override;

   private:
    DeletableBloomFilter<(1 << 14), 8> filter_;
};

class SetDetector : public AbstractBlockDetector {
   public:
    SetDetector(size_t threshold) : threshold_(threshold), AbstractBlockDetector() {}

    SetDetector() : SetDetector(1) {}

    void insert(uint64_t value) override;
    void remove(uint64_t value) override;
    bool query(uint64_t value) override;
    void dumpInfo() override;

   private:
    std::unordered_set<uint64_t> set_;
    const size_t threshold_;
};

#endif  // CDCACHE_BLOCKDETECTOR_H
