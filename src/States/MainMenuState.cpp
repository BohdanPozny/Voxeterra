#include "States/MainMenuState.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

MainMenuState::MainMenuState(Engine* engine) 
    : m_engine(engine) {
    createUI();
}

void MainMenuState::createUI() {
    // Transparent root panel used as the widget container.
    m_menuPanel = std::make_unique<UIPanel>(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)
    );

    // "New Game" button (centred, upper half).
    auto newGameBtn = std::make_unique<UIButton>(
        glm::vec2(0.3f, 0.38f),
        glm::vec2(0.4f, 0.12f),
        "New Game",
        glm::vec4(0.2f, 0.8f, 0.2f, 1.0f),  // normal
        glm::vec4(0.3f, 1.0f, 0.3f, 1.0f),  // hover
        glm::vec4(0.1f, 0.6f, 0.1f, 1.0f)   // pressed
    );
    newGameBtn->setOnClick([this]() {
        std::cout << "[UI] New Game clicked! Starting game..." << std::endl;
        m_nextState = GameState::PLAYING;
        m_shouldChangeState = true;
    });
    m_newGameButton = newGameBtn.get();
    m_menuPanel->addChild(std::move(newGameBtn));
    
    // "Exit" button (centred, lower half).
    auto exitBtn = std::make_unique<UIButton>(
        glm::vec2(0.3f, 0.54f),
        glm::vec2(0.4f, 0.12f),
        "Exit",
        glm::vec4(0.8f, 0.2f, 0.2f, 1.0f),  // normal
        glm::vec4(1.0f, 0.3f, 0.3f, 1.0f),
        glm::vec4(0.6f, 0.1f, 0.1f, 1.0f)
    );
    exitBtn->setOnClick([this]() {
        std::cout << "[UI] Exit clicked! Closing game..." << std::endl;
        m_shouldExit = true;
        if (m_engine) {
            glfwSetWindowShouldClose(m_engine->getWindow().getWindow(), GLFW_TRUE);
        }
    });
    m_exitButton = exitBtn.get();
    m_menuPanel->addChild(std::move(exitBtn));
    
    std::cout << "[MainMenu] UI created: 2 centered buttons (New Game, Exit)" << std::endl;
}

void MainMenuState::onEnter() {
    std::cout << "[MainMenuState] Entering main menu with UI" << std::endl;
    std::cout << "=== VOXTERRA MAIN MENU ===" << std::endl;
    std::cout << "Click in the CENTER of window:" << std::endl;
    std::cout << "  - Upper area = NEW GAME (green)" << std::endl;
    std::cout << "  - Lower area = EXIT (red)" << std::endl;
    
    // Menu needs a visible cursor for button clicks.
    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetWindowTitle(m_engine->getWindow().getWindow(), "Voxterra - MAIN MENU (Click center: top=Play, bottom=Exit)");
    }
}

void MainMenuState::onExit() {
    std::cout << "[MainMenuState] Leaving main menu" << std::endl;
}

void MainMenuState::update(float deltaTime) {
    if (m_menuPanel) {
        m_menuPanel->update(deltaTime);
    }
}

void MainMenuState::render() {
    // Rendering is performed by Engine via UIRenderer::renderUI().
}

void MainMenuState::handleInput() {
    if (!m_engine || !m_menuPanel) return;

    auto& input = m_engine->getInput();
    glm::vec2 normalizedMouse = input.getNormalizedMousePosition();
    bool justClicked = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);

    // Dispatch to the widget tree (edge-triggered click).
    m_menuPanel->handleInput(normalizedMouse, justClicked);
}

UIElement* MainMenuState::getUIRoot() {
    return m_menuPanel.get();
}
