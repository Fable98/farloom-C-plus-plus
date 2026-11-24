#include "test_framework.hpp"
#include "args.hpp"

using namespace farloom::loadtest;

void test_loadtest_args() {
    std::vector<std::string> empty = {};
    auto def = parse_args(empty);
    ASSERT_TRUE(def.is_ok());
    ASSERT_EQ(def.value().addr, "127.0.0.1:7878");
    ASSERT_EQ(def.value().path, "/");
    ASSERT_EQ(def.value().requests, 1000);
    ASSERT_EQ(def.value().concurrency, 8);

    std::vector<std::string> opts = {
        "--addr", "10.0.0.1:80", "--path", "/sleep", "--requests", "5", "--concurrency", "3"
    };
    auto a = parse_args(opts);
    ASSERT_TRUE(a.is_ok());
    ASSERT_EQ(a.value().addr, "10.0.0.1:80");
    ASSERT_EQ(a.value().path, "/sleep");
    ASSERT_EQ(a.value().requests, 5);
    ASSERT_EQ(a.value().concurrency, 3);

    std::vector<std::string> inl = {
        "--addr=localhost:9000", "--path=/index.html", "--requests=42", "--concurrency=16"
    };
    auto b = parse_args(inl);
    ASSERT_TRUE(b.is_ok());
    ASSERT_EQ(b.value().addr, "localhost:9000");
    ASSERT_EQ(b.value().path, "/index.html");
    ASSERT_EQ(b.value().requests, 42);
    ASSERT_EQ(b.value().concurrency, 16);

    std::vector<std::string> zreq = {"--requests", "0"};
    ASSERT_TRUE(parse_args(zreq).is_err());

    std::vector<std::string> zconc = {"--concurrency", "0"};
    ASSERT_TRUE(parse_args(zconc).is_err());

    std::vector<std::string> h = {"--help"};
    ASSERT_EQ(parse_args(h).error().kind, ArgsErrorKind::HelpRequested);
}

int main() {
    RUN_TEST(test_loadtest_args);
    return 0;
}
