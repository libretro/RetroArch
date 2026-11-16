#ifndef BLOOM_SM3_HLSL_H
#define BLOOM_SM3_HLSL_H

static const char bloom_sm3_ps_source[] =
   "uniform sampler2D s;uniform float4 p;"
   "float3 light(float2 uv){return max(tex2D(s,uv).rgb-p.z,0);}"
   "float4 main_fragment(float2 uv:TEXCOORD0):COLOR{"
   "float3 c=light(uv);if(dot(p.xy,p.xy)>0){c*=.227027;"
   "c+=(light(uv+p.xy*1.384615)+light(uv-p.xy*1.384615))*.316216;"
   "c+=(light(uv+p.xy*3.230769)+light(uv-p.xy*3.230769))*.0702703;}"
   "return float4(c*p.w,0);}";
#endif
