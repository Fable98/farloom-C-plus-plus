#include "static_files.hpp"
#include "http/percent.hpp"
#include "mime.hpp"
#include "logger.hpp"

#include <cerrno>
#include <fstream>
#include <format>
#include <span>

namespace farloom {

namespace {

bool is_valid_utf8(std::span<const uint8_t> bytes) {
    size_t i = 0;
    while (i < bytes.size()) {
        uint8_t b = bytes[i];
        if (b <= 0x7F) {
            i++;
        } else if ((b & 0xE0) == 0xC0) {
            if (i + 1 >= bytes.size() || (bytes[i + 1] & 0xC0) != 0x80) return false;
            if (b < 0xC2) return false;
            i += 2;
        } else if ((b & 0xF0) == 0xE0) {
            if (i + 2 >= bytes.size() || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80) return false;
            if (b == 0xE0 && bytes[i + 1] < 0xA0) return false;
            if (b == 0xED && bytes[i + 1] >= 0xA0) return false;
            i += 3;
        } else if ((b & 0xF8) == 0xF0) {
            if (i + 3 >= bytes.size() || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 || (bytes[i + 3] & 0xC0) != 0x80) return false;
            if (b == 0xF0 && bytes[i + 1] < 0x90) return false;
            if (b == 0xF4 && bytes[i + 1] > 0x8F) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

bool is_not_found(const std::error_code& ec) {
    if (ec == std::errc::no_such_file_or_directory ||
        ec == std::errc::not_a_directory ||
        ec == std::errc::filename_too_long ||
        ec == std::errc::invalid_argument) {
        return true;
    }
    int v = ec.value();
    if (v == ENOENT || v == ENOTDIR || v == EINVAL || v == 36 || v == 63) {
        return true;
    }
#if defined(_WIN32)
    if (v == 2 || v == 3 || v == 123 || v == 161 || v == 267) {
        return true;
    }
#endif
    return false;
}

} // namespace

StaticFiles::StaticFiles(std::filesystem::path canonical_root)
    : root_(std::move(canonical_root)) {}

Result<std::filesystem::path, ResolveError> StaticFiles::resolve(std::string_view raw_path) const {
    // 1. Decode
    auto decode_res = http::percent::percent_decode(raw_path);
    if (decode_res.is_err()) {
        return Result<std::filesystem::path, ResolveError>::err(
            ResolveError{ResolveErrorKind::Traversal, "bad encoding", ""}
        );
    }
    const auto& bytes = decode_res.value();

    // 2. UTF-8
    if (!is_valid_utf8(bytes)) {
        return Result<std::filesystem::path, ResolveError>::err(
            ResolveError{ResolveErrorKind::NotFound, "", ""}
        );
    }
    std::string decoded(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    // 3. Whole-path checks
    if (decoded.find('\0') != std::string::npos) {
        return Result<std::filesystem::path, ResolveError>::err(
            ResolveError{ResolveErrorKind::Traversal, "nul", ""}
        );
    }
    if (decoded.find('\\') != std::string::npos) {
        return Result<std::filesystem::path, ResolveError>::err(
            ResolveError{ResolveErrorKind::Traversal, "backslash", ""}
        );
    }
    if (decoded.starts_with("//")) {
        return Result<std::filesystem::path, ResolveError>::err(
            ResolveError{ResolveErrorKind::Traversal, "leading //", ""}
        );
    }

    // 4. Segment checks
    std::filesystem::path candidate = root_;
    size_t start = 0;
    while (start < decoded.size()) {
        size_t next = decoded.find('/', start);
        std::string_view segment = (next == std::string::npos)
            ? std::string_view(decoded).substr(start)
            : std::string_view(decoded).substr(start, next - start);

        if (segment.empty() || segment == ".") {
            // continue
        } else if (segment == "..") {
            return Result<std::filesystem::path, ResolveError>::err(
                ResolveError{ResolveErrorKind::Traversal, "dotdot", ""}
            );
        } else if (segment.find(':') != std::string_view::npos) {
            return Result<std::filesystem::path, ResolveError>::err(
                ResolveError{ResolveErrorKind::Traversal, "colon", ""}
            );
        } else {
            candidate /= segment;
        }

        if (next == std::string_view::npos) break;
        start = next + 1;
    }

    // 5. Directory index
    std::error_code ec_status;
    if (std::filesystem::is_directory(candidate, ec_status)) {
        candidate /= "index.html";
    }

    // 6. Canonicalize
    std::error_code ec;
    auto canonical = std::filesystem::canonical(candidate, ec);
    if (ec) {
        if (is_not_found(ec)) {
            return Result<std::filesystem::path, ResolveError>::err(
                ResolveError{ResolveErrorKind::NotFound, "", ec.message()}
            );
        } else {
            return Result<std::filesystem::path, ResolveError>::err(
                ResolveError{ResolveErrorKind::Io, "", ec.message()}
            );
        }
    }

    // 7. Containment check (component-wise)
    auto r_it = root_.begin();
    auto c_it = canonical.begin();
    for (; r_it != root_.end() && c_it != canonical.end(); ++r_it, ++c_it) {
        if (*r_it != *c_it) {
            return Result<std::filesystem::path, ResolveError>::err(
                ResolveError{ResolveErrorKind::Traversal, "outside root", ""}
            );
        }
    }
    if (r_it != root_.end()) {
        return Result<std::filesystem::path, ResolveError>::err(
            ResolveError{ResolveErrorKind::Traversal, "outside root", ""}
        );
    }

    // 8. Regular file
    if (!std::filesystem::is_regular_file(canonical, ec)) {
        return Result<std::filesystem::path, ResolveError>::err(
            ResolveError{ResolveErrorKind::NotFound, "", ""}
        );
    }

    return Result<std::filesystem::path, ResolveError>::ok(std::move(canonical));
}

http::Response StaticFiles::serve(const http::Request& req) const {
    return serve_from(req, std::nullopt);
}

http::Response StaticFiles::serve_from(const http::Request& req, std::optional<std::string_view> client) const {
    if (req.method != http::Method(http::MethodKind::Get)) {
        return http::Response::method_not_allowed({http::Method(http::MethodKind::Get)});
    }

    auto resolved = resolve(req.path);
    if (resolved.is_err()) {
        const auto& err = resolved.error();
        if (err.kind == ResolveErrorKind::Traversal) {
            if (client.has_value()) {
                logger::path_traversal(*client, req.path, err.rule);
            } else {
                logger::warn(
                    "path_traversal",
                    std::format(
                        "path={} rule={}",
                        logger::escape_value(req.path),
                        logger::escape_value(err.rule)
                    )
                );
            }
            return http::Response::error(http::StatusCode::NotFound);
        } else if (err.kind == ResolveErrorKind::NotFound) {
            return http::Response::error(http::StatusCode::NotFound);
        } else {
            logger::error("io", std::format("resolve {}: {}", req.path, err.message));
            return http::Response::error(http::StatusCode::InternalServerError);
        }
    }

    const auto& path = resolved.value();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return http::Response::error(http::StatusCode::NotFound);
        }
        logger::error("io", std::format("read {}: failed to open file", logger::display_path(path).string()));
        return http::Response::error(http::StatusCode::InternalServerError);
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (size > 0 && !file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        logger::error("io", std::format("read {}: read error", logger::display_path(path).string()));
        return http::Response::error(http::StatusCode::InternalServerError);
    }

    return http::Response::new_resp(http::StatusCode::Ok)
        .with_body(mime::mime_for_path(path), std::move(buffer));
}

} // namespace farloom
