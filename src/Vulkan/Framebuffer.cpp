#include "Vulkan/Framebuffer.hpp"
#include <iostream>

Framebuffer::~Framebuffer() noexcept {
    cleanup();
}

void Framebuffer::cleanup() noexcept {
    if (m_deviceHandle != VK_NULL_HANDLE) {
        for (auto framebuffer : m_framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_deviceHandle, framebuffer, nullptr);
            }
        }
        m_framebuffers.clear();
    }
}

bool Framebuffer::init(VkDevice device, 
                       VkRenderPass renderPass,
                       const std::vector<VkImageView>& swapchainImageViews,
                       VkImageView depthImageView,
                       VkExtent2D swapchainExtent) noexcept {
    m_deviceHandle = device;
    m_framebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        VkImageView attachments[] = {
            swapchainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_deviceHandle, &framebufferInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "[Framebuffer] Failed to create framebuffer " << i << std::endl;
            return false;
        }
    }

    std::cout << "[Framebuffer] Created " << m_framebuffers.size() << " framebuffers with depth" << std::endl;
    return true;
}
