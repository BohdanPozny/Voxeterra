#pragma once

#include <string>
#include <map>

class Config {
private:
    std::string m_configPath;
    
    // Graphics settings
    int m_windowWidth = 800;
    int m_windowHeight = 600;
    bool m_fullscreen = false;
    bool m_vsync = true;
    int m_renderDistance = 8;  // in chunks
    float m_fov = 70.0f;
    
    // Controls
    float m_mouseSensitivity = 0.1f;
    float m_movementSpeed = 10.0f;
    
    // Audio
    float m_masterVolume = 1.0f;
    float m_musicVolume = 0.7f;
    float m_sfxVolume = 0.8f;

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
    float getFOV() const { return m_fov; }
    
    void setWindowWidth(int width) { m_windowWidth = width; }
    void setWindowHeight(int height) { m_windowHeight = height; }
    void setFullscreen(bool fullscreen) { m_fullscreen = fullscreen; }
    void setVsync(bool vsync) { m_vsync = vsync; }
    void setRenderDistance(int distance) { m_renderDistance = distance; }
    void setFOV(float fov) { m_fov = fov; }
    
    // Controls getters/setters
    float getMouseSensitivity() const { return m_mouseSensitivity; }
    float getMovementSpeed() const { return m_movementSpeed; }
    
    void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    void setMovementSpeed(float speed) { m_movementSpeed = speed; }
    
    // Audio getters/setters
    float getMasterVolume() const { return m_masterVolume; }
    float getMusicVolume() const { return m_musicVolume; }
    float getSFXVolume() const { return m_sfxVolume; }
    
    void setMasterVolume(float volume) { m_masterVolume = volume; }
    void setMusicVolume(float volume) { m_musicVolume = volume; }
    void setSFXVolume(float volume) { m_sfxVolume = volume; }
};
