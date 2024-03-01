#ifndef CDCACHE_UTILS_H
#define CDCACHE_UTILS_H

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#define DEBUG

#ifdef WIN32
#define FN (__builtin_strrchr(__FILE__, '\\') ? __builtin_strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define FN (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define LOGGER(...)
// #define LOGGER(...) log(FN, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ERROR(...) error_msg(FN, __FUNCTION__, __LINE__, __VA_ARGS__)

// #define INFO(...) fprintf(stdout, __VA_ARGS__)

void log(const char *file_name, const char *function_name, size_t line, const char *fmt, ...);
void error_msg(const char *file_name, const char *function_name, size_t line, const char *fmt, ...);

#ifdef DEBUG
#define Assert(Expr, ...) M_Assert(#Expr, Expr, __FILE__, __LINE__, __VA_ARGS__)
#else
#define Assert(Expr, ...) assert(Expr);
#endif

void M_Assert(const char *expr_str, bool expr, const char *file, int line, const char *fmt, ...);

// disable data copy
struct NonCopyable {
    NonCopyable &operator=(const NonCopyable &) = delete;

    NonCopyable(const NonCopyable &) = delete;

    NonCopyable() = default;
};

uint32_t hash3char(const uint8_t *str);

time_t get_timestamp();

std::string timestamp_to_string(time_t time);

// typedefs
typedef uint8_t byte_t;
typedef uint64_t addr_t;
typedef uint64_t fp_t;
typedef int pos_t;

template <typename T>
std::string vec2str(const std::vector<T> &v) {
    if (v.empty()) return "";
    std::string res;
    for (int i = 0; i < v.size() - 1; i++) {
        res += std::to_string(v[i]) + ", ";
    }
    res += std::to_string(v.back());
    return res;
}
#endif

std::string str_to_hex(const std::string &s);
std::vector<byte_t> read_file(const std::string &file_name);

// simple profiler
#define PROF_TIMER(label, Codes)                                                       \
    auto start_##label = std::chrono::high_resolution_clock::now();                    \
    Codes auto e_##label = std::chrono::high_resolution_clock ::now() - start_##label; \
    auto time_##label = std::chrono::duration_cast<std::chrono::microseconds>(e_##label).count();
