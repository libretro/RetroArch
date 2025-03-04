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

#ifndef _INPUT_OSK_H
#define _INPUT_OSK_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>

#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#define OSK_CHARS_PER_LINE 11

RETRO_BEGIN_DECLS

enum osk_type
{
   OSK_TYPE_UNKNOWN    = 0U,
   OSK_LOWERCASE_LATIN,
   OSK_UPPERCASE_LATIN,
   OSK_SYMBOLS_PAGE1,
#ifdef HAVE_LANGEXTRA
   OSK_HIRAGANA_PAGE1,
   OSK_HIRAGANA_PAGE2,
   OSK_KATAKANA_PAGE1,
   OSK_KATAKANA_PAGE2,
   OSK_KOREAN_PAGE1,
#endif
   OSK_TYPE_LAST
};

void input_event_osk_append(
      input_keyboard_line_t *keyboard_line,
      enum osk_type *osk_idx,
      unsigned *osk_last_codepoint,
      unsigned *osk_last_codepoint_len,
      int ptr,
      bool show_symbol_pages,
      const char *word,
      size_t len);

void osk_update_last_codepoint(
      unsigned *last_codepoint,
      unsigned *last_codepoint_len,
      const char *word);

RETRO_END_DECLS

#endif
