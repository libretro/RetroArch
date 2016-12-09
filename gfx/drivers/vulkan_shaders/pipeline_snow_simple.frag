#version 310 es

layout(location = 0) in vec3 VertexCoord;

layout(std140, set = 0, binding = 0) uniform UBO
{
   float time;
} constants;

void main()
{
   gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);
}
