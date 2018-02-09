/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#define SRC(...) #__VA_ARGS__
SRC(

   Texture2D<float4> t0;
   RWTexture2D<float4> u0;
   sampler s0;

   cbuffer CBr
   {
       uint src_level;
       float2 texel_size;
   }

   static float w[4]= {0.090845, 0.409155, 0.409155, 0.090845};
   [numthreads(8, 8, 1)]
   void CSMain(uint3 DTid : SV_DispatchThreadID)
   {
      int i;
      int j;
      float4 c = 0.0f;
      for (i = 0; i < 4; i++)
         for (j = 0; j < 4; j++)
         {
            float4 c0 = t0.SampleLevel(s0, texel_size * (DTid.xy + 0.5f * float2(i - 0.5f,j - 0.5f)), src_level);
            c0.rgb *= c0.a;
            c += w[i] * w[j] * c0;
         }
      c.rgb /= c.a;

      u0[DTid.xy] = c;
      return;

   }
)
