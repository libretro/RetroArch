#version 310 es

layout(binding = 0, std140) uniform UBO
{
   mat4 mvp; 
};

in vec4 aVertex;
in vec3 aNormal;
out vec3 vNormal;

void main()
{
    gl_Position = mvp * aVertex;
    vNormal = aNormal;
}
