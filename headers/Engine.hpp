#pragma once

#include "Window.hpp"
#include "Vulkan/Renderer.hpp"
#include "World/WorldRenderer.hpp"
#include "Camera.hpp"
#include "StateManager.hpp"
#include "Config.hpp"
#include "UI/UIRenderer.hpp"
#include "Input/InputManager.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class Engine {
private:
    // Declaration order matches creation order; destructors run in reverse,
    // which gives us correct teardown for Vulkan resources.
    Window m_window;
    Renderer m_renderer;

    WorldRenderer m_worldRenderer;
    Camera m_camera;
    InputManager m_input;

    StateManager m_stateManager;
    Config m_config;
    UIRenderer m_uiRenderer;

    bool m_isRunning = false;

    bool initWindow();
    bool initVulkan();
    void mainLoop();
    void cleanup();

    void drawFrame();
    void processInput(float deltaTime);

public:
    Engine() = default;
    ~Engine() noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool init();
    void run();
    
    // Accessors used by game states.
    Window& getWindow() { return m_window; }
    Config& getConfig() { return m_config; }
    Camera& getCamera() { return m_camera; }
    UIRenderer& getUIRenderer() { return m_uiRenderer; }
    InputManager& getInput() { return m_input; }
    WorldRenderer& getWorldRenderer() { return m_worldRenderer; }
};
