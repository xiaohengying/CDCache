#ifndef CDCACHE_FPINDEX_H
#define CDCACHE_FPINDEX_H

#include <unordered_map>

#include "utils.h"

/**
finger print index
*/
struct FPIndexData {
    addr_t cacheline_addr;
    fp_t raw_fingerprint;
};

class AbstractFPIndex {
   public:
    virtual bool query(fp_t fp, FPIndexData &data) = 0;

    virtual bool insert(fp_t fp, FPIndexData data) = 0;

    virtual bool remove(fp_t fp) = 0;

    virtual size_t size() = 0;

    virtual ~AbstractFPIndex() = default;
};

class SimpleFPIndex final : public AbstractFPIndex {
   public:
    bool query(fp_t fp, FPIndexData &data) override;

    bool insert(fp_t fp, FPIndexData data) override;

    bool remove(fp_t fp) override;
    size_t size() override;

    ~SimpleFPIndex() override;

   private:
    std::unordered_map<fp_t, FPIndexData> table_;
};

#endif  // CDCACHE_FPINDEX_H
