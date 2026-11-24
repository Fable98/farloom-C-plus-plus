#include "test_framework.hpp"
#include "stats.hpp"

using namespace farloom::loadtest;

void test_stats_recording() {
    Stats s;
    s.record_status(200);
    s.record_status(200);
    s.record_status(503);
    s.record_error();
    ASSERT_EQ(s.by_status.at(200), 2);
    ASSERT_EQ(s.by_status.at(503), 1);
    ASSERT_EQ(s.errors, 1);
    ASSERT_EQ(s.total(), 4);

    Stats b;
    b.record_status(200);
    b.record_status(404);
    b.record_error();

    s.merge(b);
    ASSERT_EQ(s.by_status.at(200), 3);
    ASSERT_EQ(s.by_status.at(503), 1);
    ASSERT_EQ(s.by_status.at(404), 1);
    ASSERT_EQ(s.errors, 2);
    ASSERT_EQ(s.total(), 7);
}

void test_status_line_parsing() {
    std::string s1 = "HTTP/1.1 200 OK\r\n\r\n";
    std::span<const uint8_t> sp1(reinterpret_cast<const uint8_t*>(s1.data()), s1.size());
    ASSERT_EQ(parse_status_line(sp1), std::make_optional<uint16_t>(200));

    std::string s2 = "HTTP/1.0 404 Not Found\r\n";
    std::span<const uint8_t> sp2(reinterpret_cast<const uint8_t*>(s2.data()), s2.size());
    ASSERT_EQ(parse_status_line(sp2), std::make_optional<uint16_t>(404));

    std::string s3 = "HTTP/1.1 503 Service Unavailable\r\n";
    std::span<const uint8_t> sp3(reinterpret_cast<const uint8_t*>(s3.data()), s3.size());
    ASSERT_EQ(parse_status_line(sp3), std::make_optional<uint16_t>(503));

    std::string s_bad = "invalid";
    std::span<const uint8_t> sp_bad(reinterpret_cast<const uint8_t*>(s_bad.data()), s_bad.size());
    ASSERT_EQ(parse_status_line(sp_bad), std::nullopt);
}

void test_report_formatting() {
    Stats s;
    s.record_status(200);
    s.record_status(200);
    s.record_status(200);
    s.record_status(503);
    std::string rep = format_report(s, std::chrono::seconds(2));
    ASSERT_TRUE(rep.starts_with("requests:    4\n"));
    ASSERT_TRUE(rep.find("status 200:  3\n") != std::string::npos);
    ASSERT_TRUE(rep.find("status 503:  1\n") != std::string::npos);
    ASSERT_TRUE(rep.find("errors:      0\n") != std::string::npos);
    ASSERT_TRUE(rep.find("elapsed:     2.000 s\n") != std::string::npos);
    ASSERT_TRUE(rep.find("throughput:  2.0 req/s\n") != std::string::npos);
}

int main() {
    RUN_TEST(test_stats_recording);
    RUN_TEST(test_status_line_parsing);
    RUN_TEST(test_report_formatting);
    return 0;
}
