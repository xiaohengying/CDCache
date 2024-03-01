#ifndef CDCACHE_CD_CACHE_H
#define CDCACHE_CD_CACHE_H

#include <cstddef>

#include "abstract_cache.h"
#include "block_detector.h"
#include "cacheline_index.h"
#include "config.h"
#include "data_block_buffer.h"
#include "deletable_bloom_filter.h"
#include "device.h"
#include "fp_index.h"
#include "lba_index.h"
#include "ssd_proxy.h"

class CDCache : public AbstractCache {
   public:
    CDCache();

    ~CDCache() override;

    bool read(LogicalBlock &block) override;

    bool write(const LogicalBlock &block, bool read_miss) override;

    void open(const std::string &name, size_t size);

   private:
    void detecteBlockType(std::vector<DataBlock> &data_blocks);

    void evictOne(addr_t id, const CachelineIndexData &data);

    void updateLBAIndex(std::vector<DataBlock> &data_blocks);

    size_t dedupBlocks(std::vector<DataBlock> &data_blocks);

    bool flushBuffer();

    DataBlockBuffer data_block_buffer_{globalEnv().c.data_block_buffer_size};
    AbstractBlockDetector *detector_;
    SSDProxy *proxy_{nullptr};
    // Indexes
    AbstractFPIndex *fp_index_{nullptr};
    AbstractLBAIndex *lba_index_{nullptr};
    CachelineIndex cacheline_index_;
};

#endif  // CDCACHE_CD_CACHE_H
