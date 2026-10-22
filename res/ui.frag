#version 330 core

in vec2 uv;
in vec3 col;

out vec4 FragColor;

uniform sampler2D tex;

void main() {
    vec4 t = texture(tex, uv);
    FragColor = vec4(t.rgb*col, t.r);
}