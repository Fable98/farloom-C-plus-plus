#include "test_framework.hpp"
#include "logger.hpp"

using namespace farloom::logger;

void test_access_line_formatting() {
    AccessRecord r{
        1718000000456,
        "worker-2",
        "127.0.0.1:54012",
        "GET",
        "/index.html",
        200,
        3
    };
    std::string line = format_access(r);
    std::string expected = "ts=1718000000456 level=INFO event=access worker=worker-2 client=127.0.0.1:54012 method=GET path=/index.html status=200 dur_ms=3\n";
    ASSERT_EQ(line, expected);
}

void test_access_line_escapes_path() {
    AccessRecord r{
        1,
        "worker-0",
        "127.0.0.1:1",
        "GET",
        "/a\r\nts=0 level=INFO",
        404,
        0
    };
    std::string line = format_access(r);
    ASSERT_TRUE(line.find("path=/a\\r\\nts=0 level=INFO ") != std::string::npos);
    ASSERT_TRUE(line.find('\r') == std::string::npos);
}

void test_startup_and_traversal_formats() {
    std::string s = format_startup("127.0.0.1:7878", 4, 64, std::filesystem::path("C:\\srv\\public"));
    ASSERT_TRUE(s.find("level=INFO event=startup addr=127.0.0.1:7878 workers=4 queue_capacity=64 static_root=C:\\srv\\public\n") != std::string::npos);

    std::string t = format_path_traversal("worker-0", "127.0.0.1:7878", "/..%2fCargo.toml", "dotdot");
    ASSERT_TRUE(t.find("level=WARN event=path_traversal worker=worker-0 client=127.0.0.1:7878 path=/..%2fCargo.toml rule=dotdot\n") != std::string::npos);

    std::string r = format_rejected("127.0.0.1:7878");
    ASSERT_TRUE(r.find("level=INFO event=rejected client=127.0.0.1:7878 status=503\n") != std::string::npos);
}

void test_error_line_quotes_and_escapes_msg() {
    std::string e = format_error("worker-3", "handler_panic", "boom\r\n\"x\"\\");
    ASSERT_TRUE(e.find("level=ERROR event=handler_panic worker=worker-3 msg=\"boom\\r\\n\\\"x\\\"\\\\\"\n") != std::string::npos);
}

void test_escape_value_cases() {
    ASSERT_EQ(escape_value(""), "");
    ASSERT_EQ(escape_value("/plain/path.html"), "/plain/path.html");
    ASSERT_EQ(escape_value("a\\b"), "a\\\\b");
    ASSERT_EQ(escape_value("say \"hi\""), "say \\\"hi\\\"");
    ASSERT_EQ(escape_value("\r"), "\\r");
    ASSERT_EQ(escape_value("\n"), "\\n");
    ASSERT_EQ(escape_value("a\tb"), "a\\tb");
}

void test_thread_label() {
    set_thread_label("acceptor");
    ASSERT_EQ(current_worker(), "acceptor");
    set_thread_label(std::nullopt);
}

int main() {
    RUN_TEST(test_access_line_formatting);
    RUN_TEST(test_access_line_escapes_path);
    RUN_TEST(test_startup_and_traversal_formats);
    RUN_TEST(test_error_line_quotes_and_escapes_msg);
    RUN_TEST(test_escape_value_cases);
    RUN_TEST(test_thread_label);
    return 0;
}
