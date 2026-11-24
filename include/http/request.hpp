#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>
#include <compare>

namespace farloom::http {

enum class MethodKind : uint8_t {
    Get = 0,
    Post = 1,
    Other = 2,
};

class Method {
public:
    Method() : kind_(MethodKind::Get) {}
    explicit Method(MethodKind kind, std::string custom = "")
        : kind_(kind), custom_(std::move(custom)) {}

    static Method parse(std::string_view token);

    MethodKind kind() const noexcept { return kind_; }
    std::string as_str() const;

    auto operator<=>(const Method& other) const {
        if (auto cmp = kind_ <=> other.kind_; cmp != 0) {
            return cmp;
        }
        return custom_ <=> other.custom_;
    }

    bool operator==(const Method& other) const = default;

private:
    MethodKind kind_{MethodKind::Get};
    std::string custom_{};
};

enum class Version : uint8_t {
    Http10,
    Http11,
};

class Headers {
public:
    Headers() = default;

    void insert(std::string_view name, std::string_view value);
    std::optional<std::string_view> get(std::string_view name) const;
    std::vector<std::string_view> get_all(std::string_view name) const;

    const std::vector<std::pair<std::string, std::string>>& raw() const noexcept {
        return headers_;
    }

    bool operator==(const Headers& other) const = default;

private:
    std::vector<std::pair<std::string, std::string>> headers_;
};

struct Request {
    Method method;
    std::string path;
    std::optional<std::string> query;
    Version version{Version::Http11};
    Headers headers;
    std::vector<uint8_t> body;

    bool operator==(const Request& other) const = default;
};

} // namespace farloom::http
