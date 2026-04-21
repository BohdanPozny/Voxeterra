#include "States/PlayingState.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

PlayingState::PlayingState(Engine* engine) 
    : m_engine(engine) {
    
    // Transparent HUD root panel.
    m_hudPanel = std::make_unique<UIPanel>(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)
    );

    // FPS label pinned to the top-left corner.
    auto fpsLabel = std::make_unique<UILabel>(
        glm::vec2(0.01f, 0.01f),
        glm::vec2(0.12f, 0.04f),
        "FPS: 0",
        16,
        glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)
    );
    // Semi-transparent backdrop for readability.
    fpsLabel->setColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
    
    m_fpsLabel = fpsLabel.get();
    m_hudPanel->addChild(std::move(fpsLabel));

    // Debug overlay labels (hidden by default, toggled with F3).
    auto makeDebugLabel = [](float y) {
        auto lbl = std::make_unique<UILabel>(
            glm::vec2(0.01f, y),
            glm::vec2(0.35f, 0.035f),
            "",
            14,
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
        );
        lbl->setColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
        lbl->setVisible(false);
        return lbl;
    };

    auto posLbl = makeDebugLabel(0.06f);
    m_debugPos = posLbl.get();
    m_hudPanel->addChild(std::move(posLbl));

    auto dirLbl = makeDebugLabel(0.10f);
    m_debugDir = dirLbl.get();
    m_hudPanel->addChild(std::move(dirLbl));

    auto miscLbl = makeDebugLabel(0.14f);
    m_debugMisc = miscLbl.get();
    m_hudPanel->addChild(std::move(miscLbl));

    auto meshLbl = makeDebugLabel(0.18f);
    m_debugMesh = meshLbl.get();
    m_hudPanel->addChild(std::move(meshLbl));

    auto chunksLbl = makeDebugLabel(0.22f);
    m_debugChunks = chunksLbl.get();
    m_hudPanel->addChild(std::move(chunksLbl));
}

void PlayingState::onEnter() {
    std::cout << "[PlayingState] Entering game..." << std::endl;
    // Capture the cursor for FPS-style camera control.
    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

void PlayingState::onExit() {
    std::cout << "[PlayingState] Exiting game..." << std::endl;
    // Release the cursor.
    if (m_engine && m_engine->getWindow().getWindow()) {
        glfwSetInputMode(m_engine->getWindow().getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void PlayingState::update(float deltaTime) {
    if (!m_engine) return;

    // Stream chunks in/out around the camera.
    m_engine->getWorldRenderer().updateStreaming(
        m_engine->getCamera().getPosition(),
        m_engine->getConfig().getRenderDistance());

    // HUD update.
    if (m_hudPanel) {
        m_hudPanel->update(deltaTime);
    }
    
    // FPS accumulator.
    m_frameCount++;
    m_fpsUpdateTimer += deltaTime;
    
    if (m_fpsUpdateTimer >= 0.5f) {  // refresh the FPS label twice a second
        m_currentFPS = m_frameCount / m_fpsUpdateTimer;
        m_frameCount = 0;
        m_fpsUpdateTimer = 0.0f;
        
        // Update FPS label text.
        if (m_fpsLabel) {
            std::ostringstream oss;
            oss << "FPS: " << std::fixed << std::setprecision(1) << m_currentFPS;
            m_fpsLabel->setText(oss.str());
        }
    }
    
    // ESC opens the pause menu.
    if (glfwGetKey(m_engine->getWindow().getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        pause();
    }

    // F3 - toggle debug overlay (edge detection)
    if (m_engine->getInput().isKeyPressed(GLFW_KEY_F3)) {
        m_debugVisible = !m_debugVisible;
        if (m_debugPos)    m_debugPos->setVisible(m_debugVisible);
        if (m_debugDir)    m_debugDir->setVisible(m_debugVisible);
        if (m_debugMisc)   m_debugMisc->setVisible(m_debugVisible);
        if (m_debugMesh)   m_debugMesh->setVisible(m_debugVisible);
        if (m_debugChunks) m_debugChunks->setVisible(m_debugVisible);
        std::cout << "[F3] Debug overlay: " << (m_debugVisible ? "ON" : "OFF") << std::endl;
    }

    // Refresh debug overlay text only while it is visible.
    if (m_debugVisible) {
        const auto& cam = m_engine->getCamera();
        glm::vec3 pos = cam.getPosition();
        glm::vec3 dir = cam.getFront();

        if (m_debugPos) {
            std::ostringstream oss;
            oss << "XYZ: " << std::fixed << std::setprecision(2)
                << pos.x << "  " << pos.y << "  " << pos.z;
            m_debugPos->setText(oss.str());
        }
        if (m_debugDir) {
            std::ostringstream oss;
            oss << "Dir: " << std::fixed << std::setprecision(2)
                << dir.x << " " << dir.y << " " << dir.z;
            m_debugDir->setText(oss.str());
        }
        if (m_debugMisc) {
            int cx = static_cast<int>(std::floor(pos.x / 64.0f));
            int cz = static_cast<int>(std::floor(pos.z / 64.0f));
            std::ostringstream oss;
            oss << "Chunk: " << cx << ", " << cz
                << "  FOV: " << m_engine->getConfig().getFOV();
            m_debugMisc->setText(oss.str());
        }

        auto& wr = m_engine->getWorldRenderer();
        if (m_debugMesh) {
            std::ostringstream oss;
            oss << "Verts: " << wr.getTotalVertices()
                << "  Tris: " << wr.getTotalTriangles();
            m_debugMesh->setText(oss.str());
        }
        if (m_debugChunks) {
            std::ostringstream oss;
            oss << "Chunks: " << wr.getVisibleChunks()
                << " / " << wr.getChunkCount() << " (vis/loaded)";
            m_debugChunks->setText(oss.str());
        }
    }
    
    // Gameplay update (Engine drives rendering / input itself for now).
}

void PlayingState::render() {
    // Engine::drawFrame performs the actual rendering.
}

void PlayingState::handleInput() {
    // Input is handled by Engine::processInput.
}

void PlayingState::pause() {
    m_nextState = GameState::PAUSED;
    m_shouldChangeState = true;
}

UIElement* PlayingState::getUIRoot() {
    return m_hudPanel.get();
}

void PlayingState::processGameplayInput(float deltaTime) {
    if (m_engine) {
        // Camera input is currently driven from Engine::processInput().
        // Kept here as a hook so the state can own gameplay input later.
        (void)deltaTime;
    }
}
