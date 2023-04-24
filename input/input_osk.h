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

#define OSK_CHARS_PER_LINE 12
#define OSK_COMPOSITION    0x40000000 
#define OSK_CHARS_MAX      (OSK_CHARS_PER_LINE*4)
#define OSK_PER_LANG       8

RETRO_BEGIN_DECLS

enum osk_type
{
   /* zero base enum.(compatible with keyboard.json)*/
   OSK_LOWERCASE_LATIN = 0U,
   OSK_UPPERCASE_LATIN,
   OSK_SYMBOLS_PAGE1,
   /* HAVE_LANGEXTRA use keyboard.json */
     OSK_TYPE_LAST
};

void input_event_osk_append(
      input_keyboard_line_t *keyboard_line,
      enum osk_type *osk_idx,
      int ptr,
      const char *word,
      size_t word_len);

void input_osk_next(
      enum osk_type *osk_idx,
      int delta );

RETRO_END_DECLS

#endif
