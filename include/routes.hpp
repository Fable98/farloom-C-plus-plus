#pragma once

#include "http/request.hpp"
#include "http/response.hpp"

namespace farloom::routes {

http::Response health(const http::Request& req);
http::Response echo(const http::Request& req);

} // namespace farloom::routes
