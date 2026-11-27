#pragma once

#include "GameState.hpp"
#include "Engine.hpp"
#include "UI/UIPanel.hpp"
#include "UI/UIButton.hpp"
#include "UI/UILabel.hpp"
#include <array>
#include <functional>
#include <memory>
#include <string>

// Settings screen reachable from the main menu and the pause menu; changes apply immediately.
class SettingsState : public IGameState {
private:
    Engine* m_engine;
    GameState m_nextState = GameState::SETTINGS;
    GameState m_returnState = GameState::MAIN_MENU;
    bool m_shouldChangeState = false;

    std::unique_ptr<UIPanel> m_panel;
    UILabel* m_distanceValue = nullptr;
    UILabel* m_fpsValue = nullptr;
    UILabel* m_speedValue = nullptr;
    UIButton* m_vsyncButton = nullptr;
    std::array<UIButton*, Config::KEY_ACTION_COUNT> m_keyButtons{};

    int m_waitingAction = -1;  // key binding awaiting a key press

    void createUI();
    void refreshLabels();
    void stepRenderDistance(int delta);
    void stepFpsLimit(int delta);
    void stepSpeed(int delta);
    // "Name  [<] value [>]" row; returns the value label.
    UILabel* addStepper(float x, float y, const std::string& name, std::function<void(int)> onStep);
    void bindKey(int key);
    void goBack();

public:
    SettingsState(Engine* engine);
    ~SettingsState() override = default;

    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;
    void render() override {}
    void handleInput() override;

    GameState getNextState() const override { return m_nextState; }
    bool shouldChangeState() const override { return m_shouldChangeState; }

    // Over the pause menu the frozen world stays visible behind the panel.
    bool shouldRenderWorld() const override { return m_returnState == GameState::PAUSED; }
    UIElement* getUIRoot() override { return m_panel.get(); }
};
