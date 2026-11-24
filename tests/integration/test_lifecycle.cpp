#include "integration_common.hpp"

#include <atomic>
#include <chrono>
#include <mutex>

using namespace farloom;
using namespace farloom::test;

void test_full_queue_rejection_503() {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};

    Router r = Router::with_builtins();
    r.get("/block", [&](const http::Request&) {
        entered.store(true);
        while (!release.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return http::Response::new_resp(http::StatusCode::Ok)
            .with_body("text/plain", {'r','e','l','e','a','s','e','d'});
    });

    Config cfg;
    cfg.workers = 1;
    cfg.queue_capacity = 1;
    TestServer server(cfg, std::move(r));

    // A occupies the only worker thread
    std::thread t_a([&]() {
        auto resp = get_request(server.host(), server.port(), "/block");
        ASSERT_EQ(resp.status, 200);
        ASSERT_EQ(resp.body_str(), "released");
    });

    while (!entered.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // B connects and sends request -> queued into the single queue slot
    int fd_b = connect_to(server.host(), server.port());
    ASSERT_TRUE(fd_b >= 0);
    std::string req_b = "GET /health HTTP/1.1\r\nHost: test\r\n\r\n";
    ::send(fd_b, req_b.data(), req_b.size(), 0);

    // Short pause so acceptor pulls B from listener into queue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // C connects -> Queue is full, acceptor rejects with 503
    int fd_c = connect_to(server.host(), server.port());
    ASSERT_TRUE(fd_c >= 0);
    auto raw_c = read_to_eof(fd_c);
    ::close(fd_c);

    auto resp_c = parse_raw_response(raw_c);
    ASSERT_TRUE(resp_c.is_ok());
    ASSERT_EQ(resp_c.value().status, 503);
    ASSERT_EQ(resp_c.value().reason, "Service Unavailable");
    ASSERT_EQ(resp_c.value().header("Retry-After"), std::make_optional(std::string_view("1")));
    ASSERT_EQ(resp_c.value().header("Connection"), std::make_optional(std::string_view("close")));

    // Release A
    release.store(true);
    t_a.join();

    // Now B completes with 200
    auto raw_b = read_to_eof(fd_b);
    ::close(fd_b);
    auto resp_b = parse_raw_response(raw_b);
    ASSERT_TRUE(resp_b.is_ok());
    ASSERT_EQ(resp_b.value().status, 200);
    ASSERT_EQ(resp_b.value().body_str(), R"({"status":"ok"})");
}

void test_idle_and_partial_timeout() {
    Config cfg;
    cfg.read_timeout = std::chrono::seconds(1);
    TestServer server(cfg, Router::with_builtins());

    int fd_idle = connect_to(server.host(), server.port());
    ASSERT_TRUE(fd_idle >= 0);
    auto start = std::chrono::steady_clock::now();
    auto raw_idle = read_to_eof(fd_idle);
    ::close(fd_idle);
    auto dur = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(dur < std::chrono::seconds(4));
    ASSERT_TRUE(raw_idle.empty());

    // Partial head
    int fd_partial = connect_to(server.host(), server.port());
    ASSERT_TRUE(fd_partial >= 0);
    std::string partial = "GET /health HTTP/1.1\r\nHost: test\r\n";
    ::send(fd_partial, partial.data(), partial.size(), 0);
    auto raw_partial = read_to_eof(fd_partial);
    ::close(fd_partial);
    ASSERT_TRUE(raw_partial.empty());

    // Server still serves after timeouts
    auto resp = get_request(server.host(), server.port(), "/health");
    ASSERT_EQ(resp.status, 200);
}

void test_shutdown_stops_listening() {
    Config cfg;
    TestServer server(cfg, Router::with_builtins());
    auto resp = get_request(server.host(), server.port(), "/health");
    ASSERT_EQ(resp.status, 200);

    uint16_t port = server.port();
    server.stop();

    // Connect should fail now
    int fd = connect_to("127.0.0.1", port, std::chrono::seconds(1));
    ASSERT_TRUE(fd < 0);
}

void test_max_requests() {
    Config cfg;
    cfg.max_requests = 3;
    TestServer server(cfg, Router::with_builtins());

    for (int i = 0; i < 3; ++i) {
        auto resp = get_request(server.host(), server.port(), "/health");
        ASSERT_EQ(resp.status, 200);
    }

    // Wait briefly for server run() to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int fd = connect_to(server.host(), server.port(), std::chrono::seconds(1));
    ASSERT_TRUE(fd < 0);
}

void test_concurrent_clients() {
    Config cfg;
    cfg.workers = 4;
    cfg.queue_capacity = 64;
    TestServer server(cfg, Router::with_builtins());

    constexpr int CLIENTS = 32;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    threads.reserve(CLIENTS);

    for (int i = 0; i < CLIENTS; ++i) {
        threads.emplace_back([&]() {
            auto resp = get_request(server.host(), server.port(), "/health");
            if (resp.status == 200) {
                success_count++;
            } else if (resp.status == 503) {
                // overloaded 503 is also acceptable under heavy concurrent load
            } else {
                ASSERT_TRUE(false);
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    ASSERT_TRUE(success_count.load() > 0);
}

int main() {
    RUN_TEST(test_full_queue_rejection_503);
    RUN_TEST(test_idle_and_partial_timeout);
    RUN_TEST(test_shutdown_stops_listening);
    RUN_TEST(test_max_requests);
    RUN_TEST(test_concurrent_clients);
    return 0;
}
