#pragma once

#include "result.hpp"
#include <string_view>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace farloom::http::percent {

struct PercentError {
    size_t index{0};
    bool operator==(const PercentError& other) const = default;
};

Result<void, PercentError> validate(std::string_view s);
Result<std::vector<uint8_t>, PercentError> percent_decode(std::string_view s);

} // namespace farloom::http::percent
