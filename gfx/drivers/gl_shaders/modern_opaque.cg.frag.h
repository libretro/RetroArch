#include "shaders_common.h"

static const char *stock_fragment_modern = CG(
   void main(
      uniform sampler2D vTexture,
      float2 TexCoord : TEXCOORD0,
      float4 out oColor : COLOR)
   {
      oColor = float4(tex2D(vTexture, TexCoord).rgb, 1.0);
   }
);
