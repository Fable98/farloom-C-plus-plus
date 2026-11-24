#pragma once

#include "test_framework.hpp"
#include "server.hpp"
#include "config.hpp"
#include "router.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace farloom::test {

struct RawResponse {
    uint16_t status{0};
    std::string reason;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<uint8_t> body;
    std::vector<uint8_t> raw;

    std::optional<std::string_view> header(std::string_view name) const {
        for (const auto& [n, v] : headers) {
            if (n.size() == name.size()) {
                bool match = true;
                for (size_t i = 0; i < n.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(n[i])) !=
                        std::tolower(static_cast<unsigned char>(name[i]))) {
                        match = false;
                        break;
                    }
                }
                if (match) return v;
            }
        }
        return std::nullopt;
    }

    size_t header_count(std::string_view name) const {
        size_t count = 0;
        for (const auto& [n, _] : headers) {
            if (n.size() == name.size()) {
                bool match = true;
                for (size_t i = 0; i < n.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(n[i])) !=
                        std::tolower(static_cast<unsigned char>(name[i]))) {
                        match = false;
                        break;
                    }
                }
                if (match) count++;
            }
        }
        return count;
    }

    std::string body_str() const {
        return std::string(body.begin(), body.end());
    }
};

inline Result<RawResponse, std::string> parse_raw_response(std::vector<uint8_t> raw) {
    std::string_view sv(reinterpret_cast<const char*>(raw.data()), raw.size());
    size_t head_end = sv.find("\r\n\r\n");
    if (head_end == std::string_view::npos) {
        return Result<RawResponse, std::string>::err("no end of head in response");
    }

    std::string_view head = sv.substr(0, head_end);
    size_t line_end = head.find("\r\n");
    std::string_view status_line = head.substr(0, line_end);

    size_t sp1 = status_line.find(' ');
    if (sp1 == std::string_view::npos) return Result<RawResponse, std::string>::err("bad status line");
    std::string_view ver = status_line.substr(0, sp1);
    if (!ver.starts_with("HTTP/1.")) return Result<RawResponse, std::string>::err("bad HTTP version");

    size_t sp2 = status_line.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos) return Result<RawResponse, std::string>::err("bad status code");
    std::string_view code_str = status_line.substr(sp1 + 1, sp2 - sp1 - 1);
    uint16_t status = static_cast<uint16_t>(std::stoi(std::string(code_str)));
    std::string reason(status_line.substr(sp2 + 1));

    std::vector<std::pair<std::string, std::string>> headers;
    size_t cur = line_end + 2;
    while (cur < head.size()) {
        size_t next = head.find("\r\n", cur);
        std::string_view line = (next == std::string_view::npos) ? head.substr(cur) : head.substr(cur, next - cur);
        if (!line.empty()) {
            size_t colon = line.find(':');
            if (colon != std::string_view::npos) {
                std::string name(line.substr(0, colon));
                std::string_view val = line.substr(colon + 1);
                while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.remove_prefix(1);
                while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.remove_suffix(1);
                headers.emplace_back(name, std::string(val));
            }
        }
        if (next == std::string_view::npos) break;
        cur = next + 2;
    }

    std::vector<uint8_t> body;
    if (raw.size() > head_end + 4) {
        body.assign(raw.begin() + static_cast<std::ptrdiff_t>(head_end + 4), raw.end());
    }

    return Result<RawResponse, std::string>::ok(RawResponse{
        status, reason, std::move(headers), std::move(body), std::move(raw)
    });
}

inline int connect_to(std::string_view host, uint16_t port, std::chrono::seconds timeout = std::chrono::seconds(10)) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct timeval tv;
    tv.tv_sec = timeout.count();
    tv.tv_usec = 0;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, std::string(host).c_str(), &addr.sin_addr);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

inline std::vector<uint8_t> read_to_eof(int fd) {
    std::vector<uint8_t> out;
    uint8_t buf[8192];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n > 0) {
            out.insert(out.end(), buf, buf + n);
        } else {
            if (errno == EINTR) continue;
            break;
        }
    }
    return out;
}

inline RawResponse raw_request(std::string_view host, uint16_t port, std::string_view req_bytes) {
    int fd = connect_to(host, port);
    if (fd < 0) {
        std::cerr << "Failed to connect to " << host << ":" << port << "\n";
        std::exit(1);
    }
    size_t total = 0;
    while (total < req_bytes.size()) {
        ssize_t n = ::send(fd, req_bytes.data() + total, req_bytes.size() - total, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }
        total += static_cast<size_t>(n);
    }
    auto raw = read_to_eof(fd);
    ::close(fd);

    auto res = parse_raw_response(std::move(raw));
    if (res.is_err()) {
        std::cerr << "Bad response: " << res.error() << "\n";
        std::exit(1);
    }
    return res.value();
}

inline RawResponse get_request(std::string_view host, uint16_t port, std::string_view path) {
    std::string req = std::format("GET {} HTTP/1.1\r\nHost: test\r\n\r\n", path);
    return raw_request(host, port, req);
}

class TestServer {
public:
    explicit TestServer(Config cfg, Router r)
        : temp_dir_(std::filesystem::temp_directory_path() / std::format("webserver-it-{}", std::rand())) {
        std::filesystem::create_directories(temp_dir_);
        if (cfg.static_root.empty() || cfg.static_root == "./public") {
            cfg.static_root = temp_dir_;
        }
        cfg.port = 0; // ephemeral port

        auto bound = Server::bind(cfg, std::move(r));
        if (bound.is_err()) {
            std::cerr << "Server::bind failed: " << bound.error() << "\n";
            std::exit(1);
        }
        server_ = std::make_unique<Server>(std::move(bound.value()));
        host_ = "127.0.0.1";
        port_ = server_->local_port();
        shutdown_ = server_->shutdown_handle();

        server_thread_ = std::thread([this]() {
            server_->run();
        });
    }

    TestServer(const TestServer&) = delete;
    TestServer& operator=(const TestServer&) = delete;
    TestServer(TestServer&& other) noexcept
        : temp_dir_(std::move(other.temp_dir_)),
          server_(std::move(other.server_)),
          host_(std::move(other.host_)),
          port_(other.port_),
          shutdown_(std::move(other.shutdown_)),
          server_thread_(std::move(other.server_thread_)),
          stopped_(other.stopped_) {
        other.stopped_ = true;
    }
    TestServer& operator=(TestServer&& other) noexcept {
        if (this != &other) {
            stop();
            temp_dir_ = std::move(other.temp_dir_);
            server_ = std::move(other.server_);
            host_ = std::move(other.host_);
            port_ = other.port_;
            shutdown_ = std::move(other.shutdown_);
            server_thread_ = std::move(other.server_thread_);
            stopped_ = other.stopped_;
            other.stopped_ = true;
        }
        return *this;
    }

    ~TestServer() {
        stop();
        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    void stop() {
        if (!stopped_) {
            stopped_ = true;
            shutdown_.shutdown();
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
        }
    }

    const std::filesystem::path& root() const noexcept { return temp_dir_; }
    std::string host() const { return host_; }
    uint16_t port() const noexcept { return port_; }
    ShutdownHandle shutdown_handle() const { return shutdown_; }

    void write_file(const std::string& rel_path, std::string_view content) {
        auto p = temp_dir_ / rel_path;
        std::filesystem::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << content;
    }

private:
    std::filesystem::path temp_dir_;
    std::unique_ptr<Server> server_;
    std::string host_;
    uint16_t port_{0};
    ShutdownHandle shutdown_;
    std::thread server_thread_;
    bool stopped_{false};
};

} // namespace farloom::test
