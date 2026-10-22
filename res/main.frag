#version 330 core

in vec2 localUV;
flat in vec4 tileRect;
in vec3 normal;

out vec4 FragColor;

uniform sampler2D tex;

void main() {
    vec2 uv = tileRect.xy + fract(localUV) * tileRect.zw;
    FragColor = texture(tex, uv);
}