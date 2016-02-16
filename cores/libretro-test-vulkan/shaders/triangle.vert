#version 310 es
layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Color;
layout(location = 0) out vec4 vColor;

layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
};

void main()
{
   gl_Position = MVP * Position;
   vColor = Color;
}
