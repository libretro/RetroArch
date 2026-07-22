#version 310 es
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

layout(set = 0, binding = 0, std140) uniform UBO
{
   mat4 MVP;
   vec4 hdr_params; /* x = paper white nits, y = expand gamut mode */
} global;

void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord = TexCoord;
}
