#version 310 es

struct Light
{
    vec3 Position;
    float Radius;
    vec4 Color;
};

uniform vec4 UBO[12];
layout(location = 0) in vec4 aVertex;
layout(location = 0) out vec4 vColor;
layout(location = 1) in vec3 aNormal;

void main()
{
    gl_Position = mat4(UBO[0], UBO[1], UBO[2], UBO[3]) * aVertex;
    vColor = vec4(0.0);
    for (int i = 0; i < 4; i++)
    {
        Light light;
        light.Position = Light(UBO[i * 2 + 4].xyz, UBO[i * 2 + 4].w, UBO[i * 2 + 5]).Position;
        light.Radius = Light(UBO[i * 2 + 4].xyz, UBO[i * 2 + 4].w, UBO[i * 2 + 5]).Radius;
        light.Color = Light(UBO[i * 2 + 4].xyz, UBO[i * 2 + 4].w, UBO[i * 2 + 5]).Color;
        vec3 L = aVertex.xyz - light.Position;
        vColor += (((UBO[i * 2 + 5]) * clamp(1.0 - (length(L) / light.Radius), 0.0, 1.0)) * dot(aNormal, normalize(L)));
    }
}

