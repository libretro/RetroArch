#include "shaders_common.h"

static const char *stock_xmb_snowflake = CG(
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

   float rand_float(float x)
   {
      return rand(float2(x, 1.0));
   }

   float snow(float3 pos, float2 uv, float o, float atime)
   {
      float2 d = (pos.xy - uv);
      float a = atan2(d.y, d.x) + sin(atime * 1.0 + o) * 10.0;
      float dist = d.x * d.x + d.y * d.y;

      if (dist < pos.z / 400.0)
      {
         float col = 0.0;
         if (sin(a * 8.0) < 0.0)
            col = 1.0;
         if (dist < pos.z / 800.0)
            col += 1.0;
         return col * pos.z;
      }
      return 0.0;
   }

   float col(float2 c, float atime, float gtime)
   {
      float color = 0.0;
      for (int i = 1; i < 15; i++)
      {
         float o = rand_float(float(i) / 3.0) * 15.0;
         float z = rand_float(float(i) + 13.0);
         float x = 1.8 - (3.6) * (rand_float(floor((gtime * ((z + 1.0) / 2.0) + o) / 2.0))
            + sin(gtime * o / 1000.0) / 10.0);
         float y = 1.0 - fmod((gtime * ((z + 1.0) / 2.0)) + o, 2.0);

         color += snow(float3(x, y, z), c, o, atime);
      }
      return color;
   }

   struct output
   {
      float4 color : COLOR;
   };

   output main_fragment(float4 fragCoord : WPOS, float4 vdata0 : TEXCOORD0)
   {
      output OUT;
      float2 osize = vdata0.xy;
      float gtime = vdata0.z;
      float atime = (gtime + 1.0) / 4.0;
      float2 uv = fragCoord.xy / osize.xy;
      uv = uv * 2.0 - 1.0;
      float2 p = uv;
      p.x *= osize.x / osize.y;

      float c = col(p, atime, gtime);
      OUT.color = float4(c, c, c, c);
      return OUT;
   }
);
