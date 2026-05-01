#include "States/PausedState.hpp"
#include <GLFW/glfw3.h>

PausedState::PausedState(Engine* engine)
    : m_engine(engine) {
    createUI();
}

void PausedState::createUI() {
    // Full-screen dim overlay so the frozen 3D scene shows through.
    m_menuPanel = std::make_unique<UIPanel>(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.55f)   // 55% black tint
    );

    // "PAUSED" title label.
    auto title = std::make_unique<UILabel>(
        glm::vec2(0.35f, 0.18f),
        glm::vec2(0.30f, 0.10f),
        "PAUSED",
        32,
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
    );
    m_menuPanel->addChild(std::move(title));

    // Button factory keeps the three buttons visually consistent.
    auto makeButton = [](float y, const std::string& text,
                         const glm::vec4& normal, const glm::vec4& hover, const glm::vec4& pressed) {
        return std::make_unique<UIButton>(
            glm::vec2(0.35f, y),
            glm::vec2(0.30f, 0.09f),
            text,
            normal, hover, pressed);
    };

    auto resumeBtn = makeButton(0.40f, "Resume",
        glm::vec4(0.20f, 0.65f, 0.25f, 1.0f),
        glm::vec4(0.30f, 0.85f, 0.35f, 1.0f),
        glm::vec4(0.10f, 0.50f, 0.15f, 1.0f));
    resumeBtn->setOnClick([this]() {
        m_nextState = GameState::PLAYING;
        m_shouldChangeState = true;
    });
    m_resumeButton = resumeBtn.get();
    m_menuPanel->addChild(std::move(resumeBtn));

    auto mainMenuBtn = makeButton(0.55f, "Main Menu",
        glm::vec4(0.70f, 0.25f, 0.25f, 1.0f),
        glm::vec4(0.90f, 0.35f, 0.35f, 1.0f),
        glm::vec4(0.50f, 0.15f, 0.15f, 1.0f));
    mainMenuBtn->setOnClick([this]() {
        m_nextState = GameState::MAIN_MENU;
        m_shouldChangeState = true;
    });
    m_mainMenuButton = mainMenuBtn.get();
    m_menuPanel->addChild(std::move(mainMenuBtn));
}

void PausedState::onEnter() {
    // Clear any stale transition request from a previous visit.
    m_shouldChangeState = false;
    m_nextState = GameState::PAUSED;

    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void PausedState::onExit() {
}

void PausedState::update(float deltaTime) {
    // Animate hover / pressed colour transitions.
    if (m_menuPanel) {
        m_menuPanel->update(deltaTime);
    }

    // ESC resumes the game (edge-triggered).
    if (m_engine && m_engine->getInput().isKeyPressed(GLFW_KEY_ESCAPE)) {
        m_nextState = GameState::PLAYING;
        m_shouldChangeState = true;
    }
}

void PausedState::render() {
    // Drawing happens in Engine::recordCommandBuffer via UIRenderer.
}

void PausedState::handleInput() {
    if (!m_engine || !m_menuPanel) return;

    auto& input = m_engine->getInput();
    glm::vec2 mouse = input.getNormalizedMousePosition();
    bool justClicked = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);

    m_menuPanel->handleInput(mouse, justClicked);
}
