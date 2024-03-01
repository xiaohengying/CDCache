#ifndef CDCACHE_ABSTRACT_CACHE_H
#define CDCACHE_ABSTRACT_CACHE_H

#include <vector>

#include "data_block.h"

class AbstractCache {
   public:
    virtual bool read(LogicalBlock &block) = 0;
    virtual bool write(const LogicalBlock &block, bool read_miss) = 0;
    virtual ~AbstractCache() = default;
};
#endif  // CDCACHE_ABSTRACT_CACHE_H
