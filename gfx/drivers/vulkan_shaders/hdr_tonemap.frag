#version 310 es

precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform highp sampler2D Source;

#include "hdr_common.glsl"

void main()
{
   vec4 source = texture(Source, vTexCoord);
   vec3 hdr = source.rgb;
   vec3 linear = hdr;
   if (settings.hdr10 == 0.0f)
   {
      /* Backbuffer is in HDR10, need to convert to linear */
      linear = ConvertHDR10ToLinear(hdr);
   }
   vec3 sdr = hdr;
   if (settings.inverse_tonemap == 0.0f)
   {
      /* Backbuffer is inverse tonemapped, need to tonemap back */
      sdr = Tonemap(linear);
   }
   FragColor = vec4(sdr, source.a);
}
