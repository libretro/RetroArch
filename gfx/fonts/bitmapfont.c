/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "fonts.h"
#include "bitmap.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../../msvc/msvc_compat.h"
#include "../../boolean.h"

#define ATLAS_COLS 16
#define ATLAS_ROWS 16
#define ATLAS_SIZE (ATLAS_COLS * ATLAS_ROWS)

typedef struct bm_renderer
{
   unsigned scale_factor;
   struct font_glyph glyphs[ATLAS_SIZE];
   struct font_atlas atlas;
} bm_renderer_t;

static const struct font_atlas *font_renderer_get_atlas(void *data)
{
   bm_renderer_t *handle = (bm_renderer_t*)data;
   return &handle->atlas;
}

static const struct font_glyph *font_renderer_get_glyph(
      void *data, uint32_t code)
{
   bm_renderer_t *handle = (bm_renderer_t*)data;
   return code < ATLAS_SIZE ? &handle->glyphs[code] : NULL;
}

static void char_to_texture(bm_renderer_t *handle, uint8_t letter,
      unsigned atlas_x, unsigned atlas_y)
{
   unsigned y, x, xo, yo;
   uint8_t *target = handle->atlas.buffer + atlas_x + 
      atlas_y * handle->atlas.width;

   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         unsigned font_pixel = x + y * FONT_WIDTH;
         uint8_t rem = 1 << (font_pixel & 7);
         unsigned offset = font_pixel >> 3;
         uint8_t col = (bitmap_bin[FONT_OFFSET(letter) + offset] & rem) ? 0xff : 0;

         uint8_t *dst = target;
         dst += x * handle->scale_factor;
         dst += y * handle->scale_factor * handle->atlas.width;

         for (yo = 0; yo < handle->scale_factor; yo++)
            for (xo = 0; xo < handle->scale_factor; xo++)
               dst[xo + yo * handle->atlas.width] = col;
      }
   }
}

static void *font_renderer_init(const char *font_path, float font_size)
{
   (void)font_path;
   unsigned i;

   bm_renderer_t *handle = (bm_renderer_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->scale_factor = (unsigned)roundf(font_size / FONT_HEIGHT);
   if (!handle->scale_factor)
      handle->scale_factor = 1;

   handle->atlas.width  = FONT_WIDTH * handle->scale_factor * ATLAS_COLS;
   handle->atlas.height = FONT_HEIGHT * handle->scale_factor * ATLAS_ROWS;
   handle->atlas.buffer = (uint8_t*)calloc(handle->atlas.width * handle->atlas.height, 1);

   for (i = 0; i < ATLAS_SIZE; i++)
   {
      unsigned x = (i % ATLAS_COLS) * handle->scale_factor * FONT_WIDTH;
      unsigned y = (i / ATLAS_COLS) * handle->scale_factor * FONT_HEIGHT;
      char_to_texture(handle, i, x, y);

      handle->glyphs[i].width = FONT_WIDTH * handle->scale_factor;
      handle->glyphs[i].height = FONT_HEIGHT * handle->scale_factor;
      handle->glyphs[i].atlas_offset_x = x;
      handle->glyphs[i].atlas_offset_y = y;
      handle->glyphs[i].draw_offset_x  = 0;
      handle->glyphs[i].draw_offset_y  = -FONT_HEIGHT_BASELINE * (int)handle->scale_factor;
      handle->glyphs[i].advance_x = (FONT_WIDTH + 1) * handle->scale_factor;
      handle->glyphs[i].advance_y = 0;
   }

   return handle;
}

static void font_renderer_free(void *data)
{
   bm_renderer_t *handle = (bm_renderer_t*)data;
   if (!handle)
      return;
   free(handle->atlas.buffer);
   free(handle);
}

static const char *font_renderer_get_default_font(void)
{
   return "";
}

font_renderer_driver_t bitmap_font_renderer = {
   font_renderer_init,
   font_renderer_get_atlas,
   font_renderer_get_glyph,
   font_renderer_free,
   font_renderer_get_default_font,
   "bitmap",
};

