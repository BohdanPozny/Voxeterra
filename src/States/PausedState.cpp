#include "States/PausedState.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

PausedState::PausedState(Engine* engine) 
    : m_engine(engine) {
}

void PausedState::onEnter() {
    std::cout << "[PausedState] Game paused" << std::endl;
    // Показати курсор
    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void PausedState::onExit() {
    std::cout << "[PausedState] Leaving pause menu" << std::endl;
}

void PausedState::update(float deltaTime) {
    // Поки що нічого
}

void PausedState::render() {
    // TODO: Рендерити pause меню поверх гри
    // Поки що просто консоль
    std::cout << "\n=== PAUSE MENU ===" << std::endl;
    std::cout << (m_selectedOption == 0 ? "> " : "  ") << "Resume" << std::endl;
    std::cout << (m_selectedOption == 1 ? "> " : "  ") << "Settings" << std::endl;
    std::cout << (m_selectedOption == 2 ? "> " : "  ") << "Main Menu" << std::endl;
}

void PausedState::handleInput() {
    if (!m_engine) return;
    
    GLFWwindow* window = m_engine->getWindow().getWindow();
    static bool upPressed = false;
    static bool downPressed = false;
    static bool enterPressed = false;
    static bool escPressed = false;
    
    // Навігація вгору
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        if (!upPressed) {
            m_selectedOption = (m_selectedOption - 1 + m_optionCount) % m_optionCount;
            upPressed = true;
        }
    } else {
        upPressed = false;
    }
    
    // Навігація вниз
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        if (!downPressed) {
            m_selectedOption = (m_selectedOption + 1) % m_optionCount;
            downPressed = true;
        }
    } else {
        downPressed = false;
    }
    
    // Вибір опції
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!enterPressed) {
            switch (m_selectedOption) {
                case 0:  // Resume
                    m_nextState = GameState::PLAYING;
                    m_shouldChangeState = true;
                    break;
                case 1:  // Settings
                    m_nextState = GameState::SETTINGS;
                    m_shouldChangeState = true;
                    break;
                case 2:  // Main Menu
                    m_nextState = GameState::MAIN_MENU;
                    m_shouldChangeState = true;
                    break;
            }
            enterPressed = true;
        }
    } else {
        enterPressed = false;
    }
    
    // ESC для Resume
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!escPressed) {
            m_nextState = GameState::PLAYING;
            m_shouldChangeState = true;
            escPressed = true;
        }
    } else {
        escPressed = false;
    }
}
