#pragma once

#include "GameState.hpp"
#include "Engine.hpp"

class PausedState : public IGameState {
private:
    Engine* m_engine;
    GameState m_nextState = GameState::PAUSED;
    bool m_shouldChangeState = false;
    
    int m_selectedOption = 0;  // 0=Resume, 1=Settings, 2=Main Menu
    const int m_optionCount = 3;

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
};
