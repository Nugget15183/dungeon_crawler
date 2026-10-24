#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec2 aPosOffset;
layout (location = 3) in vec4 aCol;
layout (location = 4) in float aWorld; //1 = woldspace 0 = ui space

uniform float uAspect;

out vec2 uv;
out vec4 col;

void main() {
    float posScale = mix(1.0 / uAspect, 1.0, aWorld);

    gl_Position = vec4(aPos.x / uAspect + aPosOffset.x * posScale, aPos.y + aPosOffset.y, 0.0, 1.0);
    uv = aUV;
    col = aCol;
}