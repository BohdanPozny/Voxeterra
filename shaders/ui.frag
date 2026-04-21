#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Якщо texCoord = (0,0) - малюємо тільки колір (без текстури)
    // Інакше - семплюємо текстуру і множимо на колір
    if (fragTexCoord.x < 0.001 && fragTexCoord.y < 0.001) {
        outColor = fragColor;
    } else {
        float alpha = texture(texSampler, fragTexCoord).r;  // Шрифт в R каналі
        outColor = vec4(fragColor.rgb, fragColor.a * alpha);
    }
}
