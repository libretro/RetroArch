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

/* Compile: gcc -o gameboy3x.so -shared gameboy3x.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation gameboy3x_get_implementation
#define softfilter_thread_data gameboy3x_softfilter_thread_data
#define filter_data gameboy3x_filter_data
#endif

/* Default colours match Gambatte's
 * Gameboy Pocket palette */
#define GAMEBOY_3X_DEFAULT_PALETTE_0    0x2A3325
#define GAMEBOY_3X_DEFAULT_PALETTE_1    0x535f49
#define GAMEBOY_3X_DEFAULT_PALETTE_2    0x86927C
#define GAMEBOY_3X_DEFAULT_PALETTE_3    0xA7B19A
#define GAMEBOY_3X_DEFAULT_PALETTE_GRID 0xADB8A0

#define GAMEBOY_3X_RGB24_TO_RGB565(rgb24) ( ((rgb24 >> 8) & 0xF800) | ((rgb24 >> 5) & 0x7E0) | ((rgb24 >> 3) & 0x1F) )

typedef struct
{
   struct
   {
      uint32_t pixel_lut[4];
      uint32_t grid_lut[4];
   } xrgb8888;

   struct
   {
      uint16_t pixel_lut[4];
      uint16_t grid_lut[4];
   } rgb565;

} gameboy3x_colors_t;

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
   gameboy3x_colors_t colors;
};

static unsigned gameboy3x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565 | SOFTFILTER_FMT_XRGB8888;
}

static unsigned gameboy3x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned gameboy3x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static uint32_t gameboy3x_get_grid_colour(unsigned palette, unsigned grid)
{
   /* > Grid colour is a 3:2 mix of palette:grid
    * > We only have four pixel colours, so can
    *   pre-calculate everything in advance */
   uint32_t palette_r = (palette & 0xFF0000) >> 16;
   uint32_t palette_g = (palette &   0xFF00) >> 8;
   uint32_t palette_b = (palette &     0xFF);

   uint32_t grid_r    = (grid    & 0xFF0000) >> 16;
   uint32_t grid_g    = (grid    &   0xFF00) >> 8;
   uint32_t grid_b    = (grid    &     0xFF);

   uint32_t mix_r     = ((3 * palette_r) + (2 * grid_r)) / 5;
   uint32_t mix_g     = ((3 * palette_g) + (2 * grid_g)) / 5;
   uint32_t mix_b     = ((3 * palette_b) + (2 * grid_b)) / 5;

   return (mix_r << 16) | (mix_g << 8) | mix_b;
}

static void gameboy3x_initialize(struct filter_data *filt,
      const struct softfilter_config *config,
      void *userdata)
{
   unsigned palette[4];
   unsigned palette_grid;
   size_t i;

   /* Read raw colour values */
   config->get_hex(userdata, "palette_0", &palette[0],
         GAMEBOY_3X_DEFAULT_PALETTE_0);
   config->get_hex(userdata, "palette_1", &palette[1],
         GAMEBOY_3X_DEFAULT_PALETTE_1);
   config->get_hex(userdata, "palette_2", &palette[2],
         GAMEBOY_3X_DEFAULT_PALETTE_2);
   config->get_hex(userdata, "palette_3", &palette[3],
         GAMEBOY_3X_DEFAULT_PALETTE_3);
   config->get_hex(userdata, "palette_grid", &palette_grid,
         GAMEBOY_3X_DEFAULT_PALETTE_GRID);

   /* Loop over palette colours */
   for (i = 0; i < 4; i++)
   {
      uint32_t grid_color;

      /* Populate pixel lookup tables */
      filt->colors.rgb565.pixel_lut[i]   = GAMEBOY_3X_RGB24_TO_RGB565(palette[i]);
      filt->colors.xrgb8888.pixel_lut[i] = palette[i];

      /* Populate grid lookup tables */
      grid_color = gameboy3x_get_grid_colour(palette[i], palette_grid);

      filt->colors.rgb565.grid_lut[i]   = GAMEBOY_3X_RGB24_TO_RGB565(grid_color);
      filt->colors.xrgb8888.grid_lut[i] = grid_color;
   }
}

static void *gameboy3x_generic_create(const struct softfilter_config *config,
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
   gameboy3x_initialize(filt, config, userdata);

   return filt;
}

static void gameboy3x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width  = width  * 3;
   *out_height = height * 3;
}

static void gameboy3x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void gameboy3x_work_cb_rgb565(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   uint16_t *pixel_lut                = filt->colors.rgb565.pixel_lut;
   uint16_t *grid_lut                 = filt->colors.rgb565.grid_lut;
   uint16_t x, y;

   for (y = 0; y < thr->height; ++y)
   {
      uint16_t *out_ptr = output;

      for (x = 0; x < thr->width; ++x)
      {
         uint16_t *out_line_ptr = out_ptr;
         uint16_t in_color      = *(input + x);
         uint16_t in_rgb_mean   =
               (in_color >> 11 & 0x1F) +
               (in_color >>  6 & 0x1F) +
               (in_color       & 0x1F);
         uint16_t out_pixel_color;
         uint16_t out_grid_color;
         uint16_t lut_index;

         /* Calculate mean value of the 3 RGB
          * colour components */
         in_rgb_mean += (in_rgb_mean +   2) >> 2;
         in_rgb_mean += (in_rgb_mean +   8) >> 4;
         in_rgb_mean += (in_rgb_mean + 128) >> 8;
         in_rgb_mean >>= 2;

         /* Convert to lookup table index
          * > This can never be greater than 3,
          *   but check anyway... */
         lut_index = in_rgb_mean >> 3;
         lut_index = (lut_index > 3) ? 3 : lut_index;

         /* Get output pixel and grid colours */
         out_pixel_color = *(pixel_lut + lut_index);
         out_grid_color  = *(grid_lut + lut_index);

         /* - Pixel layout (p = pixel, g = grid) -
          * Before:  After:
          * (p)      (g)(p)(p)
          *          (g)(p)(p)
          *          (g)(g)(g)
          */

         /* Row 1: (g)(p)(p) */
         *out_line_ptr       = out_grid_color;
         *(out_line_ptr + 1) = out_pixel_color;
         *(out_line_ptr + 2) = out_pixel_color;
         out_line_ptr       += out_stride;

         /* Row 2: (g)(p)(p) */
         *out_line_ptr       = out_grid_color;
         *(out_line_ptr + 1) = out_pixel_color;
         *(out_line_ptr + 2) = out_pixel_color;
         out_line_ptr       += out_stride;

         /* Row 3: (g)(g)(g) */
         *out_line_ptr       = out_grid_color;
         *(out_line_ptr + 1) = out_grid_color;
         *(out_line_ptr + 2) = out_grid_color;

         out_ptr += 3;
      }

      input  += in_stride;
      output += out_stride * 3;
   }
}

static void gameboy3x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *input              = (const uint32_t*)thr->in_data;
   uint32_t *output                   = (uint32_t*)thr->out_data;
   uint32_t in_stride                 = (uint32_t)(thr->in_pitch >> 2);
   uint32_t out_stride                = (uint32_t)(thr->out_pitch >> 2);
   uint32_t *pixel_lut                = filt->colors.xrgb8888.pixel_lut;
   uint32_t *grid_lut                 = filt->colors.xrgb8888.grid_lut;
   uint32_t x, y;

   for (y = 0; y < thr->height; ++y)
   {
      uint32_t *out_ptr = output;

      for (x = 0; x < thr->width; ++x)
      {
         uint32_t *out_line_ptr = out_ptr;
         uint32_t in_color      = *(input + x);
         uint32_t in_rgb_mean   =
               (in_color >> 16 & 0xFF) +
               (in_color >>  8 & 0xFF) +
               (in_color       & 0xFF);
         uint32_t out_pixel_color;
         uint32_t out_grid_color;
         uint32_t lut_index;

         /* Calculate mean value of the 3 RGB
          * colour components */
         in_rgb_mean += (in_rgb_mean +   2) >> 2;
         in_rgb_mean += (in_rgb_mean +   8) >> 4;
         in_rgb_mean += (in_rgb_mean + 128) >> 8;
         in_rgb_mean >>= 2;

         /* Convert to lookup table index
          * > This can never be greater than 3,
          *   but check anyway... */
         lut_index = in_rgb_mean >> 6;
         lut_index = (lut_index > 3) ? 3 : lut_index;

         /* Get output pixel and grid colours */
         out_pixel_color = *(pixel_lut + lut_index);
         out_grid_color  = *(grid_lut + lut_index);

         /* - Pixel layout (p = pixel, g = grid) -
          * Before:  After:
          * (p)      (g)(p)(p)
          *          (g)(p)(p)
          *          (g)(g)(g)
          */

         /* Row 1: (g)(p)(p) */
         *out_line_ptr       = out_grid_color;
         *(out_line_ptr + 1) = out_pixel_color;
         *(out_line_ptr + 2) = out_pixel_color;
         out_line_ptr       += out_stride;

         /* Row 2: (g)(p)(p) */
         *out_line_ptr       = out_grid_color;
         *(out_line_ptr + 1) = out_pixel_color;
         *(out_line_ptr + 2) = out_pixel_color;
         out_line_ptr       += out_stride;

         /* Row 3: (g)(g)(g) */
         *out_line_ptr       = out_grid_color;
         *(out_line_ptr + 1) = out_grid_color;
         *(out_line_ptr + 2) = out_grid_color;

         out_ptr += 3;
      }

      input  += in_stride;
      output += out_stride * 3;
   }
}

static void gameboy3x_generic_packets(void *data,
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
      packets[0].work = gameboy3x_work_cb_rgb565;
   else if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
      packets[0].work = gameboy3x_work_cb_xrgb8888;

   packets[0].thread_data = thr;
}

static const struct softfilter_implementation gameboy3x_generic = {
   gameboy3x_generic_input_fmts,
   gameboy3x_generic_output_fmts,

   gameboy3x_generic_create,
   gameboy3x_generic_destroy,

   gameboy3x_generic_threads,
   gameboy3x_generic_output,
   gameboy3x_generic_packets,

   SOFTFILTER_API_VERSION,
   "Gameboy3x",
   "gameboy3x",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &gameboy3x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
