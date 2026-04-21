# Camera

## Огляд

`Camera` реалізує FPS-стиль камери з підтримкою руху клавіатурою та огляду мишею. Використовує GLM для матриць view та projection.

## API

```cpp
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera() = default;

    void init(const glm::vec3& position);

    // Оновлення
    void update(float deltaTime);

    // Матриці
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

    // Ввід
    void processKeyboard(int key, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset, 
                               bool constrainPitch = true);

    // Getters
    glm::vec3 getPosition() const { return m_position; }
    glm::vec3 getFront() const { return m_front; }
    glm::vec3 getUp() const { return m_up; }
    float getYaw() const { return m_yaw; }
    float getPitch() const { return m_pitch; }

    // Setters
    void setPosition(const glm::vec3& pos) { m_position = pos; }
    void setMovementSpeed(float speed) { m_movementSpeed = speed; }
    void setMouseSensitivity(float sens) { m_mouseSensitivity = sens; }
    void setFOV(float fov) { m_fov = fov; }

private:
    // Позиція та орієнтація
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    // Кути Ейлера
    float m_yaw = -90.0f;    // Початково дивимося на -Z
    float m_pitch = 0.0f;

    // Налаштування
    float m_movementSpeed = 10.0f;
    float m_mouseSensitivity = 0.1f;
    float m_fov = 70.0f;

    // Розміри вікна (для projection)
    float m_aspectRatio = 800.0f / 600.0f;

    void updateCameraVectors();
};
```

## Ініціалізація

```cpp
void Camera::init(const glm::vec3& position) {
    m_position = position;
    m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    m_yaw = -90.0f;  // Дивимося вздовж -Z
    m_pitch = 0.0f;

    updateCameraVectors();
}
```

## Оновлення векторів камери

```cpp
void Camera::updateCameraVectors() {
    // Розраховуємо front вектор з кутів Ейлера
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);

    // Перераховуємо right та up
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}
```

## Матриці

### View Matrix

```cpp
glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position,           // Позиція камери
                       m_position + m_front, // Куди дивимося
                       m_up);                // Вектор вгору
}
```

### Projection Matrix

```cpp
glm::mat4 Camera::getProjectionMatrix() const {
    return glm::perspective(glm::radians(m_fov),  // FOV
                          m_aspectRatio,          // Aspect ratio
                          0.1f,                   // Near plane
                          1000.0f);               // Far plane
}
```

## Обробка клавіатури

```cpp
void Camera::processKeyboard(int key, float deltaTime) {
    float velocity = m_movementSpeed * deltaTime;

    switch (key) {
        case GLFW_KEY_W:  // Вперед
            m_position += m_front * velocity;
            break;
        case GLFW_KEY_S:  // Назад
            m_position -= m_front * velocity;
            break;
        case GLFW_KEY_A:  // Вліво
            m_position -= m_right * velocity;
            break;
        case GLFW_KEY_D:  // Вправо
            m_position += m_right * velocity;
            break;
        case GLFW_KEY_SPACE:  // Вгору
            m_position += m_worldUp * velocity;
            break;
        case GLFW_KEY_LEFT_SHIFT:  // Вниз
            m_position -= m_worldUp * velocity;
            break;
    }
}
```

## Обробка миші

```cpp
void Camera::processMouseMovement(float xOffset, float yOffset, 
                                   bool constrainPitch) {
    xOffset *= m_mouseSensitivity;
    yOffset *= m_mouseSensitivity;

    m_yaw += xOffset;
    m_pitch += yOffset;

    // Обмеження pitch щоб уникнути перевертання
    if (constrainPitch) {
        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;
    }

    updateCameraVectors();
}
```

## Координатна система

```
        Y (вгору)
        │
        │    Z (назад)
        │   /
        │  /
        │ /
        └────────── X (праворуч)
```

| Напрям | Клавіша | Вектор |
|--------|---------|--------|
| Вперед | W | +m_front |
| Назад | S | -m_front |
| Вліво | A | -m_right |
| Вправо | D | +m_right |
| Вгору | Space | +m_worldUp |
| Вниз | Shift | -m_worldUp |

## Параметри

| Параметр | За замовчуванням | Опис |
|----------|-----------------|------|
| `m_fov` | 70.0f | Field of view (градуси) |
| `m_movementSpeed` | 10.0f | Швидкість руху (одиниць/с) |
| `m_mouseSensitivity` | 0.1f | Чутливість миші |
| `m_yaw` | -90.0f | Горизонтальний кут |
| `m_pitch` | 0.0f | Вертикальний кут |

## Використання в Engine

```cpp
void Engine::processInput(float deltaTime) {
    // Рух
    if (m_input.isKeyDown(GLFW_KEY_W))
        m_camera.processKeyboard(GLFW_KEY_W, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_S))
        m_camera.processKeyboard(GLFW_KEY_S, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_A))
        m_camera.processKeyboard(GLFW_KEY_A, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_D))
        m_camera.processKeyboard(GLFW_KEY_D, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_SPACE))
        m_camera.processKeyboard(GLFW_KEY_SPACE, deltaTime);
    if (m_input.isKeyDown(GLFW_KEY_LEFT_SHIFT))
        m_camera.processKeyboard(GLFW_KEY_LEFT_SHIFT, deltaTime);

    // Огляд мишою
    glm::vec2 mouseDelta = m_input.getMouseDelta();
    if (mouseDelta.x != 0 || mouseDelta.y != 0) {
        m_camera.processMouseMovement(mouseDelta.x, -mouseDelta.y);
    }
}
```

## Дивіться також

- [[input/index\|Input Manager]] — джерело вводу
- [[vulkan/WorldRenderer\|WorldRenderer]] — використовує camera matrices
