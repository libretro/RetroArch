#version 310 es

precision highp float;
precision highp int;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform highp sampler2D Source;

#include "hdr_encode.glsl"

void main()
{
   if(global.HDRMode == 3u)
   {
      /* PQ HDR10 to scRGB conversion.
       * Source is PQ-encoded Rec.2020 from the shader's output.
       * Decode PQ to linear, convert gamut to Rec.709, scale for scRGB. */
      vec4 pq = texture(Source, vTexCoord);
      FragColor = vec4(HDR10ToscRGB(pq.rgb), pq.a);
   }
   else if(global.HDRMode == 2u)
   {
      /* scRGB mode: sRGB to linear for scRGB / HDR16 swapchain.
       * Scale by MaxNits / kscRGBWhiteNits because scRGB 1.0 = 80 nits.
       * Using MaxNits (not BrightnessNits) so the SDR UI fills the
       * display's native range, preserving tonal balance uniformly. */
      if((global.Scanlines > 0.0) && (global.OutputSize.y > 240.0 * 4.0))
      {
         /* Scanlines() returns linear Rec.709 with mask already applied in Rec.709 space.
          * scRGB units: 1.0 = 80 nits. */
         vec3 linear = Scanlines(vTexCoord);

         FragColor = vec4(linear * (global.BrightnessNits / kscRGBWhiteNits), 1.0);
      }
      else
      {
         /* Convert to Rec.2020 with ExpandGamut colour boost,
          * then always back to Rec.709 for scRGB output.
          * For Super (gamut 3), To2020 passes through Rec.709 data —
          * k2020to709 then "interprets" it as Rec.2020, giving maximum boost. */
         vec4 linear = To2020(Sample(kDefaultColor, vTexCoord));

         linear.rgb = linear.rgb * k2020to709;

         linear.rgb *= global.BrightnessNits / kscRGBWhiteNits;
         FragColor = linear;
      }
   }
   else if((global.InverseTonemap > 0.0) && (global.HDR10 > 0.0))
   {
      if((global.Scanlines > 0.0) && (global.OutputSize.y > 240.0 * 4.0))
      {
         /* Scanlines() returns linear Rec.2020 with InverseTonemap and mask applied */
         FragColor = vec4(HDR10(Scanlines(vTexCoord)), 1.0);
      }
      else
      {
         FragColor = HDR10(HDR(To2020(Sample(kDefaultColor, vTexCoord))));
      }
   }
   else if(global.InverseTonemap > 0.0)
   {
      FragColor = HDR(To2020(Sample(kDefaultColor, vTexCoord)));
   }
   else if(global.HDR10 > 0.0)
   {
      FragColor = HDR10(To2020(Sample(kDefaultColor, vTexCoord)));
   }
   else
   {
      FragColor = texture(Source, vTexCoord);
   }
}