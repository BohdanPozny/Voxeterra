#include "States/SettingsState.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

SettingsState::SettingsState(Engine* engine, Config* config) 
    : m_engine(engine)
    , m_config(config) {
}

void SettingsState::onEnter() {
    std::cout << "[SettingsState] Entering settings" << std::endl;
    // Показати курсор
    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void SettingsState::onExit() {
    std::cout << "[SettingsState] Leaving settings" << std::endl;
    // Зберегти налаштування
    if (m_config) {
        m_config->save();
    }
}

void SettingsState::update(float deltaTime) {
    // Поки що нічого
}

void SettingsState::render() {
    // TODO: Рендерити settings меню
    // Поки що просто консоль
    if (!m_config) return;
    
    std::cout << "\n=== SETTINGS ===" << std::endl;
    std::cout << (m_selectedOption == 0 ? "> " : "  ") 
              << "FOV: " << m_config->getFOV() << std::endl;
    std::cout << (m_selectedOption == 1 ? "> " : "  ") 
              << "Render Distance: " << m_config->getRenderDistance() << std::endl;
    std::cout << (m_selectedOption == 2 ? "> " : "  ") 
              << "Mouse Sensitivity: " << m_config->getMouseSensitivity() << std::endl;
    std::cout << (m_selectedOption == 3 ? "> " : "  ") << "Back" << std::endl;
}

void SettingsState::handleInput() {
    if (!m_engine || !m_config) return;
    
    GLFWwindow* window = m_engine->getWindow().getWindow();
    static bool upPressed = false;
    static bool downPressed = false;
    static bool leftPressed = false;
    static bool rightPressed = false;
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
    
    // Зміна значень (ліво/право)
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        if (!leftPressed) {
            switch (m_selectedOption) {
                case 0:  // FOV
                    m_config->setFOV(std::max(30.0f, m_config->getFOV() - 5.0f));
                    break;
                case 1:  // Render Distance
                    m_config->setRenderDistance(std::max(2, m_config->getRenderDistance() - 1));
                    break;
                case 2:  // Mouse Sensitivity
                    m_config->setMouseSensitivity(std::max(0.01f, m_config->getMouseSensitivity() - 0.01f));
                    break;
            }
            leftPressed = true;
        }
    } else {
        leftPressed = false;
    }
    
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        if (!rightPressed) {
            switch (m_selectedOption) {
                case 0:  // FOV
                    m_config->setFOV(std::min(120.0f, m_config->getFOV() + 5.0f));
                    break;
                case 1:  // Render Distance
                    m_config->setRenderDistance(std::min(32, m_config->getRenderDistance() + 1));
                    break;
                case 2:  // Mouse Sensitivity
                    m_config->setMouseSensitivity(std::min(1.0f, m_config->getMouseSensitivity() + 0.01f));
                    break;
            }
            rightPressed = true;
        }
    } else {
        rightPressed = false;
    }
    
    // Enter або ESC для повернення
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!enterPressed && m_selectedOption == 3) {  // Back
            m_nextState = m_returnState;
            m_shouldChangeState = true;
            enterPressed = true;
        }
    } else {
        enterPressed = false;
    }
    
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!escPressed) {
            m_nextState = m_returnState;
            m_shouldChangeState = true;
            escPressed = true;
        }
    } else {
        escPressed = false;
    }
}
