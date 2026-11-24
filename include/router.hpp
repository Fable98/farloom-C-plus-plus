#pragma once

#include "http/request.hpp"
#include "http/response.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace farloom {

using Handler = std::function<http::Response(const http::Request&)>;

enum class RouteMatchKind : uint8_t {
    Found,
    MethodNotAllowed,
    NotFound,
};

struct RouteMatch {
    RouteMatchKind kind{RouteMatchKind::NotFound};
    const Handler* handler{nullptr};
    std::vector<http::Method> allow{};
};

class Router {
public:
    Router() = default;

    Router& get(std::string path, Handler h);
    Router& post(std::string path, Handler h);
    Router& add(http::Method method, std::string path, Handler h);

    RouteMatch lookup(const http::Method& method, std::string_view path) const;

    static Router with_builtins();

private:
    std::unordered_map<std::string, std::map<http::Method, Handler>> routes_;
};

} // namespace farloom
