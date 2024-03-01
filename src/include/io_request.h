#ifndef CDCACHE_IOREQUEST_H
#define CDCACHE_IOREQUEST_H
#include <cstddef>
#include <ostream>
#include <string>

#include "data_block.h"
#include "utils.h"
enum IOType { None = -1, Read = 0, Write = 1 };
struct IORequest {
    IOType type = None;
    addr_t address = 0;
    std::vector<byte_t> data;
    fp_t fp{0xffffffff};
    IORequest() = default;
    IORequest(IOType t, addr_t addr) : type(t), address(addr) {}
    static DataBlock toDataBlock(IORequest& request);
    friend std::ostream& operator<<(std::ostream& os, const IORequest& request);
};

std::vector<IORequest> readTestTracesFromFile(const std::string& trace_file_name, const std::string& data_file_name,
                                              size_t data_block_size);

// dynamic reader (not actually asynchronous)  for memory saving
class AsyncIORequestReader {
    struct Info {
        IOType type = None;
        addr_t address = 0;
        size_t data_block_index;
        fp_t fp{0xffffffff};
    };

   public:
    AsyncIORequestReader(const std::string& trace_file_name, const std::string& data_file_name, size_t data_block_size);
    bool hasNext() const;
    IORequest getRequest();
    void init();
    size_t size() const;
    ~AsyncIORequestReader();

   private:
    std::string trace_file_name_;
    std::string data_file_name_;
    size_t data_block_size_;
    int data_fd_{-1};
    std::ifstream if_trace_;
    std::vector<Info> info_list_;
    // status
    size_t index = 0;
};

#endif  // CDCACHE_IOREQUEST_H
