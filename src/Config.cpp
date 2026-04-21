#include "Config.hpp"
#include "utils/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

Config::Config(const std::string& configPath) 
    : m_configPath(configPath) {
}

bool Config::load() {
    std::ifstream file(m_configPath);
    if (!file.is_open()) {
        std::cout << "[Config] File not found, using defaults: " << m_configPath << std::endl;
        return false;
    }
    
    try {
        json j;
        file >> j;
        
        // Graphics
        if (j.contains("graphics")) {
            auto& graphics = j["graphics"];
            if (graphics.contains("windowWidth")) m_windowWidth = graphics["windowWidth"];
            if (graphics.contains("windowHeight")) m_windowHeight = graphics["windowHeight"];
            if (graphics.contains("fullscreen")) m_fullscreen = graphics["fullscreen"];
            if (graphics.contains("vsync")) m_vsync = graphics["vsync"];
            if (graphics.contains("renderDistance")) m_renderDistance = graphics["renderDistance"];
            if (graphics.contains("fov")) m_fov = graphics["fov"];
        }
        
        // Controls
        if (j.contains("controls")) {
            auto& controls = j["controls"];
            if (controls.contains("mouseSensitivity")) m_mouseSensitivity = controls["mouseSensitivity"];
            if (controls.contains("movementSpeed")) m_movementSpeed = controls["movementSpeed"];
        }
        
        // Audio
        if (j.contains("audio")) {
            auto& audio = j["audio"];
            if (audio.contains("masterVolume")) m_masterVolume = audio["masterVolume"];
            if (audio.contains("musicVolume")) m_musicVolume = audio["musicVolume"];
            if (audio.contains("sfxVolume")) m_sfxVolume = audio["sfxVolume"];
        }
        
        std::cout << "[Config] Loaded from: " << m_configPath << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[Config] Parse error: " << e.what() << std::endl;
        file.close();
        return false;
    }
}

bool Config::save() {
    try {
        json j;
        
        // Graphics
        j["graphics"]["windowWidth"] = m_windowWidth;
        j["graphics"]["windowHeight"] = m_windowHeight;
        j["graphics"]["fullscreen"] = m_fullscreen;
        j["graphics"]["vsync"] = m_vsync;
        j["graphics"]["renderDistance"] = m_renderDistance;
        j["graphics"]["fov"] = m_fov;
        
        // Controls
        j["controls"]["mouseSensitivity"] = m_mouseSensitivity;
        j["controls"]["movementSpeed"] = m_movementSpeed;
        
        // Audio
        j["audio"]["masterVolume"] = m_masterVolume;
        j["audio"]["musicVolume"] = m_musicVolume;
        j["audio"]["sfxVolume"] = m_sfxVolume;
        
        std::ofstream file(m_configPath);
        if (!file.is_open()) {
            std::cerr << "[Config] Failed to save: " << m_configPath << std::endl;
            return false;
        }
        
        file << j.dump(2);
        std::cout << "[Config] Saved to: " << m_configPath << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[Config] Save error: " << e.what() << std::endl;
        return false;
    }
}
