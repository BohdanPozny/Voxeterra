#pragma once

#include "UI/UIElement.hpp"
#include <string>
#include <functional>

// UIButton - кнопка з callback (НАСЛІДУВАННЯ від UIElement)
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
    
    // Реалізація віртуальних методів (ПОЛІМОРФІЗМ)
    void update(float deltaTime) override {
        // Оновлення кольору в залежності від стану
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
    
    void render() override {
        if (!m_visible) return;
        // TODO: Рендеринг кнопки через Vulkan
    }
    
    void handleInput(const glm::vec2& mousePos, bool mousePressed) override {
        if (!m_enabled) return;
        
        m_hovered = contains(mousePos);
        
        if (m_hovered && mousePressed && !m_wasPressed) {
            m_wasPressed = true;
        } else if (!mousePressed && m_wasPressed) {
            // Click event
            if (m_hovered && m_onClick) {
                m_onClick();
            }
            m_wasPressed = false;
        }
    }
    
    // Специфічні методи для Button
    void setOnClick(std::function<void()> callback) { m_onClick = callback; }
    void setText(const std::string& text) { m_text = text; }
    std::string getText() const { return m_text; }
};
