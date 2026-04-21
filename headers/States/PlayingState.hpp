#pragma once

#include "GameState.hpp"
#include "Engine.hpp"
#include "UI/UIPanel.hpp"
#include "UI/UILabel.hpp"
#include <memory>

class PlayingState : public IGameState {
private:
    Engine* m_engine;  // non-owning reference
    GameState m_nextState = GameState::PLAYING;
    bool m_shouldChangeState = false;
    
    // FPS counter
    std::unique_ptr<UIPanel> m_hudPanel;
    UILabel* m_fpsLabel = nullptr;
    float m_fpsUpdateTimer = 0.0f;
    int m_frameCount = 0;
    float m_currentFPS = 0.0f;

    // F3 debug overlay
    UILabel* m_debugPos = nullptr;
    UILabel* m_debugDir = nullptr;
    UILabel* m_debugMisc = nullptr;
    UILabel* m_debugMesh = nullptr;
    UILabel* m_debugChunks = nullptr;
    bool m_debugVisible = false;

public:
    PlayingState(Engine* engine);
    ~PlayingState() override = default;
    
    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;
    void render() override;
    void handleInput() override;
    
    GameState getNextState() const override { return m_nextState; }
    bool shouldChangeState() const override { return m_shouldChangeState; }
    
    UIPanel* getHUDPanel() const { return m_hudPanel.get(); }

    void pause();

    // Polymorphic render hooks
    std::array<float, 4> getClearColor() const override {
        return {0.0f, 0.0f, 0.2f, 1.0f};  // dark-blue gameplay background
    }
    bool shouldRenderWorld() const override { return true; }
    UIElement* getUIRoot() override;
    void processGameplayInput(float deltaTime) override;
};
