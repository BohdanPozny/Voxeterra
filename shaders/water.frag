#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    // Subtle directional shading so water faces remain distinguishable.
    float ndl = max(dot(normalize(fragNormal), normalize(vec3(0.4, 1.0, 0.3))), 0.0);
    vec3 tinted = fragColor * (0.7 + 0.3 * ndl);
    outColor = vec4(tinted, 0.55);
}
