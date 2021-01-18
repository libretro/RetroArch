#include "shaders_common.h"

static const char *stock_vertex_xmb_simple_modern = CG(
   float iqhash(float n)
   {
      return frac(sin(n) * 43758.5453);
   }

   float noise(float3 x)
   {
      float3 p = floor(x);
      float3 f = frac(x);
      f = f * f * (3.0 - 2.0 * f);
      float n = p.x + p.y * 57.0 + 113.0 * p.z;
      return lerp(lerp(lerp(iqhash(n), iqhash(n + 1.0), f.x),
         lerp(iqhash(n + 57.0), iqhash(n + 58.0), f.x), f.y),
         lerp(lerp(iqhash(n + 113.0), iqhash(n + 114.0), f.x),
         lerp(iqhash(n + 170.0), iqhash(n + 171.0), f.x), f.y), f.z);
   }

   void main(
      uniform float Time,
      float3 VertexCoord,
      float4 out oPosition : POSITION)
   {
      float3 v = float3(VertexCoord.x, 0.0, VertexCoord.y);
      float3 v2 = v;
      v2.x = v2.x + Time / 2.0;
      v2.z = v.z * 4.0;
      v.y = -cos((v.x + v.z / 3.0 + Time) * 2.0) / 10.0 - noise(v2.xyz) / 3.0;
      oPosition = float4(v, 1.0);
   }
);
