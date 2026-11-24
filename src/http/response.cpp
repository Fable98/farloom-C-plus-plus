#include "http/response.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <sys/socket.h>
#include <unistd.h>

namespace farloom::http {

namespace {

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

uint16_t status_code(StatusCode sc) {
    return static_cast<uint16_t>(sc);
}

const char* status_reason(StatusCode sc) {
    switch (sc) {
        case StatusCode::Ok:
            return "OK";
        case StatusCode::BadRequest:
            return "Bad Request";
        case StatusCode::NotFound:
            return "Not Found";
        case StatusCode::MethodNotAllowed:
            return "Method Not Allowed";
        case StatusCode::PayloadTooLarge:
            return "Payload Too Large";
        case StatusCode::InternalServerError:
            return "Internal Server Error";
        case StatusCode::ServiceUnavailable:
            return "Service Unavailable";
    }
    return "";
}

Response Response::new_resp(StatusCode status) {
    Response r;
    r.status = status;
    return r;
}

Response& Response::with_header(std::string name, std::string value) {
    headers.emplace_back(std::move(name), std::move(value));
    return *this;
}

Response& Response::with_body(std::string_view content_type, std::vector<uint8_t> body_bytes) {
    auto it = std::remove_if(headers.begin(), headers.end(), [](const auto& h) {
        return equals_ignore_case(h.first, "content-type");
    });
    headers.erase(it, headers.end());
    headers.emplace_back("Content-Type", std::string(content_type));
    body = std::move(body_bytes);
    return *this;
}

Response Response::error(StatusCode status) {
    std::string text = std::format("{} {}\n", status_code(status), status_reason(status));
    std::vector<uint8_t> b(text.begin(), text.end());
    Response r = new_resp(status);
    r.with_body("text/plain; charset=utf-8", std::move(b));
    return r;
}

Response Response::method_not_allowed(const std::vector<Method>& allow) {
    std::vector<Method> sorted_methods = allow;
    std::sort(sorted_methods.begin(), sorted_methods.end());
    sorted_methods.erase(std::unique(sorted_methods.begin(), sorted_methods.end()), sorted_methods.end());

    std::string allow_str;
    for (size_t i = 0; i < sorted_methods.size(); ++i) {
        if (i > 0) allow_str.append(", ");
        allow_str.append(sorted_methods[i].as_str());
    }

    Response r = error(StatusCode::MethodNotAllowed);
    r.with_header("Allow", std::move(allow_str));
    return r;
}

bool Response::has_header(std::string_view name) const {
    for (const auto& [n, _] : headers) {
        if (equals_ignore_case(n, name)) {
            return true;
        }
    }
    return false;
}

std::vector<uint8_t> Response::to_bytes() const {
    std::string head = std::format("HTTP/1.1 {} {}\r\n", status_code(status), status_reason(status));

    for (const auto& [name, value] : headers) {
        if (equals_ignore_case(name, "content-length") || equals_ignore_case(name, "connection")) {
            continue;
        }
        head.append(name);
        head.append(": ");
        head.append(value);
        head.append("\r\n");
    }

    if (!body.empty() && !has_header("content-type")) {
        head.append("Content-Type: application/octet-stream\r\n");
    }

    head.append(std::format("Content-Length: {}\r\n", body.size()));
    head.append("Connection: close\r\n\r\n");

    std::vector<uint8_t> out(head.begin(), head.end());
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

bool Response::write_response(int fd) const {
    auto bytes = to_bytes();
    size_t total = 0;
    while (total < bytes.size()) {
        ssize_t n = ::send(fd, bytes.data() + total, bytes.size() - total, 0);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR)) {
                continue;
            }
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

} // namespace farloom::http
