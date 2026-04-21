#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in uint inVoxelType;

layout(location = 0) out vec3 fragColor;

void main() {
    // Центруємо та масштабуємо
    vec3 pos = (inPosition - vec3(40.0, 4.0, 40.0)) * 0.02;
    
    // Проста ортогональна проекція: X,Y на екран
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
    
    // Колір залежить від висоти для діагностики
    if (inPosition.y > 5.0) {
        fragColor = vec3(1.0, 0.0, 0.0);  // червоний - верх
    } else {
        fragColor = vec3(0.0, 1.0, 0.0);  // зелений - низ
    }
}
