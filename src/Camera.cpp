#include "Camera.hpp"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : m_position(position)
    , m_worldUp(up)
    , m_yaw(yaw)
    , m_pitch(pitch)
    , m_fov(45.0f)
    , m_aspectRatio(16.0f / 9.0f)
    , m_nearPlane(0.1f)
    , m_farPlane(4000.0f)
{
    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);
    
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    return glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
}

void Camera::processKeyboard(int direction, float deltaTime) {
    float velocity = 40.0f * deltaTime;
    
    // 0=Forward, 1=Backward, 2=Left, 3=Right, 4=Up, 5=Down
    if (direction == 0) {
        m_position += m_front * velocity;
    }
    if (direction == 1) {
        m_position -= m_front * velocity;
    }
    if (direction == 2) {
        m_position -= m_right * velocity;
    }
    if (direction == 3) {
        m_position += m_right * velocity;
    }
    if (direction == 4) {
        m_position += m_up * velocity;
    }
    if (direction == 5) {
        m_position -= m_up * velocity;
    }
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    m_yaw += xoffset;
    m_pitch += yoffset;

    // Clamp pitch to avoid gimbal flip.
    if (m_pitch > 89.0f)
        m_pitch = 89.0f;
    if (m_pitch < -89.0f)
        m_pitch = -89.0f;

    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset) {
    m_fov -= yoffset;
    if (m_fov < 1.0f)
        m_fov = 1.0f;
    if (m_fov > 90.0f)
        m_fov = 90.0f;
}
