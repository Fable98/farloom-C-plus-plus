#pragma once

#include "http/request.hpp"
#include "http/response.hpp"
#include "http/parser.hpp"
#include "router.hpp"
#include "static_files.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>

namespace farloom {

struct Connection {
    int fd{-1};
    std::string peer;

    Connection() = default;
    Connection(int f, std::string p) : fd(f), peer(std::move(p)) {}
    ~Connection() {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&& other) noexcept : fd(other.fd), peer(std::move(other.peer)) {
        other.fd = -1;
    }
    Connection& operator=(Connection&& other) noexcept {
        if (this != &other) {
            if (fd >= 0) ::close(fd);
            fd = other.fd;
            peer = std::move(other.peer);
            other.fd = -1;
        }
        return *this;
    }
};

struct AppContext {
    Router router;
    StaticFiles static_files;
    http::Limits limits;
};

void close_gracefully(int fd, std::chrono::milliseconds budget);
http::Response dispatch(const AppContext& ctx, const http::Request& req);
http::Response dispatch_from(const AppContext& ctx, const http::Request& req, std::optional<std::string_view> client);
void handle_connection(const AppContext& ctx, Connection conn);

} // namespace farloom
