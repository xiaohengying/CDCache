#ifndef CDCACHE_TRACE_READER_H
#define CDCACHE_TRACE_READER_H

#include <cstddef>
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

#include "io_request.h"
#include "utils.h"

//    [ts in ns]
//    [pid]
//    [process]
//    [lba]
//    [size in 512 Bytes blocks]
//    [Write or Read]
//    [major device number]
//    [minor device number]
//    [MD5 per 512/8192 Bytes]
// READ
// 89968195792462 20782 gzip 283193184 8 R 6 0 56f11b711d91a065a2b6458eca924523
struct FIUTraceRecord {
    uint64_t timestamp;
    int pid;
    std::string process_name;
    uint64_t lba;
    size_t block_size;
    IOType io_type;
    std::string device_number;
    std::string hash;
    friend std::istream &operator>>(std::istream &input, FIUTraceRecord &r);
};

std::vector<FIUTraceRecord> readFIUTraceFromFile(const std::string &file_name, size_t expect_block_size);

bool parseFIUTraceRecordFromLine(const std::string &line, FIUTraceRecord &record);

// 1456095145.133076000,0.000806000,R,0,899056122368,131072
// Timestamp,Response,IOType,LUN,Offset,Size
/**
 * @brief
 Timestamp,Response,IOType,LUN,Offset,Size

  - Timestamp is the time the I/O was issued.
    The timestamp is given as a Unix time (seconds since 1/1/1970) with a fractional part.
    Although the fractional part is nine digits, it is accurate only to the microsecond level;
    please  ignore the nanosecond part.
    If you need to process the timestamps in their original local timezone, it is UTC+0900 (JST).
    For example:
     > head 2016022219-LUN4.csv.gz  ← (Mon, 22 Feb 2016 19:00:00 JST)
       1456135200.013118000 ← (Mon, 22 Feb 2016 10:00:00 GMT)
  - Response is the time needed to complete the I/O.
  - IOType is "Read(R)", "Write(W)", or ""(blank).
    The blank indicates that there was no response message.
  - LUN is the LUN index (0,1,2,3,4, or 5).
  - Offset is the starting offset of the I/O in bytes from the start of
    the logical disk.
  - Size is the transfer size of the I/O request in bytes.
 *
 */

struct Systor17TraceRecord {
    double timestamp{0.0};
    double response{0.0};
    IOType io_type{Read};
    int lun{-1};
    addr_t lba{0};
    size_t block_size{0};
    std::string toString();
};

std::vector<Systor17TraceRecord> readSystorTraceFromFile(const std::string &file_name, size_t expect_block_size);

bool parseSystorTraceRecordFromLine(const std::string &line, Systor17TraceRecord &record);

// same format with austere
// 1721815040 4096 W 51d346c8bbef83bb777b719864218b9dd611dd98 2.288647
struct SYNTraceRecord {
    size_t lba{};
    size_t block_size{};
    IOType io_type{};
    std::string block_hash;
    double compression_ratio{};
};
std::vector<SYNTraceRecord> readSYNTraceFromFile(const std::string &file_name, size_t expect_block_size);

bool parseSYNTraceRecordFromLine(const std::string &line, SYNTraceRecord &record);

// 128166385295514663,hm,0,Write,9056014336,2048,2833
struct MSRTraceRecord {
    int64_t timestamp{0};
    std::string host_name;
    int disk_number{0};
    IOType io_type{Read};
    addr_t lba{0};
    size_t block_size{0};
    int response{0};
};

std::vector<MSRTraceRecord> readMSRTraceFromFile(const std::string &file_name, size_t expect_block_size);

bool parseMSRTraceRecordFromLine(const std::string &line, MSRTraceRecord &r);

#endif
// 用于DEBUG测试的数据
