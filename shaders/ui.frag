#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Font coverage is in R; solid quads sample a white texel of the atlas.
    float alpha = texture(texSampler, fragTexCoord).r;
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}
