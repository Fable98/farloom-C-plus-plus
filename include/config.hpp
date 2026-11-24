#pragma once

#include "result.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace farloom {

constexpr std::string_view USAGE =
R"(Usage: webserver [OPTIONS]

Options (both `--opt value` and `--opt=value` are accepted):
  --host <HOST>                 Bind address             [default: 127.0.0.1]
  --port <PORT>                 Port, 0 = ephemeral      [default: 7878]
  --workers <N>                 Worker threads, >= 1     [default: 4]
  --queue-capacity <N>          Job queue capacity, >= 1 [default: 64]
  --static-root <DIR>           Static file directory    [default: ./public]
  --read-timeout-secs <SECS>    Socket read timeout, >= 1  [default: 5]
  --write-timeout-secs <SECS>   Socket write timeout, >= 1 [default: 5]
  --max-body-bytes <BYTES>      Maximum request body     [default: 1048576]
  --max-requests <N>            Shut down after N accepted connections, >= 1
  -h, --help                    Print this help
)";

struct Config {
    std::string host{"127.0.0.1"};
    uint16_t port{7878};
    size_t workers{4};
    size_t queue_capacity{64};
    std::filesystem::path static_root{"./public"};
    std::chrono::seconds read_timeout{5};
    std::chrono::seconds write_timeout{5};
    uint64_t max_body_bytes{1048576};
    std::optional<uint64_t> max_requests{std::nullopt};

    static Config default_config() { return Config(); }

    Result<std::filesystem::path, std::string> validate_static_root() const;

    bool operator==(const Config& other) const = default;
};

enum class ConfigErrorKind {
    UnknownOption,
    MissingValue,
    InvalidValue,
    MustBePositive,
    HelpRequested,
};

struct ConfigError {
    ConfigErrorKind kind;
    std::string option;
    std::string value;
    std::string expected;
    std::string message;

    bool operator==(const ConfigError& other) const = default;
};

Result<Config, ConfigError> parse_config(std::span<const std::string> args);
Result<Config, ConfigError> parse_config(int argc, char* argv[]);

} // namespace farloom
