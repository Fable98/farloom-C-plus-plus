#include "logger.hpp"

#include <iostream>
#include <chrono>
#include <mutex>
#include <format>
#include <pthread.h>

namespace farloom::logger {

namespace {
std::mutex g_stdout_mutex;
std::mutex g_stderr_mutex;

thread_local std::optional<std::string> t_label = std::nullopt;

std::string quote(std::string_view msg) {
    return std::format("\"{}\"", escape_value(msg));
}

void write_stdout(std::string_view line) {
    std::lock_guard<std::mutex> lock(g_stdout_mutex);
    std::cout.write(line.data(), static_cast<std::streamsize>(line.size()));
    std::cout.flush();
}

void write_stderr(std::string_view line) {
    std::lock_guard<std::mutex> lock(g_stderr_mutex);
    std::cerr.write(line.data(), static_cast<std::streamsize>(line.size()));
    std::cerr.flush();
}
} // namespace

uint64_t now_ms() {
    using namespace std::chrono;
    auto now = system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(duration_cast<milliseconds>(now).count());
}

void set_thread_label(std::optional<std::string> label) {
    t_label = std::move(label);
}

std::string current_worker() {
    if (t_label.has_value() && !t_label->empty()) {
        return *t_label;
    }
    char name[64] = {0};
    if (pthread_getname_np(pthread_self(), name, sizeof(name)) == 0 && name[0] != '\0') {
        return std::string(name);
    }
    return "acceptor";
}

std::string escape_value(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c == '\\') {
            out.append("\\\\");
        } else if (c == '"') {
            out.append("\\\"");
        } else if (c == '\r') {
            out.append("\\r");
        } else if (c == '\n') {
            out.append("\\n");
        } else if (c == '\t') {
            out.append("\\t");
        } else if (c < 0x20 || c == 0x7F) {
            out.append(std::format("\\u{{{:x}}}", static_cast<uint32_t>(c)));
        } else if (c == 0xE2 && i + 2 < value.size() &&
                   static_cast<unsigned char>(value[i + 1]) == 0x80 &&
                   (static_cast<unsigned char>(value[i + 2]) == 0xA8 || static_cast<unsigned char>(value[i + 2]) == 0xA9)) {
            // U+2028 or U+2029 line separator
            uint32_t cp = (static_cast<unsigned char>(value[i + 2]) == 0xA8) ? 0x2028 : 0x2029;
            out.append(std::format("\\u{{{:x}}}", cp));
            i += 2;
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

std::filesystem::path display_path(const std::filesystem::path& path) {
    std::string s = path.string();
    if (s.rfind(R"(\\?\UNC\)", 0) == 0) {
        return std::filesystem::path(R"(\\)" + s.substr(8));
    }
    if (s.rfind(R"(\\?\)", 0) == 0) {
        return std::filesystem::path(s.substr(4));
    }
    return path;
}

std::string format_access(const AccessRecord& r) {
    return std::format(
        "ts={} level=INFO event=access worker={} client={} method={} path={} status={} dur_ms={}\n",
        r.ts_ms,
        escape_value(r.worker),
        r.client,
        escape_value(r.method),
        escape_value(r.path),
        r.status,
        r.dur_ms
    );
}

std::string format_event(std::string_view level, std::string_view event, std::string_view fields) {
    std::string line = std::format("ts={} level={} event={}", now_ms(), level, event);
    if (!fields.empty()) {
        line.push_back(' ');
        std::string escaped_fields;
        for (char c : fields) {
            if (c == '\r') {
                escaped_fields.append("\\r");
            } else if (c == '\n') {
                escaped_fields.append("\\n");
            } else {
                escaped_fields.push_back(c);
            }
        }
        line.append(escaped_fields);
    }
    line.push_back('\n');
    return line;
}

std::string format_startup(
    std::string_view addr,
    size_t workers,
    size_t queue_capacity,
    const std::filesystem::path& static_root
) {
    return format_event(
        "INFO",
        "startup",
        std::format(
            "addr={} workers={} queue_capacity={} static_root={}",
            addr, workers, queue_capacity, static_root.string()
        )
    );
}

std::string format_rejected(std::string_view client) {
    return format_event("INFO", "rejected", std::format("client={} status=503", client));
}

std::string format_path_traversal(
    std::string_view worker,
    std::string_view client,
    std::string_view path,
    std::string_view rule
) {
    return format_event(
        "WARN",
        "path_traversal",
        std::format(
            "worker={} client={} path={} rule={}",
            escape_value(worker),
            client,
            escape_value(path),
            escape_value(rule)
        )
    );
}

std::string format_error(std::string_view worker, std::string_view event, std::string_view msg) {
    return format_event(
        "ERROR",
        event,
        std::format("worker={} msg={}", escape_value(worker), quote(msg))
    );
}

void startup(std::string_view addr, size_t workers, size_t queue_capacity, const std::filesystem::path& static_root) {
    write_stdout(format_startup(addr, workers, queue_capacity, static_root));
}

void access(const AccessRecord& r) {
    write_stdout(format_access(r));
}

void rejected(std::string_view client) {
    write_stdout(format_rejected(client));
}

void path_traversal(std::string_view client, std::string_view path, std::string_view rule) {
    write_stderr(format_path_traversal(current_worker(), client, path, rule));
}

void info_event(std::string_view event, std::string_view fields) {
    write_stdout(format_event("INFO", event, fields));
}

void warn(std::string_view event, std::string_view fields) {
    std::string all = std::format("worker={}", escape_value(current_worker()));
    if (!fields.empty()) {
        all.push_back(' ');
        all.append(fields);
    }
    write_stderr(format_event("WARN", event, all));
}

void error(std::string_view event, std::string_view msg) {
    write_stderr(format_error(current_worker(), event, msg));
}

} // namespace farloom::logger
