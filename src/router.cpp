#include "router.hpp"

namespace farloom {

Router& Router::get(std::string path, Handler h) {
    return add(http::Method(http::MethodKind::Get), std::move(path), std::move(h));
}

Router& Router::post(std::string path, Handler h) {
    return add(http::Method(http::MethodKind::Post), std::move(path), std::move(h));
}

Router& Router::add(http::Method method, std::string path, Handler h) {
    routes_[std::move(path)][std::move(method)] = std::move(h);
    return *this;
}

RouteMatch Router::lookup(const http::Method& method, std::string_view path) const {
    auto it = routes_.find(std::string(path));
    if (it == routes_.end()) {
        return RouteMatch{RouteMatchKind::NotFound, nullptr, {}};
    }
    const auto& methods = it->second;
    auto m_it = methods.find(method);
    if (m_it != methods.end()) {
        return RouteMatch{RouteMatchKind::Found, &m_it->second, {}};
    }

    std::vector<http::Method> allow;
    allow.reserve(methods.size());
    for (const auto& [m, _] : methods) {
        allow.push_back(m);
    }
    return RouteMatch{RouteMatchKind::MethodNotAllowed, nullptr, std::move(allow)};
}

} // namespace farloom
