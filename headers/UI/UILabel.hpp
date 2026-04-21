#pragma once

#include "UI/UIElement.hpp"
#include <string>

// UILabel - текстовий елемент (НАСЛІДУВАННЯ від UIElement)
class UILabel : public UIElement {
private:
    std::string m_text;
    float m_fontSize;
    glm::vec4 m_textColor;

public:
    UILabel(const glm::vec2& position, const glm::vec2& size, 
            const std::string& text, float fontSize = 1.0f,
            const glm::vec4& textColor = glm::vec4(1.0f))
        : UIElement(position, size, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f))  // Прозорий фон
        , m_text(text)
        , m_fontSize(fontSize)
        , m_textColor(textColor) {}
    
    // Реалізація віртуальних методів (ПОЛІМОРФІЗМ)
    void update(float deltaTime) override {
        // Label статичний, нічого не робимо
    }
    
    void render() override {
        if (!m_visible) return;
        // TODO: Рендеринг тексту
    }
    
    void handleInput(const glm::vec2& mousePos, bool mousePressed) override {
        // Label не обробляє input
    }
    
    // Методи для роботи з текстом
    void setText(const std::string& text) { m_text = text; }
    std::string getText() const { return m_text; }
    void setTextColor(const glm::vec4& color) { m_textColor = color; }
    glm::vec4 getTextColor() const { return m_textColor; }
};
