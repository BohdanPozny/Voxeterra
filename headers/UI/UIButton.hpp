#pragma once

#include "UI/UIElement.hpp"
#include <string>
#include <functional>

// Clickable button with hover / pressed state tracking.
class UIButton : public UIElement {
private:
    std::string m_text;
    glm::vec4 m_normalColor;
    glm::vec4 m_hoverColor;
    glm::vec4 m_pressedColor;
    std::function<void()> m_onClick;
    bool m_wasPressed = false;

public:
    UIButton(const glm::vec2& position, const glm::vec2& size, 
             const std::string& text,
             const glm::vec4& normalColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
             const glm::vec4& hoverColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f),
             const glm::vec4& pressedColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f))
        : UIElement(position, size, normalColor)
        , m_text(text)
        , m_normalColor(normalColor)
        , m_hoverColor(hoverColor)
        , m_pressedColor(pressedColor) {}
    
    void update(float /*deltaTime*/) override {
        // Pick the colour that matches the current interaction state.
        if (!m_enabled) {
            m_color = glm::vec4(0.2f, 0.2f, 0.2f, 0.5f);  // Disabled
        } else if (m_wasPressed) {
            m_color = m_pressedColor;
        } else if (m_hovered) {
            m_color = m_hoverColor;
        } else {
            m_color = m_normalColor;
        }
    }
    
    // Drawing is performed by UIRenderer, which walks the widget tree.
    void render() override {}
    
    void handleInput(const glm::vec2& mousePos, bool mousePressed) override {
        if (!m_enabled) return;
        
        m_hovered = contains(mousePos);
        
        if (m_hovered && mousePressed && !m_wasPressed) {
            m_wasPressed = true;
        } else if (!mousePressed && m_wasPressed) {
            if (m_hovered && m_onClick) m_onClick();
            m_wasPressed = false;
        }
    }
    void setOnClick(std::function<void()> callback) { m_onClick = callback; }
    void setText(const std::string& text) { m_text = text; }
    std::string getText() const { return m_text; }
};
