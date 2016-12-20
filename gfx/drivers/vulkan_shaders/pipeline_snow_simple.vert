#version 310 es
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
} global;

void main()
{
   gl_Position = global.MVP * vec4(TexCoord, 0.0, 1.0);
   vTexCoord = TexCoord;
}
