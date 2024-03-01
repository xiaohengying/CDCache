#include "block_detector.h"

#include "config.h"
#include "utils.h"

void BloomFilterDetector::insert(uint64_t value) { this->filter_.insert(value); }
void BloomFilterDetector::remove(uint64_t value) { this->filter_.remove(value); }
bool BloomFilterDetector::query(uint64_t value) { return this->filter_.query(value); }
// SetDetector
// SetDetector::SetDetector(size_t value) {}

void SetDetector::insert(uint64_t value) { this->set_.insert(value); }
void SetDetector::remove(uint64_t value) { this->set_.erase(value); }
bool SetDetector::query(uint64_t value) { return this->set_.count(value) >= threshold_; }
void SetDetector::dumpInfo() {}