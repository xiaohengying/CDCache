#include "device.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "config.h"
#include "utils.h"

namespace {
    size_t get_device_size(int fd) {
        uint32_t offset = 1024;
        uint32_t prev_offset = 0;
        char buffer[1024];
        char *read_buffer = (char *)(((long)buffer + 511) & ~511);

        while (true) {
            if (::lseek(fd, offset, SEEK_SET) == -1) {
                offset -= (offset - prev_offset) / 2;
                continue;
            }
            int nr = 0;
            if ((nr = ::read(fd, read_buffer, (size_t)512)) != 512) {
                if (nr >= 0) {
                    offset += nr;
                    return offset;
                } else {
                    if (prev_offset == offset) {
                        return offset;
                    } else {
                        offset -= (offset - prev_offset) / 2;
                    }
                }
            } else {
                prev_offset = offset;
                offset *= 2;
            }
        }
    }

}  // namespace

AbstractBlockDevice::~AbstractBlockDevice() {}

int EmptyBlockDevice::read(uint64_t addr, uint8_t *buf, uint32_t len) { return 512; }

int EmptyBlockDevice::write(uint64_t addr, const uint8_t *buf, uint32_t len) { return 512; }

bool EmptyBlockDevice::open(const char *filename, uint64_t size) { return true; }

FileBlockDevice::FileBlockDevice() = default;

bool FileBlockDevice::open(const char *filename, uint64_t size) {
    // 直接创建心的设备文件，不再打开已有的，不影响测试
    // create new for test
    return open_new_device(filename, size);

    // struct stat stat_buf {};
    // int handle = ::stat(filename, &stat_buf);
    // if (handle == -1) {
    //     if (errno == ENOENT) {
    //         return open_new_device(filename, size);
    //     } else {
    //         // unexpected error
    //         return -1;
    //     }
    // } else {
    //     return open_existing_device(filename, size, &stat_buf);
    // }
}

int FileBlockDevice::write(uint64_t addr, const uint8_t *buf, uint32_t len) {
    //    LOGGER("address: %zu len: %zu", addr, len);
    assert(addr % 512 == 0);
    assert(len % 512 == 0);
    if (addr + len > size_) {
        len -= addr + len - size_;
    }

    int n_written_bytes = 0;
    while (true) {
        int n = ::pwrite(fd_, buf, len, addr);
        Assert(n >= 0, "Block device write data_blocks failure with error %s", std::strerror(errno));
        n_written_bytes += n;
        if (n == len) {
            break;
        } else {
            buf += n;
            len -= n;
        }
    }
    return n_written_bytes;
}

int FileBlockDevice::read(uint64_t addr, uint8_t *buf, uint32_t len) {
    assert(addr % 512 == 0);
    assert(len % 512 == 0);

    if (addr + len > size_) {
        len -= addr + len - size_;
    }

    int n_read_bytes = 0;
    while (true) {
        int n = ::pread(fd_, buf, len, addr);
        Assert(n >= 0, "Block device readDataBlocks failure with error %s", std::strerror(errno));
        n_read_bytes += n;
        if (n == len) {
            break;
        } else {
            buf += n;
            len -= n;
        }
    }

    return n_read_bytes;
}

FileBlockDevice::~FileBlockDevice() { ::close(fd_); }

void FileBlockDevice::sync() { ::syncfs(fd_); }

bool FileBlockDevice::open_new_device(const char *filename, uint64_t size) {
    LOGGER("Open a new device!");
    Assert(size > 0, "A new file needs to be created, however the size given is 0");
    auto flags = O_RDWR | O_CREAT;
    // if (globalConfig().use_direct_io) {
    //     flags |= O_DIRECT;
    // }

    this->fd_ = ::open(filename, flags, 0666);
    this->size_ = size;
    Assert(this->fd_ > 0, "Create device %s failure with returned fd %d", filename, this->fd_);
    auto r = ::ftruncate(this->fd_, static_cast<long>(size));
    Assert(r == 0, "Can not truncate the given block device with error %s", std::strerror(errno));
    return true;
}

bool FileBlockDevice::open_existing_device(const char *filename, uint64_t size, const struct stat *statbuf) {
    LOGGER("Open existing device");
    auto flag = O_RDWR;
    // if (globalConfig().use_direct_io) {
    //     flag |= O_DIRECT;
    // }
    this->fd_ = ::open(filename, flag);
    Assert(this->fd_ > 0, "Open device %s failed with return fd value %d", filename, this->fd_);

    if (size == 0) {
        if (S_ISREG(statbuf->st_mode))
            size = statbuf->st_size;
        else
            size = get_device_size(this->fd_);
    }
    this->size_ = size;
    return true;
}

void FileBlockDevice::close() {
    ::close(this->fd_);
    this->fd_ = -1;
    this->size_ = 0;
}
void FileBlockDevice::fill_zero() {}

int MemBlockDevice::write(uint64_t addr, const uint8_t *buf, uint32_t len) {
    assert(addr % 512 == 0);
    assert(len % 512 == 0);
    if (addr + len > size_) len -= addr + len - size_;

    memcpy(this->device_buffer_ + addr, buf, len);
    return len;
}

int MemBlockDevice::read(uint64_t addr, uint8_t *buf, uint32_t len) {
    assert(addr % 512 == 0);
    assert(len % 512 == 0);
    if (addr + len > size_) len -= addr + len - size_;
    memcpy(buf, this->device_buffer_ + addr, len);
    return static_cast<int>(len);
}

bool MemBlockDevice::open(const char *filename, uint64_t size) {
    this->size_ = size;
    this->device_buffer_ = new byte_t[size];
    return true;
}

MemBlockDevice::MemBlockDevice() : device_buffer_(nullptr) {}
MemBlockDevice::~MemBlockDevice() { delete[] this->device_buffer_; }
