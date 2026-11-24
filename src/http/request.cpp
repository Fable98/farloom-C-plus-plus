#include "http/request.hpp"

#include <algorithm>
#include <cctype>

namespace farloom::http {

namespace {

std::string to_lower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

bool equals_ignore_case(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

Method Method::parse(std::string_view token) {
    if (token == "GET") {
        return Method(MethodKind::Get);
    }
    if (token == "POST") {
        return Method(MethodKind::Post);
    }
    return Method(MethodKind::Other, std::string(token));
}

std::string Method::as_str() const {
    switch (kind_) {
        case MethodKind::Get:
            return "GET";
        case MethodKind::Post:
            return "POST";
        case MethodKind::Other:
            return custom_;
    }
    return "";
}

void Headers::insert(std::string_view name, std::string_view value) {
    headers_.emplace_back(to_lower(name), std::string(value));
}

std::optional<std::string_view> Headers::get(std::string_view name) const {
    for (const auto& [n, v] : headers_) {
        if (equals_ignore_case(n, name)) {
            return v;
        }
    }
    return std::nullopt;
}

std::vector<std::string_view> Headers::get_all(std::string_view name) const {
    std::vector<std::string_view> out;
    for (const auto& [n, v] : headers_) {
        if (equals_ignore_case(n, name)) {
            out.push_back(v);
        }
    }
    return out;
}

} // namespace farloom::http
