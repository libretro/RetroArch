#version 450

/* rect.slang with its push constants nearly full, so the frame's
 * rectangle goes in the uniform block. */

#pragma parameter P0 "P0" 0.0 0.0 1.0 1.0
#pragma parameter P1 "P1" 0.0 0.0 1.0 1.0
#pragma parameter P2 "P2" 0.0 0.0 1.0 1.0
#pragma parameter P3 "P3" 0.0 0.0 1.0 1.0
#pragma parameter P4 "P4" 0.0 0.0 1.0 1.0
#pragma parameter P5 "P5" 0.0 0.0 1.0 1.0
#pragma parameter P6 "P6" 0.0 0.0 1.0 1.0
#pragma parameter P7 "P7" 0.0 0.0 1.0 1.0
#pragma parameter P8 "P8" 0.0 0.0 1.0 1.0
#pragma parameter P9 "P9" 0.0 0.0 1.0 1.0
#pragma parameter P10 "P10" 0.0 0.0 1.0 1.0
#pragma parameter P11 "P11" 0.0 0.0 1.0 1.0
#pragma parameter P12 "P12" 0.0 0.0 1.0 1.0

layout(push_constant) uniform Push
{
   vec4 OutputSize;
   vec4 SourceSize;
   vec4 OriginalSize;
   float P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12;
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
   /* The parameters are all 0. */
   float z  = params.P0 + params.P1 + params.P2 + params.P3 + params.P4
            + params.P5 + params.P6 + params.P7 + params.P8 + params.P9
            + params.P10 + params.P11 + params.P12;
   vec2  px = floor(vTexCoord * params.OutputSize.xy);
   vec2  uv = (px + 0.5) * params.OutputSize.zw * 1.25 - 0.125 + z;
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
