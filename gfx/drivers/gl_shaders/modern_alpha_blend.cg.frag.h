#include "shaders_common.h"

static const char *stock_fragment_modern_blend = CG(
   void main(
      uniform sampler2D vTexture,
      uniform float4 bgcolor,
      float2 TexCoord : TEXCOORD0,
      float4 Color : COLOR,
      float4 out oColor : COLOR)
   {
      if (bgcolor.a > 0.0)
         oColor = bgcolor;
      else
         oColor = tex2D(vTexture, TexCoord) * Color;
   }
);
