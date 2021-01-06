#include "shaders_common.h"

static const char *stock_vertex_xmb_ribbon_modern = CG(
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

   float height(float3 pos, float time)
   {
      const float twoPi = 2.0 * 3.14159;
      float k = twoPi / 20.0;
      float omega = twoPi / 15.0;
      float y = sin(k * pos.x - omega * time);
      y += noise(float3(0.27) * float3(0.4 * pos.x, 3.0, 2.0 * pos.z - 0.5 * time));
      return y;
   }

   void main(
      uniform float Time,
      float3 VertexCoord,
      float3 out TexCoord : TEXCOORD0,
      float4 out oPosition : POSITION)
   {
      float3 pos = float3(VertexCoord.x, VertexCoord.z, VertexCoord.y);
      pos.y = height(pos, Time);
      oPosition = float4(pos.x, (pos.y/3.0), pos.z, 1.0);
      TexCoord = pos;
   }
);
