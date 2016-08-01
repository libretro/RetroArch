#include "shaders_common.h"

static const char *stock_cg_d3d9_program = CG(
    void main_vertex
    (
    	float4 position : POSITION,
    	float2 texCoord : TEXCOORD0,
      float4 color : COLOR,

      uniform float4x4 modelViewProj,

    	out float4 oPosition : POSITION,
    	out float2 otexCoord : TEXCOORD0,
      out float4 oColor : COLOR
    )
    {
    	oPosition = mul(modelViewProj, position);
    	otexCoord = texCoord;
      oColor = color;
    }

    float4 main_fragment(in float4 color : COLOR, float2 tex : TEXCOORD0, uniform sampler2D s0 : TEXUNIT0) : COLOR
    {
       return color * tex2D(s0, tex);
    }
);
