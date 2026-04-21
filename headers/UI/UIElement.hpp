#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

// Abstract base for every UI widget.
class UIElement {
protected:
    glm::vec2 m_position;  // normalised screen coords [0,1]
    glm::vec2 m_size;      // normalised size [0,1]
    glm::vec4 m_color;     // background colour (RGBA)
    bool m_visible = true;
    bool m_enabled = true;
    bool m_hovered = false;
    bool m_focused = false;
    
    UIElement* m_parent = nullptr;
    std::vector<std::unique_ptr<UIElement>> m_children;

public:
    UIElement(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
        : m_position(position), m_size(size), m_color(color) {}
    
    virtual ~UIElement() = default;
    
    // Polymorphic interface:
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    virtual void handleInput(const glm::vec2& mousePos, bool mousePressed) = 0;
    
    // Accessors.
    void setPosition(const glm::vec2& pos) { m_position = pos; }
    void setSize(const glm::vec2& size) { m_size = size; }
    void setColor(const glm::vec4& color) { m_color = color; }
    void setVisible(bool visible) { m_visible = visible; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    glm::vec2 getPosition() const { return m_position; }
    glm::vec2 getSize() const { return m_size; }
    glm::vec4 getColor() const { return m_color; }
    bool isVisible() const { return m_visible; }
    bool isEnabled() const { return m_enabled; }
    bool isHovered() const { return m_hovered; }
    bool isFocused() const { return m_focused; }
    
    // Children own their subtree; parent pointer is non-owning.
    void addChild(std::unique_ptr<UIElement> child) {
        child->m_parent = this;
        m_children.push_back(std::move(child));
    }
    
    const std::vector<std::unique_ptr<UIElement>>& getChildren() const {
        return m_children;
    }
    
    // Hit test in normalised screen coords.
    bool contains(const glm::vec2& point) const {
        return point.x >= m_position.x && point.x <= m_position.x + m_size.x &&
               point.y >= m_position.y && point.y <= m_position.y + m_size.y;
    }
};
