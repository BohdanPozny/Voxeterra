#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

enum class KeyAction { Forward, Back, Left, Right, Up, Down, Count };

class Config {
public:
    static constexpr int MIN_RENDER_DISTANCE = 2;
    static constexpr std::size_t KEY_ACTION_COUNT = static_cast<std::size_t>(KeyAction::Count);

private:
    std::string m_configPath;
    
    // Graphics settings
    int m_windowWidth = 800;
    int m_windowHeight = 600;
    bool m_fullscreen = false;
    bool m_vsync = true;
    int m_renderDistance = 8;  // in chunks
    int m_fpsLimit = 0;        // 0 = unlimited
    float m_fov = 70.0f;
    
    // Controls
    float m_mouseSensitivity = 0.1f;
    float m_movementSpeed = 10.0f;  // m/s
    std::array<int, KEY_ACTION_COUNT> m_keys;  // GLFW key codes
    
    // Audio
    float m_masterVolume = 1.0f;
    float m_musicVolume = 0.7f;
    float m_sfxVolume = 0.8f;

    // UI
    std::string m_uiFont = "fonts/default.ttf";
    int m_uiFontSize = 64;  // atlas raster height in pixels

public:
    Config(const std::string& configPath = "config.json");
    
    bool load();
    bool save();
    
    // Graphics getters/setters
    int getWindowWidth() const { return m_windowWidth; }
    int getWindowHeight() const { return m_windowHeight; }
    bool isFullscreen() const { return m_fullscreen; }
    bool isVsync() const { return m_vsync; }
    int getRenderDistance() const { return m_renderDistance; }
    int getFpsLimit() const { return m_fpsLimit; }
    float getFOV() const { return m_fov; }
    
    void setWindowWidth(int width) { m_windowWidth = width; }
    void setWindowHeight(int height) { m_windowHeight = height; }
    void setFullscreen(bool fullscreen) { m_fullscreen = fullscreen; }
    void setVsync(bool vsync) { m_vsync = vsync; }
    void setRenderDistance(int distance) {
        m_renderDistance = std::max(distance, MIN_RENDER_DISTANCE);
    }
    void setFpsLimit(int limit) { m_fpsLimit = std::max(limit, 0); }
    void setFOV(float fov) { m_fov = fov; }
    
    // Controls getters/setters
    float getMouseSensitivity() const { return m_mouseSensitivity; }
    float getMovementSpeed() const { return m_movementSpeed; }
    
    void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    void setMovementSpeed(float speed) { m_movementSpeed = speed; }

    int getKey(KeyAction action) const { return m_keys[static_cast<std::size_t>(action)]; }
    void setKey(KeyAction action, int key) { m_keys[static_cast<std::size_t>(action)] = key; }
    
    // Audio getters/setters
    float getMasterVolume() const { return m_masterVolume; }
    float getMusicVolume() const { return m_musicVolume; }
    float getSFXVolume() const { return m_sfxVolume; }
    
    void setMasterVolume(float volume) { m_masterVolume = volume; }
    void setMusicVolume(float volume) { m_musicVolume = volume; }
    void setSFXVolume(float volume) { m_sfxVolume = volume; }

    // UI getters/setters
    const std::string& getUIFont() const { return m_uiFont; }
    int getUIFontSize() const { return m_uiFontSize; }

    void setUIFont(const std::string& font) { m_uiFont = font; }
    void setUIFontSize(int size) { m_uiFontSize = size; }
};
