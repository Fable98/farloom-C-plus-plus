#include "test_framework.hpp"
#include "http/percent.hpp"

using namespace farloom::http::percent;

void test_mixed_case_dot_escapes_decode_to_dotdot() {
    auto res = percent_decode("%2e%2E");
    ASSERT_TRUE(res.is_ok());
    std::vector<uint8_t> expected = {'.', '.'};
    ASSERT_EQ(res.value(), expected);
    ASSERT_TRUE(validate("%2e%2E").is_ok());
}

void test_malformed_sequences_report_index_of_percent() {
    std::vector<std::pair<std::string, size_t>> cases = {
        {"%", 0},
        {"%4", 0},
        {"%zz", 0},
        {"/a%", 2},
        {"/a%4", 2},
        {"/ok%20/%zz", 7},
        {"%4g", 0},
        {"%g4", 0},
    };
    for (const auto& [input, index] : cases) {
        auto v = validate(input);
        ASSERT_TRUE(v.is_err());
        ASSERT_EQ(v.error().index, index);

        auto d = percent_decode(input);
        ASSERT_TRUE(d.is_err());
        ASSERT_EQ(d.error().index, index);
    }
}

void test_plus_is_preserved() {
    auto r1 = percent_decode("/a+b");
    ASSERT_TRUE(r1.is_ok());
    std::string s1(r1.value().begin(), r1.value().end());
    ASSERT_EQ(s1, "/a+b");

    auto r2 = percent_decode("%2B+");
    ASSERT_TRUE(r2.is_ok());
    std::string s2(r2.value().begin(), r2.value().end());
    ASSERT_EQ(s2, "++");
}

void test_encoded_utf8_decodes_to_original_bytes() {
    std::string encoded = "/caf%C3%A9/%E6%97%A5%E6%9C%AC/%F0%9F%A6%80";
    std::string original = "/caf\u00e9/\u65e5\u672c/\U0001f980";

    auto res = percent_decode(encoded);
    ASSERT_TRUE(res.is_ok());
    std::string decoded(res.value().begin(), res.value().end());
    ASSERT_EQ(decoded, original);
}

void test_raw_non_ascii_input_is_copied_unchanged() {
    std::string s = "/caf\u00e9";
    ASSERT_TRUE(validate(s).is_ok());
    auto res = percent_decode(s);
    ASSERT_TRUE(res.is_ok());
    std::string decoded(res.value().begin(), res.value().end());
    ASSERT_EQ(decoded, s);
}

void test_decoding_may_produce_invalid_utf8() {
    auto res = percent_decode("%FF%00");
    ASSERT_TRUE(res.is_ok());
    std::vector<uint8_t> expected = {0xFF, 0x00};
    ASSERT_EQ(res.value(), expected);
}

void test_empty_and_plain_strings() {
    ASSERT_TRUE(validate("").is_ok());
    auto res1 = percent_decode("");
    ASSERT_TRUE(res1.is_ok());
    ASSERT_TRUE(res1.value().empty());

    auto res2 = percent_decode("/index.html");
    ASSERT_TRUE(res2.is_ok());
    std::string s2(res2.value().begin(), res2.value().end());
    ASSERT_EQ(s2, "/index.html");
}

int main() {
    RUN_TEST(test_mixed_case_dot_escapes_decode_to_dotdot);
    RUN_TEST(test_malformed_sequences_report_index_of_percent);
    RUN_TEST(test_plus_is_preserved);
    RUN_TEST(test_encoded_utf8_decodes_to_original_bytes);
    RUN_TEST(test_raw_non_ascii_input_is_copied_unchanged);
    RUN_TEST(test_decoding_may_produce_invalid_utf8);
    RUN_TEST(test_empty_and_plain_strings);
    return 0;
}
