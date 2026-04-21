#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // texCoord == (0,0) is the convention for "no texture"; otherwise
    // sample the font atlas and modulate the colour by its alpha.
    if (fragTexCoord.x < 0.001 && fragTexCoord.y < 0.001) {
        outColor = fragColor;
    } else {
        float alpha = texture(texSampler, fragTexCoord).r;  // font coverage is in R
        outColor = vec4(fragColor.rgb, fragColor.a * alpha);
    }
}
