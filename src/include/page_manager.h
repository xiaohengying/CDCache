#ifndef CDCACHE_BLOCKMANAGER_H
#define CDCACHE_BLOCKMANAGER_H

#include <bitset>
#include <cstdint>
#include <memory_resource>
#include <vector>

#include "utils.h"

// Block device page allocator
class AbstractPageManager {
   public:
    explicit AbstractPageManager(std::size_t size) : size_(size) {}

    virtual bool allocate(size_t len, std::vector<addr_t> &blocks) = 0;

    virtual bool reclaim(addr_t blockAddress) = 0;

    virtual size_t free_blocks() = 0;

    virtual ~AbstractPageManager() = default;

   protected:
    const std::size_t size_;
};

class StackedDiscretePageManager : public AbstractPageManager {
   public:
    explicit StackedDiscretePageManager(size_t size)
        : AbstractPageManager(size), begin_(0), end_(size), free_blocks_(size) {
        fprintf(stderr, "Allocator: total %zu blocks\n", size);
    }
    bool allocate(size_t len, std::vector<addr_t> &blocks) override;
    bool reclaim(addr_t blockAddress) override;

    size_t free_blocks() override;
    ~StackedDiscretePageManager() override = default;

   private:
    bool allocate_one(addr_t &addr);

    size_t free_blocks_{0};
    size_t begin_{0};
    const size_t end_{0};
    std::vector<size_t> recycled_;
};

#endif  // CDCACHE_BLOCKMANAGER_H
