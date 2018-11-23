#include "shaders_common.h"

static const char *nuklear_shader = CG(
   struct input
   {
      float time;
   };

   void main_vertex
   (
    float4 position	: POSITION,
    float4 color	: COLOR,
    float2 texCoord : TEXCOORD0,

    uniform float4x4 modelViewProj,

    out float4 oPosition : POSITION,
    out float4 oColor    : COLOR,
    out float2 otexCoord : TEXCOORD
   )
   {
      oPosition = mul(modelViewProj, position);
      oColor    = color;
      otexCoord = texCoord;
   }

   struct output
   {
      float4 color    : COLOR;
   };

   output main_fragment(float2 texCoord : TEXCOORD0, uniform sampler2D Texture : TEXUNIT0, uniform input IN)\
   {
      output OUT;
      OUT.color = tex2D(Texture, texCoord);
      return OUT;
   }
);
