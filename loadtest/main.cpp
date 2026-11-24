#include "args.hpp"
#include "stats.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <format>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace farloom::loadtest {

namespace {

constexpr std::chrono::seconds IO_TIMEOUT{10};

struct ResolvedTarget {
    sockaddr_storage addr{};
    socklen_t len{0};
};

Result<std::vector<ResolvedTarget>, std::string> resolve(std::string_view addr_str) {
    size_t colon = addr_str.rfind(':');
    if (colon == std::string_view::npos) {
        return Result<std::vector<ResolvedTarget>, std::string>::err("no port in address");
    }
    std::string host(addr_str.substr(0, colon));
    std::string port(addr_str.substr(colon + 1));

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    int err = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (err != 0) {
        return Result<std::vector<ResolvedTarget>, std::string>::err(gai_strerror(err));
    }

    std::vector<ResolvedTarget> targets;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        ResolvedTarget t;
        std::memcpy(&t.addr, p->ai_addr, p->ai_addrlen);
        t.len = p->ai_addrlen;
        targets.push_back(t);
    }
    ::freeaddrinfo(res);

    if (targets.empty()) {
        return Result<std::vector<ResolvedTarget>, std::string>::err("no addresses found");
    }
    return Result<std::vector<ResolvedTarget>, std::string>::ok(targets);
}

std::vector<uint8_t> build_request(std::string_view addr, std::string_view path) {
    std::string req = std::format("GET {} HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n", path, addr);
    return std::vector<uint8_t>(req.begin(), req.end());
}

std::optional<std::pair<uint16_t, double>> send_one(
    const std::vector<ResolvedTarget>& targets,
    const std::vector<uint8_t>& request
) {
    using namespace std::chrono;
    int fd = -1;
    for (const auto& target : targets) {
        fd = ::socket(target.addr.ss_family, SOCK_STREAM, 0);
        if (fd < 0) continue;

        struct timeval tv;
        tv.tv_sec = IO_TIMEOUT.count();
        tv.tv_usec = 0;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (::connect(fd, reinterpret_cast<const sockaddr*>(&target.addr), target.len) == 0) {
            break;
        }
        ::close(fd);
        fd = -1;
    }

    if (fd < 0) return std::nullopt;

    auto start = high_resolution_clock::now();

    size_t total_sent = 0;
    while (total_sent < request.size()) {
        ssize_t n = ::send(fd, request.data() + total_sent, request.size() - total_sent, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            ::close(fd);
            return std::nullopt;
        }
        total_sent += static_cast<size_t>(n);
    }

    std::vector<uint8_t> buf;
    uint8_t chunk[8192];
    while (true) {
        ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n == 0) break;
        if (n > 0) {
            buf.insert(buf.end(), chunk, chunk + n);
        } else {
            if (errno == EINTR) continue;
            break;
        }
    }
    ::close(fd);

    auto end = high_resolution_clock::now();
    double latency_ms = duration<double, std::milli>(end - start).count();

    if (buf.empty()) return std::nullopt;

    auto code = parse_status_line(buf);
    if (code.has_value()) {
        return std::make_pair(*code, latency_ms);
    }
    return std::nullopt;
}

Stats run_load(const LoadArgs& args, const std::vector<ResolvedTarget>& targets) {
    std::atomic<uint64_t> counter{0};
    auto request = build_request(args.addr, args.path);
    uint64_t total = args.requests;

    std::vector<std::thread> handles;
    std::vector<Stats> thread_stats(args.concurrency);
    handles.reserve(args.concurrency);

    for (size_t i = 0; i < args.concurrency; ++i) {
        handles.emplace_back([i, &counter, total, &targets, &request, &thread_stats]() {
            Stats local;
            while (counter.fetch_add(1, std::memory_order_relaxed) < total) {
                auto res = send_one(targets, request);
                if (res.has_value()) {
                    local.record_status(res->first, res->second);
                } else {
                    local.record_error();
                }
            }
            thread_stats[i] = std::move(local);
        });
    }

    Stats merged;
    for (size_t i = 0; i < handles.size(); ++i) {
        if (handles[i].joinable()) {
            handles[i].join();
            merged.merge(std::move(thread_stats[i]));
        }
    }
    return merged;
}

} // namespace

} // namespace farloom::loadtest

int main(int argc, char* argv[]) {
    using namespace farloom::loadtest;

    auto args_res = parse_args(argc, argv);
    if (args_res.is_err()) {
        const auto& err = args_res.error();
        if (err.kind == ArgsErrorKind::HelpRequested) {
            std::cout << USAGE;
            return 0;
        }
        std::cerr << "error: " << err.message << "\n";
        std::cerr << USAGE;
        return 2;
    }
    const auto& args = args_res.value();

    auto targets_res = resolve(args.addr);
    if (targets_res.is_err()) {
        std::cerr << "error: cannot resolve --addr '" << args.addr << "': " << targets_res.error() << "\n";
        return 2;
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto stats = run_load(args, targets_res.value());
    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    std::cout << format_report(stats, elapsed);

    if (stats.total() == stats.errors) {
        std::cerr << "error: no responses received; is the server running at " << args.addr << "?\n";
        return 1;
    }
    return 0;
}
