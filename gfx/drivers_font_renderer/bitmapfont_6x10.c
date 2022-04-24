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

#include <libretro.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <boolean.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <streams/rzip_stream.h>
#include <retro_miscellaneous.h>

#include "../../file_path_special.h"
#include "../../verbosity.h"

#include "bitmapfont_6x10.h"

/* MACROS */
/* extended ASCII: Basic Latin + Latin-1 Supplement */
#define FONT_6X10_FILE_ENG      "bitmap6x10_eng.bin"
#define FONT_6X10_SIZE_ENG      2048
#define FONT_6X10_GLYPH_MIN_ENG 0x0
#define FONT_6X10_GLYPH_MAX_ENG 0xFF

/* Latin Supplement Extended: Latin Extended A + B */
#define FONT_6X10_FILE_LSE      "bitmap6x10_lse.bin"
#define FONT_6X10_SIZE_LSE      2688
#define FONT_6X10_GLYPH_MIN_LSE 0x100
#define FONT_6X10_GLYPH_MAX_LSE 0x24F

/* Loads a font of the specified language
 * Returns NULL if language is invalid or
 * font file is missing */
bitmapfont_lut_t *bitmapfont_6x10_load(unsigned language)
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

   font_dir[0]  = '\0';
   font_path[0] = '\0';

   /* Get font file associated with
    * specified language */
   switch (language)
   {
      /* Needed individually for any non-Latin languages */
      case RETRO_LANGUAGE_ENGLISH:
      {
         font_file = FONT_6X10_FILE_ENG;
         font_size = FONT_6X10_SIZE_ENG;
         glyph_min = FONT_6X10_GLYPH_MIN_ENG;
         glyph_max = FONT_6X10_GLYPH_MAX_ENG;
         break;
      }

      /* All Latin alphabet languages go here */
      case RETRO_LANGUAGE_FRENCH:
      case RETRO_LANGUAGE_SPANISH:
      case RETRO_LANGUAGE_GERMAN:
      case RETRO_LANGUAGE_ITALIAN:
      case RETRO_LANGUAGE_DUTCH:
      case RETRO_LANGUAGE_PORTUGUESE_BRAZIL:
      case RETRO_LANGUAGE_PORTUGUESE_PORTUGAL:
      case RETRO_LANGUAGE_ESPERANTO:
      case RETRO_LANGUAGE_POLISH:
      case RETRO_LANGUAGE_VIETNAMESE:
      case RETRO_LANGUAGE_TURKISH:
      case RETRO_LANGUAGE_SLOVAK:
      case RETRO_LANGUAGE_ASTURIAN:
      case RETRO_LANGUAGE_FINNISH:
      case RETRO_LANGUAGE_INDONESIAN:
      case RETRO_LANGUAGE_SWEDISH:
      case RETRO_LANGUAGE_CZECH:
      /* These languages are not yet added
      case RETRO_LANGUAGE_ROMANIAN:
      case RETRO_LANGUAGE_CROATIAN:
      case RETRO_LANGUAGE_HUNGARIAN:
      case RETRO_LANGUAGE_SERBIAN:
      case RETRO_LANGUAGE_WELSH:
      */
      {
         font_file = FONT_6X10_FILE_LSE;
         font_size = FONT_6X10_SIZE_LSE;
         glyph_min = FONT_6X10_GLYPH_MIN_LSE;
         glyph_max = FONT_6X10_GLYPH_MAX_LSE;
         break;
      }

      default:
         break;
   }

   /* Sanity check: should only trigger on bug */
   if (string_is_empty(font_file))
   {
      RARCH_WARN("[bitmap 6x10] No font file found for specified language: %u\n", language);
      goto error;
   }

   /* Get font path */
   fill_pathname_application_special(font_dir, sizeof(font_dir),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_RGUI_FONT);
   fill_pathname_join(font_path, font_dir, font_file,
         sizeof(font_path));

   /* Attempt to read bitmap file */
   if (!rzipstream_read_file(font_path, &bitmap_raw, &len))
   {
      RARCH_WARN("[bitmap 6x10] Failed to read font file: %s\n", font_path);
      goto error;
   }

   /* Ensure that we have the correct number
    * of bytes */
   if (len != font_size)
   {
      RARCH_WARN("[bitmap 6x10] Font file has invalid size: %s\n", font_path);
      goto error;
   }

   bitmap_char = (unsigned char *)bitmap_raw;
   num_glyphs  = (glyph_max - glyph_min) + 1;

   /* Initialise font struct */
   font = (bitmapfont_lut_t*)calloc(1, sizeof(bitmapfont_lut_t));
   if (!font)
      goto error;

   font->glyph_min = glyph_min;
   font->glyph_max = glyph_max;

   /* Note: Need to use a calloc() here, otherwise
    * we'll get undefined behaviour when calling
    * bitmapfont_free_lut() if the following loop fails */
   font->lut = (bool**)calloc(1, num_glyphs * sizeof(bool*));
   if (!font->lut)
      goto error;

   /* Loop over all possible characters */
   for (symbol_index = 0; symbol_index < num_glyphs; symbol_index++)
   {
      /* Allocate memory for current symbol */
      font->lut[symbol_index] = (bool*)malloc(FONT_6X10_WIDTH *
            FONT_6X10_HEIGHT * sizeof(bool));
      if (!font->lut[symbol_index])
         goto error;

      for (j = 0; j < FONT_6X10_HEIGHT; j++)
      {
         for (i = 0; i < FONT_6X10_WIDTH; i++)
         {
            uint8_t rem     = 1 << ((i + j * FONT_6X10_WIDTH) & 7);
            unsigned offset = (i + j * FONT_6X10_WIDTH) >> 3;

            /* LUT value is 'true' if specified glyph
             * position contains a pixel */
            font->lut[symbol_index][i + (j * FONT_6X10_WIDTH)] =
                  (bitmap_char[FONT_6X10_OFFSET(symbol_index) + offset] & rem) > 0;
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

