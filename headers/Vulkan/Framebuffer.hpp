#pragma once

#include <vulkan/vulkan.h>
#include <vector>

class Framebuffer {
private:
    std::vector<VkFramebuffer> m_framebuffers;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;

public:
    Framebuffer() = default;
    ~Framebuffer() noexcept;

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    bool init(VkDevice device, 
              VkRenderPass renderPass,
              const std::vector<VkImageView>& swapchainImageViews,
              VkImageView depthImageView,
              VkExtent2D swapchainExtent) noexcept;

    const std::vector<VkFramebuffer>& getFramebuffers() const noexcept { return m_framebuffers; }
    VkFramebuffer getFramebuffer(size_t index) const noexcept { return m_framebuffers[index]; }
};
