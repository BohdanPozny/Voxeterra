#pragma once

#include "Window.hpp"
#include "Vulkan/Instance.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/Swapchain.hpp"
#include "Vulkan/RenderPass.hpp"
#include "Vulkan/Framebuffer.hpp"
#include "Vulkan/Pipeline.hpp"
#include "Vulkan/CommandPool.hpp"
#include "Vulkan/Buffer.hpp"
#include "Vulkan/DepthBuffer.hpp"
#include "World/World.hpp"
#include "Camera.hpp"
#include "StateManager.hpp"
#include "Config.hpp"
#include "UI/UIRenderer.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class Engine {
private:
    // Порядок важливий! Деструктори викликаються у зворотному порядку
    // Останній оголошений = перший знищений
    
    // Vulkan об'єкти (у порядку створення, знищуються у зворотному)
    Window m_window;           // Створюється першим
    Instance m_instance;       // Потрібен для Surface
    Device m_device;           // Потрібен для всього
    Swapchain m_swapchain;
    RenderPass m_renderPass;
    DepthBuffer m_depthBuffer;
    Framebuffer m_framebuffer;
    Pipeline m_pipeline;
    CommandPool m_commandPool; // Створюється останнім, знищується першим
    
    // Воксельний світ
    std::unique_ptr<World> m_world;
    std::vector<std::unique_ptr<Buffer>> m_vertexBuffers;
    std::vector<std::unique_ptr<Buffer>> m_indexBuffers;

    // Camera
    Camera m_camera;
    bool m_firstMouse = true;
    float m_lastX = 400.0f;
    float m_lastY = 300.0f;
    
    // Uniform buffers для MVP матриць
    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };
    std::vector<std::unique_ptr<Buffer>> m_uniformBuffers;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    // Synchronization objects
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;

    // State management
    StateManager m_stateManager;
    Config m_config;
    UIRenderer m_uiRenderer;
    bool m_isRunning = false;

    bool initWindow();
    bool initVulkan();
    void mainLoop();
    void cleanup();

    void initWorld();
    void updateVoxelBuffers();
    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage);
    
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void processInput(float deltaTime);
    void mouseCallback(double xpos, double ypos);
    
    static void staticMouseCallback(GLFWwindow* window, double xpos, double ypos);

public:
    Engine() = default;
    ~Engine() noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool init();
    void run();
    
    // Публічні методи для станів
    Window& getWindow() { return m_window; }
    Config& getConfig() { return m_config; }
    Camera& getCamera() { return m_camera; }
    UIRenderer& getUIRenderer() { return m_uiRenderer; }
};
