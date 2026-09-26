#version 450

/* Writes the pass's output size, and the red of what it samples: the
 * final pass shows its own size and the size of the pass before it. */

layout(push_constant) uniform Push
{
   vec4 OutputSize;
} params;

layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord   = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

void main()
{
   FragColor = vec4(params.OutputSize.xy / 2048.0,
         texture(Source, vTexCoord).r, 1.0);
}
