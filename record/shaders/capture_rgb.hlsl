/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
cbuffer Crop : register(b0) { float4 crop; };
Texture2D<float4> image : register(t0);
SamplerState point_sampler : register(s0);
struct Vertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Vertex capture_vs(uint id : SV_VertexID)
{
   Vertex v;
   float2 uv = float2((id << 1) & 2, id & 2);
   v.position = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
   v.uv = crop.xy + uv * crop.zw;
   return v;
}
float4 capture_ps(Vertex v) : SV_Target
{
   return image.SampleLevel(point_sampler, v.uv, 0);
}
