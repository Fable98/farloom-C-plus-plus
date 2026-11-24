#pragma once

#include <filesystem>
#include <string_view>

namespace farloom::mime {

std::string_view mime_for_path(const std::filesystem::path& path);

} // namespace farloom::mime
