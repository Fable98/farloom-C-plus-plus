#pragma once

#include "result.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace farloom::loadtest {

constexpr std::string_view USAGE =
R"(Usage: loadtest [OPTIONS]

Options:
  --addr <HOST:PORT>       Server address            (default 127.0.0.1:7878)
  --path <PATH>            Request path              (default /)
  --requests <N>           Total requests, >= 1      (default 1000)
  --concurrency <N>        Client threads, >= 1      (default 8)
  -h, --help               Print this help
)";

struct LoadArgs {
    std::string addr{"127.0.0.1:7878"};
    std::string path{"/"};
    uint64_t requests{1000};
    size_t concurrency{8};

    static LoadArgs default_args() { return LoadArgs(); }

    bool operator==(const LoadArgs& other) const = default;
};

enum class ArgsErrorKind {
    UnknownOption,
    MissingValue,
    InvalidValue,
    MustBePositive,
    HelpRequested,
};

struct ArgsError {
    ArgsErrorKind kind;
    std::string option;
    std::string value;
    std::string expected;
    std::string message;

    bool operator==(const ArgsError& other) const = default;
};

Result<LoadArgs, ArgsError> parse_args(std::span<const std::string> args);
Result<LoadArgs, ArgsError> parse_args(int argc, char* argv[]);

} // namespace farloom::loadtest
