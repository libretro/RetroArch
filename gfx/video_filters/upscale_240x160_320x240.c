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

/* Compile: gcc -o upscale_240x160_320x240.so -shared upscale_240x160_320x240.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation upscale_240x160_320x240_get_implementation
#define softfilter_thread_data upscale_240x160_320x240_softfilter_thread_data
#define filter_data upscale_240x160_320x240_filter_data
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
 * Approximately bilinear scaler, 240x160 to 320x240
 * Copyright (C) 2014 hi-ban, Nebuleon <nebuleon.fumika@gmail.com>
 * (Optimisations by jdgleaver)
 *******************************************************************/

#define UPSCALE_240__WEIGHT_1_1(A, B, out, tmp)      \
   *(out) = ((A + B + ((A ^ B) & 0x821)) >> 1)

#define UPSCALE_240__WEIGHT_1_3(A, B, out, tmp)      \
   tmp = ((A + B + ((A ^ B) & 0x821)) >> 1);         \
   *(out) = ((tmp + B - ((tmp ^ B) & 0x821)) >> 1)

#define UPSCALE_240__WEIGHT_3_1(A, B, out, tmp)      \
   tmp = ((A + B + ((A ^ B) & 0x821)) >> 1);         \
   *(out) = ((A + tmp - ((A ^ tmp) & 0x821)) >> 1)

/* Upscales a 240x160 image to 320x240 using an approximate bilinear
 * resampling algorithm that only uses integer math */
void upscale_240x160_to_320x240(uint16_t *dst, const uint16_t *src,
      uint16_t dst_stride, uint16_t src_stride)
{
   /* There are 80 blocks of 3 pixels horizontally,
    * and 10 blocks of 16 pixels vertically
    * Each block of 3x16 becomes 4x17 */
   uint32_t block_x;
   uint32_t block_y;

   for (block_y = 0; block_y < 10; block_y++) 
   { 
      const uint16_t *block_src = src + block_y * src_stride * 16;
      uint16_t *block_dst       = dst + block_y * dst_stride * 17;

      for (block_x = 0; block_x < 80; block_x++)
      {

         const uint16_t *block_src_ptr = block_src;
         uint16_t *block_dst_ptr       = block_dst;

         uint16_t _1,   _2,  _3,
                  _4,   _5,  _6,
                  _7,   _8,  _9,
                  _10, _11, _12,
                  _13, _14, _15,
                  _16, _17, _18,
                  _19, _20, _21,
                  _22, _23, _24,
                  _25, _26, _27,
                  _28, _29, _30,
                  _31, _32, _33,
                  _34, _35, _36,
                  _37, _38, _39,
                  _40, _41, _42,
                  _43, _44, _45,
                  _46, _47, _48;

         uint16_t   _7_8_weight_1_3;
         uint16_t   _8_9_weight_1_1;
         uint16_t _10_11_weight_1_3;
         uint16_t _11_12_weight_1_1;
         uint16_t _13_14_weight_1_3;
         uint16_t _14_15_weight_1_1;
         uint16_t _16_17_weight_1_3;
         uint16_t _17_18_weight_1_1;
         uint16_t _19_20_weight_1_3;
         uint16_t _20_21_weight_1_1;
         uint16_t _22_23_weight_1_3;
         uint16_t _23_24_weight_1_1;
         uint16_t _25_26_weight_1_3;
         uint16_t _26_27_weight_1_1;
         uint16_t _28_29_weight_1_3;
         uint16_t _29_30_weight_1_1;
         uint16_t _31_32_weight_1_3;
         uint16_t _32_33_weight_1_1;
         uint16_t _34_35_weight_1_3;
         uint16_t _35_36_weight_1_1;
         uint16_t _37_38_weight_1_3;
         uint16_t _38_39_weight_1_1;
         uint16_t _40_41_weight_1_3;
         uint16_t _41_42_weight_1_1;

         uint16_t tmp;

         /* Horizontally:
          * Before(3):
          * (a)(b)(c)
          * After(4):
          * (a)(abbb)(bc)(c)
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
         _1 = *(block_src_ptr    );
         _2 = *(block_src_ptr + 1);
         _3 = *(block_src_ptr + 2);

         *(block_dst_ptr    ) = _1;
         UPSCALE_240__WEIGHT_1_3( _1,  _2, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1( _2,  _3, block_dst_ptr + 2, tmp);
         *(block_dst_ptr + 3) = _3;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 2 -- */
         _4 = *(block_src_ptr    );
         _5 = *(block_src_ptr + 1);
         _6 = *(block_src_ptr + 2);

         *(block_dst_ptr    ) = _4;
         UPSCALE_240__WEIGHT_1_3( _4,  _5, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1( _5,  _6, block_dst_ptr + 2, tmp);
         *(block_dst_ptr + 3) = _6;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 3 -- */    
         _7 = *(block_src_ptr    );
         _8 = *(block_src_ptr + 1);
         _9 = *(block_src_ptr + 2);

         *(block_dst_ptr    ) = _7;  /* TODO check */
         UPSCALE_240__WEIGHT_1_3( _7, _8, &_7_8_weight_1_3, tmp);
         *(block_dst_ptr + 1) = _7_8_weight_1_3;
         UPSCALE_240__WEIGHT_1_1(_8, _9, block_dst_ptr + 2, tmp);
         *(block_dst_ptr + 3) = _9;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 4 -- */
         _10 = *(block_src_ptr    );
         _11 = *(block_src_ptr + 1);
         _12 = *(block_src_ptr + 2);

         /* TODO check */

         UPSCALE_240__WEIGHT_1_3( _7, _10, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_10, _11, &_10_11_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_7_8_weight_1_3, _10_11_weight_1_3, block_dst_ptr + 1, tmp);         
         UPSCALE_240__WEIGHT_1_1(_8, _9, &_8_9_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_11, _12, &_11_12_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_8_9_weight_1_1, _11_12_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_8, _9, &_8_9_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_9, _12, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 5 -- */
         _13 = *(block_src_ptr    );
         _14 = *(block_src_ptr + 1);
         _15 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_10, _13, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_13, _14, &_13_14_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_10_11_weight_1_3, _13_14_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_11, _12, &_11_12_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_14, _15, &_14_15_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_11_12_weight_1_1, _14_15_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_11, _12, &_8_9_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_12, _15, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 6 -- */
         _16 = *(block_src_ptr    );
         _17 = *(block_src_ptr + 1);
         _18 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_13, _16, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_16, _17, &_16_17_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_13_14_weight_1_3, _16_17_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_14, _15, &_14_15_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_17, _18, &_17_18_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_14_15_weight_1_1, _17_18_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_14, _15, &_11_12_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_15, _18, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 7 -- */
         _19 = *(block_src_ptr    );
         _20 = *(block_src_ptr + 1);
         _21 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_16, _19, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_19, _20, &_19_20_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_16_17_weight_1_3, _19_20_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_17, _18, &_17_18_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_20, _21, &_20_21_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_17_18_weight_1_1, _20_21_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_17, _18, &_14_15_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_18, _21, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 8 -- */
         _22 = *(block_src_ptr    );
         _23 = *(block_src_ptr + 1);
         _24 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_19, _22, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_22, _23, &_22_23_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_19_20_weight_1_3, _22_23_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_20, _21, &_20_21_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_23, _24, &_23_24_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_20_21_weight_1_1, _23_24_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_20, _21, &_17_18_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_21, _24, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 9 -- */
         _25 = *(block_src_ptr    );
         _26 = *(block_src_ptr + 1);
         _27 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_22, _25, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_25, _26, &_25_26_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_22_23_weight_1_3, _25_26_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_23, _24, &_23_24_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_26, _27, &_26_27_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_23_24_weight_1_1, _26_27_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_23, _24, &_20_21_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_24, _27, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 10 -- */
         _28 = *(block_src_ptr    );
         _29 = *(block_src_ptr + 1);
         _30 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_25, _28, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_28, _29, &_28_29_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_25_26_weight_1_3, _28_29_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_26, _27, &_26_27_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_29, _30, &_29_30_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_26_27_weight_1_1, _29_30_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_26, _27, &_23_24_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_27, _30, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 11 -- */
         _31 = *(block_src_ptr    );
         _32 = *(block_src_ptr + 1);
         _33 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_28, _31, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_31, _32, &_31_32_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_28_29_weight_1_3, _31_32_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_29, _30, &_29_30_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_32, _33, &_32_33_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_29_30_weight_1_1, _32_33_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_29, _30, &_26_27_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_30, _33, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 12 -- */
         _34 = *(block_src_ptr    );
         _35 = *(block_src_ptr + 1);
         _36 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_31, _34, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_34, _35, &_34_35_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_31_32_weight_1_3, _34_35_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_32, _33, &_32_33_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_35, _36, &_35_36_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_32_33_weight_1_1, _35_36_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_32, _33, &_29_30_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_33, _36, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 13 -- */
         _37 = *(block_src_ptr    );
         _38 = *(block_src_ptr + 1);
         _39 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_34, _37, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_37, _38, &_37_38_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_34_35_weight_1_3, _37_38_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_35, _37, &_35_36_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_38, _39, &_38_39_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_35_36_weight_1_1, _38_39_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_35, _36, &_32_33_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_36, _39, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 14 -- */
         _40 = *(block_src_ptr    );
         _41 = *(block_src_ptr + 1);
         _42 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT_1_3(_37, _40, block_dst_ptr, tmp);
         UPSCALE_240__WEIGHT_1_3(_40, _41, &_40_41_weight_1_3, tmp);
         UPSCALE_240__WEIGHT_1_3(_37_38_weight_1_3, _40_41_weight_1_3, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_38, _40, &_38_39_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_1(_41, _42, &_41_42_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_38_39_weight_1_1, _41_42_weight_1_1, block_dst_ptr + 2, tmp);
         UPSCALE_240__WEIGHT_3_1(_38, _39, &_35_36_weight_1_1, tmp);
         UPSCALE_240__WEIGHT_1_3(_39, _42, block_dst_ptr + 3, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 15 -- */
         *(block_dst_ptr    ) = _40;
         *(block_dst_ptr + 1) = _40_41_weight_1_3;
         *(block_dst_ptr + 2) = _41_42_weight_1_1;
         *(block_dst_ptr + 3) = _42;

         block_dst_ptr += dst_stride;

         /* -- Row 16 -- */
         _43 = *(block_src_ptr    );
         _44 = *(block_src_ptr + 1);
         _45 = *(block_src_ptr + 2);

         *(block_dst_ptr    ) = _43;
         UPSCALE_240__WEIGHT_1_3(_43, _44, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_44, _45, block_dst_ptr + 2, tmp);
         *(block_dst_ptr + 3) = _45;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 17 -- */
         _46 = *(block_src_ptr    );
         _47 = *(block_src_ptr + 1);
         _48 = *(block_src_ptr + 2);

         *(block_dst_ptr    ) = _46;
         UPSCALE_240__WEIGHT_1_3(_46, _47, block_dst_ptr + 1, tmp);
         UPSCALE_240__WEIGHT_1_1(_47, _48, block_dst_ptr + 2, tmp);
         *(block_dst_ptr + 3) = _48;

         block_src += 3;
         block_dst += 4;
      }
   }

   /* The above scaling produces an output image 238 pixels high
    * > Last two rows must be zeroed out */
   memset(dst + (238 * dst_stride), 0, sizeof(uint16_t) * dst_stride);
   memset(dst + (239 * dst_stride), 0, sizeof(uint16_t) * dst_stride);
}

/*******************************************************************
 *******************************************************************/

static unsigned upscale_240x160_320x240_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565;
}

static unsigned upscale_240x160_320x240_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned upscale_240x160_320x240_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *upscale_240x160_320x240_generic_create(const struct softfilter_config *config,
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

static void upscale_240x160_320x240_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   if ((width == 240) &&
       (height == 160))
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

static void upscale_240x160_320x240_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void upscale_240x160_320x240_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   unsigned width                     = thr->width;
   unsigned height                    = thr->height;

   if (width == 240)
   {
      if (height == 160)
      {
         upscale_240x160_to_320x240(output, input, out_stride, in_stride);
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

static void upscale_240x160_320x240_generic_packets(void *data,
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
      packets[0].work = upscale_240x160_320x240_work_cb_rgb565;
   }
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation upscale_240x160_320x240_generic = {
   upscale_240x160_320x240_generic_input_fmts,
   upscale_240x160_320x240_generic_output_fmts,

   upscale_240x160_320x240_generic_create,
   upscale_240x160_320x240_generic_destroy,

   upscale_240x160_320x240_generic_threads,
   upscale_240x160_320x240_generic_output,
   upscale_240x160_320x240_generic_packets,

   SOFTFILTER_API_VERSION,
   "Upscale_240x160-320x240",
   "upscale_240x160_320x240",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &upscale_240x160_320x240_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
