#include "Engine.hpp"
#include "States/MainMenuState.hpp"
#include "States/PlayingState.hpp"
#include "States/PausedState.hpp"
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

    // Trigger onEnter() for the initial state (MAIN_MENU).
    m_stateManager.update(0.0f);

    m_isRunning = true;
    return true;
}

bool Engine::initWindow() {
    if (!m_window.init(m_config.getWindowWidth(), m_config.getWindowHeight(),
                       "Voxterra", m_config.isFullscreen())) {
        std::cerr << "[Engine] Failed to create window" << std::endl;
        return false;
    }
    m_input.init(m_window.getWindow());
    m_input.setCursorEnabled(false);
    return true;
}

bool Engine::initVulkan() {
    if (!m_renderer.init(m_window, m_config.isVsync())) {
        std::cerr << "[Engine] Failed to initialize Renderer" << std::endl;
        return false;
    }

    uint32_t imageCount = m_renderer.getImageCount();
    if (!m_worldRenderer.init(m_renderer.getDevice(), m_renderer.getRenderPass(),
                              m_renderer.getSwapchainExtent(), imageCount)) {
        std::cerr << "[Engine] Failed to init world renderer" << std::endl;
        return false;
    }
    m_worldRenderer.generateWorld();

    VkExtent2D ext = m_renderer.getSwapchainExtent();
    m_camera.setAspectRatio(static_cast<float>(ext.width) / static_cast<float>(ext.height));

    if (!m_uiRenderer.init(m_renderer.getDevice(), m_renderer.getRenderPass(), m_renderer.getSwapchainExtent())) {
        std::cerr << "[Engine] Failed to init UI renderer" << std::endl;
        return false;
    }

    std::cout << "[Engine] Vulkan initialized via Renderer" << std::endl;
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

    m_renderer.waitIdle();
}

void Engine::drawFrame() {
    uint32_t imageIndex;
    VkCommandBuffer commandBuffer = m_renderer.beginFrame(m_window, imageIndex);
    if (commandBuffer == VK_NULL_HANDLE) {
        // Swapchain recreated or failed. Update aspect ratio.
        VkExtent2D ext = m_renderer.getSwapchainExtent();
        if (ext.width > 0 && ext.height > 0) {
            m_camera.setAspectRatio(static_cast<float>(ext.width) / static_cast<float>(ext.height));
        }
        return;
    }

    // Update aspect ratio every successful frame start in case of implicit resize.
    VkExtent2D ext = m_renderer.getSwapchainExtent();
    m_camera.setAspectRatio(static_cast<float>(ext.width) / static_cast<float>(ext.height));

    m_worldRenderer.updateUniforms(imageIndex, m_camera);

    std::array<float, 4> clearColor = {0.2f, 0.2f, 0.25f, 1.0f};
    IGameState* currentState = m_stateManager.getState(m_stateManager.getCurrentState());
    if (currentState) {
        auto color = currentState->getClearColor();
        clearColor = {color[0], color[1], color[2], color[3]};
    }

    m_renderer.beginRenderPass(commandBuffer, imageIndex, clearColor);

    if (currentState && currentState->shouldRenderWorld()) {
        m_worldRenderer.render(commandBuffer, imageIndex);
    }

    if (currentState) {
        UIElement* uiRoot = currentState->getUIRoot();
        if (uiRoot) {
            m_uiRenderer.renderUI(uiRoot, commandBuffer);
        }
    }

    m_renderer.endRenderPass(commandBuffer);
    m_renderer.endFrame(m_window, imageIndex);
}

void Engine::cleanup() {
    m_renderer.waitIdle();
    m_worldRenderer.cleanup();
    m_renderer.cleanup();
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

    // ESC is reserved for the active state (PlayingState opens the pause menu,
    // PausedState resumes/exits) — do NOT close the window here.
}
