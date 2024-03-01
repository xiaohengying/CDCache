#include "trace_reader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "io_request.h"
#include "utils.h"

// 89968195792462 20782 gzip 283193184 8 R 6 0 56f11b711d91a065a2b6458eca924523
//[ts in ns] [pid] [process] [lba] [size in 512 Bytes blocks] [Write or Read]
//  [major device number] [minor device number] [MD5 per 4096 Bytes]
std::istream &operator>>(std::istream &input, FIUTraceRecord &r) {
    char io_opt;
    int major_device_number, minor_device_number;
    input >> r.timestamp >> r.pid >> r.process_name >> r.lba >> r.block_size >> io_opt >> major_device_number >>
        minor_device_number >> r.hash;
    r.block_size = r.block_size * 512;
    r.lba *= 512;
    r.io_type = io_opt == 'R' ? Read : Write;
    r.device_number = std::to_string(major_device_number) + "_" + std::to_string(minor_device_number);
    return input;
}
bool parseFIUTraceRecordFromLine(const std::string &line, FIUTraceRecord &record) {
    std::stringstream s(line);
    return (s >> record).operator bool();
}

std::vector<FIUTraceRecord> readFIUTraceFromFile(const std::string &file_name, size_t expect_block_size) {
    std::ifstream input(file_name);
    std::vector<FIUTraceRecord> res;
    Assert(input.is_open(), "Can not open file %s", file_name.c_str());
    FIUTraceRecord r;
    while (input >> r) {
        // 0表示接受所有大小的数据块
        // 0 -> accept all data blocks
        if (expect_block_size == 0) {
            res.push_back(r);
        } else {
            if (r.block_size == expect_block_size) {
                res.push_back(r);
            }
        }
    }
    return res;
}

// SYN
std::vector<SYNTraceRecord> readSYNTraceFromFile(const std::string &file_name, size_t expect_block_size) {
    std::ifstream input(file_name);
    std::vector<SYNTraceRecord> res;
    Assert(input.is_open(), "Can not open file %s", file_name.c_str());
    std::string line;
    while (std::getline(input, line)) {
        SYNTraceRecord record;
        if (parseSYNTraceRecordFromLine(line, record)) {
            if (expect_block_size == 0 || expect_block_size == record.block_size) {
                res.push_back(record);
            }
        }
    }
    return res;
}
bool parseSYNTraceRecordFromLine(const std::string &line, SYNTraceRecord &record) {
    std::stringstream s(line);
    char io_type;
    bool res = (s >> record.lba >> record.block_size >> io_type >> record.block_hash >> record.compression_ratio)
                   .
                   operator bool();
    if (!res) return false;
    if (record.block_size == 0 || record.lba % record.block_size != 0) return false;  // filter for austere cache
    if (io_type == 'R') {
        record.io_type = Read;
        return true;
    } else if (io_type == 'W') {
        record.io_type = Write;
        return true;
    } else {
        return false;
    }
}

// SYSTOR17

std::vector<Systor17TraceRecord> readSystorTraceFromFile(const std::string &file_name, size_t expect_block_size) {
    std::ifstream input(file_name);
    std::vector<Systor17TraceRecord> res;
    Assert(input.is_open(), "Can not open file %s", file_name.c_str());
    std::string line;
    while (std::getline(input, line)) {
        Systor17TraceRecord record;
        if (parseSystorTraceRecordFromLine(line, record)) {
            if (expect_block_size == 0 || expect_block_size == record.block_size) {
                res.push_back(record);
            }
        }
    }
    return res;
}
/**
 1456095145.133076000,0.000806000,R,0,899056122368,131072
 Timestamp,Response,IOType,LUN,Offset,Size
 */
bool parseSystorTraceRecordFromLine(const std::string &line, Systor17TraceRecord &record) {
    char io_type{'U'};
    int s = sscanf(line.c_str(), "%lf,%lf,%c,%d,%zu,%zu", &record.timestamp, &record.response, &io_type, &record.lun,
                   &record.lba, &record.block_size);
    if (record.block_size == 0 || record.lba % record.block_size != 0) return false;  // filter for austere cache
    if (io_type == 'R') {
        record.io_type = Read;
    } else if (io_type == 'W') {
        record.io_type = Write;
    } else {
        return false;
    }
    return s == 6;
}

std::string Systor17TraceRecord::toString() {
    return "time: " + std::to_string(this->timestamp) + ","               //
           + "resp: " + std::to_string(this->response) + ", "             //
           + "offset: " + std::to_string(this->lba) + ", "                //
           + "lun: " + std::to_string(this->lun) + ", "                   //
           + "type: " + (this->io_type == Read ? "Read" : "Write") + ","  //
           + "size: " + std::to_string(this->block_size);
}

// MSR Trace

bool parseMSRTraceRecordFromLine(const std::string &line, MSRTraceRecord &r) {
    std::string buffer;
    std::vector<std::string> tokens;
    for (auto &c : line) {
        if (c == ' ') continue;
        if (c == ',') {
            tokens.push_back(buffer);
            buffer.clear();
        } else {
            buffer.push_back(c);
        }
    }
    if (!buffer.empty()) tokens.push_back(buffer);
    if (tokens.size() != 7) return false;
    r.timestamp = strtoull(tokens[0].c_str(), nullptr, 10);
    r.host_name = tokens[1];
    r.disk_number = strtoull(tokens[2].c_str(), nullptr, 10);
    r.lba = strtoull(tokens[4].c_str(), nullptr, 10);
    r.block_size = strtoull(tokens[5].c_str(), nullptr, 10);
    r.response = strtoull(tokens[6].c_str(), nullptr, 10);
    //[3]

    if (r.lba % r.block_size != 0) return false;  // filter for austere cache
    if (tokens[3] == "Read") {
        r.io_type = Read;
    } else if (tokens[3] == "Write") {
        r.io_type = Write;
    } else {
        return false;
    }
    return true;
}

std::vector<MSRTraceRecord> readMSRTraceFromFile(const std::string &file_name, size_t expect_block_size) {
    std::ifstream input(file_name);
    std::vector<MSRTraceRecord> res;
    Assert(input.is_open(), "Can not open file %s", file_name.c_str());
    std::string line;
    while (std::getline(input, line)) {
        MSRTraceRecord record;
        if (parseMSRTraceRecordFromLine(line, record)) {
            if (expect_block_size == 0 || expect_block_size == record.block_size) {
                res.push_back(record);
            }
        }
    }
    return res;
}
