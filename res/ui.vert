#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec2 aPosOffset;
layout (location = 3) in vec4 aCol;

out vec2 uv;
out vec4 col;

void main() {
    gl_Position = vec4(aPos+vec3(aPosOffset, 0), 1.0);
    uv = aUV;
    col=aCol;
}