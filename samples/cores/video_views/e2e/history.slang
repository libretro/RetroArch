#version 450

/* Draws Source unchanged, and samples OriginalHistory7 so the chain
 * keeps eight frames. */

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
layout(set = 0, binding = 3) uniform sampler2D OriginalHistory7;

void main()
{
   /* History is never negative, so min() adds nothing. */
   FragColor = vec4(texture(Source, vTexCoord).rgb
      + min(texture(OriginalHistory7, vTexCoord).rgb, vec3(0.0)), 1.0);
}
