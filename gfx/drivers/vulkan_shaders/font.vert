#version 310 es
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec4 Color;
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;

layout(push_constant, std140) uniform UBO
{
   mat4 MVP;
   vec4 texsize;
} global;

void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord = TexCoord;
   vColor = Color;
}
