#include "config.hpp"

#include <charconv>
#include <format>

namespace farloom {

namespace {

constexpr std::string_view HOST = "--host";
constexpr std::string_view PORT = "--port";
constexpr std::string_view WORKERS = "--workers";
constexpr std::string_view QUEUE_CAPACITY = "--queue-capacity";
constexpr std::string_view STATIC_ROOT = "--static-root";
constexpr std::string_view READ_TIMEOUT = "--read-timeout-secs";
constexpr std::string_view WRITE_TIMEOUT = "--write-timeout-secs";
constexpr std::string_view MAX_BODY = "--max-body-bytes";
constexpr std::string_view MAX_REQUESTS = "--max-requests";

const std::string_view OPTIONS[] = {
    HOST, PORT, WORKERS, QUEUE_CAPACITY, STATIC_ROOT,
    READ_TIMEOUT, WRITE_TIMEOUT, MAX_BODY, MAX_REQUESTS
};

template <typename T>
Result<T, ConfigError> parse_num(std::string_view option, std::string_view value, std::string_view expected) {
    T val = 0;
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), val);
    if (ec != std::errc() || ptr != value.data() + value.size()) {
        std::string msg = std::format("invalid value '{}' for {}: expected {}", value, option, expected);
        return Result<T, ConfigError>::err(
            ConfigError{ConfigErrorKind::InvalidValue, std::string(option), std::string(value), std::string(expected), msg}
        );
    }
    return Result<T, ConfigError>::ok(val);
}

} // namespace

Result<std::filesystem::path, std::string> Config::validate_static_root() const {
    std::error_code ec;
    auto canonical = std::filesystem::canonical(static_root, ec);
    if (ec) {
        return Result<std::filesystem::path, std::string>::err(
            std::format("invalid static root '{}': {}", static_root.string(), ec.message())
        );
    }
    if (!std::filesystem::is_directory(canonical, ec)) {
        return Result<std::filesystem::path, std::string>::err(
            std::format("{} is not a directory", static_root.string())
        );
    }
    return Result<std::filesystem::path, std::string>::ok(canonical);
}

Result<Config, ConfigError> parse_config(std::span<const std::string> args) {
    Config cfg = Config::default_config();
    size_t i = 0;

    while (i < args.size()) {
        const std::string& arg = args[i++];
        if (arg == "--help" || arg == "-h") {
            return Result<Config, ConfigError>::err(
                ConfigError{ConfigErrorKind::HelpRequested, "", "", "", "help requested"}
            );
        }

        std::string_view name = arg;
        std::optional<std::string_view> inline_val;
        size_t eq_pos = arg.find('=');
        if (eq_pos != std::string::npos && arg.starts_with("--")) {
            name = std::string_view(arg).substr(0, eq_pos);
            inline_val = std::string_view(arg).substr(eq_pos + 1);
        }

        bool found = false;
        for (auto opt : OPTIONS) {
            if (opt == name) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::string msg = std::format("unknown option '{}'", arg);
            return Result<Config, ConfigError>::err(
                ConfigError{ConfigErrorKind::UnknownOption, arg, "", "", msg}
            );
        }

        std::string_view value;
        if (inline_val.has_value()) {
            value = *inline_val;
        } else {
            if (i < args.size()) {
                const auto& next = args[i];
                if (next.starts_with("--") && next.size() > 2) {
                    std::string msg = std::format("missing value for {}", name);
                    return Result<Config, ConfigError>::err(
                        ConfigError{ConfigErrorKind::MissingValue, std::string(name), "", "", msg}
                    );
                }
                value = args[i++];
            } else {
                std::string msg = std::format("missing value for {}", name);
                return Result<Config, ConfigError>::err(
                    ConfigError{ConfigErrorKind::MissingValue, std::string(name), "", "", msg}
                );
            }
        }

        if (name == HOST) {
            cfg.host = std::string(value);
        } else if (name == PORT) {
            auto r = parse_num<uint16_t>(name, value, "integer 0-65535");
            if (r.is_err()) return Result<Config, ConfigError>::err(r.error());
            cfg.port = r.value();
        } else if (name == WORKERS) {
            auto r = parse_num<size_t>(name, value, "positive integer");
            if (r.is_err()) return Result<Config, ConfigError>::err(r.error());
            cfg.workers = r.value();
        } else if (name == QUEUE_CAPACITY) {
            auto r = parse_num<size_t>(name, value, "positive integer");
            if (r.is_err()) return Result<Config, ConfigError>::err(r.error());
            cfg.queue_capacity = r.value();
        } else if (name == STATIC_ROOT) {
            cfg.static_root = std::filesystem::path(value);
        } else if (name == READ_TIMEOUT) {
            auto r = parse_num<uint64_t>(name, value, "positive integer number of seconds");
            if (r.is_err()) return Result<Config, ConfigError>::err(r.error());
            cfg.read_timeout = std::chrono::seconds(r.value());
        } else if (name == WRITE_TIMEOUT) {
            auto r = parse_num<uint64_t>(name, value, "positive integer number of seconds");
            if (r.is_err()) return Result<Config, ConfigError>::err(r.error());
            cfg.write_timeout = std::chrono::seconds(r.value());
        } else if (name == MAX_BODY) {
            auto r = parse_num<uint64_t>(name, value, "non-negative integer");
            if (r.is_err()) return Result<Config, ConfigError>::err(r.error());
            cfg.max_body_bytes = r.value();
        } else if (name == MAX_REQUESTS) {
            auto r = parse_num<uint64_t>(name, value, "positive integer");
            if (r.is_err()) return Result<Config, ConfigError>::err(r.error());
            cfg.max_requests = r.value();
        }
    }

    if (cfg.workers == 0) {
        return Result<Config, ConfigError>::err(
            ConfigError{ConfigErrorKind::MustBePositive, std::string(WORKERS), "", "", std::format("{} must be greater than 0", WORKERS)}
        );
    }
    if (cfg.queue_capacity == 0) {
        return Result<Config, ConfigError>::err(
            ConfigError{ConfigErrorKind::MustBePositive, std::string(QUEUE_CAPACITY), "", "", std::format("{} must be greater than 0", QUEUE_CAPACITY)}
        );
    }
    if (cfg.read_timeout.count() == 0) {
        return Result<Config, ConfigError>::err(
            ConfigError{ConfigErrorKind::MustBePositive, std::string(READ_TIMEOUT), "", "", std::format("{} must be greater than 0", READ_TIMEOUT)}
        );
    }
    if (cfg.write_timeout.count() == 0) {
        return Result<Config, ConfigError>::err(
            ConfigError{ConfigErrorKind::MustBePositive, std::string(WRITE_TIMEOUT), "", "", std::format("{} must be greater than 0", WRITE_TIMEOUT)}
        );
    }
    if (cfg.max_requests.has_value() && *cfg.max_requests == 0) {
        return Result<Config, ConfigError>::err(
            ConfigError{ConfigErrorKind::MustBePositive, std::string(MAX_REQUESTS), "", "", std::format("{} must be greater than 0", MAX_REQUESTS)}
        );
    }

    return Result<Config, ConfigError>::ok(cfg);
}

Result<Config, ConfigError> parse_config(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc > 1 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return parse_config(args);
}

} // namespace farloom
