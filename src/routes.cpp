#include "routes.hpp"
#include "router.hpp"

namespace farloom::routes {

http::Response health(const http::Request& /*req*/) {
    std::string body = R"({"status":"ok"})";
    std::vector<uint8_t> body_bytes(body.begin(), body.end());
    return http::Response::new_resp(http::StatusCode::Ok)
        .with_body("application/json", std::move(body_bytes));
}

http::Response echo(const http::Request& req) {
    auto ct = req.headers.get("content-type");
    std::string_view content_type = ct.has_value() ? *ct : "application/octet-stream";
    return http::Response::new_resp(http::StatusCode::Ok)
        .with_body(content_type, req.body);
}

} // namespace farloom::routes

namespace farloom {

Router Router::with_builtins() {
    Router r;
    r.get("/health", routes::health);
    r.post("/api/echo", routes::echo);
    return r;
}

} // namespace farloom
