#pragma once

#include <array>

// Forward declarations
class UIElement;

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

    // Polymorphic render/input hooks consumed by Engine.

    // Clear colour (RGBA); defaults to a neutral menu grey.
    virtual std::array<float, 4> getClearColor() const {
        return {0.2f, 0.2f, 0.25f, 1.0f};
    }

    // Whether the 3D voxel world should be rendered beneath the UI.
    virtual bool shouldRenderWorld() const { return false; }

    // Optional UI root to draw over the scene (nullptr = no UI).
    virtual UIElement* getUIRoot() { return nullptr; }

    // Gameplay input hook (camera/movement). Only PLAYING overrides it.
    virtual void processGameplayInput(float /*deltaTime*/) {}
};
