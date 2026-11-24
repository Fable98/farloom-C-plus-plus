#include "args.hpp"

#include <charconv>
#include <format>
#include <vector>

namespace farloom::loadtest {

namespace {

constexpr std::string_view ADDR = "--addr";
constexpr std::string_view PATH = "--path";
constexpr std::string_view REQUESTS = "--requests";
constexpr std::string_view CONCURRENCY = "--concurrency";

const std::string_view OPTIONS[] = {ADDR, PATH, REQUESTS, CONCURRENCY};

template <typename T>
Result<T, ArgsError> parse_num(std::string_view option, std::string_view value, std::string_view expected) {
    T val = 0;
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), val);
    if (ec != std::errc() || ptr != value.data() + value.size()) {
        std::string msg = std::format("invalid value '{}' for {}: expected {}", value, option, expected);
        return Result<T, ArgsError>::err(
            ArgsError{ArgsErrorKind::InvalidValue, std::string(option), std::string(value), std::string(expected), msg}
        );
    }
    return Result<T, ArgsError>::ok(val);
}

} // namespace

Result<LoadArgs, ArgsError> parse_args(std::span<const std::string> args) {
    LoadArgs out = LoadArgs::default_args();
    size_t i = 0;

    while (i < args.size()) {
        const std::string& arg = args[i++];
        if (arg == "--help" || arg == "-h") {
            return Result<LoadArgs, ArgsError>::err(
                ArgsError{ArgsErrorKind::HelpRequested, "", "", "", "help requested"}
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
            return Result<LoadArgs, ArgsError>::err(
                ArgsError{ArgsErrorKind::UnknownOption, arg, "", "", msg}
            );
        }

        std::string_view value;
        if (inline_val.has_value()) {
            value = *inline_val;
        } else {
            if (i < args.size()) {
                value = args[i++];
            } else {
                std::string msg = std::format("missing value for {}", name);
                return Result<LoadArgs, ArgsError>::err(
                    ArgsError{ArgsErrorKind::MissingValue, std::string(name), "", "", msg}
                );
            }
        }

        if (name == ADDR) {
            out.addr = std::string(value);
        } else if (name == PATH) {
            if (!value.starts_with('/')) {
                std::string msg = std::format("invalid value '{}' for {}: expected path starting with '/' without spaces or control characters", value, name);
                return Result<LoadArgs, ArgsError>::err(
                    ArgsError{ArgsErrorKind::InvalidValue, std::string(name), std::string(value), "path starting with '/' without spaces or control characters", msg}
                );
            }
            for (char c : value) {
                unsigned char uc = static_cast<unsigned char>(c);
                if (uc <= 0x20 || uc == 0x7F) {
                    std::string msg = std::format("invalid value '{}' for {}: expected path starting with '/' without spaces or control characters", value, name);
                    return Result<LoadArgs, ArgsError>::err(
                        ArgsError{ArgsErrorKind::InvalidValue, std::string(name), std::string(value), "path starting with '/' without spaces or control characters", msg}
                    );
                }
            }
            out.path = std::string(value);
        } else if (name == REQUESTS) {
            auto r = parse_num<uint64_t>(name, value, "positive integer");
            if (r.is_err()) return Result<LoadArgs, ArgsError>::err(r.error());
            out.requests = r.value();
        } else if (name == CONCURRENCY) {
            auto r = parse_num<size_t>(name, value, "positive integer");
            if (r.is_err()) return Result<LoadArgs, ArgsError>::err(r.error());
            out.concurrency = r.value();
        }
    }

    if (out.requests == 0) {
        return Result<LoadArgs, ArgsError>::err(
            ArgsError{ArgsErrorKind::MustBePositive, std::string(REQUESTS), "", "", std::format("{} must be at least 1", REQUESTS)}
        );
    }
    if (out.concurrency == 0) {
        return Result<LoadArgs, ArgsError>::err(
            ArgsError{ArgsErrorKind::MustBePositive, std::string(CONCURRENCY), "", "", std::format("{} must be at least 1", CONCURRENCY)}
        );
    }

    return Result<LoadArgs, ArgsError>::ok(out);
}

Result<LoadArgs, ArgsError> parse_args(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc > 1 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return parse_args(args);
}

} // namespace farloom::loadtest
