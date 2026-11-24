#include "test_framework.hpp"
#include "http/response.hpp"

using namespace farloom::http;

void test_error_response_serializes_with_owned_headers() {
    auto bytes = Response::error(StatusCode::NotFound).to_bytes();
    std::string text(bytes.begin(), bytes.end());
    std::string expected =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 14\r\n"
        "Connection: close\r\n\r\n"
        "404 Not Found\n";
    ASSERT_EQ(text, expected);
}

void test_handler_framing_headers_are_dropped_and_default_type_added() {
    auto resp = Response::new_resp(StatusCode::Ok)
        .with_header("content-length", "999")
        .with_header("CONNECTION", "keep-alive")
        .with_header("X-Test", "1");
    std::string b = "abc";
    resp.body = std::vector<uint8_t>(b.begin(), b.end());
    auto bytes = resp.to_bytes();
    std::string text(bytes.begin(), bytes.end());
    std::string expected =
        "HTTP/1.1 200 OK\r\n"
        "X-Test: 1\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: 3\r\n"
        "Connection: close\r\n\r\n"
        "abc";
    ASSERT_EQ(text, expected);
}

void test_method_not_allowed_sorts_allow_list() {
    std::vector<Method> methods = {Method(MethodKind::Post), Method(MethodKind::Get)};
    auto resp = Response::method_not_allowed(methods);
    ASSERT_EQ(status_code(resp.status), 405);
    bool found_allow = false;
    for (const auto& [n, v] : resp.headers) {
        if (n == "Allow" && v == "GET, POST") {
            found_allow = true;
            break;
        }
    }
    ASSERT_TRUE(found_allow);
}

void test_empty_body_has_no_content_type() {
    auto bytes = Response::new_resp(StatusCode::Ok).to_bytes();
    std::string text(bytes.begin(), bytes.end());
    std::string expected = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    ASSERT_EQ(text, expected);
}

void test_reason_phrase_table() {
    std::vector<std::tuple<StatusCode, uint16_t, std::string>> all = {
        {StatusCode::Ok, 200, "OK"},
        {StatusCode::BadRequest, 400, "Bad Request"},
        {StatusCode::NotFound, 404, "Not Found"},
        {StatusCode::MethodNotAllowed, 405, "Method Not Allowed"},
        {StatusCode::PayloadTooLarge, 413, "Payload Too Large"},
        {StatusCode::InternalServerError, 500, "Internal Server Error"},
        {StatusCode::ServiceUnavailable, 503, "Service Unavailable"},
    };
    for (const auto& [status, code, reason] : all) {
        ASSERT_EQ(status_code(status), code);
        ASSERT_EQ(std::string(status_reason(status)), reason);
        auto bytes = Response::new_resp(status).to_bytes();
        std::string text(bytes.begin(), bytes.end());
        ASSERT_TRUE(text.starts_with(std::format("HTTP/1.1 {} {}\r\n", code, reason)));
    }
}

void test_every_error_status_is_text_plain_with_code_and_reason() {
    std::vector<StatusCode> errs = {
        StatusCode::BadRequest,
        StatusCode::NotFound,
        StatusCode::MethodNotAllowed,
        StatusCode::PayloadTooLarge,
        StatusCode::InternalServerError,
        StatusCode::ServiceUnavailable,
    };
    for (auto sc : errs) {
        auto resp = Response::error(sc);
        auto bytes = resp.to_bytes();
        std::string text(bytes.begin(), bytes.end());
        ASSERT_TRUE(text.find("Content-Type: text/plain; charset=utf-8\r\n") != std::string::npos);
        ASSERT_TRUE(text.find(std::format("Content-Length: {}\r\n", resp.body.size())) != std::string::npos);
        std::string body(resp.body.begin(), resp.body.end());
        ASSERT_TRUE(body.find(std::to_string(status_code(sc))) != std::string::npos);
        ASSERT_TRUE(body.find(status_reason(sc)) != std::string::npos);
    }
}

int main() {
    RUN_TEST(test_error_response_serializes_with_owned_headers);
    RUN_TEST(test_handler_framing_headers_are_dropped_and_default_type_added);
    RUN_TEST(test_method_not_allowed_sorts_allow_list);
    RUN_TEST(test_empty_body_has_no_content_type);
    RUN_TEST(test_reason_phrase_table);
    RUN_TEST(test_every_error_status_is_text_plain_with_code_and_reason);
    return 0;
}
