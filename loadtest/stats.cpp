#include "stats.hpp"

#include <algorithm>
#include <format>
#include <numeric>

namespace farloom::loadtest {

namespace {

std::string line(std::string_view label, std::string_view value) {
    return std::format("{:<13}{}\n", label, value);
}

} // namespace

void Stats::record_status(uint16_t code, double latency_ms) {
    by_status[code]++;
    if (latency_ms > 0.0) {
        latencies_ms.push_back(latency_ms);
    }
}

void Stats::record_error() {
    errors++;
}

void Stats::merge(Stats other) {
    for (const auto& [code, count] : other.by_status) {
        by_status[code] += count;
    }
    errors += other.errors;
    latencies_ms.insert(latencies_ms.end(), other.latencies_ms.begin(), other.latencies_ms.end());
}

uint64_t Stats::total() const noexcept {
    uint64_t sum = errors;
    for (const auto& [_, count] : by_status) {
        sum += count;
    }
    return sum;
}

std::optional<uint16_t> parse_status_line(std::span<const uint8_t> head) {
    std::string_view sv(reinterpret_cast<const char*>(head.data()), head.size());
    std::string_view rest;
    if (sv.starts_with("HTTP/1.1 ")) {
        rest = sv.substr(9);
    } else if (sv.starts_with("HTTP/1.0 ")) {
        rest = sv.substr(9);
    } else {
        return std::nullopt;
    }

    if (rest.size() < 3) return std::nullopt;
    for (size_t i = 0; i < 3; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(rest[i]))) {
            return std::nullopt;
        }
    }

    if (rest.size() > 3) {
        char c = rest[3];
        if (c != ' ' && c != '\r' && c != '\n') {
            return std::nullopt;
        }
    }

    uint16_t code = static_cast<uint16_t>(
        (rest[0] - '0') * 100 + (rest[1] - '0') * 10 + (rest[2] - '0')
    );
    if (code >= 100 && code <= 599) {
        return code;
    }
    return std::nullopt;
}

std::string format_report(const Stats& stats, std::chrono::nanoseconds elapsed) {
    uint64_t tot = stats.total();
    double secs = std::chrono::duration<double>(elapsed).count();
    double rps = (secs > 0.0) ? (static_cast<double>(tot) / secs) : 0.0;

    std::string out;
    out.append(line("requests:", std::to_string(tot)));
    for (const auto& [code, count] : stats.by_status) {
        out.append(line(std::format("status {}:", code), std::to_string(count)));
    }
    out.append(line("errors:", std::to_string(stats.errors)));
    out.append(line("elapsed:", std::format("{:.3f} s", secs)));
    out.append(line("throughput:", std::format("{:.1f} req/s", rps)));

    if (!stats.latencies_ms.empty()) {
        std::vector<double> sorted = stats.latencies_ms;
        std::sort(sorted.begin(), sorted.end());
        double min_lat = sorted.front();
        double max_lat = sorted.back();
        double sum_lat = std::accumulate(sorted.begin(), sorted.end(), 0.0);
        double avg_lat = sum_lat / static_cast<double>(sorted.size());
        size_t p50_idx = static_cast<size_t>(sorted.size() * 0.50);
        size_t p95_idx = static_cast<size_t>(sorted.size() * 0.95);
        size_t p99_idx = static_cast<size_t>(sorted.size() * 0.99);
        if (p95_idx >= sorted.size()) p95_idx = sorted.size() - 1;
        if (p99_idx >= sorted.size()) p99_idx = sorted.size() - 1;

        out.append(line("latency avg:", std::format("{:.2f} ms", avg_lat)));
        out.append(line("latency min:", std::format("{:.2f} ms", min_lat)));
        out.append(line("latency p50:", std::format("{:.2f} ms", sorted[p50_idx])));
        out.append(line("latency p95:", std::format("{:.2f} ms", sorted[p95_idx])));
        out.append(line("latency p99:", std::format("{:.2f} ms", sorted[p99_idx])));
        out.append(line("latency max:", std::format("{:.2f} ms", max_lat)));
    }

    return out;
}

} // namespace farloom::loadtest
