#include "shaders_common.h"

static const char *stock_vertex_xmb_ribbon_modern = CG(
   void main(
      uniform float Time,
      float3 VertexCoord,
      float4 out TexCoord : TEXCOORD0,
      float4 out oPosition : POSITION)
   {
      float x = VertexCoord.x;
      float row = floor((VertexCoord.y + 1.0) * 31.5 + 0.5);
      float z = min(row, 53.0) * (2.0 / 53.0) - 1.0;
      float a = x * 1.45 + Time * 0.08;
      float b = z * 1.75 + x * (1.2 + 0.45 * z) - Time * 0.06;
      float c = z * (5.8 + 1.2 * z) + x * 0.75 - Time * 0.09;
      float d = z * 10.0 + x * (2.2 - 0.6 * z) + Time * 0.075;
      float width = 0.13 + 0.05 * cos(a);
      float y = -0.28 + 0.12 * sin(a) + width * cos(b) * cos(c)
            + 0.04 * sin(d) + 0.06 * z;
      float dx = 0.174 * cos(a) - 0.0725 * sin(a) * cos(b) * cos(c)
            - width * ((1.2 + 0.45 * z) * sin(b) * cos(c) + 0.75 * cos(b) * sin(c))
            + 0.04 * (2.2 - 0.6 * z) * cos(d);
      float dz = -width * ((1.75 + 0.45 * x) * sin(b) * cos(c)
            + (5.8 + 2.4 * z) * cos(b) * sin(c))
            + 0.04 * (10.0 - 0.6 * x) * cos(d) + 0.06;
      float stripes = 0.65 + 0.35 * cos(z * 28.0);
      float w = 1.0 + 0.10 * z;
      float3 normal = float3(dx, -1.12, 1.12 * dz);
      float3 eye = float3(-0.112 * x, -0.10 * y, -w);
      float angle = dot(normal, eye) / sqrt(dot(normal, normal) * dot(eye, eye));
      TexCoord = float4(angle, (dz * w - 0.10 * y) / (w * w), z,
            stripes * (1.0 - step(52.5, row)));
      oPosition = float4(1.12 * x, y, 0.0, w);
      if (row > 54.5)
      {
         float layer = floor((row - 55.0) / 3.0);
         float t = row - 56.0 - 3.0 * layer;
         TexCoord = float4(x, t, 2.0 + layer, 1.0 - abs(t));
         oPosition = float4(x, -0.28 + 0.06 * sin(a) + t * (0.70 + 0.12 * layer), 0.0, 1.0);
      }
   }
);
