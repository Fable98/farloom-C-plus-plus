#pragma once

#include "config.hpp"
#include "connection.hpp"
#include "router.hpp"
#include "thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

namespace farloom {

class ShutdownHandle {
public:
    ShutdownHandle() : flag_(std::make_shared<std::atomic<bool>>(false)) {}

    void shutdown() const {
        if (flag_) flag_->store(true, std::memory_order_seq_cst);
    }

    bool is_shutdown() const noexcept {
        return flag_ ? flag_->load(std::memory_order_seq_cst) : false;
    }

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

class Server {
public:
    static Result<Server, std::string> bind(Config config, Router router);

    ~Server();
    Server(Server&&) noexcept;
    Server& operator=(Server&&) noexcept;
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    std::string local_addr() const;
    uint16_t local_port() const noexcept { return local_port_; }
    ShutdownHandle shutdown_handle() const { return shutdown_; }

    Result<void, std::string> run();

private:
    Server(
        int listener_fd,
        std::string local_ip,
        uint16_t local_port,
        std::unique_ptr<ThreadPool<Connection>> pool,
        ShutdownHandle shutdown,
        Config config,
        std::shared_ptr<AppContext> ctx
    );

    int listener_fd_{-1};
    std::string local_ip_;
    uint16_t local_port_{0};
    std::unique_ptr<ThreadPool<Connection>> pool_;
    ShutdownHandle shutdown_;
    Config config_;
    std::shared_ptr<AppContext> ctx_;
};

} // namespace farloom
