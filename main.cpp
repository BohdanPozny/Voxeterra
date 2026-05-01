#include "headers/Engine.hpp"
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <system_error>

#include "utils/Platform.hpp"

int main() {
    if (auto dir = Platform::getExecutableDir(); !dir.empty()) {
        std::error_code ec;
        std::filesystem::current_path(dir, ec);
    }

    Engine engine;
    
    if (!engine.init()) {
        std::cerr << "[Main] Failed to initialize engine" << std::endl;
        return EXIT_FAILURE;
    }

    engine.run();

    return EXIT_SUCCESS;
}
