#version 330 core

in vec2 localUV;
flat in vec4 tileRect;
in vec3 normal;
in vec3 fragPos;

out vec4 FragColor;

uniform sampler2D tex;

#define MAX_LIGHTS 16
uniform int uNumLights;
uniform vec3  uLightPos[MAX_LIGHTS];
uniform vec3  uLightColor[MAX_LIGHTS];   // color * intensity
uniform float uLightRadius[MAX_LIGHTS];

uniform vec3 ambient;

void main() {
    vec2 uv = tileRect.xy + fract(localUV) * tileRect.zw;
    vec4 albedo = texture(tex, uv);

    vec3 lit = ambient;
    vec3 n = normalize(normal);

    for(int i=0; i < uNumLights; i++) {
        vec3 toLight = uLightPos[i] - fragPos;
        float dist = length(toLight);
        if (dist > uLightRadius[i]) continue;

        float diff = max(dot(n, toLight / dist), 0.0);

        float x = dist / uLightRadius[i];
        float atten = clamp(1.0 - x * x, 0.0, 1.0);
        atten *= atten;

        lit += uLightColor[i] * diff * atten;
    }

    FragColor = vec4(albedo.rgb * lit, albedo.a);
}