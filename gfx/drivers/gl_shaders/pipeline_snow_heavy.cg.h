#include "shaders_common.h"

static const char *stock_xmb_snow_heavy = CG(
   struct input
   {
      float time;
      float2 OutputSize;
   };

   void main_vertex
   (
      float4 position : POSITION,
      float4 color    : COLOR,
      float2 texCoord : TEXCOORD0,

      uniform float4x4 modelViewProj,
      uniform input IN,

      out float4 oPosition : POSITION,
      out float4 vdata0    : TEXCOORD0
   )
   {
      oPosition = mul(modelViewProj, position);
      vdata0 = float4(IN.OutputSize, IN.time, 0.0);
   }

   float rand(float2 co)
   {
      return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
   }

   float dist_func(float2 distv)
   {
      float dist = sqrt((distv.x * distv.x) + (distv.y * distv.y)) * (40.0 / 3.5);
      dist = clamp(dist, 0.0, 1.0);
      return cos(dist * (3.14159265358 * 0.5)) * 0.5;
   }

   float random_dots(float2 co)
   {
      float part = 1.0 / 20.0;
      float2 cd = floor(co / part);
      float p = rand(cd);

      if (p > 0.005 * (0.7 * 40.0))
         return 0.0;

      float2 dpos = (float2(frac(p * 2.0), p) + float2(2.0, 2.0)) * 0.25;
      float2 cellpos = frac(co / part);
      float2 distv = (cellpos - dpos);

      return dist_func(distv);
   }

   float snow(float2 pos, float t, float scale)
   {
      pos.x += cos(pos.y * 1.2 + t * 3.14159 * 2.0 + 1.0 / scale) / (8.0 / scale) * 4.0;
      float2 scroll = t * scale * float2(-0.5, 1.0) * 4.0;
      pos += frac(scroll / 0.05) * 0.05;
      return random_dots(pos / scale) * (scale * 0.5 + 0.5);
   }

   struct output
   {
      float4 color : COLOR;
   };

   output main_fragment(float4 fragCoord : WPOS, float4 vdata0 : TEXCOORD0)
   {
      output OUT;
      float2 osize = vdata0.xy;
      float tim = vdata0.z * 0.4 * 0.25;
      float2 pos = fragCoord.xy / osize.xx;
      float a = 0.0;
      a += snow(pos, tim, 1.0);
      a += snow(pos, tim, 0.7);
      a += snow(pos, tim, 0.6);
      a += snow(pos, tim, 0.5);
      a += snow(pos, tim, 0.4);
      a += snow(pos, tim, 0.3);
      a += snow(pos, tim, 0.25);
      a += snow(pos, tim, 0.125);
      a = a * min(pos.y * 4.0, 1.0);
      OUT.color = float4(1.0, 1.0, 1.0, a);
      return OUT;
   }
);
