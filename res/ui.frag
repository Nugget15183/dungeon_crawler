#version 330 core

in vec2 uv;

out vec4 FragColor;

uniform sampler2D tex;

void main() {
    vec4 t = texture(tex, uv);
    FragColor = vec4(vec3(1.0), t.r);
}