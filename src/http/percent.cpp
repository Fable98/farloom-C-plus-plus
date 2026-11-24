#include "http/percent.hpp"

#include <optional>

namespace farloom::http::percent {

namespace {

std::optional<uint8_t> hex_value(uint8_t b) {
    if (b >= '0' && b <= '9') {
        return static_cast<uint8_t>(b - '0');
    }
    if (b >= 'a' && b <= 'f') {
        return static_cast<uint8_t>(b - 'a' + 10);
    }
    if (b >= 'A' && b <= 'F') {
        return static_cast<uint8_t>(b - 'A' + 10);
    }
    return std::nullopt;
}

Result<uint8_t, PercentError> decode_at(std::string_view s, size_t i) {
    if (i + 2 >= s.size()) {
        return Result<uint8_t, PercentError>::err(PercentError{i});
    }
    auto hi = hex_value(static_cast<uint8_t>(s[i + 1]));
    auto lo = hex_value(static_cast<uint8_t>(s[i + 2]));
    if (hi.has_value() && lo.has_value()) {
        return Result<uint8_t, PercentError>::ok(static_cast<uint8_t>((*hi << 4) | *lo));
    }
    return Result<uint8_t, PercentError>::err(PercentError{i});
}

} // namespace

Result<void, PercentError> validate(std::string_view s) {
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '%') {
            auto res = decode_at(s, i);
            if (res.is_err()) {
                return Result<void, PercentError>::err(res.error());
            }
            i += 3;
        } else {
            i += 1;
        }
    }
    return Result<void, PercentError>::ok();
}

Result<std::vector<uint8_t>, PercentError> percent_decode(std::string_view s) {
    std::vector<uint8_t> out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '%') {
            auto res = decode_at(s, i);
            if (res.is_err()) {
                return Result<std::vector<uint8_t>, PercentError>::err(res.error());
            }
            out.push_back(res.value());
            i += 3;
        } else {
            out.push_back(static_cast<uint8_t>(s[i]));
            i += 1;
        }
    }
    return Result<std::vector<uint8_t>, PercentError>::ok(std::move(out));
}

} // namespace farloom::http::percent
