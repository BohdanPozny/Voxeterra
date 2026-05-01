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
#include <vulkan/vulkan.h>
#include <array>
#include <memory>

class Renderer {
private:
    Instance m_instance;
    Device m_device;
    Swapchain m_swapchain;
    RenderPass m_renderPass;
    DepthBuffer m_depthBuffer;
    Framebuffer m_framebuffer;
    CommandPool m_commandPool;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    static const int MAX_FRAMES_IN_FLIGHT = 2;
    FrameSync m_frameSync;
    uint32_t m_currentFrame = 0;

    bool m_isFrameStarted = false;

    void recreateSwapchain(Window& window);

public:
    Renderer() = default;
    ~Renderer() noexcept;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(Window& window);
    void cleanup();

    // Frame lifecycle
    VkCommandBuffer beginFrame(Window& window, uint32_t& outImageIndex);
    void beginRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::array<float, 4>& clearColor);
    void endRenderPass(VkCommandBuffer commandBuffer);
    void endFrame(Window& window, uint32_t imageIndex);

    void waitIdle();

    // Accessors
    Device& getDevice() { return m_device; }
    VkRenderPass getRenderPass() const { return m_renderPass.getRenderPass(); }
    VkExtent2D getSwapchainExtent() { return m_swapchain.getExtent(); }
    uint32_t getImageCount() { return static_cast<uint32_t>(m_swapchain.getImageViews().size()); }
    VkFormat getSwapchainFormat() { return m_swapchain.getFormat(); }
};
