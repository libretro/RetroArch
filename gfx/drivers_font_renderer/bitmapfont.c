/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>

#include "bitmap.h"

#include "../font_driver.h"

#define BMP_ATLAS_COLS 16
#define BMP_ATLAS_ROWS 16
#define BMP_ATLAS_SIZE (BMP_ATLAS_COLS * BMP_ATLAS_ROWS)

/* Padding is required between each glyph in
 * the atlas to prevent texture bleed when
 * drawing with linear filtering enabled */
#define BMP_ATLAS_PADDING 1

typedef struct bm_renderer
{
   unsigned scale_factor;
   struct font_glyph glyphs[BMP_ATLAS_SIZE];
   struct font_atlas atlas;
   struct font_line_metrics line_metrics;
} bm_renderer_t;

/* Generates a boolean LUT:
 *   lut[num_glyphs][glyph_width * glyph_height]
 * LUT value is 'true' if glyph pixel has a
 * non-zero value.
 * Returned object must be freed using
 * bitmapfont_free_lut().
 * Returns NULL in the event of an error. */
bitmapfont_lut_t *bitmapfont_get_lut(void)
{
   bitmapfont_lut_t *font = NULL;
   size_t symbol_index;
   size_t i, j;

   /* Initialise font struct */
   font = (bitmapfont_lut_t*)calloc(1, sizeof(bitmapfont_lut_t));
   if (!font)
      goto error;

   font->glyph_min = 0;
   font->glyph_max = BMP_ATLAS_SIZE - 1;

   /* Note: Need to use a calloc() here, otherwise
    * we'll get undefined behaviour when calling
    * bitmapfont_free_lut() if the following loop fails */
   font->lut = (bool**)calloc(1, BMP_ATLAS_SIZE * sizeof(bool*));
   if (!font->lut)
      goto error;

   /* Loop over all possible characters */
   for (symbol_index = 0; symbol_index < BMP_ATLAS_SIZE; symbol_index++)
   {
      /* Allocate memory for current symbol */
      font->lut[symbol_index] = (bool*)malloc(FONT_WIDTH *
            FONT_HEIGHT * sizeof(bool));
      if (!font->lut[symbol_index])
         goto error;

      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem     = 1 << ((i + j * FONT_WIDTH) & 7);
            unsigned offset = (i + j * FONT_WIDTH) >> 3;

            /* LUT value is 'true' if specified glyph
             * position contains a pixel */
            font->lut[symbol_index][i + (j * FONT_WIDTH)] =
                  (bitmap_bin[FONT_OFFSET(symbol_index) + offset] & rem) > 0;
         }
      }
   }

   return font;

error:
   if (font)
      bitmapfont_free_lut(font);

   return NULL;
}

void bitmapfont_free_lut(bitmapfont_lut_t *font)
{
   if (!font)
      return;

   if (font->lut)
   {
      size_t num_glyphs = (font->glyph_max - font->glyph_min) + 1;
      size_t i;

      for (i = 0; i < num_glyphs; i++)
      {
         if (font->lut[i])
            free(font->lut[i]);
         font->lut[i] = NULL;
      }

      free(font->lut);
   }

   free(font);
}

static struct font_atlas *font_renderer_bmp_get_atlas(void *data)
{
   bm_renderer_t *handle = (bm_renderer_t*)data;
   if (!handle)
      return NULL;
   return &handle->atlas;
}

static const struct font_glyph *font_renderer_bmp_get_glyph(
      void *data, uint32_t code)
{
   bm_renderer_t *handle = (bm_renderer_t*)data;
   if (!handle)
      return NULL;
   return code < BMP_ATLAS_SIZE ? &handle->glyphs[code] : NULL;
}

static void char_to_texture(bm_renderer_t *handle, uint8_t letter,
      unsigned atlas_x, unsigned atlas_y)
{
   unsigned y, x;
   uint8_t *target = handle->atlas.buffer + atlas_x +
      atlas_y * handle->atlas.width;

   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         unsigned xo, yo;
         unsigned font_pixel = x + y * FONT_WIDTH;
         uint8_t rem         = 1 << (font_pixel & 7);
         unsigned offset     = font_pixel >> 3;
         uint8_t col         = (bitmap_bin[FONT_OFFSET(letter) + offset] & rem) ? 0xff : 0;
         uint8_t *dst        = target;

         dst                += x * handle->scale_factor;
         dst                += y * handle->scale_factor * handle->atlas.width;

         for (yo = 0; yo < handle->scale_factor; yo++)
            for (xo = 0; xo < handle->scale_factor; xo++)
               dst[xo + yo * handle->atlas.width] = col;
      }
   }
   handle->atlas.dirty = true;
}

static void *font_renderer_bmp_init(const char *font_path, float font_size)
{
   unsigned i;
   bm_renderer_t *handle = (bm_renderer_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   (void)font_path;

   handle->scale_factor    = (unsigned)roundf(font_size / FONT_HEIGHT);
   if (!handle->scale_factor)
      handle->scale_factor = 1;

   handle->atlas.width  = (BMP_ATLAS_PADDING + (FONT_WIDTH  * handle->scale_factor)) * BMP_ATLAS_COLS;
   handle->atlas.height = (BMP_ATLAS_PADDING + (FONT_HEIGHT * handle->scale_factor)) * BMP_ATLAS_ROWS;
   handle->atlas.buffer = (uint8_t*)calloc(handle->atlas.width * handle->atlas.height, 1);

   for (i = 0; i < BMP_ATLAS_SIZE; i++)
   {
      unsigned x                       = (i % BMP_ATLAS_COLS) *
         (BMP_ATLAS_PADDING + (handle->scale_factor * FONT_WIDTH));
      unsigned y                       = (i / BMP_ATLAS_COLS) *
         (BMP_ATLAS_PADDING + (handle->scale_factor * FONT_HEIGHT));

      char_to_texture(handle, i, x, y);

      handle->glyphs[i].width          = FONT_WIDTH * handle->scale_factor;
      handle->glyphs[i].height         = FONT_HEIGHT * handle->scale_factor;
      handle->glyphs[i].atlas_offset_x = x;
      handle->glyphs[i].atlas_offset_y = y;
      handle->glyphs[i].draw_offset_x  = 0;
      handle->glyphs[i].draw_offset_y  = -FONT_HEIGHT_BASELINE_OFFSET * handle->scale_factor;
      handle->glyphs[i].advance_x      = FONT_WIDTH_STRIDE * handle->scale_factor;
      handle->glyphs[i].advance_y      = 0;
   }

   handle->line_metrics.ascender       = (float)FONT_HEIGHT_BASELINE_OFFSET * handle->scale_factor;
   handle->line_metrics.descender      = (float)(FONT_HEIGHT - FONT_HEIGHT_BASELINE_OFFSET) * handle->scale_factor;
   handle->line_metrics.height         = (float)FONT_HEIGHT_STRIDE * handle->scale_factor;

   return handle;
}

static void font_renderer_bmp_free(void *data)
{
   bm_renderer_t *handle = (bm_renderer_t*)data;
   if (!handle)
      return;
   free(handle->atlas.buffer);
   free(handle);
}

static const char *font_renderer_bmp_get_default_font(void)
{
   return "";
}

static bool font_renderer_bmp_get_line_metrics(
      void* data, struct font_line_metrics **metrics)
{
   bm_renderer_t *handle = (bm_renderer_t*)data;

   if (!handle)
      return false;

   *metrics = &handle->line_metrics;
   return true;
}

font_renderer_driver_t bitmap_font_renderer = {
   font_renderer_bmp_init,
   font_renderer_bmp_get_atlas,
   font_renderer_bmp_get_glyph,
   font_renderer_bmp_free,
   font_renderer_bmp_get_default_font,
   "font_renderer_bmp",
   font_renderer_bmp_get_line_metrics
};
