#pragma once

#include "result.hpp"
#include "http/request.hpp"
#include "http/response.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace farloom {

enum class ResolveErrorKind : uint8_t {
    Traversal,
    NotFound,
    Io,
};

struct ResolveError {
    ResolveErrorKind kind{ResolveErrorKind::NotFound};
    std::string rule{};
    std::string message{};
};

class StaticFiles {
public:
    explicit StaticFiles(std::filesystem::path canonical_root);

    const std::filesystem::path& root() const noexcept { return root_; }

    Result<std::filesystem::path, ResolveError> resolve(std::string_view raw_path) const;
    http::Response serve(const http::Request& req) const;
    http::Response serve_from(const http::Request& req, std::optional<std::string_view> client) const;

private:
    std::filesystem::path root_;
};

} // namespace farloom
