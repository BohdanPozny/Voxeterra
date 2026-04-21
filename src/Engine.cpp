#include "Engine.hpp"
#include "States/MainMenuState.hpp"
#include "States/PlayingState.hpp"
#include "States/PausedState.hpp"
#include "States/SettingsState.hpp"
#include <iostream>
#include <array>
#include <cstring>
#include <GLFW/glfw3.h>

Engine::~Engine() noexcept {
    cleanup();
}

bool Engine::init() {
    // Завантажити конфіг
    m_config.load();
    
    if (!initWindow()) {
        return false;
    }

    if (!initVulkan()) {
        return false;
    }
    
    // Ініціалізувати стани
    m_stateManager.registerState(GameState::MAIN_MENU, 
        std::make_unique<MainMenuState>(this));
    m_stateManager.registerState(GameState::PLAYING, 
        std::make_unique<PlayingState>(this));
    m_stateManager.registerState(GameState::PAUSED, 
        std::make_unique<PausedState>(this));
    m_stateManager.registerState(GameState::SETTINGS, 
        std::make_unique<SettingsState>(this, &m_config));
    
    // Ініціалізувати початковий стан (вже MAIN_MENU за замовчуванням)
    m_stateManager.update(0.0f);  // Це викличе onEnter для поточного стану

    m_isRunning = true;
    return true;
}

bool Engine::initWindow() {
    if (!m_window.init(800, 600, "Voxterra")) {
        std::cerr << "[Engine] Not create Window" << std::endl;
        return false;
    }
    
    // Налаштування mouse input
    GLFWwindow* window = m_window.getWindow();
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, staticMouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    return true;
}

bool Engine::initVulkan() {
    // Ініціалізація Instance
    if (!m_instance.init("Voxterra")) {
        std::cerr << "[Engine] Not create Instance" << std::endl;
        return false;
    }

    // Створення Surface
    if (!m_window.createWindowSurface(m_instance.getInstance(), &m_surface)) {
        std::cerr << "[Engine] Not create Surface" << std::endl;
        return false;
    }

    // Ініціалізація Device
    if (!m_device.init(m_instance.getInstance(), m_surface)) {
        std::cerr << "[Engine] Not create Device" << std::endl;
        return false;
    }

    // Ініціалізація Swapchain
    if (!m_swapchain.init(m_device, m_window, m_surface)) {
        std::cerr << "[Engine] Not create Swapchain" << std::endl;
        return false;
    }

    // Ініціалізація Render Pass
    if (!m_renderPass.init(m_device.getLogicalDevice(), m_swapchain.getFormat())) {
        std::cerr << "[Engine] Not create Render Pass" << std::endl;
        return false;
    }

    // Ініціалізація Depth Buffer
    if (!m_depthBuffer.init(m_device, m_swapchain.getExtent())) {
        std::cerr << "[Engine] Not create Depth Buffer" << std::endl;
        return false;
    }

    // Ініціалізація Framebuffers
    if (!m_framebuffer.init(m_device.getLogicalDevice(), 
                            m_renderPass.getRenderPass(),
                            m_swapchain.getImageViews(),
                            m_depthBuffer.getImageView(),
                            m_swapchain.getExtent())) {
        std::cerr << "[Engine] Not create Framebuffers" << std::endl;
        return false;
    }

    // Створення descriptor set layout
    std::cout << "[Engine] Creating descriptor set layout..." << std::endl;
    createDescriptorSetLayout();
    
    if (m_descriptorSetLayout == VK_NULL_HANDLE) {
        std::cerr << "[Engine] Descriptor set layout is NULL!" << std::endl;
        return false;
    }
    std::cout << "[Engine] Descriptor set layout created: " << m_descriptorSetLayout << std::endl;
    
    // Ініціалізація Graphics Pipeline (воксельний з vertex input)
    std::cout << "[Engine] Creating voxel graphics pipeline..." << std::endl;
    if (!m_pipeline.initWithVertexInput(m_device.getLogicalDevice(),
                                        m_renderPass.getRenderPass(),
                                        m_swapchain.getExtent(),
                                        "shaders/voxel.vert.spv",
                                        "shaders/voxel.frag.spv",
                                        m_descriptorSetLayout)) {
        std::cerr << "[Engine] Not create Voxel Graphics Pipeline" << std::endl;
        return false;
    }
    std::cout << "[Engine] Voxel graphics pipeline created" << std::endl;

    // Ініціалізація Command Pool
    if (!m_commandPool.init(m_device.getLogicalDevice(), 
                            m_device.getQueueFamilies().graphicsFamily.value())) {
        std::cerr << "[Engine] Not create Command Pool" << std::endl;
        return false;
    }

    // Виділення Command Buffers
    if (!m_commandPool.allocateCommandBuffers(m_swapchain.getImageViews().size())) {
        std::cerr << "[Engine] Not allocate Command Buffers" << std::endl;
        return false;
    }

    // Створення Synchronization Objects
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device.getLogicalDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device.getLogicalDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device.getLogicalDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            std::cerr << "[Engine] Failed to create synchronization objects" << std::endl;
            return false;
        }
    }

    // Ініціалізація воксельного світу
    initWorld();
    
    // Створення uniform buffers та descriptor sets
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    
    // Налаштування камери
    m_camera.setAspectRatio(static_cast<float>(m_swapchain.getExtent().width) / 
                            static_cast<float>(m_swapchain.getExtent().height));
    
    // Ініціалізація UI Renderer
    if (!m_uiRenderer.init(m_device, m_renderPass.getRenderPass(), m_swapchain.getExtent())) {
        std::cerr << "[Engine] Failed to initialize UI Renderer" << std::endl;
        return false;
    }

    std::cout << "[Engine] Vulkan initialized successfully" << std::endl;
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
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        glfwPollEvents();
        
        // Оновлення поточного стану
        m_stateManager.handleInput();
        m_stateManager.update(deltaTime);
        
        // Завжди рендеримо вікно (світ або меню)
        if (m_stateManager.getCurrentState() == GameState::PLAYING) {
            processInput(deltaTime);
        }
        
        // Рендеримо кадр (світ + UI)
        drawFrame();
        
        // Рендеринг UI стану (меню тощо)
        m_stateManager.render();
    }

    // Чекаємо завершення всіх операцій перед виходом
    vkDeviceWaitIdle(m_device.getLogicalDevice());
}

void Engine::drawFrame() {
    static bool firstFrame = true;
    
    // Чекаємо на fence поточного кадру
    vkWaitForFences(m_device.getLogicalDevice(), 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    // Отримуємо наступний image зі swapchain
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_device.getLogicalDevice(), m_swapchain.getSwapchain(), UINT64_MAX,
                                             m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
    
    // Оновлюємо uniform buffer
    updateUniformBuffer(imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // TODO: recreateSwapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "[Engine] Failed to acquire swapchain image" << std::endl;
        return;
    }

    // Скидаємо fence тільки якщо будемо submit'ити роботу
    vkResetFences(m_device.getLogicalDevice(), 1, &m_inFlightFences[m_currentFrame]);

    // Записуємо command buffer
    VkCommandBuffer commandBuffer = m_commandPool.getCommandBuffer(m_currentFrame);
    vkResetCommandBuffer(commandBuffer, 0);
    recordCommandBuffer(commandBuffer, imageIndex);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_device.getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
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

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // TODO: recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        std::cerr << "[Engine] Failed to present swapchain image" << std::endl;
    }

    // Переходимо до наступного кадру
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    
    if (firstFrame) {
        std::cout << "[Engine] First frame rendered successfully" << std::endl;
        firstFrame = false;
    }
}

void Engine::cleanup() {
    // Чекаємо завершення всіх операцій GPU перед cleanup
    if (m_device.getLogicalDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device.getLogicalDevice());
        std::cout << "[Engine] GPU idle, starting cleanup..." << std::endl;
    }
    
    // Очищення sync objects (створені останніми)
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device.getLogicalDevice(), m_imageAvailableSemaphores[i], nullptr);
            m_imageAvailableSemaphores[i] = VK_NULL_HANDLE;
        }
        if (m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device.getLogicalDevice(), m_renderFinishedSemaphores[i], nullptr);
            m_renderFinishedSemaphores[i] = VK_NULL_HANDLE;
        }
        if (m_inFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(m_device.getLogicalDevice(), m_inFlightFences[i], nullptr);
            m_inFlightFences[i] = VK_NULL_HANDLE;
        }
    }
    
    // Очищення воксельних buffers (перед descriptor sets)
    m_vertexBuffers.clear();
    m_indexBuffers.clear();
    m_world.reset();
    
    // Очищення uniform buffers (перед descriptor pool)
    m_uniformBuffers.clear();
    
    // Очищення descriptor sets (автоматично через pool)
    m_descriptorSets.clear();
    
    // Очищення descriptor pool
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device.getLogicalDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    
    // Очищення descriptor set layout
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device.getLogicalDevice(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    
    // CommandPool cleanup (автоматично через деструктор класу)
    // Pipeline cleanup (автоматично через деструктор класу)
    // Framebuffer cleanup (автоматично через деструктор класу)
    // DepthBuffer cleanup (автоматично через деструктор класу)
    // RenderPass cleanup (автоматично через деструктор класу)
    
    // Swapchain cleanup (ЯВНО, перед Surface!)
    m_swapchain.cleanup();
    
    // Surface cleanup (перед Instance)
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance.getInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    
    // Device та Instance cleanup (автоматично через деструктори)
    // GLFW cleanup буде після деструкторів всіх об'єктів
    
    std::cout << "[Engine] Cleanup completed" << std::endl;
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

    // Початок Render Pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass.getRenderPass();
    renderPassInfo.framebuffer = m_framebuffer.getFramebuffer(imageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.getExtent();

    // Clear values (color + depth)
    std::array<VkClearValue, 2> clearValues{};
    // Сірий фон для меню, синій для гри
    if (m_stateManager.getCurrentState() == GameState::MAIN_MENU) {
        clearValues[0].color = {{0.2f, 0.2f, 0.25f, 1.0f}};  // темно-сірий
    } else {
        clearValues[0].color = {{0.0f, 0.0f, 0.2f, 1.0f}};  // темно-синій
    }
    clearValues[1].depthStencil = {1.0f, 0};  // depth = 1.0
    
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind Graphics Pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getPipeline());
    
    // Bind descriptor set (MVP матриці)
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            m_pipeline.getLayout(), 0, 1, 
                            &m_descriptorSets[imageIndex], 0, nullptr);

    // Рендеримо воксельні чанки ТІЛЬКИ якщо не в меню
    if (m_stateManager.getCurrentState() == GameState::PLAYING) {
        size_t chunkIndex = 0;
        size_t drawCalls = 0;
        for (const auto& [pos, chunk] : m_world->getChunks()) {
            if (chunk->getIndices().empty()) {
                continue;
            }

            if (chunkIndex >= m_vertexBuffers.size()) {
                break;
            }

            VkBuffer vertexBuffers[] = {m_vertexBuffers[chunkIndex]->getBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffers[chunkIndex]->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(chunk->getIndices().size()), 1, 0, 0, 0);

            drawCalls++;
            chunkIndex++;
        }
        
        static bool firstLog = true;
        if (firstLog) {
            std::cout << "[Engine] Rendered " << drawCalls << " chunks" << std::endl;
            firstLog = false;
        }
    }
    
    // Рендеримо UI (кнопки меню або HUD)
    if (m_stateManager.getCurrentState() == GameState::MAIN_MENU) {
        // Отримуємо UI панель з MainMenuState
        auto* mainMenuState = dynamic_cast<MainMenuState*>(m_stateManager.getState(GameState::MAIN_MENU));
        if (mainMenuState && mainMenuState->getMenuPanel()) {
            m_uiRenderer.renderUI(mainMenuState->getMenuPanel(), commandBuffer);
        }
    } else if (m_stateManager.getCurrentState() == GameState::PLAYING) {
        // Рендеримо HUD (FPS counter)
        auto* playingState = dynamic_cast<PlayingState*>(m_stateManager.getState(GameState::PLAYING));
        if (playingState && playingState->getHUDPanel()) {
            m_uiRenderer.renderUI(playingState->getHUDPanel(), commandBuffer);
        }
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "[Engine] Failed to record command buffer" << std::endl;
    }
}

void Engine::initWorld() {
    // Створюємо світ
    m_world = std::make_unique<World>();
    m_world->generateTestWorld();
    m_world->updateChunks();

    // Створюємо buffers для всіх чанків
    updateVoxelBuffers();
}

void Engine::updateVoxelBuffers() {
    // Очищуємо старі buffers
    m_vertexBuffers.clear();
    m_indexBuffers.clear();

    // Створюємо buffers для кожного чанку
    for (const auto& [pos, chunk] : m_world->getChunks()) {
        const auto& vertices = chunk->getVertices();
        const auto& indices = chunk->getIndices();

        if (vertices.empty() || indices.empty()) {
            continue;
        }

        // Vertex buffer
        auto vertexBuffer = std::make_unique<Buffer>();
        if (vertexBuffer->init(m_device,
                               vertices.size() * sizeof(VoxelVertex),
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            vertexBuffer->copyData(vertices.data(), vertices.size() * sizeof(VoxelVertex));
            m_vertexBuffers.push_back(std::move(vertexBuffer));
        }

        // Index buffer
        auto indexBuffer = std::make_unique<Buffer>();
        if (indexBuffer->init(m_device,
                              indices.size() * sizeof(uint32_t),
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            indexBuffer->copyData(indices.data(), indices.size() * sizeof(uint32_t));
            m_indexBuffers.push_back(std::move(indexBuffer));
        }
    }

    std::cout << "[Engine] Created buffers for " << m_vertexBuffers.size() << " chunks" << std::endl;
}

void Engine::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_device.getLogicalDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Engine] Failed to create descriptor set layout" << std::endl;
    } else {
        std::cout << "[Engine] Descriptor set layout created" << std::endl;
    }
}

void Engine::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    
    size_t imageCount = m_swapchain.getImageViews().size();
    m_uniformBuffers.resize(imageCount);

    for (size_t i = 0; i < imageCount; i++) {
        auto uniformBuffer = std::make_unique<Buffer>();
        if (!uniformBuffer->init(m_device,
                                 bufferSize,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            std::cerr << "[Engine] Failed to create uniform buffer " << i << std::endl;
        }
        m_uniformBuffers[i] = std::move(uniformBuffer);
    }
    
    std::cout << "[Engine] Created " << imageCount << " uniform buffers" << std::endl;
}

void Engine::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(m_swapchain.getImageViews().size());

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(m_swapchain.getImageViews().size());

    if (vkCreateDescriptorPool(m_device.getLogicalDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        std::cerr << "[Engine] Failed to create descriptor pool" << std::endl;
    } else {
        std::cout << "[Engine] Descriptor pool created" << std::endl;
    }
}

void Engine::createDescriptorSets() {
    size_t imageCount = m_swapchain.getImageViews().size();
    std::vector<VkDescriptorSetLayout> layouts(imageCount, m_descriptorSetLayout);
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(imageCount);
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(imageCount);
    if (vkAllocateDescriptorSets(m_device.getLogicalDevice(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "[Engine] Failed to allocate descriptor sets" << std::endl;
        return;
    }

    for (size_t i = 0; i < imageCount; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_device.getLogicalDevice(), 1, &descriptorWrite, 0, nullptr);
    }
    
    std::cout << "[Engine] Created " << imageCount << " descriptor sets" << std::endl;
}

void Engine::updateUniformBuffer(uint32_t currentImage) {
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = m_camera.getViewMatrix();
    ubo.proj = m_camera.getProjectionMatrix();
    
    // Vulkan має інвертовану Y-координату
    ubo.proj[1][1] *= -1;

    void* data;
    vkMapMemory(m_device.getLogicalDevice(), m_uniformBuffers[currentImage]->getMemory(), 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(m_device.getLogicalDevice(), m_uniformBuffers[currentImage]->getMemory());
}

void Engine::processInput(float deltaTime) {
    GLFWwindow* window = m_window.getWindow();
    
    // WASD рух
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        m_camera.processKeyboard(0, deltaTime);  // Forward
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        m_camera.processKeyboard(1, deltaTime);  // Backward
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        m_camera.processKeyboard(2, deltaTime);  // Left
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        m_camera.processKeyboard(3, deltaTime);  // Right
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        m_camera.processKeyboard(4, deltaTime);  // Up
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        m_camera.processKeyboard(5, deltaTime);  // Down
        
    // ESC для виходу
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void Engine::staticMouseCallback(GLFWwindow* window, double xpos, double ypos) {
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine) {
        engine->mouseCallback(xpos, ypos);
    }
}

void Engine::mouseCallback(double xpos, double ypos) {
    if (m_firstMouse) {
        m_lastX = xpos;
        m_lastY = ypos;
        m_firstMouse = false;
    }

    float xoffset = xpos - m_lastX;
    float yoffset = m_lastY - ypos;  // reversed: y ranges bottom to top

    m_lastX = xpos;
    m_lastY = ypos;

    m_camera.processMouseMovement(xoffset, yoffset);
}
