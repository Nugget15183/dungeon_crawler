#version 330 core

in vec4 col;
in vec2 ndc;
in float emissive;

out vec4 FragColor;

uniform float uAspect;

#define MAX_LIGHTS 16
uniform int   uNumLights;
uniform vec2  uLightPos[MAX_LIGHTS];
uniform vec3  uLightColor[MAX_LIGHTS];
uniform float uLightRadius[MAX_LIGHTS];
uniform vec3  uAmbient;

void main() {
    vec3 lit = uAmbient;

    for (int i = 0; i < uNumLights; i++) {
        vec2 d = (uLightPos[i] - ndc) * vec2(uAspect, 1.0); //correct for aspect
        float dist = length(d);

        if (dist > uLightRadius[i]) continue;

        float x = dist / uLightRadius[i];
        float atten = clamp(1.0 - x * x, 0.0, 1.0);
        atten *= atten;

        lit += uLightColor[i] * atten;
    }

    vec3 shaded = col.rgb * lit;
    FragColor = vec4(mix(shaded, col.rgb, emissive), col.a);
}