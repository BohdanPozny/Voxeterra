#include "StateManager.hpp"
#include <iostream>

StateManager::StateManager() 
    : m_currentState(GameState::MAIN_MENU)
    , m_nextState(GameState::MAIN_MENU)
    , m_shouldChangeState(false) {
}

void StateManager::registerState(GameState state, std::unique_ptr<IGameState> stateObj) {
    m_states[state] = std::move(stateObj);
}

void StateManager::changeState(GameState newState) {
    m_nextState = newState;
    m_shouldChangeState = true;
}

void StateManager::update(float deltaTime) {
    static bool firstUpdate = true;
    
    // Fire onEnter for the initial state on the first update.
    if (firstUpdate) {
        if (m_states.find(m_currentState) != m_states.end()) {
            m_states[m_currentState]->onEnter();
        }
        firstUpdate = false;
    }
    
    // Handle pending state transition requested last frame.
    if (m_shouldChangeState) {
        if (m_states.find(m_currentState) != m_states.end()) {
            m_states[m_currentState]->onExit();
        }
        
        m_currentState = m_nextState;
        
        if (m_states.find(m_currentState) != m_states.end()) {
            m_states[m_currentState]->onEnter();
        }
        
        m_shouldChangeState = false;
    }
    
    // Drive the active state.
    if (m_states.find(m_currentState) != m_states.end()) {
        m_states[m_currentState]->update(deltaTime);
        
        // State may request a transition mid-update.
        if (m_states[m_currentState]->shouldChangeState()) {
            changeState(m_states[m_currentState]->getNextState());
        }
    }
}

void StateManager::render() {
    if (m_states.find(m_currentState) != m_states.end()) {
        m_states[m_currentState]->render();
    }
}

void StateManager::handleInput() {
    if (m_states.find(m_currentState) != m_states.end()) {
        m_states[m_currentState]->handleInput();
    }
}
