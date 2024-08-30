/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2020 - Daniel De Matteis
 *  Copyright (C) 2019-2020 - James Leaver
 *  Copyright (C)      2020 - trngaje
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <boolean.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <streams/rzip_stream.h>
#include <retro_miscellaneous.h>

#include "../../file_path_special.h"

#include "bitmapfont_10x10.h"

#define FONT_10X10_FILE_ENG      "bitmap10x10_eng.bin"
#define FONT_10X10_SIZE_ENG      3328
#define FONT_10X10_GLYPH_MIN_ENG 0x0
#define FONT_10X10_GLYPH_MAX_ENG 0xFF

#define FONT_10X10_FILE_CHN      "bitmap10x10_chn.bin"
#define FONT_10X10_SIZE_CHN      272896
#define FONT_10X10_GLYPH_MIN_CHN 0x4E00
#define FONT_10X10_GLYPH_MAX_CHN 0x9FFF

#define FONT_10X10_FILE_JPN      "bitmap10x10_jpn.bin"
#define FONT_10X10_SIZE_JPN      3328
#define FONT_10X10_GLYPH_MIN_JPN 0x3000
#define FONT_10X10_GLYPH_MAX_JPN 0x30FF

#define FONT_10X10_FILE_KOR      "bitmap10x10_kor.bin"
#define FONT_10X10_SIZE_KOR      145236
#define FONT_10X10_GLYPH_MIN_KOR 0xAC00
#define FONT_10X10_GLYPH_MAX_KOR 0xD7A3

#define FONT_10X10_FILE_RUS      "bitmap10x10_rus.bin"
#define FONT_10X10_SIZE_RUS      1248
#define FONT_10X10_GLYPH_MIN_RUS 0x400
#define FONT_10X10_GLYPH_MAX_RUS 0x45F

#define FONT_10X10_OFFSET(x) ((x) * ((FONT_10X10_HEIGHT * FONT_10X10_WIDTH + 7) / 8))

/* Loads a font of the specified language
 * Returns NULL if language is invalid or
 * font file is missing */
bitmapfont_lut_t *bitmapfont_10x10_load(unsigned language)
{
   char font_dir[PATH_MAX_LENGTH];
   char font_path[PATH_MAX_LENGTH];
   const char *font_file      = NULL;
   void *bitmap_raw           = NULL;
   unsigned char *bitmap_char = NULL;
   bitmapfont_lut_t *font     = NULL;
   int64_t font_size          = 0;
   int64_t len                = 0;
   size_t glyph_min           = 0;
   size_t glyph_max           = 0;
   size_t num_glyphs          = 0;
   size_t symbol_index;
   size_t i, j;

   /* Get font file associated with
    * specified language */
   switch (language)
   {
      case RETRO_LANGUAGE_ENGLISH:
         font_file = FONT_10X10_FILE_ENG;
         font_size = FONT_10X10_SIZE_ENG;
         glyph_min = FONT_10X10_GLYPH_MIN_ENG;
         glyph_max = FONT_10X10_GLYPH_MAX_ENG;
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         font_file = FONT_10X10_FILE_CHN;
         font_size = FONT_10X10_SIZE_CHN;
         glyph_min = FONT_10X10_GLYPH_MIN_CHN;
         glyph_max = FONT_10X10_GLYPH_MAX_CHN;
         break;
      case RETRO_LANGUAGE_JAPANESE:
         font_file = FONT_10X10_FILE_JPN;
         font_size = FONT_10X10_SIZE_JPN;
         glyph_min = FONT_10X10_GLYPH_MIN_JPN;
         glyph_max = FONT_10X10_GLYPH_MAX_JPN;
         break;
      case RETRO_LANGUAGE_KOREAN:
         font_file = FONT_10X10_FILE_KOR;
         font_size = FONT_10X10_SIZE_KOR;
         glyph_min = FONT_10X10_GLYPH_MIN_KOR;
         glyph_max = FONT_10X10_GLYPH_MAX_KOR;
         break;
      case RETRO_LANGUAGE_RUSSIAN:
         font_file = FONT_10X10_FILE_RUS;
         font_size = FONT_10X10_SIZE_RUS;
         glyph_min = FONT_10X10_GLYPH_MIN_RUS;
         glyph_max = FONT_10X10_GLYPH_MAX_RUS;
         break;
      default:
         break;
   }

   if (string_is_empty(font_file))
      goto error;

   /* Get font path */
   fill_pathname_application_special(font_dir, sizeof(font_dir),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_RGUI_FONT);
   fill_pathname_join_special(font_path, font_dir, font_file,
         sizeof(font_path));

   /* Attempt to read bitmap file */
   if (!rzipstream_read_file(font_path, &bitmap_raw, &len))
      goto error;

   /* Ensure that we have the correct number
    * of bytes */
   if (len != font_size)
      goto error;

   bitmap_char = (unsigned char *)bitmap_raw;
   num_glyphs  = (glyph_max - glyph_min) + 1;

   /* Initialise font struct */
   if (!(font = (bitmapfont_lut_t*)calloc(1, sizeof(bitmapfont_lut_t))))
      goto error;

   font->glyph_min = glyph_min;
   font->glyph_max = glyph_max;

   /* Note: Need to use a calloc() here, otherwise
    * we'll get undefined behaviour when calling
    * bitmapfont_free_lut() if the following loop fails */
   if (!(font->lut = (bool**)calloc(1, num_glyphs * sizeof(bool*))))
      goto error;

   /* Loop over all possible characters */
   for (symbol_index = 0; symbol_index < num_glyphs; symbol_index++)
   {
      /* Allocate memory for current symbol */
      font->lut[symbol_index] = (bool*)malloc(FONT_10X10_WIDTH *
            FONT_10X10_HEIGHT * sizeof(bool));
      if (!font->lut[symbol_index])
         goto error;

      for (j = 0; j < FONT_10X10_HEIGHT; j++)
      {
         for (i = 0; i < FONT_10X10_WIDTH; i++)
         {
            uint8_t rem     = 1 << ((i + j * FONT_10X10_WIDTH) & 7);
            size_t offset   = (i + j * FONT_10X10_WIDTH) >> 3;

            /* LUT value is 'true' if specified glyph
             * position contains a pixel */
            font->lut[symbol_index][i + (j * FONT_10X10_WIDTH)] =
                  (bitmap_char[FONT_10X10_OFFSET(symbol_index) + offset] & rem) > 0;
         }
      }
   }

   /* Clean up */
   free(bitmap_raw);

   return font;

error:
   if (bitmap_raw)
      free(bitmap_raw);

   if (font)
      bitmapfont_free_lut(font);

   return NULL;
}
