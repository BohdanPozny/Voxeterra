#pragma once

enum class GameState {
    MAIN_MENU,
    PLAYING,
    PAUSED,
    SETTINGS
};

class IGameState {
public:
    virtual ~IGameState() = default;
    
    virtual void onEnter() = 0;
    virtual void onExit() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    virtual void handleInput() = 0;
    
    virtual GameState getNextState() const = 0;
    virtual bool shouldChangeState() const = 0;
};
