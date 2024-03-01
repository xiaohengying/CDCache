#ifndef CDCACHE_LBA_INDEX_H
#define CDCACHE_LBA_INDEX_H

#include <unordered_map>

#include "utils.h"
/**
LBA index
*/
class AbstractLBAIndex {
   public:
    virtual bool query(addr_t address, fp_t &fp) = 0;

    virtual bool insert(addr_t address, fp_t fp) = 0;

    virtual bool remove(addr_t address) = 0;

    virtual ~AbstractLBAIndex() = default;
};

class SimpleLBAIndex : public AbstractLBAIndex {
   public:
    bool query(addr_t address, fp_t &fp) override;

    bool insert(addr_t address, fp_t fp) override;

    bool remove(addr_t address) override;

    ~SimpleLBAIndex() override = default;

   private:
    std::unordered_map<addr_t, fp_t> table_;
};

#endif  // CDCACHE_LBA_INDEX_H
