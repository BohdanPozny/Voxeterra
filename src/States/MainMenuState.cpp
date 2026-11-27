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
        glm::vec2(0.3f, 0.30f),
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
    
    auto settingsBtn = std::make_unique<UIButton>(
        glm::vec2(0.3f, 0.46f),
        glm::vec2(0.4f, 0.12f),
        "Settings",
        glm::vec4(0.25f, 0.35f, 0.70f, 1.0f),
        glm::vec4(0.35f, 0.47f, 0.90f, 1.0f),
        glm::vec4(0.15f, 0.25f, 0.50f, 1.0f)
    );
    settingsBtn->setOnClick([this]() {
        m_nextState = GameState::SETTINGS;
        m_shouldChangeState = true;
    });
    m_menuPanel->addChild(std::move(settingsBtn));

    // "Exit" button (centred, lower half).
    auto exitBtn = std::make_unique<UIButton>(
        glm::vec2(0.3f, 0.62f),
        glm::vec2(0.4f, 0.12f),
        "Exit",
        glm::vec4(0.8f, 0.2f, 0.2f, 1.0f),  // normal
        glm::vec4(1.0f, 0.3f, 0.3f, 1.0f),
        glm::vec4(0.6f, 0.1f, 0.1f, 1.0f)
    );
    exitBtn->setOnClick([this]() {
        std::cout << "[UI] Exit clicked! Closing game..." << std::endl;
        if (m_engine) {
            glfwSetWindowShouldClose(m_engine->getWindow().getWindow(), GLFW_TRUE);
        }
    });
    m_exitButton = exitBtn.get();
    m_menuPanel->addChild(std::move(exitBtn));
    
    std::cout << "[MainMenu] UI created: New Game, Settings, Exit" << std::endl;
}

void MainMenuState::onEnter() {
    // Clear any stale transition request from a previous visit.
    m_shouldChangeState = false;
    m_nextState = GameState::MAIN_MENU;

    // Menu needs a visible cursor for button clicks.
    if (m_engine && m_engine->getWindow().getWindow()) {
        m_engine->getInput().setCursorEnabled(true);
        glfwSetWindowTitle(m_engine->getWindow().getWindow(), "Voxterra");
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
    bool mouseDown = input.isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);

    // Buttons fire on release, so they need the held state rather than the edge.
    m_menuPanel->handleInput(normalizedMouse, mouseDown);
}

UIElement* MainMenuState::getUIRoot() {
    return m_menuPanel.get();
}
