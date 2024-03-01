#include "io_request.h"

#include <bits/types/clock_t.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstddef>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "utils.h"

DataBlock IORequest::toDataBlock(IORequest& request) {
    DataBlock data_block(request.data, request.address);
    return data_block;
}
std::ostream& operator<<(std::ostream& os, const IORequest& request) {
    std::string output = request.type == Read ? "R  " : "W  ";
    output += std::to_string(request.address);
    output += " ";
    for (int i = 0; i < 10; i++) {
        char buff[4] = {0, 0, 0, 0};
        sprintf(buff, "%02x ", request.data[i]);
        output.append(buff);
    }
    output += "...";
    os << output;
    return os;
}

std::vector<IORequest> readTestTracesFromFile(const std::string& trace_file_name, const std::string& data_file_name,
                                              size_t data_block_size) {
    std::ifstream if_data(data_file_name, std::ios::binary | std::ios::ate);
    std::ifstream if_trace(trace_file_name);
    if (!if_data.is_open() || !if_trace.is_open()) {
        ERROR("Can not open file %s or %s", trace_file_name.c_str(), data_file_name.c_str());
        return {};
    }

    auto trace_size = if_data.tellg();
    if_data.seekg(0, std::ios::beg);
    std::vector<byte_t> buffer(trace_size);
    if (!if_data.read((char*)buffer.data(), trace_size)) {
        ERROR("can not read data %s", data_file_name.c_str());
    }

    // split buffer
    auto total_size = buffer.size();
    auto data_block_number = total_size / data_block_size;
    LOGGER("total %zu KB data, %zu data_blocks", total_size / 1024, data_block_number);
    std::vector<std::vector<byte_t>> data_blocks;
    for (auto i = 0; i < data_block_number; i++) {
        auto data_block =
            std::vector<byte_t>(buffer.begin() + i * data_block_size, buffer.begin() + (i + 1) * data_block_size);
        data_blocks.push_back(data_block);
        Assert(data_block.size() == data_block_size, "Error data_block size");
    }
    Assert(data_blocks.size() == data_block_number, "Error data_block number");

    // read traces
    std::vector<IORequest> req_list;
    std::string line;
    while (std::getline(if_trace, line)) {
        std::istringstream iss(line);
        IORequest req;
        uint64_t lba;
        char read_type;
        size_t data_block_index;
        if (!(iss >> lba >> read_type >> data_block_index)) {
            printf("Read failure");
            break;
        }
        req.address = lba;
        req.type = read_type == 'R' ? IOType::Read : IOType::Write;
        Assert(data_block_index < data_blocks.size(), "Error data index with index  = %zu", data_block_index);
        req.data = data_blocks[data_block_index];
        req_list.push_back(req);
    }
    if_data.close();
    if_trace.close();

    return req_list;
}

AsyncIORequestReader::AsyncIORequestReader(const std::string& trace_file_name, const std::string& data_file_name,
                                           size_t data_block_size)
    : trace_file_name_(trace_file_name), data_file_name_(data_file_name), data_block_size_(data_block_size) {
    this->init();
}

void AsyncIORequestReader::init() {
    this->data_fd_ = ::open(this->data_file_name_.c_str(), O_RDONLY);
    Assert(this->data_fd_ > 0, "Can not open data file %s", this->data_file_name_.c_str());
    this->if_trace_.open(this->trace_file_name_);
    Assert(this->if_trace_.is_open(), "Can not open trace file %s", this->data_file_name_.c_str());

    std::string line;
    while (std::getline(if_trace_, line)) {
        std::istringstream iss(line);
        AsyncIORequestReader::Info info;
        uint64_t lba;
        char read_type;
        size_t data_block_index;
        if (!(iss >> lba >> read_type >> data_block_index)) {
            printf("Read failure");
            break;
        }

        info.data_block_index = data_block_index;
        info.address = lba;
        info.type = read_type == 'R' ? IOType::Read : IOType::Write;
        info_list_.push_back(info);
    }
}

bool AsyncIORequestReader::hasNext() const { return this->index < this->info_list_.size(); }

size_t AsyncIORequestReader::size() const { return this->info_list_.size(); }

IORequest AsyncIORequestReader::getRequest() {
    // this->index++;
    const auto info = this->info_list_[this->index];
    auto data = std::vector<byte_t>(this->data_block_size_, 0);
    auto _ = pread(this->data_fd_, data.data(), this->data_block_size_, info.data_block_index * this->data_block_size_);
    IORequest req;
    req.address = info.address;
    req.data = data;
    req.fp = info.fp;
    req.type = info.type;
    this->index++;
    return req;
}

AsyncIORequestReader::~AsyncIORequestReader() {
    ::close(this->data_fd_);
    this->if_trace_.close();
}