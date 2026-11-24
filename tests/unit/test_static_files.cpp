#include "test_framework.hpp"
#include "static_files.hpp"

#include <filesystem>
#include <fstream>
#include <format>

using namespace farloom;
using namespace farloom::http;

namespace {

struct Fixture {
    std::filesystem::path temp_dir;
    std::filesystem::path root;
    std::unique_ptr<StaticFiles> sf;

    Fixture() {
        temp_dir = std::filesystem::temp_directory_path() / std::format("webserver-static-{}", std::rand());
        std::filesystem::create_directories(temp_dir);
        root = temp_dir / "public";
        std::filesystem::create_directories(root / "sub");
        std::filesystem::create_directories(root / "empty");

        std::ofstream(root / "index.html") << "<h1>home</h1>";
        std::ofstream(root / "a.css") << "body{}";
        std::ofstream(root / "sub" / "index.html") << "sub index";
        std::ofstream(root / "sub" / "x.txt") << "xx";
        std::ofstream(temp_dir / "secret.txt") << "secret";

        sf = std::make_unique<StaticFiles>(std::filesystem::canonical(root));
    }

    ~Fixture() {
        std::filesystem::remove_all(temp_dir);
    }
};

Request make_req(Method method, std::string path) {
    Request r;
    r.method = std::move(method);
    r.path = std::move(path);
    return r;
}

} // namespace

void test_serves_files_and_directory_index() {
    Fixture f;
    auto r1 = f.sf->serve(make_req(Method(MethodKind::Get), "/a.css"));
    ASSERT_EQ(status_code(r1.status), 200);
    std::string b1(r1.body.begin(), r1.body.end());
    ASSERT_EQ(b1, "body{}");

    auto r2 = f.sf->serve(make_req(Method(MethodKind::Get), "/"));
    ASSERT_EQ(status_code(r2.status), 200);
    std::string b2(r2.body.begin(), r2.body.end());
    ASSERT_EQ(b2, "<h1>home</h1>");

    auto r3 = f.sf->serve(make_req(Method(MethodKind::Get), "/sub/"));
    ASSERT_EQ(status_code(r3.status), 200);
    std::string b3(r3.body.begin(), r3.body.end());
    ASSERT_EQ(b3, "sub index");

    auto r4 = f.sf->serve(make_req(Method(MethodKind::Get), "/./sub//x%2Etxt"));
    ASSERT_EQ(status_code(r4.status), 200);
    std::string b4(r4.body.begin(), r4.body.end());
    ASSERT_EQ(b4, "xx");
}

void test_missing_paths_are_404() {
    Fixture f;
    for (const char* p : {"/nope.txt", "/empty", "/empty/", "/index.html/x", "/sub/x.txt/y/z", "/%FF"}) {
        auto r = f.sf->serve(make_req(Method(MethodKind::Get), p));
        ASSERT_EQ(status_code(r.status), 404);
    }
}

void test_traversal_attempts_are_rejected() {
    Fixture f;
    std::vector<std::pair<std::string, std::string>> cases = {
        {"/../secret.txt", "dotdot"},
        {"/sub/%2e%2e/%2E%2E/secret.txt", "dotdot"},
        {"/..%2fsecret.txt", "dotdot"},
        {"/%5c..%5csecret.txt", "backslash"},
        {"/a\\b", "backslash"},
        {"/a%00.txt", "nul"},
        {"//server/share", "leading //"},
        {"/%2f%2fserver/share", "leading //"},
        {"/C:/Windows/win.ini", "colon"},
        {"/c%3a/x", "colon"},
        {"/index.html::$DATA", "colon"},
    };
    for (const auto& [p, rule] : cases) {
        auto res = f.sf->resolve(p);
        ASSERT_TRUE(res.is_err());
        ASSERT_EQ(res.error().kind, ResolveErrorKind::Traversal);
        ASSERT_EQ(res.error().rule, rule);
        auto r = f.sf->serve(make_req(Method(MethodKind::Get), p));
        ASSERT_EQ(status_code(r.status), 404);
    }
}

void test_sibling_directory_is_outside_root() {
    auto tmp = std::filesystem::temp_directory_path() / std::format("webserver-sibling-{}", std::rand());
    std::filesystem::create_directories(tmp / "public");
    std::filesystem::create_directories(tmp / "public2");
    std::ofstream(tmp / "public" / "index.html") << "ok";
    std::ofstream(tmp / "public2" / "secret.txt") << "secret";

    auto root = std::filesystem::canonical(tmp / "public");
    StaticFiles sf(root);

    for (const char* p : {
        "/../public2/secret.txt",
        "/..%2fpublic2%2fsecret.txt",
        "/%2e%2e/public2/secret.txt",
        "/..\\public2\\secret.txt",
        "/public2/secret.txt",
        "/2/secret.txt",
        "/secret.txt",
    }) {
        auto res = sf.resolve(p);
        ASSERT_TRUE(res.is_err());
        auto resp = sf.serve(make_req(Method(MethodKind::Get), p));
        ASSERT_EQ(status_code(resp.status), 404);
    }
    std::filesystem::remove_all(tmp);
}

void test_oversized_and_reserved_names_are_404_not_500() {
    Fixture f;
    std::string long_name = "/" + std::string(300, 'a');
    std::string deep_name;
    for (int i = 0; i < 500; ++i) deep_name += "/a";

    for (const auto& p : {
        "/con", "/nul", "/aux.txt", "/COM1", "/index.html.", "/index.html%20", "/a*b",
        "/a%3Fb", "/a<b", "/a%22b", "/a|b", "/%01", "/%7f", "/...", long_name.c_str(),
        deep_name.c_str()
    }) {
        auto res = f.sf->resolve(p);
        ASSERT_TRUE(res.is_err());
        ASSERT_EQ(res.error().kind, ResolveErrorKind::NotFound);
        auto resp = f.sf->serve(make_req(Method(MethodKind::Get), p));
        ASSERT_EQ(status_code(resp.status), 404);
    }
}

int main() {
    RUN_TEST(test_serves_files_and_directory_index);
    RUN_TEST(test_missing_paths_are_404);
    RUN_TEST(test_traversal_attempts_are_rejected);
    RUN_TEST(test_sibling_directory_is_outside_root);
    RUN_TEST(test_oversized_and_reserved_names_are_404_not_500);
    return 0;
}
