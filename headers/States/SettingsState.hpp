#pragma once

#include "GameState.hpp"
#include "Engine.hpp"
#include "Config.hpp"

class SettingsState : public IGameState {
private:
    Engine* m_engine;
    Config* m_config;
    GameState m_nextState = GameState::SETTINGS;
    GameState m_returnState = GameState::MAIN_MENU;  // Куди повертатися
    bool m_shouldChangeState = false;
    
    int m_selectedOption = 0;
    const int m_optionCount = 4;  // FOV, Render Distance, Mouse Sensitivity, Back

public:
    SettingsState(Engine* engine, Config* config);
    ~SettingsState() override = default;
    
    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;
    void render() override;
    void handleInput() override;
    
    GameState getNextState() const override { return m_nextState; }
    bool shouldChangeState() const override { return m_shouldChangeState; }
    
    void setReturnState(GameState state) { m_returnState = state; }
};
