#include "Vulkan/Renderer.hpp"
#include <iostream>
#include <GLFW/glfw3.h>

Renderer::~Renderer() noexcept {
    cleanup();
}

bool Renderer::init(Window& window, bool vsync) {
    m_vsync = vsync;
    if (!m_instance.init("Voxterra")) {
        std::cerr << "[Renderer] Failed to create Vulkan instance" << std::endl;
        return false;
    }
    if (!window.createWindowSurface(m_instance.getInstance(), &m_surface)) {
        std::cerr << "[Renderer] Failed to create window surface" << std::endl;
        return false;
    }
    if (!m_device.init(m_instance.getInstance(), m_surface)) {
        std::cerr << "[Renderer] Failed to create device" << std::endl;
        return false;
    }

    if (!m_swapchain.init(m_device, window, m_surface, m_vsync)) {
        std::cerr << "[Renderer] Failed to create swapchain" << std::endl;
        return false;
    }
    if (!m_renderPass.init(m_device.getLogicalDevice(), m_swapchain.getFormat())) {
        std::cerr << "[Renderer] Failed to create render pass" << std::endl;
        return false;
    }
    if (!m_depthBuffer.init(m_device, m_swapchain.getExtent())) {
        std::cerr << "[Renderer] Failed to create depth buffer" << std::endl;
        return false;
    }
    if (!m_framebuffer.init(m_device.getLogicalDevice(),
                            m_renderPass.getRenderPass(),
                            m_swapchain.getImageViews(),
                            m_depthBuffer.getImageView(),
                            m_swapchain.getExtent())) {
        std::cerr << "[Renderer] Failed to create framebuffers" << std::endl;
        return false;
    }

    if (!m_commandPool.init(m_device.getLogicalDevice(),
                            m_device.getQueueFamilies().graphicsFamily.value())) {
        std::cerr << "[Renderer] Failed to create command pool" << std::endl;
        return false;
    }
    if (!m_commandPool.allocateCommandBuffers(m_swapchain.getImageViews().size())) {
        std::cerr << "[Renderer] Failed to allocate command buffers" << std::endl;
        return false;
    }

    if (!m_frameSync.init(m_device.getLogicalDevice(), MAX_FRAMES_IN_FLIGHT)) {
        std::cerr << "[Renderer] Failed to create sync objects" << std::endl;
        return false;
    }

    std::cout << "[Renderer] Initialized successfully" << std::endl;
    return true;
}

void Renderer::cleanup() {
    if (m_device.getLogicalDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device.getLogicalDevice());
    }

    m_frameSync.cleanup();
    m_swapchain.cleanup();

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance.getInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

void Renderer::waitIdle() {
    if (m_device.getLogicalDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device.getLogicalDevice());
    }
}

void Renderer::recreateSwapchain(Window& window) {
    int width = 0, height = 0;
    window.getFramebufferSize(&width, &height);
    while (width == 0 || height == 0) {
        window.getFramebufferSize(&width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_device.getLogicalDevice());

    m_framebuffer.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.cleanup();

    if (!m_swapchain.init(m_device, window, m_surface, m_vsync) ||
        !m_depthBuffer.init(m_device, m_swapchain.getExtent()) ||
        !m_framebuffer.init(m_device.getLogicalDevice(),
                            m_renderPass.getRenderPass(),
                            m_swapchain.getImageViews(),
                            m_depthBuffer.getImageView(),
                            m_swapchain.getExtent())) {
        std::cerr << "[Renderer] Failed to recreate swapchain resources" << std::endl;
    }
}

VkCommandBuffer Renderer::beginFrame(Window& window, uint32_t& outImageIndex) {
    if (m_isFrameStarted) {
        std::cerr << "[Renderer] Frame already started!" << std::endl;
        return VK_NULL_HANDLE;
    }

    m_frameSync.waitForFence(m_currentFrame);

    VkResult result = vkAcquireNextImageKHR(m_device.getLogicalDevice(),
                                            m_swapchain.getSwapchain(), UINT64_MAX,
                                            m_frameSync.getImageAvailable(m_currentFrame),
                                            VK_NULL_HANDLE, &outImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(window);
        return VK_NULL_HANDLE;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "[Renderer] Failed to acquire swapchain image" << std::endl;
        return VK_NULL_HANDLE;
    }

    m_isFrameStarted = true;
    m_frameSync.resetFence(m_currentFrame);

    VkCommandBuffer commandBuffer = m_commandPool.getCommandBuffer(m_currentFrame);
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "[Renderer] Failed to begin recording command buffer" << std::endl;
        return VK_NULL_HANDLE;
    }

    return commandBuffer;
}

void Renderer::beginRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::array<float, 4>& clearColor) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass.getRenderPass();
    renderPassInfo.framebuffer = m_framebuffer.getFramebuffer(imageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.getExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkExtent2D extent = m_swapchain.getExtent();
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Renderer::endRenderPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}

void Renderer::endFrame(Window& window, uint32_t imageIndex) {
    if (!m_isFrameStarted) {
        std::cerr << "[Renderer] Cannot end frame before starting it!" << std::endl;
        return;
    }

    VkCommandBuffer commandBuffer = m_commandPool.getCommandBuffer(m_currentFrame);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "[Renderer] Failed to record command buffer" << std::endl;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_frameSync.getImageAvailable(m_currentFrame)};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = {m_frameSync.getRenderFinished(m_currentFrame)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_device.getGraphicsQueue(), 1, &submitInfo, m_frameSync.getFence(m_currentFrame)) != VK_SUCCESS) {
        std::cerr << "[Renderer] Failed to submit draw command buffer" << std::endl;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {m_swapchain.getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(m_device.getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.wasResized()) {
        window.resetResizedFlag();
        recreateSwapchain(window);
    } else if (result != VK_SUCCESS) {
        std::cerr << "[Renderer] Failed to present swapchain image" << std::endl;
    }

    m_isFrameStarted = false;
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
