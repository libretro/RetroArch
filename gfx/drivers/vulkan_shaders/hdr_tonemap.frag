#version 310 es

precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform highp sampler2D Source;

#include "hdr_common.glsl"

/* sRGB electrical-optical transfer function (linear -> sRGB encoded bytes).
 * Needed because the readback render target is B8G8R8A8_UNORM (linear),
 * so we must apply the sRGB OETF ourselves for the saved PNG to look
 * correct in any standard sRGB viewer. */
vec3 LinearToSRGB(vec3 c)
{
   vec3 clamped = clamp(c, vec3(0.0), vec3(1.0));
   vec3 lo = clamped * 12.92;
   vec3 hi = 1.055 * pow(clamped, vec3(1.0 / 2.4)) - 0.055;
   bvec3 cutoff = lessThanEqual(clamped, vec3(0.0031308));
   return mix(hi, lo, vec3(cutoff));
}

/* Mode tag passed from vulkan_run_hdr_pipeline() for the readback pipeline:
 *   1 = backbuffer is HDR10 PQ (RGB10A2, BT.2020, ST.2084 encoded)
 *   2 = backbuffer is scRGB   (FP16, BT.709 linear, 1.0 == 80 nits)
 * Any other value falls through to a passthrough path (e.g. if the
 * backbuffer is already SDR). */

void main()
{
   vec4 source = texture(Source, vTexCoord);
   vec3 sdr_linear;

   if (global.HDRMode == 1u)
   {
      /* HDR10 PQ: decode PQ -> linear BT.709, then inverse-of-inverse-tonemap
       * back down to SDR linear using the same paper_white the forward pass used. */
      vec3 hdr_linear = HDR10ToLinear(source.rgb);
      sdr_linear      = Tonemap(hdr_linear,
                                global.BrightnessNits,
                                global.BrightnessNits);
   }
   else if (global.HDRMode == 2u)
   {
      /* scRGB: already linear BT.709, but scaled such that 1.0 = 80 nits
       * and the SDR UI sits at paper_white_nits. Undo that scaling so SDR
       * paper white maps back to 1.0. scRGB can carry negative / >1 values
       * for out-of-gamut / super-white content; LinearToSRGB will clamp. */
      sdr_linear = source.rgb * (kscRGBWhiteNits / global.BrightnessNits);
   }
   else
   {
      /* Passthrough: backbuffer is already SDR linear (shouldn't happen on
       * the HDR readback path, but keeps the shader well-defined). */
      sdr_linear = source.rgb;
   }

   /* B8G8R8A8_UNORM target — apply sRGB OETF so the PNG looks right. */
   FragColor = vec4(LinearToSRGB(sdr_linear), 1.0);
}
