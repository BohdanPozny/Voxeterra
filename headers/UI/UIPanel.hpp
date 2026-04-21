#pragma once

#include "UI/UIElement.hpp"

// UIPanel - контейнер для інших UI елементів (НАСЛІДУВАННЯ + КОМПОЗИЦІЯ)
class UIPanel : public UIElement {
public:
    UIPanel(const glm::vec2& position, const glm::vec2& size, 
            const glm::vec4& color = glm::vec4(0.1f, 0.1f, 0.1f, 0.9f))
        : UIElement(position, size, color) {}
    
    // Реалізація віртуальних методів (ПОЛІМОРФІЗМ)
    void update(float deltaTime) override {
        if (!m_visible) return;
        
        // Оновлюємо всі дочірні елементи (КОМПОЗИЦІЯ)
        for (auto& child : m_children) {
            child->update(deltaTime);
        }
    }
    
    void render() override {
        if (!m_visible) return;
        
        // TODO: Рендеримо фон панелі
        
        // Рендеримо всі дочірні елементи (КОМПОЗИЦІЯ)
        for (auto& child : m_children) {
            child->render();
        }
    }
    
    void handleInput(const glm::vec2& mousePos, bool mousePressed) override {
        if (!m_enabled) return;
        
        // Передаємо input всім дочірнім елементам (КОМПОЗИЦІЯ)
        for (auto& child : m_children) {
            child->handleInput(mousePos, mousePressed);
        }
    }
};
