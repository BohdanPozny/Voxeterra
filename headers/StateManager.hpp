#pragma once

#include "GameState.hpp"
#include <memory>
#include <map>

class StateManager {
private:
    std::map<GameState, std::unique_ptr<IGameState>> m_states;
    GameState m_currentState;
    GameState m_nextState;
    bool m_shouldChangeState = false;

public:
    StateManager();
    ~StateManager() = default;
    
    void registerState(GameState state, std::unique_ptr<IGameState> stateObj);
    void changeState(GameState newState);
    
    void update(float deltaTime);
    void render();
    void handleInput();
    
    GameState getCurrentState() const { return m_currentState; }
    IGameState* getState(GameState state) const {
        auto it = m_states.find(state);
        return (it != m_states.end()) ? it->second.get() : nullptr;
    }
};
