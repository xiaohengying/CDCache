#include "utils.h"

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "config.h"
#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

void log(const char *file_name, const char *function_name, size_t line, const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    fprintf(globalEnv().c.logger, KGRN "[%s:%zu @ %s]: %s", file_name, line, function_name, KWHT);
    vfprintf(globalEnv().c.logger, fmt, args);
    fprintf(globalEnv().c.logger, "\n");
    fflush(globalEnv().c.logger);
#endif
}

void error_msg(const char *file_name, const char *function_name, size_t line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(globalEnv().c.logger, KRED "[ERROR] [%s:%zu @ %s]: %s", file_name, line, function_name, KWHT);
    vfprintf(globalEnv().c.logger, fmt, args);
    fprintf(globalEnv().c.logger, "\n");
    fflush(globalEnv().c.logger);
}
void M_Assert(const char *expr_str, bool expr, const char *file, int line, const char *msg, ...) {
    if (!expr) {
        fprintf(stderr, KRED "Assert failed:\t");
        va_list args;
        va_start(args, msg);
        vfprintf(stderr, msg, args);
        fprintf(stderr, "\nExpected: %s\n", expr_str);
        fprintf(stderr, "At Source: %s:%d\n", file, line);
        abort();
    }
}

uint32_t hash3char(const uint8_t *str) {
    if (!str || !(str + 1) || !(str + 2)) {
        throw std::runtime_error("Invalid string when calculate hash");
    }
    return (static_cast<uint32_t>(str[0]) << 16) | (static_cast<uint32_t>(str[1]) << 8) |
           (static_cast<uint32_t>(str[2]));
}
// 封装下因为可能后面要用到其他的时间戳
time_t get_timestamp() { return std::time(nullptr); }

std::string timestamp_to_string(time_t time) {
    auto tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H-%M-%S");
    return oss.str();
}

std::string str_to_hex(const std::string &s) {
    std::string map = "0123456789abcdef";
    std::string res;
    for (int i = 0; i < s.size(); i++) {
        uint8_t c = s[i];
        res.push_back(map[c / 16]);
        res.push_back(map[c % 16]);
    }
    return res;
}

std::vector<byte_t> read_file(const std::string &file_name) {
    std::ifstream input(file_name, std::ios::binary);
    std::vector<byte_t> bytes((std::istreambuf_iterator<char>(input)), (std::istreambuf_iterator<char>()));
    input.close();
    return bytes;
}
