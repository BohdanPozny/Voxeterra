#include "Engine.hpp"
#include "States/MainMenuState.hpp"
#include "States/PlayingState.hpp"
#include "States/PausedState.hpp"
#include "States/SettingsState.hpp"
#include "UI/UIElement.hpp"
#include <iostream>
#include <array>
#include <cstring>
#include <GLFW/glfw3.h>

Engine::~Engine() noexcept {
    cleanup();
}

bool Engine::init() {
    m_config.load();

    if (!initWindow())  return false;
    if (!initVulkan())  return false;

    // Register all game states.
    m_stateManager.registerState(GameState::MAIN_MENU,
        std::make_unique<MainMenuState>(this));
    m_stateManager.registerState(GameState::PLAYING,
        std::make_unique<PlayingState>(this));
    m_stateManager.registerState(GameState::PAUSED,
        std::make_unique<PausedState>(this));
    m_stateManager.registerState(GameState::SETTINGS,
        std::make_unique<SettingsState>(this, &m_config));

    // Trigger onEnter() for the initial state (MAIN_MENU).
    m_stateManager.update(0.0f);

    m_isRunning = true;
    return true;
}

bool Engine::initWindow() {
    if (!m_window.init(m_config.getWindowWidth(), m_config.getWindowHeight(), "Voxterra")) {
        std::cerr << "[Engine] Failed to create window" << std::endl;
        return false;
    }
    m_input.init(m_window.getWindow());
    m_input.setCursorEnabled(false);
    return true;
}

bool Engine::initVulkan() {
    // Instance + surface + device.
    if (!m_instance.init("Voxterra")) {
        std::cerr << "[Engine] Failed to create Vulkan instance" << std::endl;
        return false;
    }
    if (!m_window.createWindowSurface(m_instance.getInstance(), &m_surface)) {
        std::cerr << "[Engine] Failed to create window surface" << std::endl;
        return false;
    }
    if (!m_device.init(m_instance.getInstance(), m_surface)) {
        std::cerr << "[Engine] Failed to create device" << std::endl;
        return false;
    }

    // Present chain: swapchain -> render pass -> depth -> framebuffers.
    if (!m_swapchain.init(m_device, m_window, m_surface)) {
        std::cerr << "[Engine] Failed to create swapchain" << std::endl;
        return false;
    }
    if (!m_renderPass.init(m_device.getLogicalDevice(), m_swapchain.getFormat())) {
        std::cerr << "[Engine] Failed to create render pass" << std::endl;
        return false;
    }
    if (!m_depthBuffer.init(m_device, m_swapchain.getExtent())) {
        std::cerr << "[Engine] Failed to create depth buffer" << std::endl;
        return false;
    }
    if (!m_framebuffer.init(m_device.getLogicalDevice(),
                            m_renderPass.getRenderPass(),
                            m_swapchain.getImageViews(),
                            m_depthBuffer.getImageView(),
                            m_swapchain.getExtent())) {
        std::cerr << "[Engine] Failed to create framebuffers" << std::endl;
        return false;
    }

    // Command pool + per-image command buffers.
    if (!m_commandPool.init(m_device.getLogicalDevice(),
                            m_device.getQueueFamilies().graphicsFamily.value())) {
        std::cerr << "[Engine] Failed to create command pool" << std::endl;
        return false;
    }
    if (!m_commandPool.allocateCommandBuffers(m_swapchain.getImageViews().size())) {
        std::cerr << "[Engine] Failed to allocate command buffers" << std::endl;
        return false;
    }

    // Per-frame synchronization primitives.
    if (!m_frameSync.init(m_device.getLogicalDevice(), MAX_FRAMES_IN_FLIGHT)) {
        std::cerr << "[Engine] Failed to create sync objects" << std::endl;
        return false;
    }

    // World + UI renderers share the main render pass.
    uint32_t imageCount = static_cast<uint32_t>(m_swapchain.getImageViews().size());
    if (!m_worldRenderer.init(m_device, m_renderPass.getRenderPass(),
                              m_swapchain.getExtent(), imageCount)) {
        std::cerr << "[Engine] Failed to init world renderer" << std::endl;
        return false;
    }
    m_worldRenderer.generateWorld();

    m_camera.setAspectRatio(static_cast<float>(m_swapchain.getExtent().width) /
                            static_cast<float>(m_swapchain.getExtent().height));

    if (!m_uiRenderer.init(m_device, m_renderPass.getRenderPass(), m_swapchain.getExtent())) {
        std::cerr << "[Engine] Failed to init UI renderer" << std::endl;
        return false;
    }

    std::cout << "[Engine] Vulkan initialized" << std::endl;
    return true;
}

void Engine::run() {
    if (!m_isRunning) {
        std::cerr << "[Engine] Engine not initialized. Call init() first." << std::endl;
        return;
    }

    mainLoop();
}

void Engine::mainLoop() {
    float lastFrame = 0.0f;

    while (!m_window.shouldClose() && m_isRunning) {
        const float now = static_cast<float>(glfwGetTime());
        const float deltaTime = now - lastFrame;
        lastFrame = now;

        glfwPollEvents();
        m_input.beginFrame();

        m_stateManager.handleInput();
        m_stateManager.update(deltaTime);

        // Only the gameplay state consumes camera input.
        IGameState* current = m_stateManager.getState(m_stateManager.getCurrentState());
        if (current && current->shouldRenderWorld()) {
            processInput(deltaTime);
        }

        drawFrame();
        m_stateManager.render();
    }

    // Ensure GPU is idle before destroying resources.
    vkDeviceWaitIdle(m_device.getLogicalDevice());
}

void Engine::drawFrame() {
    m_frameSync.waitForFence(m_currentFrame);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_device.getLogicalDevice(),
                                            m_swapchain.getSwapchain(), UINT64_MAX,
                                            m_frameSync.getImageAvailable(m_currentFrame),
                                            VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "[Engine] Failed to acquire swapchain image" << std::endl;
        return;
    }

    m_worldRenderer.updateUniforms(imageIndex, m_camera);

    // Only reset fence when we actually submit work.
    m_frameSync.resetFence(m_currentFrame);

    VkCommandBuffer commandBuffer = m_commandPool.getCommandBuffer(m_currentFrame);
    vkResetCommandBuffer(commandBuffer, 0);
    recordCommandBuffer(commandBuffer, imageIndex);

    // Submit command buffer
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
        std::cerr << "[Engine] Failed to submit draw command buffer" << std::endl;
        return;
    }

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {m_swapchain.getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_device.getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window.wasResized()) {
        m_window.resetResizedFlag();
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        std::cerr << "[Engine] Failed to present swapchain image" << std::endl;
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Engine::cleanup() {
    // Destruction order is significant: swapchain before surface, surface before instance.
    // Other Vulkan objects clean themselves up via RAII.
    if (m_device.getLogicalDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device.getLogicalDevice());
    }

    m_worldRenderer.cleanup();
    m_frameSync.cleanup();
    m_swapchain.cleanup();

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance.getInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

void Engine::recreateSwapchain() {
    // Block while the window is minimised (zero-sized framebuffer).
    int width = 0, height = 0;
    m_window.getFramebufferSize(&width, &height);
    while (width == 0 || height == 0) {
        m_window.getFramebufferSize(&width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_device.getLogicalDevice());

    m_framebuffer.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.cleanup();

    if (!m_swapchain.init(m_device, m_window, m_surface) ||
        !m_depthBuffer.init(m_device, m_swapchain.getExtent()) ||
        !m_framebuffer.init(m_device.getLogicalDevice(),
                            m_renderPass.getRenderPass(),
                            m_swapchain.getImageViews(),
                            m_depthBuffer.getImageView(),
                            m_swapchain.getExtent())) {
        std::cerr << "[Engine] Failed to recreate swapchain resources" << std::endl;
        return;
    }

    VkExtent2D ext = m_swapchain.getExtent();
    m_camera.setAspectRatio(static_cast<float>(ext.width) / static_cast<float>(ext.height));
}

void Engine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "[Engine] Failed to begin recording command buffer" << std::endl;
        return;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass.getRenderPass();
    renderPassInfo.framebuffer = m_framebuffer.getFramebuffer(imageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.getExtent();

    // Clear values come from the active state (polymorphic).
    std::array<VkClearValue, 2> clearValues{};
    IGameState* currentState = m_stateManager.getState(m_stateManager.getCurrentState());
    if (currentState) {
        auto color = currentState->getClearColor();
        clearValues[0].color = {{color[0], color[1], color[2], color[3]}};
    } else {
        clearValues[0].color = {{0.2f, 0.2f, 0.25f, 1.0f}};
    }
    clearValues[1].depthStencil = {1.0f, 0};
    
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Dynamic viewport/scissor so resizing does not require pipeline recreation.
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

    // Current state decides whether to draw the 3D world.
    if (currentState && currentState->shouldRenderWorld()) {
        m_worldRenderer.render(commandBuffer, imageIndex);
    }

    // UI root is supplied polymorphically by the state.
    if (currentState) {
        UIElement* uiRoot = currentState->getUIRoot();
        if (uiRoot) {
            m_uiRenderer.renderUI(uiRoot, commandBuffer);
        }
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "[Engine] Failed to record command buffer" << std::endl;
    }
}

void Engine::processInput(float deltaTime) {
    // Camera::processKeyboard direction enum: 0=fwd 1=back 2=left 3=right 4=up 5=down.
    if (m_input.isKeyDown(GLFW_KEY_W))          m_camera.processKeyboard(0, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_S))          m_camera.processKeyboard(1, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_A))          m_camera.processKeyboard(2, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_D))          m_camera.processKeyboard(3, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_SPACE))      m_camera.processKeyboard(4, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_LEFT_SHIFT)) m_camera.processKeyboard(5, deltaTime);

    // Invert Y so pushing the mouse down lowers pitch.
    glm::vec2 mouseDelta = m_input.getMouseDelta();
    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
        m_camera.processMouseMovement(mouseDelta.x, -mouseDelta.y);
    }

    if (m_input.isKeyPressed(GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(m_window.getWindow(), true);
    }
}
