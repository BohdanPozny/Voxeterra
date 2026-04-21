#include "States/MainMenuState.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

MainMenuState::MainMenuState(Engine* engine) 
    : m_engine(engine) {
    createUI();
}

void MainMenuState::createUI() {
    // Створюємо панель меню (невидима - тільки контейнер)
    m_menuPanel = std::make_unique<UIPanel>(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)  // Прозора
    );
    
    // Кнопка "New Game" - маленька, по центру вгорі
    auto newGameBtn = std::make_unique<UIButton>(
        glm::vec2(0.4f, 0.4f),   // Центр по X, трохи вище центру
        glm::vec2(0.2f, 0.08f),  // Ширина 20%, висота 8%
        "New Game",
        glm::vec4(0.2f, 0.8f, 0.2f, 1.0f),  // Яскраво-зелений
        glm::vec4(0.3f, 1.0f, 0.3f, 1.0f),  // Світліший при hover
        glm::vec4(0.1f, 0.6f, 0.1f, 1.0f)   // Темніший при press
    );
    newGameBtn->setOnClick([this]() {
        std::cout << "[UI] New Game clicked! Starting game..." << std::endl;
        m_nextState = GameState::PLAYING;
        m_shouldChangeState = true;
    });
    m_newGameButton = newGameBtn.get();
    m_menuPanel->addChild(std::move(newGameBtn));
    
    // Кнопка "Exit" - маленька, по центру внизу
    auto exitBtn = std::make_unique<UIButton>(
        glm::vec2(0.4f, 0.52f),  // Центр по X, трохи нижче
        glm::vec2(0.2f, 0.08f),  // Така ж ширина і висота
        "Exit",
        glm::vec4(0.8f, 0.2f, 0.2f, 1.0f),  // Яскраво-червоний
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
    
    // Показати курсор для кліків
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
    // Рендеримо UI панель з кнопками
    if (m_menuPanel && m_engine) {
        // TODO: Інтегрувати з UIRenderer
        // UI рендериться через callbacks
    }
}

void MainMenuState::handleInput() {
    if (!m_engine || !m_menuPanel) return;
    
    GLFWwindow* window = m_engine->getWindow().getWindow();
    
    // Отримуємо позицію миші
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    
    // Конвертуємо в normalized координати (0-1)
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glm::vec2 normalizedMouse(
        static_cast<float>(mouseX) / width,
        static_cast<float>(mouseY) / height
    );
    
    // Перевіряємо клік
    static bool wasPressed = false;
    bool isPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    
    // Передаємо input в UI систему
    m_menuPanel->handleInput(normalizedMouse, isPressed && !wasPressed);
    
    wasPressed = isPressed;
}
