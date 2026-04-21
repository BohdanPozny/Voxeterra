#pragma once

#include "UI/UIElement.hpp"

// Container element: propagates update/input/render to its children.
class UIPanel : public UIElement {
public:
    UIPanel(const glm::vec2& position, const glm::vec2& size, 
            const glm::vec4& color = glm::vec4(0.1f, 0.1f, 0.1f, 0.9f))
        : UIElement(position, size, color) {}
    
    void update(float deltaTime) override {
        if (!m_visible) return;
        for (auto& child : m_children) child->update(deltaTime);
    }

    // UIRenderer is responsible for drawing the panel and its subtree.
    void render() override {}

    void handleInput(const glm::vec2& mousePos, bool mousePressed) override {
        if (!m_enabled) return;
        for (auto& child : m_children) child->handleInput(mousePos, mousePressed);
    }
};
