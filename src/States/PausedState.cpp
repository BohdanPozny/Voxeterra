#include "States/PausedState.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

PausedState::PausedState(Engine* engine) 
    : m_engine(engine) {
}

void PausedState::onEnter() {
    std::cout << "[PausedState] Game paused" << std::endl;
    // Show the cursor for menu navigation.
    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void PausedState::onExit() {
    std::cout << "[PausedState] Leaving pause menu" << std::endl;
}

void PausedState::update(float /*deltaTime*/) {
    // No simulation while paused.
}

void PausedState::render() {
    // Console-only for now; a UI overlay is planned.
    std::cout << "\n=== PAUSE MENU ===" << std::endl;
    std::cout << (m_selectedOption == 0 ? "> " : "  ") << "Resume" << std::endl;
    std::cout << (m_selectedOption == 1 ? "> " : "  ") << "Settings" << std::endl;
    std::cout << (m_selectedOption == 2 ? "> " : "  ") << "Main Menu" << std::endl;
}

void PausedState::handleInput() {
    if (!m_engine) return;

    auto& input = m_engine->getInput();

    // Edge-triggered up navigation.
    if (input.isKeyPressed(GLFW_KEY_UP) || input.isKeyPressed(GLFW_KEY_W)) {
        m_selectedOption = (m_selectedOption - 1 + m_optionCount) % m_optionCount;
    }

    // Down navigation.
    if (input.isKeyPressed(GLFW_KEY_DOWN) || input.isKeyPressed(GLFW_KEY_S)) {
        m_selectedOption = (m_selectedOption + 1) % m_optionCount;
    }

    // Activate selected option.
    if (input.isKeyPressed(GLFW_KEY_ENTER) || input.isKeyPressed(GLFW_KEY_SPACE)) {
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
    }

    // ESC resumes the game.
    if (input.isKeyPressed(GLFW_KEY_ESCAPE)) {
        m_nextState = GameState::PLAYING;
        m_shouldChangeState = true;
    }
}
