#ifndef BLOOM_SM4_HLSL_H
#define BLOOM_SM4_HLSL_H

static const char bloom_vs_source[] =
   "struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};"
   "O VSMain(uint id:SV_VertexID){O o;o.uv=float2(id&1,id>>1);"
   "o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}";

static const char bloom_ps_source[] =
   "cbuffer U{float4 p;}Texture2D<float4> t;SamplerState s;"
   "float3 light(float2 uv){return max(t.Sample(s,uv).rgb-p.z,0);}"
   "float4 PSMain(float4 pos:SV_POSITION,float2 uv:TEXCOORD0):SV_TARGET{"
   "float3 c=light(uv);if(dot(p.xy,p.xy)>0){c*=.227027;"
   "c+=(light(uv+p.xy*1.384615)+light(uv-p.xy*1.384615))*.316216;"
   "c+=(light(uv+p.xy*3.230769)+light(uv-p.xy*3.230769))*.0702703;}"
   "return float4(c*p.w,0);}";
#endif
