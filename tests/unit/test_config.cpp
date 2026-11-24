#include "test_framework.hpp"
#include "config.hpp"

#include <filesystem>
#include <fstream>

using namespace farloom;

namespace {

std::filesystem::path make_temp_dir(const std::string& tag) {
    auto p = std::filesystem::temp_directory_path() / std::format("webserver-config-{}-{}", tag, std::rand());
    std::filesystem::create_directories(p);
    return p;
}

} // namespace

void test_empty_args_give_defaults() {
    std::vector<std::string> args = {};
    auto res = parse_config(args);
    ASSERT_TRUE(res.is_ok());
    ASSERT_EQ(res.value(), Config::default_config());
}

void test_zero_workers_is_rejected() {
    std::vector<std::string> args = {"--workers", "0"};
    auto res = parse_config(args);
    ASSERT_TRUE(res.is_err());
    ASSERT_EQ(res.error().kind, ConfigErrorKind::MustBePositive);
    ASSERT_EQ(res.error().option, "--workers");
}

void test_zero_queue_capacity_is_rejected() {
    std::vector<std::string> args = {"--queue-capacity", "0"};
    auto res = parse_config(args);
    ASSERT_TRUE(res.is_err());
    ASSERT_EQ(res.error().kind, ConfigErrorKind::MustBePositive);
    ASSERT_EQ(res.error().option, "--queue-capacity");
}

void test_zero_port_is_accepted() {
    std::vector<std::string> args = {"--port", "0"};
    auto res = parse_config(args);
    ASSERT_TRUE(res.is_ok());
    ASSERT_EQ(res.value().port, 0);
}

void test_separate_and_inline_values_are_equivalent() {
    std::vector<std::string> sep = {
        "--host", "0.0.0.0", "--port", "8080", "--workers", "8", "--queue-capacity", "16",
        "--static-root", "site", "--read-timeout-secs", "2", "--write-timeout-secs", "3",
        "--max-body-bytes", "10", "--max-requests", "7"
    };
    std::vector<std::string> inl = {
        "--host=0.0.0.0", "--port=8080", "--workers=8", "--queue-capacity=16",
        "--static-root=site", "--read-timeout-secs=2", "--write-timeout-secs=3",
        "--max-body-bytes=10", "--max-requests=7"
    };
    auto r1 = parse_config(sep);
    auto r2 = parse_config(inl);
    ASSERT_TRUE(r1.is_ok());
    ASSERT_TRUE(r2.is_ok());
    ASSERT_EQ(r1.value(), r2.value());
    ASSERT_EQ(r1.value().host, "0.0.0.0");
    ASSERT_EQ(r1.value().port, 8080);
    ASSERT_EQ(r1.value().workers, 8);
    ASSERT_EQ(r1.value().queue_capacity, 16);
    ASSERT_EQ(r1.value().static_root, std::filesystem::path("site"));
    ASSERT_EQ(r1.value().read_timeout, std::chrono::seconds(2));
    ASSERT_EQ(r1.value().write_timeout, std::chrono::seconds(3));
    ASSERT_EQ(r1.value().max_body_bytes, 10);
    ASSERT_EQ(r1.value().max_requests, std::make_optional<uint64_t>(7));
}

void test_last_value_wins() {
    std::vector<std::string> args = {"--port", "1", "--port=2", "--port", "3"};
    auto res = parse_config(args);
    ASSERT_TRUE(res.is_ok());
    ASSERT_EQ(res.value().port, 3);
}

void test_unknown_option_is_rejected() {
    std::vector<std::string> args = {"--bogus", "1"};
    auto res = parse_config(args);
    ASSERT_TRUE(res.is_err());
    ASSERT_EQ(res.error().kind, ConfigErrorKind::UnknownOption);

    std::vector<std::string> pos = {"positional"};
    auto res2 = parse_config(pos);
    ASSERT_TRUE(res2.is_err());
    ASSERT_EQ(res2.error().kind, ConfigErrorKind::UnknownOption);
}

void test_missing_value_is_rejected() {
    std::vector<std::string> args1 = {"--port"};
    auto r1 = parse_config(args1);
    ASSERT_TRUE(r1.is_err());
    ASSERT_EQ(r1.error().kind, ConfigErrorKind::MissingValue);

    std::vector<std::string> args2 = {"--port", "--workers", "2"};
    auto r2 = parse_config(args2);
    ASSERT_TRUE(r2.is_err());
    ASSERT_EQ(r2.error().kind, ConfigErrorKind::MissingValue);
}

void test_invalid_value_display_names_option() {
    std::vector<std::string> args1 = {"--port", "70000"};
    auto r1 = parse_config(args1);
    ASSERT_TRUE(r1.is_err());
    ASSERT_TRUE(r1.error().message.find("--port") != std::string::npos);

    std::vector<std::string> args2 = {"--workers=abc"};
    auto r2 = parse_config(args2);
    ASSERT_TRUE(r2.is_err());
    ASSERT_TRUE(r2.error().message.find("--workers") != std::string::npos);
}

void test_help_is_requested() {
    std::vector<std::string> h1 = {"--help"};
    ASSERT_EQ(parse_config(h1).error().kind, ConfigErrorKind::HelpRequested);
    std::vector<std::string> h2 = {"-h"};
    ASSERT_EQ(parse_config(h2).error().kind, ConfigErrorKind::HelpRequested);
    std::vector<std::string> h3 = {"--port", "1", "--help"};
    ASSERT_EQ(parse_config(h3).error().kind, ConfigErrorKind::HelpRequested);
}

void test_validate_static_root() {
    auto dir = make_temp_dir("valid");
    Config c;
    c.static_root = dir;
    auto r = c.validate_static_root();
    ASSERT_TRUE(r.is_ok());
    ASSERT_EQ(r.value(), std::filesystem::canonical(dir));
    std::filesystem::remove_all(dir);

    Config c_miss;
    c_miss.static_root = dir / "nonexistent";
    ASSERT_TRUE(c_miss.validate_static_root().is_err());

    auto file_dir = make_temp_dir("file");
    auto file = file_dir / "index.html";
    std::ofstream(file) << "hi";
    Config c_file;
    c_file.static_root = file;
    ASSERT_TRUE(c_file.validate_static_root().is_err());
    std::filesystem::remove_all(file_dir);
}

int main() {
    RUN_TEST(test_empty_args_give_defaults);
    RUN_TEST(test_zero_workers_is_rejected);
    RUN_TEST(test_zero_queue_capacity_is_rejected);
    RUN_TEST(test_zero_port_is_accepted);
    RUN_TEST(test_separate_and_inline_values_are_equivalent);
    RUN_TEST(test_last_value_wins);
    RUN_TEST(test_unknown_option_is_rejected);
    RUN_TEST(test_missing_value_is_rejected);
    RUN_TEST(test_invalid_value_display_names_option);
    RUN_TEST(test_help_is_requested);
    RUN_TEST(test_validate_static_root);
    return 0;
}
