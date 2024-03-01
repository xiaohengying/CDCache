#ifndef CDCACHE_STORAGE_SYSTEM_H
#define CDCACHE_STORAGE_SYSTEM_H
#include "cd_cache.h"
#include "device.h"
#include "io_request.h"

/*
Storage instance simulation
*/

class StorageSystem {
   public:
    StorageSystem() = default;

    void processIORequest(IORequest& request);

    void init(const std::string& p_name, size_t p_size, const std::string& s_name, size_t s_size);

    void setUseCache(bool value);

    ~StorageSystem();

   private:
    void directlyProcess(IORequest& request);
    void cacheWrite(IORequest& request);
    void cacheRead(IORequest& request);
    AbstractBlockDevice* primary_device_{};
    bool use_cache_{false};
    AbstractCache* cache_{nullptr};        // cache
    std::unordered_set<addr_t> lba_list_;  // Only for scenarios without cache
};

#endif  // CDCACHE_STORAGE_SYSTEM_H
