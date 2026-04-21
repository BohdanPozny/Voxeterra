#pragma once

#include "GameState.hpp"
#include "Engine.hpp"
#include "UI/UIPanel.hpp"
#include "UI/UIButton.hpp"
#include "UI/UILabel.hpp"
#include <memory>

class MainMenuState : public IGameState {
private:
    Engine* m_engine;
    GameState m_nextState = GameState::MAIN_MENU;
    bool m_shouldChangeState = false;
    bool m_shouldExit = false;
    
    // UI елементи (ООП!)
    std::unique_ptr<UIPanel> m_menuPanel;
    UIButton* m_newGameButton = nullptr;
    UIButton* m_settingsButton = nullptr;
    UIButton* m_exitButton = nullptr;
    
    void createUI();

public:
    MainMenuState(Engine* engine);
    ~MainMenuState() override = default;
    
    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;
    void render() override;
    void handleInput() override;
    
    GameState getNextState() const override { return m_nextState; }
    bool shouldChangeState() const override { return m_shouldChangeState; }
    
    bool shouldExitGame() const { return m_shouldExit; }
    UIPanel* getMenuPanel() const { return m_menuPanel.get(); }
};
