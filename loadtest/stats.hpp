#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace farloom::loadtest {

struct Stats {
    std::map<uint16_t, uint64_t> by_status;
    uint64_t errors{0};
    std::vector<double> latencies_ms;

    void record_status(uint16_t code, double latency_ms = 0.0);
    void record_error();
    void merge(Stats other);
    uint64_t total() const noexcept;
};

std::optional<uint16_t> parse_status_line(std::span<const uint8_t> head);
std::string format_report(const Stats& stats, std::chrono::nanoseconds elapsed);

} // namespace farloom::loadtest
