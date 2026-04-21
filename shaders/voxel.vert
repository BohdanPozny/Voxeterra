#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in uint inVoxelType;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
    // Колір залежно від типу воксела - один воксель один колір
    if (inVoxelType == 1u) {  // STONE
        fragColor = vec3(0.5, 0.5, 0.5);  // сірий
    } else if (inVoxelType == 2u) {  // DIRT
        fragColor = vec3(0.55, 0.35, 0.2);  // коричневий
    } else if (inVoxelType == 3u) {  // GRASS
        fragColor = vec3(0.3, 0.7, 0.3);  // зелений
    } else if (inVoxelType == 4u) {  // SAND
        fragColor = vec3(0.9, 0.85, 0.6);  // жовтуватий
    } else if (inVoxelType == 5u) {  // WATER
        fragColor = vec3(0.2, 0.4, 0.8);  // синій
    } else {
        fragColor = vec3(1.0, 1.0, 1.0);  // білий (невідомий тип)
    }
    
    // Простий lighting на основі нормалі
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diffuse = max(dot(inNormal, lightDir), 0.3);  // ambient = 0.3
    fragColor *= diffuse;
    
    fragNormal = inNormal;
}
