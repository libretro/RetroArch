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
    for (int _103 = 0; _103 < 4; _103++)
    {
        vec3 _68 = aVertex.xyz - Light(UBO[_103 * 2 + 4].xyz, UBO[_103 * 2 + 4].w, UBO[_103 * 2 + 5]).Position;
        vColor += (((UBO[_103 * 2 + 5]) * clamp(1.0 - (length(_68) / Light(UBO[_103 * 2 + 4].xyz, UBO[_103 * 2 + 4].w, UBO[_103 * 2 + 5]).Radius), 0.0, 1.0)) * dot(aNormal, normalize(_68)));
    }
}

