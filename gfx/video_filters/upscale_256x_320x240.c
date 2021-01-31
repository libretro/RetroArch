/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2018 - Daniel De Matteis
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

/* Compile: gcc -o upscale_256x_320x240.so -shared upscale_256x_320x240.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation upscale_256x_320x240_get_implementation
#define softfilter_thread_data upscale_256x_320x240_softfilter_thread_data
#define filter_data upscale_256x_320x240_filter_data
#endif

struct softfilter_thread_data
{
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned colfmt;
   unsigned width;
   unsigned height;
   int first;
   int last;
};

struct filter_data
{
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
};

/*******************************************************************
 * Approximately bilinear scaler, 256x224 to 320x240
 * Copyright (C) 2014 hi-ban, Nebuleon <nebuleon.fumika@gmail.com>
 * (Optimisations by jdgleaver)
 *******************************************************************/

#define UPSCALE_256__WEIGHT_1_1(A, B, out, tmp)      \
   *(out) = ((A + B + ((A ^ B) & 0x821)) >> 1)

#define UPSCALE_256__WEIGHT_1_3(A, B, out, tmp)      \
   tmp = ((A + B + ((A ^ B) & 0x821)) >> 1);         \
   *(out) = ((tmp + B - ((tmp ^ B) & 0x821)) >> 1)

#define UPSCALE_256__WEIGHT_3_1(A, B, out, tmp)      \
   tmp = ((A + B + ((A ^ B) & 0x821)) >> 1);         \
   *(out) = ((A + tmp - ((A ^ tmp) & 0x821)) >> 1)

/* Upscales a 256x224 image to 320x240 using an approximate bilinear
 * resampling algorithm that only uses integer math */
void upscale_256x224_to_320x240(uint16_t *dst, const uint16_t *src,
      uint16_t dst_stride, uint16_t src_stride)
{
   /* There are 64 blocks of 4 pixels horizontally, and 14 of 16 vertically
    * Each block of 4x16 becomes 5x17 */
   uint32_t block_x;
   uint32_t block_y;

   for (block_y = 0; block_y < 14; block_y++)
   {
      const uint16_t *block_src = src + block_y * src_stride * 16;
      uint16_t *block_dst       = dst + block_y * dst_stride * 17;

      for (block_x = 0; block_x < 64; block_x++)
      {
         const uint16_t *block_src_ptr = block_src;
         uint16_t *block_dst_ptr       = block_dst;

         uint16_t _1,   _2,  _3,  _4,
                  _5,   _6,  _7,  _8,
                  _9,  _10, _11, _12,
                  _13, _14, _15, _16,
                  _17, _18, _19, _20,
                  _21, _22, _23, _24,
                  _25, _26, _27, _28,
                  _29, _30, _31, _32,
                  _33, _34, _35, _36,
                  _37, _38, _39, _40,
                  _41, _42, _43, _44,
                  _45, _46, _47, _48,
                  _49, _50, _51, _52,
                  _53, _54, _55, _56,
                  _57, _58, _59, _60,
                  _61, _62, _63, _64;

         uint16_t  _9_10_weight_1_3;
         uint16_t _11_12_weight_3_1;
         uint16_t _13_14_weight_1_3;
         uint16_t _10_11_weight_1_1;
         uint16_t _14_15_weight_1_1;
         uint16_t _15_16_weight_3_1;
         uint16_t _17_18_weight_1_3;
         uint16_t _18_19_weight_1_1;
         uint16_t _19_20_weight_3_1;
         uint16_t _21_22_weight_1_3;
         uint16_t _22_23_weight_1_1;
         uint16_t _23_24_weight_3_1;
         uint16_t _25_26_weight_1_3;
         uint16_t _26_27_weight_1_1;
         uint16_t _27_28_weight_3_1;
         uint16_t _29_30_weight_1_3;
         uint16_t _30_31_weight_1_1;
         uint16_t _31_32_weight_3_1;
         uint16_t _33_34_weight_1_3;
         uint16_t _34_35_weight_1_1;
         uint16_t _35_36_weight_3_1;
         uint16_t _37_38_weight_1_3;
         uint16_t _38_39_weight_1_1;
         uint16_t _39_40_weight_3_1;
         uint16_t _41_42_weight_1_3;
         uint16_t _42_43_weight_1_1;
         uint16_t _43_44_weight_3_1;
         uint16_t _45_46_weight_1_3;
         uint16_t _46_47_weight_1_1;
         uint16_t _47_48_weight_3_1;
         uint16_t _49_50_weight_1_3;
         uint16_t _50_51_weight_1_1;
         uint16_t _51_52_weight_3_1;
         uint16_t _53_54_weight_1_3;
         uint16_t _54_55_weight_1_1;
         uint16_t _55_56_weight_3_1;

         uint16_t tmp;

         /* Horizontally:
          * Before(4):
          * (a)(b)(c)(d)
          * After(5):
          * (a)(abbb)(bc)(cccd)(d)
          *
          * Vertically:
          * Before(16): After(17):
          * (a)       (a)
          * (b)       (b)
          * (c)       (c)
          * (d)       (cddd)
          * (e)       (deee)
          * (f)       (efff)
          * (g)       (fggg)
          * (h)       (ghhh)
          * (i)       (hi)
          * (j)       (iiij)
          * (k)       (jjjk)
          * (l)       (kkkl)
          * (m)       (lllm)
          * (n)       (mmmn)
          * (o)       (n)
          * (p)       (o)
          *           (p)
          */

         /* -- Row 1 -- */
         _1  = *(block_src_ptr    );
         _2  = *(block_src_ptr + 1);
         _3  = *(block_src_ptr + 2);
         _4  = *(block_src_ptr + 3);

         *(block_dst_ptr    ) = _1;
         UPSCALE_256__WEIGHT_1_3( _1,  _2, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1( _2,  _3, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1( _3,  _4, block_dst_ptr + 3, tmp);
         *(block_dst_ptr + 4) = _4;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 2 -- */
         _5  = *(block_src_ptr    );
         _6  = *(block_src_ptr + 1);
         _7  = *(block_src_ptr + 2);
         _8  = *(block_src_ptr + 3);

         *(block_dst_ptr    ) = _5;
         UPSCALE_256__WEIGHT_1_3( _5,  _6, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1( _6,  _7, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1( _7,  _8, block_dst_ptr + 3, tmp);
         *(block_dst_ptr + 4) = _8;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 3 -- */
         _9  = *(block_src_ptr    );
         _10 = *(block_src_ptr + 1);
         _11 = *(block_src_ptr + 2);
         _12 = *(block_src_ptr + 3);

         *(block_dst_ptr    ) = _9;
         UPSCALE_256__WEIGHT_1_3( _9, _10, &_9_10_weight_1_3, tmp);
         *(block_dst_ptr + 1) = _9_10_weight_1_3;
         UPSCALE_256__WEIGHT_1_1(_10, _11, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_11, _12, &_11_12_weight_3_1, tmp);
         *(block_dst_ptr + 3) = _11_12_weight_3_1;
         *(block_dst_ptr + 4) = _12;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 4 -- */
         _13 = *(block_src_ptr    );
         _14 = *(block_src_ptr + 1);
         _15 = *(block_src_ptr + 2);
         _16 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_1_3( _9, _13, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_13, _14, &_13_14_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_1_3(_9_10_weight_1_3, _13_14_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_10, _11, &_10_11_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_1_1(_14, _15, &_14_15_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_10_11_weight_1_1, _14_15_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_15, _16, &_15_16_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_11_12_weight_3_1, _15_16_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_1_3(_12, _16, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 5 -- */
         _17 = *(block_src_ptr    );
         _18 = *(block_src_ptr + 1);
         _19 = *(block_src_ptr + 2);
         _20 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_1_3(_13, _17, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_17, _18, &_17_18_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_1_3(_13_14_weight_1_3, _17_18_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_18, _19, &_18_19_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_14_15_weight_1_1, _18_19_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_19, _20, &_19_20_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_15_16_weight_3_1, _19_20_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_1_3(_16, _20, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 6 -- */
         _21 = *(block_src_ptr    );
         _22 = *(block_src_ptr + 1);
         _23 = *(block_src_ptr + 2);
         _24 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_1_3(_17, _21, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_21, _22, &_21_22_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_1_3(_17_18_weight_1_3, _21_22_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_22, _23, &_22_23_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_18_19_weight_1_1, _22_23_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_23, _24, &_23_24_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_19_20_weight_3_1, _23_24_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_1_3(_20, _24, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 7 -- */
         _25 = *(block_src_ptr    );
         _26 = *(block_src_ptr + 1);
         _27 = *(block_src_ptr + 2);
         _28 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_1_3(_21, _25, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_25, _26, &_25_26_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_1_3(_21_22_weight_1_3, _25_26_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_26, _27, &_26_27_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_22_23_weight_1_1, _26_27_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_27, _28, &_27_28_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_23_24_weight_3_1, _27_28_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_1_3(_24, _28, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 8 -- */
         _29 = *(block_src_ptr    );
         _30 = *(block_src_ptr + 1);
         _31 = *(block_src_ptr + 2);
         _32 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_1_3(_25, _29, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_29, _30, &_29_30_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_1_3(_25_26_weight_1_3, _29_30_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_30, _31, &_30_31_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_26_27_weight_1_1, _30_31_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_31, _32, &_31_32_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_1_3(_27_28_weight_3_1, _31_32_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_1_3(_28, _32, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 9 -- */
         _33 = *(block_src_ptr    );
         _34 = *(block_src_ptr + 1);
         _35 = *(block_src_ptr + 2);
         _36 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_1_1(_29, _33, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_33, _34, &_33_34_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_1_1(_29_30_weight_1_3, _33_34_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_34, _35, &_34_35_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_1_1(_30_31_weight_1_1, _34_35_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_35, _36, &_35_36_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_1_1(_31_32_weight_3_1, _35_36_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_1_1(_32, _36, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 10 -- */
         _37 = *(block_src_ptr    );
         _38 = *(block_src_ptr + 1);
         _39 = *(block_src_ptr + 2);
         _40 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_3_1(_33, _37, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_37, _38, &_37_38_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_3_1(_33_34_weight_1_3, _37_38_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_38, _39, &_38_39_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_34_35_weight_1_1, _38_39_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_39, _40, &_39_40_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_35_36_weight_3_1, _39_40_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_3_1(_36, _40, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 11 -- */
         _41 = *(block_src_ptr    );
         _42 = *(block_src_ptr + 1);
         _43 = *(block_src_ptr + 2);
         _44 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_3_1(_37, _41, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_41, _42, &_41_42_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_3_1(_37_38_weight_1_3, _41_42_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_42, _43, &_42_43_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_38_39_weight_1_1, _42_43_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_43, _44, &_43_44_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_39_40_weight_3_1, _43_44_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_3_1(_40, _44, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 12 -- */
         _45 = *(block_src_ptr    );
         _46 = *(block_src_ptr + 1);
         _47 = *(block_src_ptr + 2);
         _48 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_3_1(_41, _45, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_45, _46, &_45_46_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_3_1(_41_42_weight_1_3, _45_46_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_46, _47, &_46_47_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_42_43_weight_1_1, _46_47_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_47, _48, &_47_48_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_43_44_weight_3_1, _47_48_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_3_1(_44, _48, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 13 -- */
         _49 = *(block_src_ptr    );
         _50 = *(block_src_ptr + 1);
         _51 = *(block_src_ptr + 2);
         _52 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_3_1(_45, _49, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_49, _50, &_49_50_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_3_1(_45_46_weight_1_3, _49_50_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_50, _51, &_50_51_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_46_47_weight_1_1, _50_51_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_51, _52, &_51_52_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_47_48_weight_3_1, _51_52_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_3_1(_48, _52, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 14 -- */
         _53 = *(block_src_ptr    );
         _54 = *(block_src_ptr + 1);
         _55 = *(block_src_ptr + 2);
         _56 = *(block_src_ptr + 3);

         UPSCALE_256__WEIGHT_3_1(_49, _53, block_dst_ptr, tmp);
         UPSCALE_256__WEIGHT_1_3(_53, _54, &_53_54_weight_1_3, tmp);
         UPSCALE_256__WEIGHT_3_1(_49_50_weight_1_3, _53_54_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_54, _55, &_54_55_weight_1_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_50_51_weight_1_1, _54_55_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_55, _56, &_55_56_weight_3_1, tmp);
         UPSCALE_256__WEIGHT_3_1(_51_52_weight_3_1, _55_56_weight_3_1, block_dst_ptr + 3, tmp);
         UPSCALE_256__WEIGHT_3_1(_52, _56, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 15 -- */
         *(block_dst_ptr    ) = _53;
         *(block_dst_ptr + 1) = _53_54_weight_1_3;
         *(block_dst_ptr + 2) = _54_55_weight_1_1;
         *(block_dst_ptr + 3) = _55_56_weight_3_1;
         *(block_dst_ptr + 4) = _56;

         block_dst_ptr += dst_stride;

         /* -- Row 16 -- */
         _57 = *(block_src_ptr    );
         _58 = *(block_src_ptr + 1);
         _59 = *(block_src_ptr + 2);
         _60 = *(block_src_ptr + 3);

         *(block_dst_ptr    ) = _57;
         UPSCALE_256__WEIGHT_1_3(_57, _58, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_58, _59, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_59, _60, block_dst_ptr + 3, tmp);
         *(block_dst_ptr + 4) = _60;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 17 -- */
         _61 = *(block_src_ptr    );
         _62 = *(block_src_ptr + 1);
         _63 = *(block_src_ptr + 2);
         _64 = *(block_src_ptr + 3);

         *(block_dst_ptr    ) = _61;
         UPSCALE_256__WEIGHT_1_3(_61, _62, block_dst_ptr + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_62, _63, block_dst_ptr + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_63, _64, block_dst_ptr + 3, tmp);
         *(block_dst_ptr + 4) = _64;

         block_src += 4;
         block_dst += 5;
      }
   }

   /* The above scaling produces an output image 238 pixels high
    * > Last two rows must be zeroed out */
   memset(dst + (238 * dst_stride), 0, sizeof(uint16_t) * dst_stride);
   memset(dst + (239 * dst_stride), 0, sizeof(uint16_t) * dst_stride);
}

void upscale_256x239_to_320x240(uint16_t *dst, const uint16_t *src,
      uint16_t dst_stride, uint16_t src_stride)
{
   /* There are 64 blocks of 4 pixels horizontally, and 239 of 1 vertically
    * Each block of 4x1 becomes 5x1 */
   uint32_t block_x;
   uint32_t block_y;

   for (block_y = 0; block_y < 239; block_y++)
   {
      const uint16_t *block_src = src + block_y * src_stride;
      uint16_t *block_dst       = dst + block_y * dst_stride;

      for (block_x = 0; block_x < 64; block_x++)
      {
         uint16_t tmp;

         /* Horizontally:
          * Before(4):
          * (a)(b)(c)(d)
          * After(5):
          * (a)(abbb)(bc)(cccd)(d)
          */

         /* Get source samples */
         uint16_t _1 = *(block_src    );
         uint16_t _2 = *(block_src + 1);
         uint16_t _3 = *(block_src + 2);
         uint16_t _4 = *(block_src + 3);

         /* Write destination samples */
         *(block_dst    ) = _1;
         UPSCALE_256__WEIGHT_1_3(_1, _2, block_dst + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_2, _3, block_dst + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_3, _4, block_dst + 3, tmp);
         *(block_dst + 4) = _4;

         block_src += 4;
         block_dst += 5;
      }
   }

   /* The above scaling produces an output image 239 pixels high
    * > Last row must be zeroed out */
   memset(dst + (239 * dst_stride), 0, sizeof(uint16_t) * dst_stride);
}

void upscale_256x240_to_320x240(uint16_t *dst, const uint16_t *src,
      uint16_t dst_stride, uint16_t src_stride)
{
   /* There are 64 blocks of 4 pixels horizontally, and 240 of 1 vertically
    * Each block of 4x1 becomes 5x1 */
   uint32_t block_x;
   uint32_t block_y;

   for (block_y = 0; block_y < 240; block_y++)
   {
      const uint16_t *block_src = src + block_y * src_stride;
      uint16_t *block_dst       = dst + block_y * dst_stride;

      for (block_x = 0; block_x < 64; block_x++)
      {
         uint16_t tmp;

         /* Horizontally:
          * Before(4):
          * (a)(b)(c)(d)
          * After(5):
          * (a)(abbb)(bc)(cccd)(d)
          */

         /* Get source samples */
         uint16_t _1 = *(block_src    );
         uint16_t _2 = *(block_src + 1);
         uint16_t _3 = *(block_src + 2);
         uint16_t _4 = *(block_src + 3);

         /* Write destination samples */
         *(block_dst    ) = _1;
         UPSCALE_256__WEIGHT_1_3(_1, _2, block_dst + 1, tmp);
         UPSCALE_256__WEIGHT_1_1(_2, _3, block_dst + 2, tmp);
         UPSCALE_256__WEIGHT_3_1(_3, _4, block_dst + 3, tmp);
         *(block_dst + 4) = _4;

         block_src += 4;
         block_dst += 5;
      }
   }
}

/*******************************************************************
 *******************************************************************/

static unsigned upscale_256x_320x240_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565;
}

static unsigned upscale_256x_320x240_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned upscale_256x_320x240_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *upscale_256x_320x240_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   (void)simd;
   (void)config;
   (void)userdata;

   if (!filt) {
      return NULL;
   }
   /* Apparently the code is not thread-safe,
    * so force single threaded operation... */
   filt->workers = (struct softfilter_thread_data*)calloc(1, sizeof(struct softfilter_thread_data));
   filt->threads = 1;
   filt->in_fmt  = in_fmt;
   if (!filt->workers) {
      free(filt);
      return NULL;
   }
   return filt;
}

static void upscale_256x_320x240_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   if ((width == 256) &&
       ((height == 224) || (height == 240) || (height == 239)))
   {
      *out_width  = 320;
      *out_height = 240;
   }
   else
   {
      *out_width  = width;
      *out_height = height;
   }
}

static void upscale_256x_320x240_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void upscale_256x_320x240_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   unsigned width                     = thr->width;
   unsigned height                    = thr->height;

   if (width == 256)
   {
      if (height == 224)
      {
         upscale_256x224_to_320x240(output, input, out_stride, in_stride);
         return;
      }
      else if (height == 240)
      {
         upscale_256x240_to_320x240(output, input, out_stride, in_stride);
         return;
      }
      else if (height == 239)
      {
         upscale_256x239_to_320x240(output, input, out_stride, in_stride);
         return;
      }
   }

   /* Input buffer is of dimensions that cannot be upscaled
    * > Simply copy input to output */

   /* If source and destination buffers have the
    * same pitch, perform fast copy of raw pixel data */
   if (in_stride == out_stride)
      memcpy(output, input, thr->out_pitch * height);
   else
   {
      /* Otherwise copy pixel data line-by-line */
      unsigned y;
      for (y = 0; y < height; y++)
      {
         memcpy(output, input, width * sizeof(uint16_t));
         input  += in_stride;
         output += out_stride;
      }
   }
}

static void upscale_256x_320x240_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   /* We are guaranteed single threaded operation
    * (filt->threads = 1) so we don't need to loop
    * over threads and can cull some code. This only
    * makes the tiniest performance difference, but
    * every little helps when running on an o3DS... */
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[0];

   thr->out_data = (uint8_t*)output;
   thr->in_data = (const uint8_t*)input;
   thr->out_pitch = output_stride;
   thr->in_pitch = input_stride;
   thr->width = width;
   thr->height = height;

   if (filt->in_fmt == SOFTFILTER_FMT_RGB565) {
      packets[0].work = upscale_256x_320x240_work_cb_rgb565;
   }
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation upscale_256x_320x240_generic = {
   upscale_256x_320x240_generic_input_fmts,
   upscale_256x_320x240_generic_output_fmts,

   upscale_256x_320x240_generic_create,
   upscale_256x_320x240_generic_destroy,

   upscale_256x_320x240_generic_threads,
   upscale_256x_320x240_generic_output,
   upscale_256x_320x240_generic_packets,

   SOFTFILTER_API_VERSION,
   "Upscale_256x-320x240",
   "upscale_256x_320x240",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &upscale_256x_320x240_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
