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

/* Compile: gcc -o gameboy4x.so -shared gameboy4x.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation gameboy4x_get_implementation
#define softfilter_thread_data gameboy4x_softfilter_thread_data
#define filter_data gameboy4x_filter_data
#endif

/* Default colours match Gambatte's
 * Gameboy Pocket palette */
#define GAMEBOY_4X_DEFAULT_PALETTE_0    0x2A3325
#define GAMEBOY_4X_DEFAULT_PALETTE_1    0x535f49
#define GAMEBOY_4X_DEFAULT_PALETTE_2    0x86927C
#define GAMEBOY_4X_DEFAULT_PALETTE_3    0xA7B19A
#define GAMEBOY_4X_DEFAULT_PALETTE_GRID 0xADB8A0

#define GAMEBOY_4X_RGB24_TO_RGB565(rgb24) ( ((rgb24 >> 8) & 0xF800) | ((rgb24 >> 5) & 0x7E0) | ((rgb24 >> 3) & 0x1F) )

typedef struct
{
   struct
   {
      uint32_t pixel_lut[4];
      uint32_t shadow_lut[4];
      uint32_t grid_lut[4];
   } xrgb8888;

   struct
   {
      uint16_t pixel_lut[4];
      uint16_t shadow_lut[4];
      uint16_t grid_lut[4];
   } rgb565;

} gameboy4x_colors_t;

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
   gameboy4x_colors_t colors;
};

static unsigned gameboy4x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565 | SOFTFILTER_FMT_XRGB8888;
}

static unsigned gameboy4x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned gameboy4x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static uint32_t gameboy4x_get_weighted_colour(
      unsigned palette, unsigned grid,
      unsigned palette_weight, unsigned grid_weight)
{
   uint32_t palette_r = (palette & 0xFF0000) >> 16;
   uint32_t palette_g = (palette &   0xFF00) >> 8;
   uint32_t palette_b = (palette &     0xFF);

   uint32_t grid_r    = (grid    & 0xFF0000) >> 16;
   uint32_t grid_g    = (grid    &   0xFF00) >> 8;
   uint32_t grid_b    = (grid    &     0xFF);

   uint32_t mix_r     = ((palette_weight * palette_r) + (grid_weight * grid_r)) /
         (palette_weight + grid_weight);
   uint32_t mix_g     = ((palette_weight * palette_g) + (grid_weight * grid_g)) /
         (palette_weight + grid_weight);
   uint32_t mix_b     = ((palette_weight * palette_b) + (grid_weight * grid_b)) /
         (palette_weight + grid_weight);

   return (mix_r << 16) | (mix_g << 8) | mix_b;
}

static void gameboy4x_initialize(struct filter_data *filt,
      const struct softfilter_config *config,
      void *userdata)
{
   unsigned palette[4];
   unsigned palette_grid;
   size_t i;

   /* Read raw colour values */
   config->get_hex(userdata, "palette_0", &palette[0],
         GAMEBOY_4X_DEFAULT_PALETTE_0);
   config->get_hex(userdata, "palette_1", &palette[1],
         GAMEBOY_4X_DEFAULT_PALETTE_1);
   config->get_hex(userdata, "palette_2", &palette[2],
         GAMEBOY_4X_DEFAULT_PALETTE_2);
   config->get_hex(userdata, "palette_3", &palette[3],
         GAMEBOY_4X_DEFAULT_PALETTE_3);
   config->get_hex(userdata, "palette_grid", &palette_grid,
         GAMEBOY_4X_DEFAULT_PALETTE_GRID);

   /* Loop over palette colours */
   for (i = 0; i < 4; i++)
   {
      uint32_t shadow_color;
      uint32_t grid_color;

      /* Populate pixel lookup tables */
      filt->colors.rgb565.pixel_lut[i]   = GAMEBOY_4X_RGB24_TO_RGB565(palette[i]);
      filt->colors.xrgb8888.pixel_lut[i] = palette[i];

      /* Populate pixel shadow lookup tables
       * > 4:3 mix of palette:grid */
      shadow_color = gameboy4x_get_weighted_colour(palette[i], palette_grid, 4, 3);

      filt->colors.rgb565.shadow_lut[i]   = GAMEBOY_4X_RGB24_TO_RGB565(shadow_color);
      filt->colors.xrgb8888.shadow_lut[i] = shadow_color;

      /* Populate grid lookup tables
       * > 2:3 mix of palette:grid
       * > Would like to set this to the pure grid
       *   colour (to highlight the pixel shadow
       *   effect), but doing so looks very peculiar.
       *   2:3 is about as light as we can make this
       *   without producing ugly optical illusions */
      grid_color = gameboy4x_get_weighted_colour(palette[i], palette_grid, 2, 3);

      filt->colors.rgb565.grid_lut[i]   = GAMEBOY_4X_RGB24_TO_RGB565(grid_color);
      filt->colors.xrgb8888.grid_lut[i] = grid_color;
   }
}

static void *gameboy4x_generic_create(const struct softfilter_config *config,
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
   gameboy4x_initialize(filt, config, userdata);

   return filt;
}

static void gameboy4x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width  = width  << 2;
   *out_height = height << 2;
}

static void gameboy4x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void gameboy4x_work_cb_rgb565(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   uint16_t *pixel_lut                = filt->colors.rgb565.pixel_lut;
   uint16_t *shadow_lut               = filt->colors.rgb565.shadow_lut;
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
         uint16_t out_shadow_color;
         uint16_t out_grid_color;
         uint16_t lut_index;
         uint16_t row_a_color[4];
         uint16_t row_b_color[4];
         uint16_t row_c_color[4];

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

         /* Get output pixel, pixel shadow and grid colours */
         out_pixel_color  = *(pixel_lut + lut_index);
         out_shadow_color = *(shadow_lut + lut_index);
         out_grid_color   = *(grid_lut + lut_index);

         /* - Pixel layout (p = pixel, s = shadow, g = grid) -
          * Before:  After:
          * (p)      (g)(p)(p)(p)
          *          (s)(p)(p)(p)
          *          (s)(p)(p)(p)
          *          (s)(s)(s)(g)
          */

         row_a_color[0] = out_grid_color;
         row_a_color[1] = out_pixel_color;
         row_a_color[2] = out_pixel_color;
         row_a_color[3] = out_pixel_color;

         row_b_color[0] = out_shadow_color;
         row_b_color[1] = out_pixel_color;
         row_b_color[2] = out_pixel_color;
         row_b_color[3] = out_pixel_color;

         row_c_color[0] = out_shadow_color;
         row_c_color[1] = out_shadow_color;
         row_c_color[2] = out_shadow_color;
         row_c_color[3] = out_grid_color;

         /* Row 1: (g)(p)(p)(p) */
         memcpy(out_line_ptr, row_a_color, sizeof(row_a_color));
         out_line_ptr       += out_stride;

         /* Row 2: (s)(p)(p)(p) */
         memcpy(out_line_ptr, row_b_color, sizeof(row_b_color));
         out_line_ptr       += out_stride;

         /* Row 3: (s)(p)(p)(p) */
         memcpy(out_line_ptr, row_b_color, sizeof(row_b_color));
         out_line_ptr       += out_stride;

         /* Row 4: (s)(s)(s)(g) */
         memcpy(out_line_ptr, row_c_color, sizeof(row_c_color));

         out_ptr += 4;
      }

      input  += in_stride;
      output += out_stride << 2;
   }
}

static void gameboy4x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *input              = (const uint32_t*)thr->in_data;
   uint32_t *output                   = (uint32_t*)thr->out_data;
   uint32_t in_stride                 = (uint32_t)(thr->in_pitch >> 2);
   uint32_t out_stride                = (uint32_t)(thr->out_pitch >> 2);
   uint32_t *pixel_lut                = filt->colors.xrgb8888.pixel_lut;
   uint32_t *shadow_lut               = filt->colors.xrgb8888.shadow_lut;
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
         uint32_t out_shadow_color;
         uint32_t out_grid_color;
         uint32_t lut_index;
         uint32_t row_a_color[4];
         uint32_t row_b_color[4];
         uint32_t row_c_color[4];

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

         /* Get output pixel, pixel shadow and grid colours */
         out_pixel_color  = *(pixel_lut + lut_index);
         out_shadow_color = *(shadow_lut + lut_index);
         out_grid_color   = *(grid_lut + lut_index);

         /* - Pixel layout (p = pixel, s = shadow, g = grid) -
          * Before:  After:
          * (p)      (g)(p)(p)(p)
          *          (s)(p)(p)(p)
          *          (s)(p)(p)(p)
          *          (s)(s)(s)(g)
          */

         row_a_color[0] = out_grid_color;
         row_a_color[1] = out_pixel_color;
         row_a_color[2] = out_pixel_color;
         row_a_color[3] = out_pixel_color;

         row_b_color[0] = out_shadow_color;
         row_b_color[1] = out_pixel_color;
         row_b_color[2] = out_pixel_color;
         row_b_color[3] = out_pixel_color;

         row_c_color[0] = out_shadow_color;
         row_c_color[1] = out_shadow_color;
         row_c_color[2] = out_shadow_color;
         row_c_color[3] = out_grid_color;

         /* Row 1: (g)(p)(p)(p) */
         memcpy(out_line_ptr, row_a_color, sizeof(row_a_color));
         out_line_ptr       += out_stride;

         /* Row 2: (s)(p)(p)(p) */
         memcpy(out_line_ptr, row_b_color, sizeof(row_b_color));
         out_line_ptr       += out_stride;

         /* Row 3: (s)(p)(p)(p) */
         memcpy(out_line_ptr, row_b_color, sizeof(row_b_color));
         out_line_ptr       += out_stride;

         /* Row 4: (s)(s)(s)(g) */
         memcpy(out_line_ptr, row_c_color, sizeof(row_c_color));

         out_ptr += 4;
      }

      input  += in_stride;
      output += out_stride << 2;
   }
}

static void gameboy4x_generic_packets(void *data,
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
      packets[0].work = gameboy4x_work_cb_rgb565;
   else if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
      packets[0].work = gameboy4x_work_cb_xrgb8888;

   packets[0].thread_data = thr;
}

static const struct softfilter_implementation gameboy4x_generic = {
   gameboy4x_generic_input_fmts,
   gameboy4x_generic_output_fmts,

   gameboy4x_generic_create,
   gameboy4x_generic_destroy,

   gameboy4x_generic_threads,
   gameboy4x_generic_output,
   gameboy4x_generic_packets,

   SOFTFILTER_API_VERSION,
   "Gameboy4x",
   "gameboy4x",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &gameboy4x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
