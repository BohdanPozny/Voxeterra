#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Column origin in voxels (xyz) and cell size (w) for the current draw.
layout(push_constant) uniform Push {
    vec4 originScale;
} pc;

// bits 0-6 x, 7-13 z, 14-22 y, 23-25 normal, 26-31 voxel type (see World/Mesher.hpp)
layout(location = 0) in uint inData;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

const vec3 kNormals[6] = vec3[](
    vec3( 1, 0, 0), vec3(-1, 0, 0),
    vec3( 0, 1, 0), vec3( 0,-1, 0),
    vec3( 0, 0, 1), vec3( 0, 0,-1));

// Indexed by VoxelTypes: air, stone, dirt, grass, sand, water.
const vec3 kColors[6] = vec3[](
    vec3(1.0, 1.0, 1.0),
    vec3(0.5, 0.5, 0.5),
    vec3(0.55, 0.35, 0.2),
    vec3(0.3, 0.7, 0.3),
    vec3(0.9, 0.85, 0.6),
    vec3(0.2, 0.4, 0.8));

void main() {
    vec3 cell = vec3(float(inData & 127u), float((inData >> 14) & 511u), float((inData >> 7) & 127u));
    vec3 voxelPos = pc.originScale.xyz + cell * pc.originScale.w;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(voxelPos, 1.0);

    vec3 normal = kNormals[(inData >> 23) & 7u];
    vec3 color = kColors[min(inData >> 26, 5u)];

    // Basic diffuse term using the face normal.
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diffuse = max(dot(normal, lightDir), 0.3);  // ambient = 0.3

    fragColor = color * diffuse;
    fragNormal = normal;
}
