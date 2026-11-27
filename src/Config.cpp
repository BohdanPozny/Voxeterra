#include "Config.hpp"
#include "utils/json.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace {
constexpr const char* kKeyActionNames[Config::KEY_ACTION_COUNT] = {
    "forward", "back", "left", "right", "up", "down"};
}

Config::Config(const std::string& configPath) 
    : m_configPath(configPath)
    , m_keys{GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT} {
}

bool Config::load() {
    std::ifstream file(m_configPath);
    if (!file.is_open()) {
        std::cout << "[Config] File not found, writing defaults: " << m_configPath << std::endl;
        save();
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
            if (graphics.contains("retina")) m_retina = graphics["retina"];
            if (graphics.contains("renderDistance")) m_renderDistance = graphics["renderDistance"];
            if (graphics.contains("fpsLimit")) setFpsLimit(graphics["fpsLimit"]);
            if (graphics.contains("fov")) m_fov = graphics["fov"];
        }
        
        // Controls
        if (j.contains("controls")) {
            auto& controls = j["controls"];
            if (controls.contains("mouseSensitivity")) m_mouseSensitivity = controls["mouseSensitivity"];
            if (controls.contains("movementSpeed")) m_movementSpeed = controls["movementSpeed"];
            if (controls.contains("keys")) {
                auto& keys = controls["keys"];
                for (std::size_t i = 0; i < KEY_ACTION_COUNT; ++i) {
                    if (keys.contains(kKeyActionNames[i])) m_keys[i] = keys[kKeyActionNames[i]];
                }
            }
        }
        
        // Audio
        if (j.contains("audio")) {
            auto& audio = j["audio"];
            if (audio.contains("masterVolume")) m_masterVolume = audio["masterVolume"];
            if (audio.contains("musicVolume")) m_musicVolume = audio["musicVolume"];
            if (audio.contains("sfxVolume")) m_sfxVolume = audio["sfxVolume"];
        }

        // UI
        if (j.contains("ui")) {
            auto& ui = j["ui"];
            if (ui.contains("font")) m_uiFont = ui["font"];
            if (ui.contains("fontSize")) m_uiFontSize = ui["fontSize"];
        }

        setRenderDistance(m_renderDistance);

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
        j["graphics"]["retina"] = m_retina;
        j["graphics"]["renderDistance"] = m_renderDistance;
        j["graphics"]["fpsLimit"] = m_fpsLimit;
        j["graphics"]["fov"] = m_fov;
        
        // Controls
        j["controls"]["mouseSensitivity"] = m_mouseSensitivity;
        j["controls"]["movementSpeed"] = m_movementSpeed;
        for (std::size_t i = 0; i < KEY_ACTION_COUNT; ++i) {
            j["controls"]["keys"][kKeyActionNames[i]] = m_keys[i];
        }
        
        // Audio
        j["audio"]["masterVolume"] = m_masterVolume;
        j["audio"]["musicVolume"] = m_musicVolume;
        j["audio"]["sfxVolume"] = m_sfxVolume;

        // UI
        j["ui"]["font"] = m_uiFont;
        j["ui"]["fontSize"] = m_uiFontSize;

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
