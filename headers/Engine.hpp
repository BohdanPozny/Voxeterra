#pragma once

#include "Window.hpp"
#include "Vulkan/Instance.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/Swapchain.hpp"
#include "Vulkan/RenderPass.hpp"
#include "Vulkan/Framebuffer.hpp"
#include "Vulkan/CommandPool.hpp"
#include "Vulkan/DepthBuffer.hpp"
#include "Vulkan/FrameSync.hpp"
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
    Instance m_instance;
    Device m_device;
    Swapchain m_swapchain;
    RenderPass m_renderPass;
    DepthBuffer m_depthBuffer;
    Framebuffer m_framebuffer;
    CommandPool m_commandPool;

    WorldRenderer m_worldRenderer;
    Camera m_camera;
    InputManager m_input;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    static const int MAX_FRAMES_IN_FLIGHT = 2;
    FrameSync m_frameSync;
    uint32_t m_currentFrame = 0;

    StateManager m_stateManager;
    Config m_config;
    UIRenderer m_uiRenderer;

    bool m_isRunning = false;

    bool initWindow();
    bool initVulkan();
    void mainLoop();
    void cleanup();

    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void processInput(float deltaTime);
    void recreateSwapchain();

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
