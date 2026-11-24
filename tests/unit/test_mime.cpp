#include "test_framework.hpp"
#include "mime.hpp"

using namespace farloom::mime;

void test_mime_types() {
    std::vector<std::pair<std::string, std::string>> table = {
        {"file.html", "text/html; charset=utf-8"},
        {"file.htm", "text/html; charset=utf-8"},
        {"file.css", "text/css; charset=utf-8"},
        {"file.js", "text/javascript; charset=utf-8"},
        {"file.json", "application/json"},
        {"file.txt", "text/plain; charset=utf-8"},
        {"file.png", "image/png"},
        {"file.jpg", "image/jpeg"},
        {"file.jpeg", "image/jpeg"},
        {"file.gif", "image/gif"},
        {"file.svg", "image/svg+xml"},
        {"file.ico", "image/x-icon"},
    };

    for (const auto& [file, expected] : table) {
        ASSERT_EQ(mime_for_path(file), expected);
    }

    ASSERT_EQ(mime_for_path("INDEX.HTML"), "text/html; charset=utf-8");
    ASSERT_EQ(mime_for_path("photo.JpEg"), "image/jpeg");
    ASSERT_EQ(mime_for_path("a/b/Style.CSS"), "text/css; charset=utf-8");

    ASSERT_EQ(mime_for_path("archive.zip"), "application/octet-stream");
    ASSERT_EQ(mime_for_path("README"), "application/octet-stream");
    ASSERT_EQ(mime_for_path(".html"), "application/octet-stream");
    ASSERT_EQ(mime_for_path("dir/"), "application/octet-stream");

    ASSERT_EQ(mime_for_path("bundle.min.js"), "text/javascript; charset=utf-8");
    ASSERT_EQ(mime_for_path("page.html.bak"), "application/octet-stream");
}

int main() {
    RUN_TEST(test_mime_types);
    return 0;
}
