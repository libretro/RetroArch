#version 310 es

layout(binding = 0, std140) uniform UBO
{
    mat4 mvp;
} _16;

in vec4 aVertex;
out vec3 vNormal;
in vec3 aNormal;

void main()
{
    gl_Position = _16.mvp * aVertex;
    vNormal = aNormal;
}

