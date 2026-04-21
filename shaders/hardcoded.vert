#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in uint inVoxelType;

layout(location = 0) out vec3 fragColor;

void main() {
    // ІГНОРУЄМО вхідні дані, малюємо hardcoded трикутник
    vec2 positions[3] = vec2[](
        vec2(0.0, -0.5),
        vec2(0.5, 0.5),
        vec2(-0.5, 0.5)
    );
    
    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);
    fragColor = vec3(1.0, 0.0, 0.0);  // червоний
}
