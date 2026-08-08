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
   if (!(font = (bitmapfont_lut_t*)calloc(1, sizeof(bitmapfont_lut_t))))
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
            size_t offset   = (i + j * FONT_WIDTH) >> 3;

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
      size_t i;
      size_t num_glyphs = (font->glyph_max - font->glyph_min) + 1;

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
