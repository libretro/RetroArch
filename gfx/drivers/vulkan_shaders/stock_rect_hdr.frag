#version 310 es

precision highp float;
precision highp int;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform highp sampler2D Source;

#include "hdr_encode.glsl"

/* A rectangle of a texture drawn as stock_rect.frag draws it, encoded
 * as hdr.frag encodes an SDR source, straight into the HDR swapchain.
 * SourceSize holds the rectangle's first and last texel centres, as
 * stock_rect.frag's Clamp; the scanline branches, which read it as a
 * size, are left out. */
void main()
{
   vec2 uv = clamp(vTexCoord, global.SourceSize.xy, global.SourceSize.zw);
   if(global.HDRMode == 3u)
      FragColor = vec4(HDR10ToscRGB(texture(Source, uv).rgb), 1.0);
   else if(global.HDRMode == 2u)
   {
      vec4 linear = To2020(Sample(kDefaultColor, uv));
      linear.rgb  = linear.rgb * k2020to709;
      FragColor   = vec4(linear.rgb
            * (global.BrightnessNits / kscRGBWhiteNits), 1.0);
   }
   else if((global.InverseTonemap > 0.0) && (global.HDR10 > 0.0))
      FragColor = vec4(HDR10(HDR(To2020(Sample(kDefaultColor, uv)))).rgb,
            1.0);
   else if(global.InverseTonemap > 0.0)
      FragColor = vec4(HDR(To2020(Sample(kDefaultColor, uv))).rgb, 1.0);
   else if(global.HDR10 > 0.0)
      FragColor = vec4(HDR10(To2020(Sample(kDefaultColor, uv))).rgb, 1.0);
   else
      FragColor = vec4(texture(Source, uv).rgb, 1.0);
}
