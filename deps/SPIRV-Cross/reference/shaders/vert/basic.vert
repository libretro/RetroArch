#version 310 es

layout(std140) uniform UBO
{
    mat4 uMVP;
} _16;

in vec4 aVertex;
out vec3 vNormal;
in vec3 aNormal;

void main()
{
    gl_Position = _16.uMVP * aVertex;
    vNormal = aNormal;
}

