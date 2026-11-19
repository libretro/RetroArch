/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 * Independent CDF 9/7 lifting, ITU-T T.800 (06/2019), F-7/F-11.
 */

cbuffer Parameters : register(b0)
{
   uint image_width;
   uint image_height;
   uint level;
   uint axis;
   uint phase;
   uint inverse;
   uint chroma_444;
   uint reserved;
};

StructuredBuffer<float> source : register(t0);
RWStructuredBuffer<float> destination : register(u0);

float forward_lift(uint step)
{
   if (step == 0) return -1.586134342059924;
   if (step == 1) return -0.052980118572961;
   if (step == 2) return 0.882911075530934;
   return 0.443506852043971;
}

[numthreads(8, 8, 1)]
void main(uint3 position : SV_DispatchThreadID)
{
   uint width = image_width >> level;
   uint height = image_height >> level;
   uint coordinate = axis == 0 ? position.x : position.y;
   uint length = axis == 0 ? width : height;
   uint stride = axis == 0 ? 1 : image_width;
   uint index = position.z * image_width * image_height
      + position.y * image_width + position.x;
   if (position.x >= width || position.y >= height || position.z >= 3)
      return;
   if (level == 0 && position.z != 0 && chroma_444 == 0)
      return;

   if (inverse == 0 && phase == 4)
   {
      uint input_coordinate = coordinate < length / 2
         ? coordinate * 2 : (coordinate - length / 2) * 2 + 1;
      uint input_index = index - coordinate * stride + input_coordinate * stride;
      float normalization = coordinate < length / 2
         ? 1.0 / 1.230174104914001 : 1.230174104914001;
      destination[index] = source[input_index] * normalization;
   }
   else if (inverse != 0 && phase == 0)
   {
      uint input_coordinate = coordinate / 2
         + ((coordinate & 1) != 0 ? length / 2 : 0);
      uint input_index = index - coordinate * stride + input_coordinate * stride;
      float normalization = (coordinate & 1) != 0
         ? 1.0 / 1.230174104914001 : 1.230174104914001;
      destination[index] = source[input_index] * normalization;
   }
   else
   {
      uint step = inverse != 0 ? phase - 1 : phase;
      uint parity = inverse != 0 ? step & 1 : 1 - (step & 1);
      float value = source[index];
      if ((coordinate & 1) == parity)
      {
         uint left = coordinate != 0 ? coordinate - 1 : 1;
         uint right = coordinate + 1 < length ? coordinate + 1 : length - 2;
         uint origin = index - coordinate * stride;
         float neighbors = source[origin + left * stride]
            + source[origin + right * stride];
         float coefficient = forward_lift(inverse != 0 ? 3 - step : step);
         value += (inverse != 0 ? -coefficient : coefficient) * neighbors;
      }
      destination[index] = value;
   }
}
