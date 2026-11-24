#pragma once

#include "result.hpp"
#include "reader.hpp"
#include "http/request.hpp"
#include <string>
#include <cstdint>
#include <cstddef>

namespace farloom::http {

struct Limits {
    static constexpr size_t DEFAULT_MAX_HEAD_BYTES = 8192;

    size_t max_head_bytes{DEFAULT_MAX_HEAD_BYTES};
    uint64_t max_body_bytes{1048576};

    static Limits with_max_body(uint64_t max_body) {
        Limits l;
        l.max_body_bytes = max_body;
        return l;
    }
};

enum class ParseErrorKind : uint8_t {
    BadRequest,
    PayloadTooLarge,
    Incomplete,
    Timeout,
    ConnectionClosed,
    Io,
};

struct ParseError {
    ParseErrorKind kind;
    std::string message;

    bool operator==(const ParseError& other) const = default;
};

Result<Request, ParseError> parse_request(farloom::io::BufferedReader& reader, const Limits& limits);

} // namespace farloom::http
