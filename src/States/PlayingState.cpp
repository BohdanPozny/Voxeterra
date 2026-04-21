#include "States/PlayingState.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <iomanip>

PlayingState::PlayingState(Engine* engine) 
    : m_engine(engine) {
    
    // Створюємо HUD панель (прозора)
    m_hudPanel = std::make_unique<UIPanel>(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)  // Прозора
    );
    
    // FPS label (верхній лівий кут) - МАЛЕНЬКИЙ прямокутник
    auto fpsLabel = std::make_unique<UILabel>(
        glm::vec2(0.02f, 0.02f),   // Трохи відступ від краю
        glm::vec2(0.08f, 0.04f),   // Маленький розмір: 8% ширини, 4% висоти
        "FPS: 0",
        16,
        glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)  // Жовтий текст
    );
    // Фон для FPS (яскравий для видимості)
    fpsLabel->setColor(glm::vec4(0.0f, 0.8f, 0.0f, 1.0f));  // Яскраво-зелений щоб побачити!
    
    m_fpsLabel = fpsLabel.get();
    m_hudPanel->addChild(std::move(fpsLabel));
}

void PlayingState::onEnter() {
    std::cout << "[PlayingState] Entering game..." << std::endl;
    // Увімкнути захоплення миші
    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

void PlayingState::onExit() {
    std::cout << "[PlayingState] Exiting game..." << std::endl;
    // Вимкнути захоплення миші
    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void PlayingState::update(float deltaTime) {
    if (!m_engine) return;
    
    // Оновлення HUD
    if (m_hudPanel) {
        m_hudPanel->update(deltaTime);
    }
    
    // Підрахунок FPS
    m_frameCount++;
    m_fpsUpdateTimer += deltaTime;
    
    if (m_fpsUpdateTimer >= 0.5f) {  // Оновлюємо кожні 0.5 секунди
        m_currentFPS = m_frameCount / m_fpsUpdateTimer;
        m_frameCount = 0;
        m_fpsUpdateTimer = 0.0f;
        
        // Оновлюємо текст FPS label
        if (m_fpsLabel) {
            std::ostringstream oss;
            oss << "FPS: " << std::fixed << std::setprecision(1) << m_currentFPS;
            m_fpsLabel->setText(oss.str());
        }
    }
    
    // Перевірка ESC для паузи
    if (glfwGetKey(m_engine->getWindow().getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        pause();
    }
    
    // Оновлення гри (поки що нічого, Engine сам все робить)
}

void PlayingState::render() {
    // Рендеринг відбувається в Engine::drawFrame
}

void PlayingState::handleInput() {
    // Input обробляється в Engine::processInput
}

void PlayingState::pause() {
    m_nextState = GameState::PAUSED;
    m_shouldChangeState = true;
}
