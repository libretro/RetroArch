#version 450
/* textureGather() of the frame */
layout(push_constant) uniform Push
{
   vec4 SourceSize;
   vec4 OriginalSize;
} params;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
layout(set = 0, binding = 3) uniform sampler2D LUT;
/* The test sets OriginalSize to the coordinate's offset and step: from
 * gl_FragCoord, so both runs compute it alike. */
#define vTexCoord (gl_FragCoord.xy * params.OriginalSize.zw + params.OriginalSize.xy)
vec4 tap(sampler2D s, vec2 uv) { return texture(s, uv); }
vec4 tap2(sampler2D s, sampler2D t, vec2 uv)
{
   return tap(s, uv) * 0.5 + texture(t, uv) * 0.0;
}
ivec2 cl(ivec2 i, int m)
{
   return clamp(i, ivec2(0), ivec2(params.SourceSize.xy) - 1 - m);
}
void main()
{
   FragColor = textureGather(Source, vTexCoord);
}
