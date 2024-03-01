#include "storage_system.h"

#include "baseline_cache.h"
#include "config.h"
#include "device.h"
#include "utils.h"

void StorageSystem::init(const std::string& p_name, size_t p_size, const std::string& s_name, size_t s_size) {
    this->primary_device_ = new EmptyBlockDevice();
    Assert(this->primary_device_->open(p_name.c_str(), p_size), "Create primary device failure");
    if (this->use_cache_) {
        auto name = globalEnv().c.cache_type;
        if (name == "cdcache") {
            this->cache_ = new CDCache();
            dynamic_cast<CDCache*>(this->cache_)->open(s_name, s_size);
        } else if (name == "baseline") {
            // calculate slot size
            this->cache_ = new AdaptedBaselineCache(globalEnv().c.cache_size / globalEnv().c.dataset_block_size);
        } else if (name == "dedup") {
            this->cache_ = new DedupCache(globalEnv().c.cache_size / globalEnv().c.dataset_block_size);
            // todo: dedup cache
        } else {
            throw std::runtime_error("Invalid Cache type");
        }
    }
    LOGGER("Storage system init finished");
}

void StorageSystem::processIORequest(IORequest& request) {
    PROF_TIMER(process, {
        if (!this->use_cache_) {
            this->directlyProcess(request);
        } else {
            if (request.type == Read) {
                this->cacheRead(request);
            } else if (request.type == Write) {
                this->cacheWrite(request);
            } else {
                ERROR("Error I/O type");
            }
        }
    });
    globalEnv().s.time_process += time_process;
}

void StorageSystem::directlyProcess(IORequest& request) {
    // if (request.type == Read) {
    //     if (this->lba_list_.count(request.address)) {
    //         request.data = std::vector<byte_t>(512, 0);
    //         LOGGER("Read from %zu", request.address);
    //         auto sz = this->primary_device_->read(request.address, request.data.data(), 512);
    //         Assert(sz == 512, "Direct read failure");
    //     } else {
    //         request.data = std::vector<byte_t>();
    //     }
    // } else {
    //     LOGGER("INSERT %lx to LBA", request.address);

    //     this->lba_list_.insert(request.address);
    //     auto sz = this->primary_device_->write(request.address, request.data.data(), 512);
    //     Assert(sz == 512, "Direct Write failure");
    // }
}

/**
 * writeDataBlocks a data_block to SSD
 * @param request
 */
void StorageSystem::cacheWrite(IORequest& request) {
    auto data_block = IORequest::toDataBlock(request);
    this->cache_->write(data_block, false);  // read miss
}

void StorageSystem::cacheRead(IORequest& request) {
    auto data_block = IORequest::toDataBlock(request);
    this->cache_->read(data_block);
    request.data = data_block.raw_data();
}

void StorageSystem::setUseCache(bool value) { this->use_cache_ = value; }
StorageSystem::~StorageSystem() {
    delete this->cache_;
    delete this->primary_device_;
}
