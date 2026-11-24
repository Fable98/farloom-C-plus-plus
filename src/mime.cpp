#include "mime.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace farloom::mime {

std::string_view mime_for_path(const std::filesystem::path& path) {
    if (!path.has_extension()) {
        return "application/octet-stream";
    }
    std::string ext = path.extension().string();
    if (ext.empty() || ext[0] != '.') {
        return "application/octet-stream";
    }
    ext = ext.substr(1);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "js") return "text/javascript; charset=utf-8";
    if (ext == "json") return "application/json";
    if (ext == "txt") return "text/plain; charset=utf-8";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";

    return "application/octet-stream";
}

} // namespace farloom::mime
