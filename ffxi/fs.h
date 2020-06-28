#pragma once

#include <filesystem>

namespace fs {
    static inline std::string path2str(const std::filesystem::path &path) {
        return path.string();
    }
}
