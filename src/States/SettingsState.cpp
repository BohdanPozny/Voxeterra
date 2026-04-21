#include "States/SettingsState.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>

SettingsState::SettingsState(Engine* engine, Config* config) 
    : m_engine(engine)
    , m_config(config) {
}

void SettingsState::onEnter() {
    std::cout << "[SettingsState] Entering settings" << std::endl;
    // Show the cursor for menu navigation.
    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void SettingsState::onExit() {
    std::cout << "[SettingsState] Leaving settings" << std::endl;
    // Persist any changes when leaving the screen.
    if (m_config) {
        m_config->save();
    }
}

void SettingsState::update(float /*deltaTime*/) {
    // Nothing to tick per frame.
}

void SettingsState::render() {
    // Console-only placeholder until the UI version is implemented.
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

    auto& input = m_engine->getInput();

    // Edge-triggered vertical navigation.
    if (input.isKeyPressed(GLFW_KEY_UP) || input.isKeyPressed(GLFW_KEY_W)) {
        m_selectedOption = (m_selectedOption - 1 + m_optionCount) % m_optionCount;
    }
    if (input.isKeyPressed(GLFW_KEY_DOWN) || input.isKeyPressed(GLFW_KEY_S)) {
        m_selectedOption = (m_selectedOption + 1) % m_optionCount;
    }

    // Left/right adjusts the selected value.
    if (input.isKeyPressed(GLFW_KEY_LEFT) || input.isKeyPressed(GLFW_KEY_A)) {
        switch (m_selectedOption) {
            case 0: m_config->setFOV(std::max(30.0f, m_config->getFOV() - 5.0f)); break;
            case 1: m_config->setRenderDistance(std::max(2, m_config->getRenderDistance() - 1)); break;
            case 2: m_config->setMouseSensitivity(std::max(0.01f, m_config->getMouseSensitivity() - 0.01f)); break;
        }
    }
    if (input.isKeyPressed(GLFW_KEY_RIGHT) || input.isKeyPressed(GLFW_KEY_D)) {
        switch (m_selectedOption) {
            case 0: m_config->setFOV(std::min(120.0f, m_config->getFOV() + 5.0f)); break;
            case 1: m_config->setRenderDistance(std::min(32, m_config->getRenderDistance() + 1)); break;
            case 2: m_config->setMouseSensitivity(std::min(1.0f, m_config->getMouseSensitivity() + 0.01f)); break;
        }
    }

    // Enter on "Back" returns to the previous state.
    if ((input.isKeyPressed(GLFW_KEY_ENTER) || input.isKeyPressed(GLFW_KEY_SPACE))
        && m_selectedOption == 3) {
        m_nextState = m_returnState;
        m_shouldChangeState = true;
    }

    // ESC also returns.
    if (input.isKeyPressed(GLFW_KEY_ESCAPE)) {
        m_nextState = m_returnState;
        m_shouldChangeState = true;
    }
}
