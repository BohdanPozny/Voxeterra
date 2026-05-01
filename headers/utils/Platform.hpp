#pragma once
#include <filesystem>

namespace Platform {
    // Returns the directory containing the running executable.
    // Lets us run the binary from any cwd while still
    // resolving "shaders/", "fonts/" and "config.json" relative to it.
    std::filesystem::path getExecutableDir();
}
