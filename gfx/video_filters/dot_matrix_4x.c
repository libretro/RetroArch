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

/* Compile: gcc -o dot_matrix_4x.so -shared dot_matrix_4x.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation dot_matrix_4x_get_implementation
#define softfilter_thread_data dot_matrix_4x_softfilter_thread_data
#define filter_data dot_matrix_4x_filter_data
#endif

/* Default grid colour: pure white */
#define DOT_MATRIX_4X_DEFAULT_GRID_COLOR 0xFFFFFF

#define DOT_MATRIX_3X_WEIGHT_10_6(c10, c6) (((c10 << 3) + (c10 << 1) + (c6 << 2) + (c6 << 1)) >> 4)

typedef struct
{
   struct
   {
      uint32_t r;
      uint32_t g;
      uint32_t b;
   } xrgb8888;

   struct
   {
      uint16_t r;
      uint16_t g;
      uint16_t b;
   } rgb565;

} dot_matrix_4x_grid_color_t;

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
   dot_matrix_4x_grid_color_t grid_color;
};

static unsigned dot_matrix_4x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565 | SOFTFILTER_FMT_XRGB8888;
}

static unsigned dot_matrix_4x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned dot_matrix_4x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void dot_matrix_4x_initialize(struct filter_data *filt,
      const struct softfilter_config *config,
      void *userdata)
{
   unsigned grid_color;

   /* Read raw grid colour */
   config->get_hex(userdata, "grid_color", &grid_color,
         DOT_MATRIX_4X_DEFAULT_GRID_COLOR);

   /* Split into 5bit RGB components */
   filt->grid_color.rgb565.r = (grid_color >> 19) & 0x1F;
   filt->grid_color.rgb565.g = (grid_color >> 11) & 0x1F;
   filt->grid_color.rgb565.b = (grid_color >>  3) & 0x1F;

   /* Split into 8bit RGB components */
   filt->grid_color.xrgb8888.r = (grid_color >> 16) & 0xFF;
   filt->grid_color.xrgb8888.g = (grid_color >>  8) & 0xFF;
   filt->grid_color.xrgb8888.b =  grid_color        & 0xFF;
}

static void *dot_matrix_4x_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   (void)simd;

   if (!filt)
      return NULL;

   /* Apparently the code is not thread-safe,
    * so force single threaded operation... */
   filt->workers = (struct softfilter_thread_data*)calloc(1, sizeof(struct softfilter_thread_data));
   filt->threads = 1;
   filt->in_fmt  = in_fmt;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }

   /* Initialise colour lookup tables */
   dot_matrix_4x_initialize(filt, config, userdata);

   return filt;
}

static void dot_matrix_4x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width  = width  << 2;
   *out_height = height << 2;
}

static void dot_matrix_4x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void dot_matrix_4x_work_cb_rgb565(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   uint16_t base_grid_r               = filt->grid_color.rgb565.r;
   uint16_t base_grid_g               = filt->grid_color.rgb565.g;
   uint16_t base_grid_b               = filt->grid_color.rgb565.b;
   uint16_t x, y;

   for (y = 0; y < thr->height; ++y)
   {
      uint16_t *out_ptr = output;

      for (x = 0; x < thr->width; ++x)
      {
         uint16_t *out_line_ptr = out_ptr;
         uint16_t pixel_color   = *(input + x);
         uint16_t pixel_r       = (pixel_color >> 11 & 0x1F);
         uint16_t pixel_g       = (pixel_color >>  6 & 0x1F);
         uint16_t pixel_b       = (pixel_color       & 0x1F);
         /* Get shadow colour
          * > 10:6 mix of pixel_color:base_grid_color */
         uint16_t shadow_r      = DOT_MATRIX_3X_WEIGHT_10_6(pixel_r, base_grid_r);
         uint16_t shadow_g      = DOT_MATRIX_3X_WEIGHT_10_6(pixel_g, base_grid_g);
         uint16_t shadow_b      = DOT_MATRIX_3X_WEIGHT_10_6(pixel_b, base_grid_b);
         uint16_t shadow_color  = (shadow_r << 11) | (shadow_g << 6) | shadow_b;
         /* Get grid colour
          * > 50:50 mix of pixel_color:base_grid_color */
         uint16_t grid_r        = (pixel_r + base_grid_r) >> 1;
         uint16_t grid_g        = (pixel_g + base_grid_g) >> 1;
         uint16_t grid_b        = (pixel_b + base_grid_b) >> 1;
         uint16_t grid_color    = (grid_r << 11) | (grid_g << 6) | grid_b;

         /* - Pixel layout (p = pixel, s = shadow, g = grid) -
          * Before:  After:
          * (p)      (g)(p)(p)(p)
          *          (s)(p)(p)(p)
          *          (s)(p)(p)(p)
          *          (s)(s)(s)(g)
          */

         /* Row 1: (g)(p)(p)(p) */
         *out_line_ptr       = grid_color;
         *(out_line_ptr + 1) = pixel_color;
         *(out_line_ptr + 2) = pixel_color;
         *(out_line_ptr + 3) = pixel_color;
         out_line_ptr       += out_stride;

         /* Row 2: (s)(p)(p)(p) */
         *out_line_ptr       = shadow_color;
         *(out_line_ptr + 1) = pixel_color;
         *(out_line_ptr + 2) = pixel_color;
         *(out_line_ptr + 3) = pixel_color;
         out_line_ptr       += out_stride;

         /* Row 3: (s)(p)(p)(p) */
         *out_line_ptr       = shadow_color;
         *(out_line_ptr + 1) = pixel_color;
         *(out_line_ptr + 2) = pixel_color;
         *(out_line_ptr + 3) = pixel_color;
         out_line_ptr       += out_stride;

         /* Row 4: (s)(s)(s)(g) */
         *out_line_ptr       = shadow_color;
         *(out_line_ptr + 1) = shadow_color;
         *(out_line_ptr + 2) = shadow_color;
         *(out_line_ptr + 3) = grid_color;

         out_ptr += 4;
      }

      input  += in_stride;
      output += out_stride << 2;
   }
}

static void dot_matrix_4x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *input              = (const uint32_t*)thr->in_data;
   uint32_t *output                   = (uint32_t*)thr->out_data;
   uint32_t in_stride                 = (uint32_t)(thr->in_pitch >> 2);
   uint32_t out_stride                = (uint32_t)(thr->out_pitch >> 2);
   uint32_t base_grid_r               = filt->grid_color.xrgb8888.r;
   uint32_t base_grid_g               = filt->grid_color.xrgb8888.g;
   uint32_t base_grid_b               = filt->grid_color.xrgb8888.b;
   uint32_t x, y;

   for (y = 0; y < thr->height; ++y)
   {
      uint32_t *out_ptr = output;

      for (x = 0; x < thr->width; ++x)
      {
         uint32_t *out_line_ptr = out_ptr;
         uint32_t pixel_color   = *(input + x);
         uint32_t pixel_r       = (pixel_color >> 16 & 0xFF);
         uint32_t pixel_g       = (pixel_color >>  8 & 0xFF);
         uint32_t pixel_b       = (pixel_color       & 0xFF);
         /* Get shadow colour
          * > 10:6 mix of pixel_color:base_grid_color */
         uint32_t shadow_r      = DOT_MATRIX_3X_WEIGHT_10_6(pixel_r, base_grid_r);
         uint32_t shadow_g      = DOT_MATRIX_3X_WEIGHT_10_6(pixel_g, base_grid_g);
         uint32_t shadow_b      = DOT_MATRIX_3X_WEIGHT_10_6(pixel_b, base_grid_b);
         uint32_t shadow_color  = (shadow_r << 16) | (shadow_g << 8) | shadow_b;
         /* Get grid colour
          * > 50:50 mix of pixel_color:base_grid_color */
         uint32_t grid_r        = (pixel_r + base_grid_r) >> 1;
         uint32_t grid_g        = (pixel_g + base_grid_g) >> 1;
         uint32_t grid_b        = (pixel_b + base_grid_b) >> 1;
         uint32_t grid_color    = (grid_r << 16) | (grid_g << 8) | grid_b;

         /* - Pixel layout (p = pixel, s = shadow, g = grid) -
          * Before:  After:
          * (p)      (g)(p)(p)(p)
          *          (s)(p)(p)(p)
          *          (s)(p)(p)(p)
          *          (s)(s)(s)(g)
          */

         /* Row 1: (g)(p)(p)(p) */
         *out_line_ptr       = grid_color;
         *(out_line_ptr + 1) = pixel_color;
         *(out_line_ptr + 2) = pixel_color;
         *(out_line_ptr + 3) = pixel_color;
         out_line_ptr       += out_stride;

         /* Row 2: (s)(p)(p)(p) */
         *out_line_ptr       = shadow_color;
         *(out_line_ptr + 1) = pixel_color;
         *(out_line_ptr + 2) = pixel_color;
         *(out_line_ptr + 3) = pixel_color;
         out_line_ptr       += out_stride;

         /* Row 3: (s)(p)(p)(p) */
         *out_line_ptr       = shadow_color;
         *(out_line_ptr + 1) = pixel_color;
         *(out_line_ptr + 2) = pixel_color;
         *(out_line_ptr + 3) = pixel_color;
         out_line_ptr       += out_stride;

         /* Row 4: (s)(s)(s)(g) */
         *out_line_ptr       = shadow_color;
         *(out_line_ptr + 1) = shadow_color;
         *(out_line_ptr + 2) = shadow_color;
         *(out_line_ptr + 3) = grid_color;

         out_ptr += 4;
      }

      input  += in_stride;
      output += out_stride << 2;
   }
}

static void dot_matrix_4x_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   /* We are guaranteed single threaded operation
    * (filt->threads = 1) so we don't need to loop
    * over threads and can cull some code */
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[0];

   thr->out_data = (uint8_t*)output;
   thr->in_data = (const uint8_t*)input;
   thr->out_pitch = output_stride;
   thr->in_pitch = input_stride;
   thr->width = width;
   thr->height = height;

   if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
      packets[0].work = dot_matrix_4x_work_cb_rgb565;
   else if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
      packets[0].work = dot_matrix_4x_work_cb_xrgb8888;

   packets[0].thread_data = thr;
}

static const struct softfilter_implementation dot_matrix_4x_generic = {
   dot_matrix_4x_generic_input_fmts,
   dot_matrix_4x_generic_output_fmts,

   dot_matrix_4x_generic_create,
   dot_matrix_4x_generic_destroy,

   dot_matrix_4x_generic_threads,
   dot_matrix_4x_generic_output,
   dot_matrix_4x_generic_packets,

   SOFTFILTER_API_VERSION,
   "Dot Matrix 4x",
   "dot_matrix_4x",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &dot_matrix_4x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
