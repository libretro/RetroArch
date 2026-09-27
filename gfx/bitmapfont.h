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

#ifndef __RARCH_FONT_BITMAPFONT_H
#define __RARCH_FONT_BITMAPFONT_H

#include <stdint.h>
#include <boolean.h>

#define FONT_WIDTH 5
#define FONT_HEIGHT 10
/* FONT_HEIGHT_BASELINE_OFFSET:
 * Distance in pixels from top of character
 * to baseline */
#define FONT_HEIGHT_BASELINE_OFFSET 8
#define FONT_WIDTH_STRIDE (FONT_WIDTH + 1)
#define FONT_HEIGHT_STRIDE (FONT_HEIGHT + 1)

#define FONT_OFFSET(x) ((x) * ((FONT_HEIGHT * FONT_WIDTH + 7) / 8))

extern const unsigned char bitmap_bin[1792];

typedef struct
{
   bool **lut;
   uint16_t glyph_min;
   uint16_t glyph_max;
} bitmapfont_lut_t;

/* Generates a boolean LUT:
 *   lut[num_glyphs][glyph_width * glyph_height]
 * LUT value is 'true' if glyph pixel has a
 * non-zero value.
 * Returned object must be freed using
 * bitmapfont_free_lut().
 * Returns NULL in the event of an error. */
bitmapfont_lut_t *bitmapfont_get_lut(void);

void bitmapfont_free_lut(bitmapfont_lut_t *font);

#endif
