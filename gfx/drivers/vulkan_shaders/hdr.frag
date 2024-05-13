#version 310 es

precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform highp sampler2D Source;

#include "hdr_common.glsl"

void main()
{
   vec4 source = texture(Source, vTexCoord);
   vec3 sdr = source.rgb;
   vec3 hdr = sdr;
   if (settings.inverse_tonemap > 0.0f)
   {
      hdr = InverseTonemap(sdr);
   }
   if (settings.hdr10 > 0.0f)
   {
      hdr = ConvertLinearToHDR10(hdr);
   }
   FragColor = vec4(hdr, source.a);
}
