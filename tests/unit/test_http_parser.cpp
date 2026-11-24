#include "test_framework.hpp"
#include "http/parser.hpp"
#include "reader.hpp"

using namespace farloom;
using namespace farloom::http;

Result<Request, ParseError> parse(std::string_view bytes) {
    io::MemoryReader mreader(bytes);
    io::BufferedReader reader(mreader);
    return parse_request(reader, Limits::with_max_body(1024));
}

void test_parses_simple_get() {
    auto req = parse("GET /index.html?x=1 HTTP/1.1\r\nHost: Example\r\n\r\n");
    ASSERT_TRUE(req.is_ok());
    ASSERT_EQ(req.value().method, Method(MethodKind::Get));
    ASSERT_EQ(req.value().path, "/index.html");
    ASSERT_EQ(req.value().query, std::make_optional(std::string("x=1")));
    ASSERT_EQ(req.value().version, Version::Http11);
    ASSERT_EQ(req.value().headers.get("host"), std::make_optional(std::string_view("Example")));
    ASSERT_TRUE(req.value().body.empty());
}

void test_parses_post_with_body_and_leaves_trailing_bytes() {
    std::string input = "POST /api/echo HTTP/1.0\r\nContent-Length:  5 \r\n\r\nhelloEXTRA";
    io::MemoryReader mreader(input);
    io::BufferedReader reader(mreader);
    auto req = parse_request(reader, Limits::with_max_body(1024));
    ASSERT_TRUE(req.is_ok());
    ASSERT_EQ(req.value().method, Method(MethodKind::Post));
    ASSERT_EQ(req.value().version, Version::Http10);
    ASSERT_EQ(req.value().headers.get("Content-Length"), std::make_optional(std::string_view("5")));
    std::string body(req.value().body.begin(), req.value().body.end());
    ASSERT_EQ(body, "hello");
}

void test_empty_input_is_connection_closed() {
    auto req = parse("");
    ASSERT_TRUE(req.is_err());
    ASSERT_EQ(req.error().kind, ParseErrorKind::ConnectionClosed);
}

void test_rejects_bare_lf_and_transfer_encoding() {
    auto r1 = parse("GET / HTTP/1.1\n\n");
    ASSERT_TRUE(r1.is_err());
    ASSERT_EQ(r1.error().kind, ParseErrorKind::BadRequest);

    auto r2 = parse("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n");
    ASSERT_TRUE(r2.is_err());
    ASSERT_EQ(r2.error().kind, ParseErrorKind::BadRequest);
}

void test_oversized_body_is_payload_too_large_without_reading() {
    std::string input = "POST / HTTP/1.1\r\nContent-Length: 2000\r\n\r\nBODY";
    io::MemoryReader mreader(input);
    io::BufferedReader reader(mreader);
    auto req = parse_request(reader, Limits::with_max_body(1024));
    ASSERT_TRUE(req.is_err());
    ASSERT_EQ(req.error().kind, ParseErrorKind::PayloadTooLarge);
}

void test_truncated_body_is_incomplete() {
    auto req = parse("POST / HTTP/1.1\r\nContent-Length: 10\r\n\r\nabc");
    ASSERT_TRUE(req.is_err());
    ASSERT_EQ(req.error().kind, ParseErrorKind::Incomplete);
}

void test_head_of_exactly_limit_succeeds_and_one_more_fails() {
    auto build = [](size_t total) {
        size_t pad = total - 23;
        std::string v = "GET / HTTP/1.1\r\nX: ";
        v.append(pad, 'a');
        v.append("\r\n\r\n");
        return v;
    };
    ASSERT_TRUE(parse(build(8192)).is_ok());
    auto r_bad = parse(build(8193));
    ASSERT_TRUE(r_bad.is_err());
    ASSERT_EQ(r_bad.error().kind, ParseErrorKind::BadRequest);
}

void test_content_length_rules() {
    auto r1 = parse("POST / HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nhello");
    ASSERT_TRUE(r1.is_ok());
    std::string body(r1.value().body.begin(), r1.value().body.end());
    ASSERT_EQ(body, "hello");

    auto r2 = parse("POST / HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nhello!");
    ASSERT_TRUE(r2.is_err());
    ASSERT_EQ(r2.error().kind, ParseErrorKind::BadRequest);

    for (const char* v : {"+5", "-1", "5,5", " "}) {
        std::string req = std::format("POST / HTTP/1.1\r\nContent-Length:{}\r\n\r\n", v);
        auto r = parse(req);
        ASSERT_TRUE(r.is_err());
        ASSERT_EQ(r.error().kind, ParseErrorKind::BadRequest);
    }

    auto r_overflow = parse("POST / HTTP/1.1\r\nContent-Length: 99999999999999999999999\r\n\r\n");
    ASSERT_TRUE(r_overflow.is_err());
    ASSERT_EQ(r_overflow.error().kind, ParseErrorKind::PayloadTooLarge);
}

void test_header_names_lowercased_and_values_trimmed() {
    auto req = parse("GET / HTTP/1.1\r\nX-Custom-HEADER: \t value \t\r\n\r\n");
    ASSERT_TRUE(req.is_ok());
    ASSERT_EQ(req.value().headers.get("x-custom-header"), std::make_optional(std::string_view("value")));
    ASSERT_EQ(req.value().headers.get("X-CUSTOM-HEADER"), std::make_optional(std::string_view("value")));
}

void test_malformed_heads_are_bad_request() {
    for (const char* s : {
        "GET / HTTP/1.1\r\nA: b\r\n continued\r\n\r\n",
        "GET / HTTP/1.1\r\nHost : x\r\n\r\n",
        "GET /a%zz HTTP/1.1\r\n\r\n",
        "GET /a?q=%4 HTTP/1.1\r\n\r\n",
        "GET /a#frag HTTP/1.1\r\n\r\n",
        "GET http://example.com/ HTTP/1.1\r\n\r\n",
        "GET / HTTP/2.0\r\n\r\n",
    }) {
        auto r = parse(s);
        ASSERT_TRUE(r.is_err());
        ASSERT_EQ(r.error().kind, ParseErrorKind::BadRequest);
    }
}

void test_raw_non_ascii_target_is_bad_request_but_encoded_is_fine() {
    ASSERT_TRUE(parse("GET /caf\u00e9 HTTP/1.1\r\n\r\n").is_err());
    ASSERT_TRUE(parse("GET /a\u2028b HTTP/1.1\r\n\r\n").is_err());
    ASSERT_TRUE(parse("GET /a?q=\u0085 HTTP/1.1\r\n\r\n").is_err());

    auto req = parse("GET /caf%C3%A9 HTTP/1.1\r\n\r\n");
    ASSERT_TRUE(req.is_ok());
    ASSERT_EQ(req.value().path, "/caf%C3%A9");
}

void test_lowercase_method_is_other() {
    auto req = parse("get / HTTP/1.1\r\n\r\n");
    ASSERT_TRUE(req.is_ok());
    ASSERT_EQ(req.value().method, Method(MethodKind::Other, "get"));
}

int main() {
    RUN_TEST(test_parses_simple_get);
    RUN_TEST(test_parses_post_with_body_and_leaves_trailing_bytes);
    RUN_TEST(test_empty_input_is_connection_closed);
    RUN_TEST(test_rejects_bare_lf_and_transfer_encoding);
    RUN_TEST(test_oversized_body_is_payload_too_large_without_reading);
    RUN_TEST(test_truncated_body_is_incomplete);
    RUN_TEST(test_head_of_exactly_limit_succeeds_and_one_more_fails);
    RUN_TEST(test_content_length_rules);
    RUN_TEST(test_header_names_lowercased_and_values_trimmed);
    RUN_TEST(test_malformed_heads_are_bad_request);
    RUN_TEST(test_raw_non_ascii_target_is_bad_request_but_encoded_is_fine);
    RUN_TEST(test_lowercase_method_is_other);
    return 0;
}
