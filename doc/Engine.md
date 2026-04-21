# Engine

## Огляд

`Engine` — центральний клас, що координує всі підсистеми гри. Відповідає за ініціалізацію, головний цикл, рендеринг та очистку ресурсів.

## API

```cpp
#pragma once
#include <memory>
#include "Window.hpp"
#include "Config.hpp"
#include "Input/InputManager.hpp"
#include "StateManager.hpp"
#include "Camera.hpp"

// Vulkan
#include "Vulkan/Instance.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/Swapchain.hpp"
#include "Vulkan/RenderPass.hpp"
#include "Vulkan/Framebuffer.hpp"
#include "Vulkan/CommandPool.hpp"
#include "Vulkan/FrameSync.hpp"
#include "Vulkan/Pipeline.hpp"
#include "Vulkan/Buffer.hpp"

// World
#include "World/WorldRenderer.hpp"

// UI
#include "UI/UIRenderer.hpp"

class Engine {
public:
    Engine() = default;
    ~Engine();

    bool init();
    void run();
    void cleanup();

    // Getters
    Window& getWindow() { return m_window; }
    InputManager& getInput() { return m_input; }
    Camera& getCamera() { return m_camera; }
    Config& getConfig() { return m_config; }

    bool shouldClose() const;

private:
    // Core
    Window m_window;
    Config m_config;
    InputManager m_input;
    StateManager m_stateManager;
    Camera m_camera;

    // Vulkan
    Instance m_instance;
    Device m_device;
    Swapchain m_swapchain;
    RenderPass m_renderPass;
    Framebuffer m_framebuffer;
    CommandPool m_commandPool;
    FrameSync m_frameSync;

    // Pipelines
    std::unique_ptr<Pipeline> m_voxelPipeline;
    std::unique_ptr<Pipeline> m_uiPipeline;

    // World
    std::unique_ptr<WorldRenderer> m_worldRenderer;

    // UI
    std::unique_ptr<UIRenderer> m_uiRenderer;

    // Frame data
    uint32_t m_currentFrame = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // Methods
    bool initVulkan();
    bool initWorld();
    bool initUI();

    void processInput(float deltaTime);
    void update(float deltaTime);
    void drawFrame();

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
};
```

## Ініціалізація

```cpp
bool Engine::init() {
    std::cout << "[Engine] Initializing Voxterra..." << std::endl;

    // 1. Завантажуємо конфігурацію
    m_config.load("config.json");

    // 2. Створюємо вікно
    if (!m_window.init(m_config.getWindowWidth(), 
                       m_config.getWindowHeight(), 
                       "Voxterra")) {
        return false;
    }

    // 3. Ініціалізуємо Vulkan
    if (!initVulkan()) return false;

    // 4. Ініціалізуємо ввід
    m_input.init(m_window.getWindow());

    // 5. Ініціалізуємо камеру
    m_camera.init(glm::vec3(0.0f, 20.0f, 0.0f));

    // 6. Ініціалізуємо світ
    if (!initWorld()) return false;

    // 7. Ініціалізуємо UI
    if (!initUI()) return false;

    // 8. Реєструємо стани
    m_stateManager.registerState(GameState::MAIN_MENU, 
                                  std::make_unique<MainMenuState>(this));
    m_stateManager.registerState(GameState::PLAYING, 
                                  std::make_unique<PlayingState>(this));
    m_stateManager.registerState(GameState::PAUSED, 
                                  std::make_unique<PausedState>(this));
    m_stateManager.registerState(GameState::SETTINGS, 
                                  std::make_unique<SettingsState>(this, &m_config));

    std::cout << "[Engine] Initialization complete!" << std::endl;
    return true;
}
```

## Vulkan Ініціалізація

```cpp
bool Engine::initVulkan() {
    // Створюємо Vulkan instance
    if (!m_instance.init()) return false;

    // Створюємо surface
    m_window.createSurface(m_instance.getInstance());

    // Ініціалізуємо пристрій
    if (!m_device.init(m_instance, m_window.getSurface())) return false;

    // Створюємо swapchain
    if (!m_swapchain.init(m_device, m_window.getSurface(),
                          m_config.getWindowWidth(),
                          m_config.getWindowHeight())) return false;

    // Створюємо render pass
    if (!m_renderPass.init(m_device.getLogicalDevice(),
                           m_swapchain.getImageFormat(),
                           VK_FORMAT_D32_SFLOAT)) return false;

    // Створюємо depth buffer
    m_depthBuffer.init(m_device, m_swapchain.getExtent());

    // Створюємо framebuffers
    if (!m_framebuffer.init(m_device.getLogicalDevice(),
                              m_renderPass.getRenderPass(),
                              m_swapchain.getImageViews(),
                              m_depthBuffer.getImageView(),
                              m_swapchain.getExtent())) return false;

    // Створюємо command pool
    if (!m_commandPool.init(m_device, 
                            m_device.getGraphicsQueueFamily(),
                            MAX_FRAMES_IN_FLIGHT)) return false;

    // Створюємо synchronization objects
    if (!m_frameSync.init(m_device.getLogicalDevice(), 
                          MAX_FRAMES_IN_FLIGHT)) return false;

    return true;
}
```

## Головний цикл

```cpp
void Engine::run() {
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!shouldClose()) {
        // Розрахунок delta time
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Починаємо кадр вводу
        m_input.beginFrame();
        glfwPollEvents();

        // Оновлення стану
        m_stateManager.update(deltaTime);
        m_stateManager.handleInput();

        // Ігровий інпут (тільки в PLAYING стані)
        IGameState* currentState = m_stateManager.getState(
            m_stateManager.getCurrentState()
        );
        if (currentState->shouldRenderWorld()) {
            processInput(deltaTime);
            currentState->processGameplayInput(deltaTime);
        }

        // Рендеринг
        if (currentState->shouldRenderWorld()) {
            m_worldRenderer->update(deltaTime);
            drawFrame();
        }
    }

    // Чекаємо завершення GPU роботи перед виходом
    vkDeviceWaitIdle(m_device.getLogicalDevice());
}
```

## Рендеринг кадру

```cpp
void Engine::drawFrame() {
    // 1. Чекаємо завершення попереднього кадру
    m_frameSync.waitForFence(m_currentFrame);

    // 2. Отримуємо наступне зображення
    uint32_t imageIndex;
    VkResult result = m_swapchain.acquireNextImage(
        m_frameSync.getImageAvailableSemaphore(m_currentFrame),
        imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Recreate swapchain (не реалізовано для простоти)
        return;
    }

    m_frameSync.resetFence(m_currentFrame);

    // 3. Оновлюємо uniform buffers
    m_worldRenderer->updateUniformBuffer(imageIndex, m_camera);

    // 4. Записуємо командний буфер
    VkCommandBuffer cmd = m_commandPool.getCommandBuffer(m_currentFrame);
    recordCommandBuffer(cmd, imageIndex);

    // 5. Відправляємо на виконання
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_frameSync.getImageAvailableSemaphore(m_currentFrame)};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = {m_frameSync.getRenderFinishedSemaphore(m_currentFrame)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(m_device.getGraphicsQueue(), 1, &submitInfo,
                  m_frameSync.getInFlightFence(m_currentFrame));

    // 6. Подаємо результат
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {m_swapchain.getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(m_device.getPresentQueue(), &presentInfo);

    // 7. Переходимо до наступного кадру
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

## Очистка

```cpp
Engine::~Engine() {
    cleanup();
}

void Engine::cleanup() {
    // Очищуємо в зворотному порядку
    m_uiRenderer.reset();
    m_worldRenderer.reset();
    m_uiPipeline.reset();
    m_voxelPipeline.reset();

    m_frameSync.cleanup();
    m_commandPool.cleanup();
    m_framebuffer.cleanup();
    m_renderPass.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.cleanup();
    m_device.cleanup();
    m_window.cleanup();
    m_instance.cleanup();
}
```

## Дивіться також

- [[states/index\|Система станів]] — управління станами гри
- [[vulkan/index\|Vulkan]] — графічна підсистема
- [[world/index\|Світ]] — воксельний світ
- [[ui/index\|UI]] — інтерфейс
- [[input/index\|Input]] — ввід
- [[config/index\|Config]] — налаштування
