#version 450

/* Reads the frame every way a preset can: sampled as Source and as
 * Original, fetched, sized and gathered, a little past its edges so the
 * wrap shows. Each part of the output shows one read as it is. The
 * reads and the parts are symmetric top to bottom, so a frame drawn
 * upside down and flipped back reads the same. */

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
layout(set = 0, binding = 3) uniform sampler2D Original;

void main()
{
   /* Pixel centres, so that runs reading the frame differently compute
    * the same coordinates. */
   vec2  px = floor(vTexCoord * params.OutputSize.xy);
   vec2  uv = (px + 0.5) * params.OutputSize.zw * 1.25 - 0.125;
   ivec2 n  = textureSize(Original, 0);
   vec4  a  = texture(Source, uv);
   vec4  b  = texture(Original, (uv - 0.5) * 1.5 + 0.5);
   vec4  c  = texelFetch(Original, ivec2(fract(uv) * vec2(n)), 0);
   vec4  g  = textureGather(Source, uv, 1);
   /* The same whichever way up the frame lies. */
   vec4  d  = vec4(max(g.x, g.w), min(g.y, g.z), max(g.y, g.z), 1.0);
   bool  r  = vTexCoord.x >= 0.5;
   bool  t  = abs(vTexCoord.y - 0.5) >= 0.25;
   FragColor = t ? (r ? d : c) : (r ? b : a);
}
