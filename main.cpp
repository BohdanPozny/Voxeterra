#include "headers/Engine.hpp"
#include <cstdlib>
#include <iostream>

int main() {
    Engine engine;
    
    if (!engine.init()) {
        std::cerr << "[Main] Failed to initialize engine" << std::endl;
        return EXIT_FAILURE;
    }

    engine.run();

    return EXIT_SUCCESS;
}
