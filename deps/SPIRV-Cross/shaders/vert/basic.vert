#version 310 es

layout(std140) uniform UBO
{
    uniform mat4 uMVP;
};
in vec4 aVertex;
in vec3 aNormal;
out vec3 vNormal;

void main()
{
    gl_Position = uMVP * aVertex;
    vNormal = aNormal;
}
