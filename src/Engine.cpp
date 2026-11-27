#include "Engine.hpp"
#include "States/MainMenuState.hpp"
#include "States/PlayingState.hpp"
#include "States/PausedState.hpp"
#include "States/SettingsState.hpp"
#include "UI/UIElement.hpp"
#include "World/Terrain.hpp"
#include "utils/Profiler.hpp"
#include <iostream>
#include <array>
#include <thread>
#include <GLFW/glfw3.h>

Engine::~Engine() noexcept {
    cleanup();
}

bool Engine::init() {
    ZoneScoped;
    m_config.load();

    // Apply Config-driven camera settings before any input is processed.
    m_camera.setFOV(m_config.getFOV());
    m_camera.setMouseSensitivity(m_config.getMouseSensitivity());
    m_camera.setMovementSpeed(m_config.getMovementSpeed());
    m_camera.setPosition(findSpawnPoint());

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
        std::make_unique<SettingsState>(this));

    // Trigger onEnter() for the initial state (MAIN_MENU).
    m_stateManager.update(0.0f);

    m_isRunning = true;
    return true;
}

bool Engine::initWindow() {
    if (!m_window.init(m_config.getWindowWidth(), m_config.getWindowHeight(),
                       "Voxterra", m_config.isFullscreen(), m_config.isRetina())) {
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

    if (!m_worldRenderer.init(m_renderer.getDevice(), m_renderer.getRenderPass(),
                              m_renderer.getSwapchainExtent(), Renderer::MAX_FRAMES_IN_FLIGHT)) {
        std::cerr << "[Engine] Failed to init world renderer" << std::endl;
        return false;
    }

    VkExtent2D ext = m_renderer.getSwapchainExtent();
    m_camera.setAspectRatio(static_cast<float>(ext.width) / static_cast<float>(ext.height));

    if (!m_uiRenderer.init(m_renderer.getDevice(), m_renderer.getRenderPass(), m_renderer.getSwapchainExtent(),
                           m_config.getUIFont(), m_config.getUIFontSize(),
                           Renderer::MAX_FRAMES_IN_FLIGHT)) {
        std::cerr << "[Engine] Failed to init UI renderer" << std::endl;
        return false;
    }

    // All pipelines exist now; later runs skip shader translation (MoltenVK) and compilation.
    m_renderer.getDevice().savePipelineCache();

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

        {
            ZoneScopedN("Input");
            glfwPollEvents();
            m_input.beginFrame();
            m_stateManager.handleInput();
        }
        m_stateManager.update(deltaTime);

        drawFrame();
        m_stateManager.render();
        limitFrameRate();
        FrameMark;
    }

    m_renderer.waitIdle();
}

void Engine::drawFrame() {
    ZoneScoped;
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

    const uint32_t frameIndex = m_renderer.getCurrentFrame();
    m_worldRenderer.updateUniforms(frameIndex, m_camera);

    std::array<float, 4> clearColor = {0.2f, 0.2f, 0.25f, 1.0f};
    IGameState* currentState = m_stateManager.getState(m_stateManager.getCurrentState());
    if (currentState) {
        auto color = currentState->getClearColor();
        clearColor = {color[0], color[1], color[2], color[3]};
    }

    m_renderer.beginRenderPass(commandBuffer, imageIndex, clearColor);

    if (currentState && currentState->shouldRenderWorld()) {
        TracyVkZone(m_renderer.getTracyContext(), commandBuffer, "World");
        m_worldRenderer.render(commandBuffer, frameIndex);
    }

    if (currentState) {
        UIElement* uiRoot = currentState->getUIRoot();
        if (uiRoot) {
            TracyVkZone(m_renderer.getTracyContext(), commandBuffer, "UI");
            m_uiRenderer.renderUI(uiRoot, commandBuffer, frameIndex, ext);
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


void Engine::limitFrameRate() {
    using Clock = std::chrono::steady_clock;
    const int limit = m_config.getFpsLimit();
    if (limit <= 0) return;

    ZoneScopedN("FPS limit");
    const auto period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / limit));
    m_nextFrameTime += period;
    const auto now = Clock::now();
    if (m_nextFrameTime <= now) {
        // Behind schedule (or limit just enabled): restart pacing instead of bursting.
        m_nextFrameTime = now;
        return;
    }
    // sleep_until overshoots by up to a scheduler tick, so spin through the last millisecond.
    std::this_thread::sleep_until(m_nextFrameTime - std::chrono::milliseconds(1));
    while (Clock::now() < m_nextFrameTime) std::this_thread::yield();
}
