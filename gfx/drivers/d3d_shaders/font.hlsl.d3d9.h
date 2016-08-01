#include "shaders_common.h"

static const char *font_hlsl_d3d9_program = CG(
      struct VS_IN
      {
         float2 Pos : POSITION;
         float2 Tex : TEXCOORD0;
      };

      struct VS_OUT
      {
         float4 Position : POSITION;
         float2 TexCoord0 : TEXCOORD0;
      };

      uniform float4 Color : register(c1);
      uniform float2 TexScale : register(c2);
      sampler FontTexture : register(s0);

      VS_OUT main_vertex( VS_IN In )
      {
         VS_OUT Out;
         Out.Position.x  = (In.Pos.x-0.5);
         Out.Position.y  = (In.Pos.y-0.5);
         Out.Position.z  = ( 0.0 );
         Out.Position.w  = ( 1.0 );
         Out.TexCoord0.x = In.Tex.x * TexScale.x;
         Out.TexCoord0.y = In.Tex.y * TexScale.y;
         return Out;
      }

      float4 main_fragment( VS_OUT In ) : COLOR0
      {
         float4 FontTexel = tex2D( FontTexture, In.TexCoord0 );
         return FontTexel;
      }
);
