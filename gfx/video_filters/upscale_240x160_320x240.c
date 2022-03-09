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

typedef struct
{
   void (*upscale_240x160_320x240)(
         uint16_t *dst, const uint16_t *src,
         uint16_t dst_stride, uint16_t src_stride);
} upscale_function_t;

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
   upscale_function_t function;
};

/*******************************************************************
 * Approximately bilinear scaler, 240x160 to 320x240
 * Copyright (C) 2014 hi-ban, Nebuleon <nebuleon.fumika@gmail.com>
 * (Optimisations by jdgleaver)
 *******************************************************************/

#define UPSCALE_240__WEIGHT(A, B, out) \
   *(out) = ((A + B + ((A ^ B) & 0x821)) >> 1)

/* Upscales a 240x160 image to 320x240 using an approximate bilinear
 * resampling algorithm that only uses integer math */
void upscale_240x160_to_320x240(uint16_t *dst, const uint16_t *src,
      uint16_t dst_stride, uint16_t src_stride)
{
   /* There are 80 blocks of 3 pixels horizontally,
    * and 80 blocks of 2 pixels vertically
    * Each block of 3x2 becomes 4x3 */
   uint32_t block_x;
   uint32_t block_y;

   for (block_y = 0; block_y < 80; block_y++) 
   { 
      const uint16_t *block_src = src + block_y * src_stride * 2;
      uint16_t *block_dst       = dst + block_y * dst_stride * 3;

      for (block_x = 0; block_x < 80; block_x++)
      {
         const uint16_t *block_src_ptr = block_src;
         uint16_t *block_dst_ptr       = block_dst;

         uint16_t _1, _2, _3,
                  _4, _5, _6;

         uint16_t _1_2;
         uint16_t _2_3;
         uint16_t _4_5;
         uint16_t _5_6;

         /* Horizontally:
          * Before(3):
          * (a)(b)(c)
          * After(4):
          * (a)(ab)(bc)(c)
          *
          * Vertically:
          * Before(2): After(3):
          * (a)        (a)
          * (b)        (ab)
          *            (b)
          */

         /* -- Row 1 -- */
         _1 = *(block_src_ptr    );
         _2 = *(block_src_ptr + 1);
         _3 = *(block_src_ptr + 2);

         *(block_dst_ptr    ) = _1;
         UPSCALE_240__WEIGHT(_1, _2, &_1_2);
         *(block_dst_ptr + 1) = _1_2;
         UPSCALE_240__WEIGHT(_2, _3, &_2_3);
         *(block_dst_ptr + 2) = _2_3;
         *(block_dst_ptr + 3) = _3;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 2 -- */
         _4 = *(block_src_ptr    );
         _5 = *(block_src_ptr + 1);
         _6 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT(_1, _4, block_dst_ptr);
         UPSCALE_240__WEIGHT(_4, _5, &_4_5);
         UPSCALE_240__WEIGHT(_1_2, _4_5, block_dst_ptr + 1);
         UPSCALE_240__WEIGHT(_5, _6, &_5_6);
         UPSCALE_240__WEIGHT(_2_3, _5_6, block_dst_ptr + 2);
         UPSCALE_240__WEIGHT(_3, _6, block_dst_ptr + 3);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 3 -- */
         *(block_dst_ptr    ) = _4;
         UPSCALE_240__WEIGHT(_4, _5, block_dst_ptr + 1);
         UPSCALE_240__WEIGHT(_5, _6, block_dst_ptr + 2);
         *(block_dst_ptr + 3) = _6;

         block_src += 3;
         block_dst += 4;
      }
   }
}

/* Upscales a 240x160 image to 320x213 (padding the result
 * to 320x240 via letterboxing) using an approximate bilinear
 * resampling algorithm that only uses integer math */
void upscale_240x160_to_320x240_aspect(uint16_t *dst, const uint16_t *src,
      uint16_t dst_stride, uint16_t src_stride)
{  
   /* There are 80 blocks of 3 pixels horizontally,
    * and 53 blocks of 3 pixels vertically
    * Each block of 3x3 becomes 4x4 */
   uint32_t block_x;
   uint32_t block_y;
   const uint16_t *block_src = NULL;
   uint16_t *block_dst       = NULL;

   /* Letterboxing - zero out first 13 rows */
   memset(dst, 0, sizeof(uint16_t) * dst_stride * 13);

   /* Scale blocks from 3x3 to 4x4 */
   for (block_y = 0; block_y < 53; block_y++) 
   { 
      block_src = src + block_y * src_stride * 3;
      block_dst = (dst + (13 * dst_stride)) + block_y * dst_stride * 4;

      for (block_x = 0; block_x < 80; block_x++)
      {
         const uint16_t *block_src_ptr = block_src;
         uint16_t *block_dst_ptr       = block_dst;

         uint16_t _1, _2, _3,
                  _4, _5, _6,
                  _7, _8, _9;

         uint16_t _1_2;
         uint16_t _2_3;
         uint16_t _4_5;
         uint16_t _5_6;
         uint16_t _7_8;
         uint16_t _8_9;

         /* Horizontally:
          * Before(3):
          * (a)(b)(c)
          * After(4):
          * (a)(ab)(bc)(c)
          *
          * Vertically:
          * Before(2): After(3):
          * (a)        (a)
          * (b)        (ab)
          * (c)        (bc)
          *            (c)
          */

         /* -- Row 1 -- */
         _1 = *(block_src_ptr    );
         _2 = *(block_src_ptr + 1);
         _3 = *(block_src_ptr + 2);

         *(block_dst_ptr    ) = _1;
         UPSCALE_240__WEIGHT(_1, _2, &_1_2);
         *(block_dst_ptr + 1) = _1_2;
         UPSCALE_240__WEIGHT(_2, _3, &_2_3);
         *(block_dst_ptr + 2) = _2_3;
         *(block_dst_ptr + 3) = _3;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 2 -- */
         _4 = *(block_src_ptr    );
         _5 = *(block_src_ptr + 1);
         _6 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT(_1, _4, block_dst_ptr);
         UPSCALE_240__WEIGHT(_4, _5, &_4_5);
         UPSCALE_240__WEIGHT(_1_2, _4_5, block_dst_ptr + 1);
         UPSCALE_240__WEIGHT(_5, _6, &_5_6);
         UPSCALE_240__WEIGHT(_2_3, _5_6, block_dst_ptr + 2);
         UPSCALE_240__WEIGHT(_3, _6, block_dst_ptr + 3);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 3 -- */
         _7 = *(block_src_ptr    );
         _8 = *(block_src_ptr + 1);
         _9 = *(block_src_ptr + 2);

         UPSCALE_240__WEIGHT(_4, _7, block_dst_ptr);
         UPSCALE_240__WEIGHT(_7, _8, &_7_8);
         UPSCALE_240__WEIGHT(_4_5, _7_8, block_dst_ptr + 1);
         UPSCALE_240__WEIGHT(_8, _9, &_8_9);
         UPSCALE_240__WEIGHT(_5_6, _8_9, block_dst_ptr + 2);
         UPSCALE_240__WEIGHT(_6, _9, block_dst_ptr + 3);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 4 -- */
         *(block_dst_ptr    ) = _7;
         *(block_dst_ptr + 1) = _7_8;
         *(block_dst_ptr + 2) = _8_9;
         *(block_dst_ptr + 3) = _9;

         block_src += 3;
         block_dst += 4;
      }
   }

   /* Above scaling excludes the last row of the
    * source image. Handle this separately. */
   block_src = src + (src_stride * 159);
   block_dst = dst + (225 * dst_stride);

   for (block_x = 0; block_x < 80; block_x++)
   {
      const uint16_t *block_src_ptr = block_src;
      uint16_t *block_dst_ptr       = block_dst;

      uint16_t _1, _2, _3;

      /* Horizontally:
       * Before(3):
       * (a)(b)(c)
       * After(4):
       * (a)(ab)(bc)(c)
       */

      /* -- Row 1 -- */
      _1 = *(block_src_ptr    );
      _2 = *(block_src_ptr + 1);
      _3 = *(block_src_ptr + 2);

      *(block_dst_ptr    ) = _1;
      UPSCALE_240__WEIGHT(_1, _2, block_dst_ptr + 1);
      UPSCALE_240__WEIGHT(_2, _3, block_dst_ptr + 2);
      *(block_dst_ptr + 3) = _3;

      block_src += 3;
      block_dst += 4;
   }

   /* Letterboxing - zero out last 14 rows */
   memset(dst + (226 * dst_stride), 0, sizeof(uint16_t) * dst_stride * 14);
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

static void upscale_240x160_320x240_initialize(struct filter_data *filt,
      const struct softfilter_config *config,
      void *userdata)
{
   int keep_aspect = 1;

   /* Assign default scaling functions */
   filt->function.upscale_240x160_320x240 = upscale_240x160_to_320x240_aspect;

   /* Read aspect ratio correction setting */
   if (config->get_int(userdata, "keep_aspect", &keep_aspect, 1) &&
       !keep_aspect)
      filt->function.upscale_240x160_320x240 = upscale_240x160_to_320x240;
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

   /* Assign scaling functions */
   upscale_240x160_320x240_initialize(filt, config, userdata);

   return filt;
}

static void upscale_240x160_320x240_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   if ((width == 240) && (height == 160))
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
   struct filter_data *filt           = (struct filter_data*)data;   
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   unsigned width                     = thr->width;
   unsigned height                    = thr->height;

   if ((width == 240) && (height == 160))
   {
      filt->function.upscale_240x160_320x240(output, input, out_stride, in_stride);
      return;
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
