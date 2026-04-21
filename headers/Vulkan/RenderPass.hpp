#pragma once

#include <vulkan/vulkan.h>

class Swapchain;

class RenderPass {
private:
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;

public:
    RenderPass() = default;
    ~RenderPass() noexcept;

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    bool init(VkDevice device, VkFormat swapchainImageFormat) noexcept;

    VkRenderPass getRenderPass() const noexcept { return m_renderPass; }
};
