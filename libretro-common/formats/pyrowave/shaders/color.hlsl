/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 * Non-constant-luminance RGB/YCbCr, ITU-R BT.709/BT.2020.
 */
cbuffer Parameters : register(b0)
{
   uint image_width;
   uint image_height;
   uint width;
   uint height;
   uint chroma_444;
   uint chroma_left;
   uint inverse;
   uint flip_y;
   uint texture_x;
   uint texture_y;
   uint texture_mode;
   uint colorimetry;
   float red;
   float blue;
   float chroma_center;
   float luma_offset;
   float luma_scale;
   float chroma_scale;
   float padding0;
   float padding1;
};

Texture2D<float4> rgb_source : register(t0);
StructuredBuffer<float> atlas_source : register(t1);
RWStructuredBuffer<float> atlas_destination : register(u0);
RWTexture2D<float4> rgb_destination : register(u1);

float3 to_bt2020(float3 v)
{
   return float3(dot(v, float3(0.627403896, 0.329283038, 0.043313066)),
         dot(v, float3(0.069097289, 0.919540395, 0.011362316)),
         dot(v, float3(0.016391439, 0.088013308, 0.895595253)));
}

float3 to_bt709(float3 v)
{
   return float3(dot(v, float3(1.660491002, -0.587641139, -0.072849863)),
         dot(v, float3(-0.124550475, 1.132899897, -0.008349423)),
         dot(v, float3(-0.018150763, -0.100578898, 1.118729661)));
}

float encode_light(float v)
{
   if ((colorimetry & 2) != 0)
   {
      float power = pow(saturate(v / 125.0), 0.1593017578125);
      return pow((0.8359375 + 18.8515625 * power) / (1.0 + 18.6875 * power), 78.84375);
   }
   v = saturate(v);
   return v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1.0 / 2.4) - 0.055;
}

float decode_light(float v)
{
   if ((colorimetry & 2) != 0)
   {
      float power = pow(v, 1.0 / 78.84375);
      return 125.0 * pow(max(power - 0.8359375, 0.0) / (18.8515625 - 18.6875 * power),
            1.0 / 0.1593017578125);
   }
   return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

float3 encode_rgb(float3 rgb)
{
   if (texture_mode == 0) return saturate(rgb);
   if ((colorimetry & 1) != 0) rgb = to_bt2020(rgb);
   return float3(encode_light(rgb.r), encode_light(rgb.g), encode_light(rgb.b));
}

float3 decode_rgb(float3 rgb)
{
   rgb = saturate(rgb);
   if (texture_mode == 0) return rgb;
   rgb = float3(decode_light(rgb.r), decode_light(rgb.g), decode_light(rgb.b));
   if ((colorimetry & 1) != 0) rgb = to_bt709(rgb);
   if (texture_mode == 2)
   {
      rgb = saturate(rgb / max(1.0, max(rgb.r, max(rgb.g, rgb.b))));
      return float3(rgb.r <= 0.0031308 ? rgb.r * 12.92 : 1.055 * pow(rgb.r, 1.0 / 2.4) - 0.055,
            rgb.g <= 0.0031308 ? rgb.g * 12.92 : 1.055 * pow(rgb.g, 1.0 / 2.4) - 0.055,
            rgb.b <= 0.0031308 ? rgb.b * 12.92 : 1.055 * pow(rgb.b, 1.0 / 2.4) - 0.055);
   }
   return rgb;
}

float3 read_rgb(int x, int y)
{
   uint row = clamp(y, 0, int(height) - 1);
   uint column = clamp(x, 0, int(width) - 1);
   if (flip_y != 0) row = height - 1 - row;
   /* Positive padding values opt into full-texture frontend scaling; zero
    * retains the original one-to-one cropped sampling and wire format. */
   if (padding0 > 0.0) column = uint((float(column) + 0.5) * padding0);
   if (padding1 > 0.0) row = uint((float(row) + 0.5) * padding1);
   return encode_rgb(rgb_source.Load(int3(texture_x + column, texture_y + row, 0)).rgb);
}

float3 read_chroma(uint x, uint y)
{
   int cx = int(x * 2);
   int cy = int(y * 2);
   float3 center = read_rgb(cx, cy) + read_rgb(cx, cy + 1);
   float3 right = read_rgb(cx + 1, cy) + read_rgb(cx + 1, cy + 1);
   float3 left = read_rgb(cx - 1, cy) + read_rgb(cx - 1, cy + 1);
   return center * 0.25 + right * (chroma_left != 0 ? 0.125 : 0.25)
         + left * (chroma_left != 0 ? 0.125 : 0.0);
}

float atlas_value(int2 p, uint component)
{
   uint shift = component != 0 && chroma_444 == 0 ? 1 : 0;
   uint2 size = uint2(width, height) >> shift;
   uint2 coordinate = uint2(clamp(p, int2(0, 0), int2(size) - 1));
   return saturate(atlas_source[component * image_width * image_height
         + coordinate.y * image_width + coordinate.x] + 0.5);
}

float chroma_value(uint2 p, uint component)
{
   if (chroma_444 != 0) return atlas_value(int2(p), component);
   float2 coordinate = float2(p) * 0.5 - float2(chroma_left != 0 ? 0.0 : 0.25, 0.25);
   int2 base = int2(floor(coordinate));
   float2 weight = frac(coordinate);
   float top = lerp(atlas_value(base, component), atlas_value(base + int2(1, 0), component), weight.x);
   float bottom = lerp(atlas_value(base + int2(0, 1), component),
         atlas_value(base + int2(1, 1), component), weight.x);
   return lerp(top, bottom, weight.y);
}

[numthreads(8, 8, 1)]
void main(uint3 position : SV_DispatchThreadID)
{
   if (inverse == 0)
   {
      uint component = position.z;
      uint shift = component != 0 && chroma_444 == 0 ? 1 : 0;
      uint2 size = uint2(image_width, image_height) >> shift;
      if (component >= 3 || any(position.xy >= size)) return;
      uint2 p = min(position.xy, (uint2(width, height) >> shift) - 1);
      float3 rgb = shift != 0 ? read_chroma(p.x, p.y) : read_rgb(int(p.x), int(p.y));
      float luma = dot(rgb, float3(red, 1.0 - red - blue, blue));
      float value = luma_offset + luma * luma_scale;
      if (component == 1) value = chroma_center + (rgb.b - luma) * chroma_scale / (2.0 * (1.0 - blue));
      if (component == 2) value = chroma_center + (rgb.r - luma) * chroma_scale / (2.0 * (1.0 - red));
      atlas_destination[component * image_width * image_height + position.y * image_width + position.x]
         = saturate(value) - 0.5;
   }
   else
   {
      if (position.z != 0 || position.x >= width || position.y >= height) return;
      float luma = (atlas_value(int2(position.xy), 0) - luma_offset) / luma_scale;
      float cb = (chroma_value(position.xy, 1) - chroma_center) / chroma_scale;
      float cr = (chroma_value(position.xy, 2) - chroma_center) / chroma_scale;
      float r = luma + cr * (2.0 * (1.0 - red));
      float b = luma + cb * (2.0 * (1.0 - blue));
      float g = (luma - red * r - blue * b) / (1.0 - red - blue);
      uint row = flip_y != 0 ? height - 1 - position.y : position.y;
      rgb_destination[uint2(texture_x + position.x, texture_y + row)] = float4(decode_rgb(float3(r, g, b)), 1.0);
   }
}
