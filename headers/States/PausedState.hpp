#pragma once

#include "GameState.hpp"
#include "Engine.hpp"
#include "UI/UIPanel.hpp"
#include "UI/UIButton.hpp"
#include "UI/UILabel.hpp"
#include <memory>

class PausedState : public IGameState {
private:
    Engine* m_engine;
    GameState m_nextState = GameState::PAUSED;
    bool m_shouldChangeState = false;

    // Owned widget tree (semi-transparent overlay + buttons).
    std::unique_ptr<UIPanel> m_menuPanel;
    UIButton* m_resumeButton   = nullptr;
    UIButton* m_mainMenuButton = nullptr;

    void createUI();

public:
    PausedState(Engine* engine);
    ~PausedState() override = default;

    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;
    void render() override;
    void handleInput() override;

    GameState getNextState() const override { return m_nextState; }
    bool shouldChangeState() const override { return m_shouldChangeState; }

    // Keep the frozen 3D world visible behind the pause overlay.
    bool shouldRenderWorld() const override { return true; }
    UIElement* getUIRoot() override { return m_menuPanel.get(); }
};
