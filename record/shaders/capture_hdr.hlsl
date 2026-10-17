/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
cbuffer Capture : register(b0)
{
   uint2 origin;
   uint2 size;
   uint scrgb;
   uint full_range;
};
Texture2D<float4> image : register(t0);
RWTexture2D<unorm float> luma : register(u0);
RWTexture2D<unorm float2> chroma : register(u1);
float3 pq(float3 light)
{
   float3 p = pow(saturate(light), 2610.0 / 16384.0);
   return pow((3424.0 / 4096.0 + (2413.0 / 128.0) * p)
         / (1.0 + (2392.0 / 128.0) * p), 2523.0 / 32.0);
}
float3 yuv(uint2 pos)
{
   float3 rgb = image.Load(int3(origin + pos, 0)).rgb;
   if (scrgb)
   {
      float3 wide = float3(dot(rgb, float3(0.627404, 0.329283, 0.043313)),
            dot(rgb, float3(0.069097, 0.919540, 0.011362)),
            dot(rgb, float3(0.016391, 0.088013, 0.895595)));
      rgb = pq(wide * (80.0 / 10000.0));
   }
   rgb = saturate(rgb);
   float y = dot(rgb, float3(0.2627, 0.6780, 0.0593));
   return float3(y, (rgb.b - y) / 1.8814, (rgb.r - y) / 1.4746);
}
[numthreads(8, 8, 1)]
void capture_cs(uint3 id : SV_DispatchThreadID)
{
   uint2 pos = id.xy * 2;
   if (any(pos >= size)) return;
   float3 a = yuv(pos), b = yuv(pos + uint2(1, 0));
   float3 c = yuv(pos + uint2(0, 1)), d = yuv(pos + uint2(1, 1));
   float scale = full_range ? 1023.0 : 876.0;
   float offset = full_range ? 0.0 : 64.0;
   luma[pos] = round(offset + scale * a.x) * (64.0 / 65535.0);
   luma[pos + uint2(1, 0)] = round(offset + scale * b.x) * (64.0 / 65535.0);
   luma[pos + uint2(0, 1)] = round(offset + scale * c.x) * (64.0 / 65535.0);
   luma[pos + uint2(1, 1)] = round(offset + scale * d.x) * (64.0 / 65535.0);
   uint left = pos.x ? pos.x - 1 : 0;
   float3 e = yuv(uint2(left, pos.y)), f = yuv(uint2(left, pos.y + 1));
   float2 uv = (e.yz + f.yz + 2.0 * (a.yz + c.yz) + b.yz + d.yz) * 0.125;
   chroma[id.xy] = clamp(round(512.0 + (full_range ? 1023.0 : 896.0) * uv),
         0.0, 1023.0) * (64.0 / 65535.0);
}
