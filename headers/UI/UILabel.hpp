#pragma once

#include "UI/UIElement.hpp"
#include <string>

// Static text label.
class UILabel : public UIElement {
private:
    std::string m_text;
    float m_fontSize;
    glm::vec4 m_textColor;

public:
    UILabel(const glm::vec2& position, const glm::vec2& size, 
            const std::string& text, float fontSize = 1.0f,
            const glm::vec4& textColor = glm::vec4(1.0f))
        : UIElement(position, size, glm::vec4(0.0f))
        , m_text(text)
        , m_fontSize(fontSize)
        , m_textColor(textColor) {}
    
    // Label has no animation or input behaviour; UIRenderer handles drawing.
    void update(float) override {}
    void render() override {}
    void handleInput(const glm::vec2&, bool) override {}
    void setText(const std::string& text) { m_text = text; }
    std::string getText() const { return m_text; }
    void setTextColor(const glm::vec4& color) { m_textColor = color; }
    glm::vec4 getTextColor() const { return m_textColor; }
};
