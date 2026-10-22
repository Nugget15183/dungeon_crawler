#version 330 core
layout (location = 0) in vec2 aCorner;
layout (location = 1) in vec2 aPos;
layout (location = 2) in float aSize;
layout (location = 3) in vec4 aCol;
layout (location = 4) in float aEmissive;

uniform float uAspect;

out vec4 col;
out vec2 ndc;
out float emissive;

void main() {
    vec2 off = aCorner * aSize;
    off.x /= uAspect;
    vec2 p = aPos + off;

    gl_Position = vec4(p, 0.0, 1.0);
    col = aCol;
    ndc = p;
    emissive = aEmissive;
}