#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec4 aRect;
layout (location = 3) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uViewProj;

out vec2 localUV;
flat out vec4 tileRect;
out vec3 normal;

void main() {
    gl_Position = uViewProj * uModel * vec4(aPos, 1.0);
    localUV = aUV;
    tileRect = aRect;

    //note: move to cpu later as a uniform
    //keep normals perpendiclar when under nonuniform scales
    normal = normalize(transpose(inverse(mat3(uModel))) * aNormal);
}