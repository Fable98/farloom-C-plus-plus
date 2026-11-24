#include "server.hpp"
#include "logger.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <format>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>

namespace farloom {

namespace {

constexpr std::chrono::milliseconds ACCEPT_POLL_INTERVAL{25};
constexpr std::chrono::milliseconds REJECT_CLOSE_BUDGET{50};

int prepare_socket(int fd, const Config& config) {
    // Blocking mode
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }

    struct timeval rtv;
    rtv.tv_sec = static_cast<time_t>(config.read_timeout.count());
    rtv.tv_usec = 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv)) != 0) {
        return -1;
    }

    struct timeval wtv;
    wtv.tv_sec = static_cast<time_t>(config.write_timeout.count());
    wtv.tv_usec = 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &wtv, sizeof(wtv)) != 0) {
        return -1;
    }
    return 0;
}

void reject_503(Connection conn) {
    int fd = conn.fd;
    std::string peer = conn.peer;

    auto resp = http::Response::error(http::StatusCode::ServiceUnavailable)
        .with_header("Retry-After", "1");
    if (!resp.write_response(fd)) {
        logger::error("write", std::format("client={} failed to write 503 response", peer));
    }
    close_gracefully(fd, REJECT_CLOSE_BUDGET);
    logger::rejected(peer);
}

} // namespace

Server::Server(
    int listener_fd,
    std::string local_ip,
    uint16_t local_port,
    std::unique_ptr<ThreadPool<Connection>> pool,
    ShutdownHandle shutdown,
    Config config,
    std::shared_ptr<AppContext> ctx
) : listener_fd_(listener_fd),
    local_ip_(std::move(local_ip)),
    local_port_(local_port),
    pool_(std::move(pool)),
    shutdown_(std::move(shutdown)),
    config_(std::move(config)),
    ctx_(std::move(ctx)) {}

Server::~Server() {
    if (listener_fd_ >= 0) {
        ::close(listener_fd_);
        listener_fd_ = -1;
    }
}

Server::Server(Server&& other) noexcept
    : listener_fd_(other.listener_fd_),
      local_ip_(std::move(other.local_ip_)),
      local_port_(other.local_port_),
      pool_(std::move(other.pool_)),
      shutdown_(std::move(other.shutdown_)),
      config_(std::move(other.config_)),
      ctx_(std::move(other.ctx_)) {
    other.listener_fd_ = -1;
}

Server& Server::operator=(Server&& other) noexcept {
    if (this != &other) {
        if (listener_fd_ >= 0) ::close(listener_fd_);
        listener_fd_ = other.listener_fd_;
        local_ip_ = std::move(other.local_ip_);
        local_port_ = other.local_port_;
        pool_ = std::move(other.pool_);
        shutdown_ = std::move(other.shutdown_);
        config_ = std::move(other.config_);
        ctx_ = std::move(other.ctx_);
        other.listener_fd_ = -1;
    }
    return *this;
}

std::string Server::local_addr() const {
    return std::format("{}:{}", local_ip_, local_port_);
}

Result<Server, std::string> Server::bind(Config config, Router router) {
    auto root_res = config.validate_static_root();
    if (root_res.is_err()) {
        return Result<Server, std::string>::err(
            std::format("invalid static root '{}': {}", config.static_root.string(), root_res.error())
        );
    }
    auto canonical_root = root_res.value();

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return Result<Server, std::string>::err(
            std::format("failed to create socket: {}", strerror(errno))
        );
    }

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port);
    if (::inet_pton(AF_INET, config.host.c_str(), &addr.sin_addr) <= 0) {
        ::close(fd);
        return Result<Server, std::string>::err(
            std::format("failed to parse host address: {}", config.host)
        );
    }

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::string err = strerror(errno);
        ::close(fd);
        return Result<Server, std::string>::err(
            std::format("failed to bind to {}:{}: {}", config.host, config.port, err)
        );
    }

    struct sockaddr_in bound_addr{};
    socklen_t bound_len = sizeof(bound_addr);
    if (::getsockname(fd, reinterpret_cast<struct sockaddr*>(&bound_addr), &bound_len) != 0) {
        std::string err = strerror(errno);
        ::close(fd);
        return Result<Server, std::string>::err(
            std::format("failed to get socket name: {}", err)
        );
    }

    char ip_buf[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &bound_addr.sin_addr, ip_buf, sizeof(ip_buf));
    uint16_t bound_port = ntohs(bound_addr.sin_port);

    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::string err = strerror(errno);
        ::close(fd);
        return Result<Server, std::string>::err(
            std::format("failed to set non-blocking: {}", err)
        );
    }

    if (::listen(fd, 128) != 0) {
        std::string err = strerror(errno);
        ::close(fd);
        return Result<Server, std::string>::err(
            std::format("failed to listen: {}", err)
        );
    }

    auto ctx = std::make_shared<AppContext>(AppContext{
        std::move(router),
        StaticFiles(canonical_root),
        http::Limits::with_max_body(config.max_body_bytes)
    });

    std::unique_ptr<ThreadPool<Connection>> pool;
    try {
        pool = std::make_unique<ThreadPool<Connection>>(
            config.workers,
            config.queue_capacity,
            [ctx](Connection conn) {
                handle_connection(*ctx, std::move(conn));
            }
        );
    } catch (const std::exception& e) {
        ::close(fd);
        return Result<Server, std::string>::err(
            std::format("failed to spawn worker thread: {}", e.what())
        );
    }

    std::string bound_ip(ip_buf);
    std::string full_addr = std::format("{}:{}", bound_ip, bound_port);
    logger::startup(full_addr, config.workers, config.queue_capacity, logger::display_path(canonical_root));

    return Result<Server, std::string>::ok(Server(
        fd,
        bound_ip,
        bound_port,
        std::move(pool),
        ShutdownHandle(),
        std::move(config),
        ctx
    ));
}

Result<void, std::string> Server::run() {
    logger::set_thread_label("acceptor");
    uint64_t accepted = 0;

    while (!shutdown_.is_shutdown()) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = ::accept(listener_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

        if (client_fd >= 0) {
            accepted++;
            if (config_.max_requests.has_value() && *config_.max_requests == accepted) {
                shutdown_.shutdown();
            }

            char client_ip[INET_ADDRSTRLEN] = {0};
            ::inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            std::string peer = std::format("{}:{}", static_cast<const char*>(client_ip), ntohs(client_addr.sin_port));

            if (prepare_socket(client_fd, config_) != 0) {
                logger::error("socket_setup", std::format("client={} {}", peer, strerror(errno)));
                ::close(client_fd);
                continue;
            }

            Connection conn(client_fd, peer);
            auto res = pool_->try_submit(std::move(conn));
            if (res.is_err()) {
                reject_503(std::move(res.error().second));
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(ACCEPT_POLL_INTERVAL);
            } else if (errno == EINTR) {
                continue;
            } else {
                logger::error("accept", strerror(errno));
                std::this_thread::sleep_for(ACCEPT_POLL_INTERVAL);
            }
        }
    }

    if (listener_fd_ >= 0) {
        ::close(listener_fd_);
        listener_fd_ = -1;
    }
    if (pool_) {
        pool_->shutdown();
    }
    logger::set_thread_label(std::nullopt);
    return Result<void, std::string>::ok();
}

} // namespace farloom
