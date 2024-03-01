#ifndef CDCACHE_DEVICE_H
#define CDCACHE_DEVICE_H

#include <cstdint>

#include "utils.h"

/*
device interface and simulation
*/

class AbstractBlockDevice {
   public:
    virtual ~AbstractBlockDevice();

    virtual int read(uint64_t addr, uint8_t *buf, uint32_t len) = 0;

    virtual int write(uint64_t addr, const uint8_t *buf, uint32_t len) = 0;
    [[nodiscard]] inline virtual size_t size() const { return this->size_; }

    virtual bool open(const char *filename, uint64_t size) = 0;

   protected:
    uint64_t size_ = 0;
};

// do nothing
class EmptyBlockDevice final : public AbstractBlockDevice {
    int read(uint64_t addr, uint8_t *buf, uint32_t len) override;

    int write(uint64_t addr, const uint8_t *buf, uint32_t len) override;

    bool open(const char *filename, uint64_t size) override;
};

// memory simulation
class MemBlockDevice final : public AbstractBlockDevice {
   public:
    MemBlockDevice();
    ~MemBlockDevice() override;

    int read(uint64_t addr, uint8_t *buf, uint32_t len) override;

    int write(uint64_t addr, const uint8_t *buf, uint32_t len) override;

    bool open(const char *filename, uint64_t size) override;

   private:
    byte_t *device_buffer_;
};

// file simulation
class FileBlockDevice final : public AbstractBlockDevice {
   public:
    FileBlockDevice();

    ~FileBlockDevice() override;

    int read(uint64_t addr, uint8_t *buf, uint32_t len) override;

    int write(uint64_t addr, const uint8_t *buf, uint32_t len) override;

    bool open(const char *filename, uint64_t size) override;

    [[nodiscard]] inline size_t size() const override { return this->size_; }

    [[nodiscard]] inline size_t fd() const { return this->fd_; }

    void sync();

    void close();

   private:
    static void fill_zero();

    bool open_new_device(const char *filename, uint64_t size);

    bool open_existing_device(const char *filename, uint64_t size, const struct stat *statbuf);

    int fd_{-1};
};

#endif
