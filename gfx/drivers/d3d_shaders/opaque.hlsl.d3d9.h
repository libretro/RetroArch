#include "shaders_common.h"

static const char *stock_hlsl_program = CG(
      void main_vertex
      (
         float3 position : POSITION,
         float4 color    : COLOR,
         float2 texCoord : TEXCOORD0,

         uniform float4x4 modelViewProj,

         out float4 oPosition : POSITION,
         out float4 oColor : COLOR,
         out float2 otexCoord : TEXCOORD
      )
      {
         oPosition = mul(modelViewProj, float4(position, 1.0f));
         oColor = color;
         otexCoord = texCoord;
      }

      struct output
      {
         float4 color: COLOR;
      };

      struct input
      {
         float2 video_size;
         float2 texture_size;
         float2 output_size;
         float frame_count;
         float frame_direction;
         float frame_rotation;
      };

      output main_fragment(float4 color : COLOR, float2 texCoord : TEXCOORD0,
      uniform sampler2D decal : TEXUNIT0, uniform input IN)
      {
         output OUT;
         OUT.color = color * tex2D(decal, texCoord);
         return OUT;
      }
);
