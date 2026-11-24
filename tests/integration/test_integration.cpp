#include "integration_common.hpp"

using namespace farloom;
using namespace farloom::test;

const std::string_view INDEX_HTML = "<!doctype html>\n<html><body>Hello \xc3\xa9</body></html>\n";
const std::string_view A_CSS = "body { color: red; }\r\n/* trailing */\n";

TestServer make_static_server() {
    Config cfg;
    TestServer server(cfg, Router::with_builtins());
    server.write_file("index.html", INDEX_HTML);
    server.write_file("a.css", A_CSS);
    return server;
}

void test_static_index_and_css() {
    auto server = make_static_server();
    for (const char* path : {"/", "/index.html"}) {
        auto resp = get_request(server.host(), server.port(), path);
        ASSERT_EQ(resp.status, 200);
        ASSERT_EQ(resp.reason, "OK");
        ASSERT_EQ(resp.body_str(), INDEX_HTML);
        ASSERT_EQ(resp.header("Content-Type"), std::make_optional(std::string_view("text/html; charset=utf-8")));
        ASSERT_EQ(resp.header("Connection"), std::make_optional(std::string_view("close")));
    }

    auto resp_css = get_request(server.host(), server.port(), "/a.css");
    ASSERT_EQ(resp_css.status, 200);
    ASSERT_EQ(resp_css.body_str(), A_CSS);
    ASSERT_EQ(resp_css.header("Content-Type"), std::make_optional(std::string_view("text/css; charset=utf-8")));
}

void test_health_route() {
    Config cfg;
    TestServer server(cfg, Router::with_builtins());
    auto resp = get_request(server.host(), server.port(), "/health");
    ASSERT_EQ(resp.status, 200);
    ASSERT_EQ(resp.reason, "OK");
    ASSERT_EQ(resp.body_str(), R"({"status":"ok"})");
    ASSERT_EQ(resp.header("Content-Type"), std::make_optional(std::string_view("application/json")));
    ASSERT_EQ(resp.header("Connection"), std::make_optional(std::string_view("close")));

    // Query string does not affect routing
    auto resp_query = get_request(server.host(), server.port(), "/health?x=1");
    ASSERT_EQ(resp_query.status, 200);
    ASSERT_EQ(resp_query.body_str(), R"({"status":"ok"})");
}

void test_echo_route() {
    Config cfg;
    TestServer server(cfg, Router::with_builtins());

    std::string body = "\x00\xff\xfe binary \x80\r\n\r\nend";
    std::string req = std::format(
        "POST /api/echo HTTP/1.1\r\nHost: test\r\nContent-Length: {}\r\nContent-Type: application/x-custom; v=1\r\n\r\n{}",
        body.size(), body
    );
    auto resp = raw_request(server.host(), server.port(), req);
    ASSERT_EQ(resp.status, 200);
    ASSERT_EQ(resp.body_str(), body);
    ASSERT_EQ(resp.header("Content-Type"), std::make_optional(std::string_view("application/x-custom; v=1")));

    // Without content type -> octet-stream
    std::string req2 = std::format(
        "POST /api/echo HTTP/1.1\r\nHost: test\r\nContent-Length: {}\r\n\r\n{}",
        body.size(), body
    );
    auto resp2 = raw_request(server.host(), server.port(), req2);
    ASSERT_EQ(resp2.status, 200);
    ASSERT_EQ(resp2.body_str(), body);
    ASSERT_EQ(resp2.header("Content-Type"), std::make_optional(std::string_view("application/octet-stream")));
}

void test_bad_request_errors() {
    auto server = make_static_server();
    for (const char* req : {
        "GET / HTTP/1.1\n\n",
        "POST /api/echo HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
        "GET /%zz HTTP/1.1\r\nHost: test\r\n\r\n",
    }) {
        auto resp = raw_request(server.host(), server.port(), req);
        ASSERT_EQ(resp.status, 400);
        ASSERT_EQ(resp.reason, "Bad Request");
        ASSERT_TRUE(resp.header("Content-Type")->starts_with("text/plain"));
        ASSERT_TRUE(resp.body_str().find("400") != std::string::npos);
        ASSERT_TRUE(resp.body_str().find("Bad Request") != std::string::npos);
    }
}

void test_not_found_and_traversal() {
    auto server = make_static_server();
    server.write_file("../secret.txt", "secret");

    for (const char* path : {
        "/missing.html",
        "/..%2f..%2fsecret.txt",
        "/..%2fsecret.txt",
        "/%5c..%5csecret.txt"
    }) {
        auto resp = get_request(server.host(), server.port(), path);
        ASSERT_EQ(resp.status, 404);
        ASSERT_EQ(resp.reason, "Not Found");
        ASSERT_TRUE(resp.body_str().find("secret") == std::string::npos);
    }
}

void test_method_not_allowed() {
    auto server = make_static_server();

    auto r1 = raw_request(server.host(), server.port(), "POST /index.html HTTP/1.1\r\nHost: test\r\nContent-Length: 0\r\n\r\n");
    ASSERT_EQ(r1.status, 405);
    ASSERT_EQ(r1.header("Allow"), std::make_optional(std::string_view("GET")));

    auto r2 = raw_request(server.host(), server.port(), "GET /api/echo HTTP/1.1\r\nHost: test\r\n\r\n");
    ASSERT_EQ(r2.status, 405);
    ASSERT_EQ(r2.header("Allow"), std::make_optional(std::string_view("POST")));

    auto r3 = raw_request(server.host(), server.port(), "DELETE /health HTTP/1.1\r\nHost: test\r\n\r\n");
    ASSERT_EQ(r3.status, 405);
    ASSERT_EQ(r3.header("Allow"), std::make_optional(std::string_view("GET")));
}

void test_payload_too_large() {
    Config cfg;
    cfg.max_body_bytes = 10;
    TestServer server(cfg, Router::with_builtins());

    std::string req = "POST /api/echo HTTP/1.1\r\nHost: test\r\nContent-Length: 11\r\n\r\n";
    auto resp = raw_request(server.host(), server.port(), req);
    ASSERT_EQ(resp.status, 413);
    ASSERT_EQ(resp.reason, "Payload Too Large");
}

void test_handler_panic_gives_500() {
    Config cfg;
    Router r = Router::with_builtins();
    r.get("/boom", [](const http::Request&) -> http::Response {
        throw std::runtime_error("boom handler exception");
    });
    TestServer server(cfg, std::move(r));

    auto r_boom = get_request(server.host(), server.port(), "/boom");
    ASSERT_EQ(r_boom.status, 500);
    ASSERT_EQ(r_boom.reason, "Internal Server Error");

    // Server continues serving requests normally
    auto r_health = get_request(server.host(), server.port(), "/health");
    ASSERT_EQ(r_health.status, 200);
    ASSERT_EQ(r_health.body_str(), R"({"status":"ok"})");
}

int main() {
    RUN_TEST(test_static_index_and_css);
    RUN_TEST(test_health_route);
    RUN_TEST(test_echo_route);
    RUN_TEST(test_bad_request_errors);
    RUN_TEST(test_not_found_and_traversal);
    RUN_TEST(test_method_not_allowed);
    RUN_TEST(test_payload_too_large);
    RUN_TEST(test_handler_panic_gives_500);
    return 0;
}
