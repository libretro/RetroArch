Texture2D<float4> image : register(t0);
SamplerState linear_sampler : register(s0);
RWStructuredBuffer<float4> output : register(u0);
[numthreads(8,8,1)] void main(uint3 id : SV_DispatchThreadID) {
if (id.x >= 257 || id.y >= 256) return; uint i=id.y*257+id.x;
output[i*2]=image.Load(int3(id.xy,0));
output[i*2+1]=image.SampleLevel(linear_sampler,(float2(id.xy)+0.5)/float2(257,256),0); }
