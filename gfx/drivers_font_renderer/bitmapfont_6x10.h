/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2020 - Daniel De Matteis
 *  Copyright (C) 2019-2020 - James Leaver
 *  Copyright (C) 2020-2022 - trngaje
 *  Copyright (C)      2022 - Michael Burgardt
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

#ifndef __RARCH_FONT_BITMAP_6X10_H
#define __RARCH_FONT_BITMAP_6X10_H

#include "bitmap.h"

#define FONT_6X10_WIDTH  6
#define FONT_6X10_HEIGHT 10
/* FONT_HEIGHT_BASELINE_OFFSET:
 * Distance in pixels from top of character
 * to baseline */
#define FONT_6X10_HEIGHT_BASELINE_OFFSET 8
#define FONT_6X10_WIDTH_STRIDE (FONT_6X10_WIDTH)
#define FONT_6X10_HEIGHT_STRIDE (FONT_6X10_HEIGHT + 1)

#define FONT_6X10_OFFSET(x) ((x) * ((FONT_6X10_HEIGHT * FONT_6X10_WIDTH + 7) / 8))

/* Loads a font of the specified language.
 * Returned object must be freed using
 * bitmapfont_free_lut().
 * Returns NULL if language is invalid or
 * font file is missing */
bitmapfont_lut_t *bitmapfont_6x10_load(unsigned language);

#endif

