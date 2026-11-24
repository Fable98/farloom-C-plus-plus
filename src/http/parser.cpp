#include "http/parser.hpp"
#include "http/percent.hpp"

#include <cctype>
#include <cerrno>
#include <string_view>
#include <vector>
#include <optional>
#include <charconv>

namespace farloom::http {

namespace {

bool is_tchar(uint8_t b) {
    if (std::isalnum(b)) return true;
    constexpr std::string_view specials = "!#$%&'*+-.^_`|~";
    return specials.find(static_cast<char>(b)) != std::string_view::npos;
}

bool is_token(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!is_tchar(static_cast<uint8_t>(c))) return false;
    }
    return true;
}

ParseError map_io_error(int err) {
    if (err == EAGAIN || err == EWOULDBLOCK || err == ETIMEDOUT) {
        return ParseError{ParseErrorKind::Timeout, "read timed out"};
    }
    return ParseError{ParseErrorKind::Io, "i/o error"};
}

Result<std::string, ParseError> read_line_limited(
    farloom::io::BufferedReader& reader,
    size_t& remaining,
    bool first
) {
    std::vector<uint8_t> buf;
    int err = 0;
    ssize_t n = reader.read_until('\n', buf, remaining + 1, err);
    if (n < 0) {
        return Result<std::string, ParseError>::err(map_io_error(err));
    }
    if (n == 0) {
        if (first) {
            return Result<std::string, ParseError>::err(
                ParseError{ParseErrorKind::ConnectionClosed, "connection closed before request"}
            );
        } else {
            return Result<std::string, ParseError>::err(
                ParseError{ParseErrorKind::Incomplete, "incomplete request"}
            );
        }
    }
    if (buf.size() > remaining) {
        return Result<std::string, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "header section too large"}
        );
    }
    if (buf.empty() || buf.back() != '\n') {
        return Result<std::string, ParseError>::err(
            ParseError{ParseErrorKind::Incomplete, "incomplete request"}
        );
    }
    if (buf.size() < 2 || buf[buf.size() - 2] != '\r') {
        return Result<std::string, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "line not terminated by CRLF"}
        );
    }
    remaining -= buf.size();
    buf.resize(buf.size() - 2); // remove \r\n
    return Result<std::string, ParseError>::ok(std::string(buf.begin(), buf.end()));
}

struct ParsedRequestLine {
    Method method;
    std::string path;
    std::optional<std::string> query;
    Version version;
};

Result<ParsedRequestLine, ParseError> parse_request_line(std::string_view line) {
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start < line.size()) {
        size_t next = line.find(' ', start);
        if (next == std::string_view::npos) {
            parts.push_back(line.substr(start));
            break;
        }
        parts.push_back(line.substr(start, next - start));
        start = next + 1;
    }
    if (parts.size() != 3) {
        return Result<ParsedRequestLine, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "request line must have three parts"}
        );
    }
    auto method_str = parts[0];
    auto target = parts[1];
    auto version_str = parts[2];

    if (!is_token(method_str)) {
        return Result<ParsedRequestLine, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "invalid method token"}
        );
    }
    if (!target.starts_with('/')) {
        return Result<ParsedRequestLine, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "request target must be origin-form"}
        );
    }
    for (char c : target) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc <= 0x20 || uc >= 0x7F || c == '#') {
            return Result<ParsedRequestLine, ParseError>::err(
                ParseError{ParseErrorKind::BadRequest, "invalid character in request target"}
            );
        }
    }
    Version version;
    if (version_str == "HTTP/1.1") {
        version = Version::Http11;
    } else if (version_str == "HTTP/1.0") {
        version = Version::Http10;
    } else {
        return Result<ParsedRequestLine, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "unsupported HTTP version"}
        );
    }

    std::string_view path_part = target;
    std::optional<std::string_view> query_part;
    size_t qmark = target.find('?');
    if (qmark != std::string_view::npos) {
        path_part = target.substr(0, qmark);
        query_part = target.substr(qmark + 1);
    }

    if (percent::validate(path_part).is_err()) {
        return Result<ParsedRequestLine, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "invalid percent-encoding"}
        );
    }
    if (query_part.has_value() && percent::validate(*query_part).is_err()) {
        return Result<ParsedRequestLine, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "invalid percent-encoding"}
        );
    }

    return Result<ParsedRequestLine, ParseError>::ok(ParsedRequestLine{
        Method::parse(method_str),
        std::string(path_part),
        query_part.has_value() ? std::make_optional(std::string(*query_part)) : std::nullopt,
        version
    });
}

Result<void, ParseError> parse_header_line(std::string_view line, Headers& headers) {
    if (line.starts_with(' ') || line.starts_with('\t')) {
        return Result<void, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "obsolete line folding"}
        );
    }
    size_t colon = line.find(':');
    if (colon == std::string_view::npos) {
        return Result<void, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "header line without colon"}
        );
    }
    std::string_view name = line.substr(0, colon);
    if (!is_token(name)) {
        return Result<void, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "invalid header name"}
        );
    }
    std::string_view value = line.substr(colon + 1);
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    for (char c : value) {
        unsigned char uc = static_cast<unsigned char>(c);
        if ((uc < 0x20 && uc != '\t') || uc == 0x7F) {
            return Result<void, ParseError>::err(
                ParseError{ParseErrorKind::BadRequest, "control character in header value"}
            );
        }
    }
    headers.insert(name, value);
    return Result<void, ParseError>::ok();
}

Result<std::optional<uint64_t>, ParseError> body_length(const Headers& headers, const Limits& limits) {
    if (headers.get("transfer-encoding").has_value()) {
        return Result<std::optional<uint64_t>, ParseError>::err(
            ParseError{ParseErrorKind::BadRequest, "Transfer-Encoding not supported"}
        );
    }
    std::optional<std::string> canonical;
    std::optional<std::string_view> raw;

    for (std::string_view v : headers.get_all("content-length")) {
        if (v.empty()) {
            return Result<std::optional<uint64_t>, ParseError>::err(
                ParseError{ParseErrorKind::BadRequest, "invalid Content-Length"}
            );
        }
        for (char c : v) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return Result<std::optional<uint64_t>, ParseError>::err(
                    ParseError{ParseErrorKind::BadRequest, "invalid Content-Length"}
                );
            }
        }
        std::string_view stripped = v;
        while (stripped.size() > 1 && stripped.front() == '0') {
            stripped.remove_prefix(1);
        }
        std::string stripped_str(stripped);
        if (canonical.has_value()) {
            if (*canonical != stripped_str) {
                return Result<std::optional<uint64_t>, ParseError>::err(
                    ParseError{ParseErrorKind::BadRequest, "conflicting Content-Length"}
                );
            }
        } else {
            canonical = stripped_str;
            raw = v;
        }
    }

    if (!raw.has_value()) {
        return Result<std::optional<uint64_t>, ParseError>::ok(std::nullopt);
    }

    uint64_t n = 0;
    auto [ptr, ec] = std::from_chars(raw->data(), raw->data() + raw->size(), n);
    if (ec != std::errc() || ptr != raw->data() + raw->size()) {
        return Result<std::optional<uint64_t>, ParseError>::err(
            ParseError{ParseErrorKind::PayloadTooLarge, "payload too large"}
        );
    }
    if (n > limits.max_body_bytes) {
        return Result<std::optional<uint64_t>, ParseError>::err(
            ParseError{ParseErrorKind::PayloadTooLarge, "payload too large"}
        );
    }
    return Result<std::optional<uint64_t>, ParseError>::ok(std::make_optional(n));
}

} // namespace

Result<Request, ParseError> parse_request(farloom::io::BufferedReader& reader, const Limits& limits) {
    size_t remaining = limits.max_head_bytes;

    auto line_res = read_line_limited(reader, remaining, true);
    if (line_res.is_err()) {
        return Result<Request, ParseError>::err(line_res.error());
    }

    auto rl_res = parse_request_line(line_res.value());
    if (rl_res.is_err()) {
        return Result<Request, ParseError>::err(rl_res.error());
    }

    Headers headers;
    while (true) {
        auto hdr_res = read_line_limited(reader, remaining, false);
        if (hdr_res.is_err()) {
            return Result<Request, ParseError>::err(hdr_res.error());
        }
        if (hdr_res.value().empty()) {
            break;
        }
        auto parse_hdr = parse_header_line(hdr_res.value(), headers);
        if (parse_hdr.is_err()) {
            return Result<Request, ParseError>::err(parse_hdr.error());
        }
    }

    auto len_res = body_length(headers, limits);
    if (len_res.is_err()) {
        return Result<Request, ParseError>::err(len_res.error());
    }

    std::vector<uint8_t> body;
    if (len_res.value().has_value() && *len_res.value() > 0) {
        uint64_t n = *len_res.value();
        body.resize(static_cast<size_t>(n));
        int err = 0;
        ssize_t read_bytes = reader.read_exact(body.data(), static_cast<size_t>(n), err);
        if (read_bytes < 0) {
            return Result<Request, ParseError>::err(map_io_error(err));
        }
        if (static_cast<uint64_t>(read_bytes) < n) {
            return Result<Request, ParseError>::err(
                ParseError{ParseErrorKind::Incomplete, "incomplete request"}
            );
        }
    }

    return Result<Request, ParseError>::ok(Request{
        rl_res.value().method,
        rl_res.value().path,
        rl_res.value().query,
        rl_res.value().version,
        std::move(headers),
        std::move(body)
    });
}

} // namespace farloom::http
