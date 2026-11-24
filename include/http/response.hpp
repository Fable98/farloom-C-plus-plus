#pragma once

#include "http/request.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace farloom::http {

enum class StatusCode : uint16_t {
    Ok = 200,
    BadRequest = 400,
    NotFound = 404,
    MethodNotAllowed = 405,
    PayloadTooLarge = 413,
    InternalServerError = 500,
    ServiceUnavailable = 503,
};

uint16_t status_code(StatusCode sc);
const char* status_reason(StatusCode sc);

struct Response {
    StatusCode status{StatusCode::Ok};
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<uint8_t> body;

    static Response new_resp(StatusCode status = StatusCode::Ok);
    Response& with_header(std::string name, std::string value);
    Response& with_body(std::string_view content_type, std::vector<uint8_t> body_bytes);
    static Response error(StatusCode status);
    static Response method_not_allowed(const std::vector<Method>& allow);

    bool has_header(std::string_view name) const;
    std::vector<uint8_t> to_bytes() const;
    bool write_response(int fd) const;

    bool operator==(const Response& other) const = default;
};

} // namespace farloom::http
