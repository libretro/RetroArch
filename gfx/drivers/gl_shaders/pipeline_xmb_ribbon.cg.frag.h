#include "shaders_common.h"

static const char *stock_fragment_xmb = CG(
   void main(
      uniform float Time,
      float3 TexCoord : TEXCOORD0,
      float4 out oColor : COLOR)
   {
      const float3 up = float3(1.0, 0.0, 0.0);
      float3 x = ddx(TexCoord);
      float3 y = ddy(TexCoord);
      float3 normal = normalize(cross(x, y));
      float c = 1.0 - dot(normal, up);
      c = (1.0 - cos(c * c)) / 3.0;
      oColor = float4(c, c, c, 1.0);
   }
);
