#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <filesystem>
#include <optional>

namespace farloom::logger {

struct AccessRecord {
    uint64_t ts_ms{0};
    std::string worker;
    std::string client;
    std::string method;
    std::string path;
    uint16_t status{0};
    uint64_t dur_ms{0};
};

uint64_t now_ms();

void set_thread_label(std::optional<std::string> label);
std::string current_worker();

std::string escape_value(std::string_view value);
std::filesystem::path display_path(const std::filesystem::path& path);

std::string format_access(const AccessRecord& r);
std::string format_event(std::string_view level, std::string_view event, std::string_view fields);
std::string format_startup(std::string_view addr, size_t workers, size_t queue_capacity, const std::filesystem::path& static_root);
std::string format_rejected(std::string_view client);
std::string format_path_traversal(std::string_view worker, std::string_view client, std::string_view path, std::string_view rule);
std::string format_error(std::string_view worker, std::string_view event, std::string_view msg);

void startup(std::string_view addr, size_t workers, size_t queue_capacity, const std::filesystem::path& static_root);
void access(const AccessRecord& r);
void rejected(std::string_view client);
void path_traversal(std::string_view client, std::string_view path, std::string_view rule);
void info_event(std::string_view event, std::string_view fields);
void warn(std::string_view event, std::string_view fields);
void error(std::string_view event, std::string_view msg);

} // namespace farloom::logger
