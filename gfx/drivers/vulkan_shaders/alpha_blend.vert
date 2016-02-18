#version 310 es
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec4 Color;
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;

layout(set = 0, binding = 0, std140) uniform UBO
{
   mat4 MVP;
} global;

void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord = TexCoord;
   vColor = Color;
}
