#include "connection.hpp"
#include "logger.hpp"
#include "reader.hpp"

#include <chrono>
#include <format>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace farloom {

namespace {

constexpr std::chrono::milliseconds WORKER_CLOSE_BUDGET{250};
constexpr size_t MAX_DRAIN_BYTES = 64 * 1024;

} // namespace

void close_gracefully(int fd, std::chrono::milliseconds budget) {
    if (fd < 0) return;
    auto deadline = std::chrono::steady_clock::now() + budget;
    ::shutdown(fd, SHUT_WR);

    uint8_t buf[4096];
    size_t drained = 0;

    while (drained < MAX_DRAIN_BYTES) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) break;
        auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now);
        if (remaining.count() <= 0) break;

        struct timeval tv;
        tv.tv_sec = static_cast<time_t>(remaining.count() / 1000000);
        tv.tv_usec = static_cast<suseconds_t>(remaining.count() % 1000000);
        if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
            break;
        }

        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n > 0) {
            drained += static_cast<size_t>(n);
        } else {
            if (errno == EINTR) continue;
            break;
        }
    }
}

http::Response dispatch(const AppContext& ctx, const http::Request& req) {
    return dispatch_from(ctx, req, std::nullopt);
}

http::Response dispatch_from(const AppContext& ctx, const http::Request& req, std::optional<std::string_view> client) {
    auto match = ctx.router.lookup(req.method, req.path);
    switch (match.kind) {
        case RouteMatchKind::Found: {
            try {
                return (*match.handler)(req);
            } catch (const std::exception& e) {
                logger::error("handler_panic", e.what());
                return http::Response::error(http::StatusCode::InternalServerError);
            } catch (...) {
                logger::error("handler_panic", "<unknown exception>");
                return http::Response::error(http::StatusCode::InternalServerError);
            }
        }
        case RouteMatchKind::MethodNotAllowed:
            return http::Response::method_not_allowed(match.allow);
        case RouteMatchKind::NotFound:
            return ctx.static_files.serve_from(req, client);
    }
    return http::Response::error(http::StatusCode::NotFound);
}

void handle_connection(const AppContext& ctx, Connection conn) {
    using namespace std::chrono;
    auto start = steady_clock::now();

    int fd = conn.fd;
    std::string peer = conn.peer;

    io::SocketReader sreader(fd);
    io::BufferedReader reader(sreader);

    auto parsed = http::parse_request(reader, ctx.limits);

    http::Response resp = http::Response::new_resp(http::StatusCode::Ok);
    std::string method;
    std::string path;

    if (parsed.is_ok()) {
        const auto& req = parsed.value();
        resp = dispatch_from(ctx, req, peer);
        method = req.method.as_str();
        path = req.path;
    } else {
        const auto& err = parsed.error();
        switch (err.kind) {
            case http::ParseErrorKind::BadRequest:
                logger::error("parse_error", std::format("client={} {}", peer, err.message));
                resp = http::Response::error(http::StatusCode::BadRequest);
                method = "-";
                path = "-";
                break;
            case http::ParseErrorKind::PayloadTooLarge:
                resp = http::Response::error(http::StatusCode::PayloadTooLarge);
                method = "-";
                path = "-";
                break;
            case http::ParseErrorKind::Timeout:
                logger::error("timeout", std::format("client={} read timed out", peer));
                return;
            case http::ParseErrorKind::Incomplete:
                logger::error("incomplete", std::format("client={} connection closed mid-request", peer));
                return;
            case http::ParseErrorKind::Io:
                logger::error("io", std::format("client={} {}", peer, err.message));
                return;
            case http::ParseErrorKind::ConnectionClosed:
                return;
        }
    }

    if (!resp.write_response(fd)) {
        logger::error("write", std::format("client={} failed to write response", peer));
        return;
    }

    auto dur_ms = static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now() - start).count());
    close_gracefully(fd, WORKER_CLOSE_BUDGET);

    std::string worker = logger::current_worker();
    logger::access(logger::AccessRecord{
        logger::now_ms(),
        worker,
        peer,
        method,
        path,
        http::status_code(resp.status),
        dur_ms
    });
}

} // namespace farloom
