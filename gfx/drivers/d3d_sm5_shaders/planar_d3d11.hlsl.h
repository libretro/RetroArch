/* Built-in compute source; compiled offline for SM5/SM6. */
#include "../../drivers_shader/video_hdr.inc"
#ifdef D3D11_PLANAR_ARRAY
   "Texture2DArray<float>  Y  : register(t0);\n"
   "Texture2DArray<float2> C0 : register(t1);\n"
   "Texture2DArray<float>  C1 : register(t2);\n"
#else
   "Texture2D<float>  Y  : register(t0);\n"
   "Texture2D<float2> C0 : register(t1);\n"
   "Texture2D<float>  C1 : register(t2);\n"
#endif
   "RWTexture2D<float4> Out : register(u0);\n"
#ifdef D3D11_PLANAR_ARRAY
   "cbuffer P : register(b0) { float4 k0; float4 k1; uint4 size; float4 hdr; uint4 origin; };\n"
#else
   "cbuffer P : register(b0) { float4 k0; float4 k1; uint4 size; float4 hdr; };\n"
#endif
   "[numthreads(8, 8, 1)]\n"
   "void main(uint3 id : SV_DispatchThreadID)\n"
   "{\n"
   "   int3 c;\n"
   "   float y;\n"
   "   float2 ch;\n"
   "   if (id.x >= size.x || id.y >= size.y)\n"
   "      return;\n"
#ifdef D3D11_PLANAR_ARRAY
   "   uint2 xy = id.xy + origin.xy;\n"
   "   y = Y.Load(int4(xy, origin.z, 0));\n"
   "   ch = C0.Load(int4(xy >> 1, origin.z, 0));\n"
#else
   "   c  = int3(id.xy >> 1, 0);\n"
   "   y  = Y.Load(int3(id.xy, 0));\n"
   "   ch = k1.z > 0.5 ? C0.Load(c) : float2(C0.Load(c).x, C1.Load(c));\n"
#endif
   "   if (k1.w > 0.5)\n"
   "      ch = ch.yx;\n"
   "   if (size.z != 0)\n"
   "   {\n"
   "      float scale = size.w != 0 ? 65535.0 / 65472.0 : 65535.0 / 65280.0;\n"
   "      y *= scale;\n"
   "      ch *= scale;\n"
   "   }\n"
   "   ch -= size.z != 0 && size.w != 0 ? 512.0 / 1023.0 : 128.0 / 255.0;\n"
   "   Out[id.xy] = float4(video_hdr_convert(k0.x * y + k0.y + float3(k0.z * ch.y,\n"
   "         k0.w * ch.x + k1.x * ch.y, k1.y * ch.x), hdr), 1.0);\n"
   "}\n"
