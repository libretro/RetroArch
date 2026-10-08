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
   /* One value per Chinese key page: the pinyin initial index page, then
    * every page of every initial, in the order of chinese_osk_pages.h.
    * A value here is the page selector - it is what the next-page key,
    * the menu's page triggers and the grid switch all step through - so
    * an initial with more than one page needs more than one value.
    * i, u and v have no page: no game title uses them as an initial. */
   OSK_CHINESE_INDEX,
   OSK_CHINESE_A_1,
   OSK_CHINESE_B_1,
   OSK_CHINESE_B_2,
   OSK_CHINESE_B_3,
   OSK_CHINESE_C_1,
   OSK_CHINESE_C_2,
   OSK_CHINESE_C_3,
   OSK_CHINESE_D_1,
   OSK_CHINESE_D_2,
   OSK_CHINESE_D_3,
   OSK_CHINESE_E_1,
   OSK_CHINESE_F_1,
   OSK_CHINESE_F_2,
   OSK_CHINESE_G_1,
   OSK_CHINESE_G_2,
   OSK_CHINESE_G_3,
   OSK_CHINESE_H_1,
   OSK_CHINESE_H_2,
   OSK_CHINESE_H_3,
   OSK_CHINESE_J_1,
   OSK_CHINESE_J_2,
   OSK_CHINESE_J_3,
   OSK_CHINESE_J_4,
   OSK_CHINESE_J_5,
   OSK_CHINESE_K_1,
   OSK_CHINESE_K_2,
   OSK_CHINESE_L_1,
   OSK_CHINESE_L_2,
   OSK_CHINESE_L_3,
   OSK_CHINESE_L_4,
   OSK_CHINESE_M_1,
   OSK_CHINESE_M_2,
   OSK_CHINESE_N_1,
   OSK_CHINESE_N_2,
   OSK_CHINESE_O_1,
   OSK_CHINESE_P_1,
   OSK_CHINESE_P_2,
   OSK_CHINESE_Q_1,
   OSK_CHINESE_Q_2,
   OSK_CHINESE_Q_3,
   OSK_CHINESE_R_1,
   OSK_CHINESE_S_1,
   OSK_CHINESE_S_2,
   OSK_CHINESE_S_3,
   OSK_CHINESE_S_4,
   OSK_CHINESE_S_5,
   OSK_CHINESE_T_1,
   OSK_CHINESE_T_2,
   OSK_CHINESE_T_3,
   OSK_CHINESE_W_1,
   OSK_CHINESE_W_2,
   OSK_CHINESE_X_1,
   OSK_CHINESE_X_2,
   OSK_CHINESE_X_3,
   OSK_CHINESE_Y_1,
   OSK_CHINESE_Y_2,
   OSK_CHINESE_Y_3,
   OSK_CHINESE_Y_4,
   OSK_CHINESE_Z_1,
   OSK_CHINESE_Z_2,
   OSK_CHINESE_Z_3,
   OSK_CHINESE_Z_4,
   OSK_CHINESE_Z_5,
#endif
   OSK_TYPE_LAST
};

/* The page after (@dir > 0) or before (@dir < 0) a page of the
 * keyboard, as its next-page key and the menu's page triggers step. */
enum osk_type input_osk_step(enum osk_type osk_idx, int dir,
      bool show_symbol_pages);

#ifdef HAVE_LANGEXTRA
/* A Chinese page laid out in @grid, 44 keys. */
void input_osk_chinese_grid(char **grid, enum osk_type osk_idx);
#endif

void input_event_osk_append(
      input_keyboard_line_t *keyboard_line,
      enum osk_type *osk_idx,
      unsigned *osk_last_codepoint,
      unsigned *osk_last_codepoint_len,
      int ptr,
      bool show_symbol_pages,
      const char *word,
      size_t len);

/**
 * input_osk_native_active:
 *
 * Whether a system-provided keyboard panel (platform IME, Steam OSK,
 * ...) is currently on screen and owns text entry.
 *
 * While this is true the frontend must not draw its own on-screen
 * keyboard over the top of it, and must not feed the keyboard line
 * from the OSK grid: the native panel is already doing both.
 *
 * @return true if a native keyboard panel is up.
 **/
bool input_osk_native_active(void);

/**
 * input_osk_native_available:
 *
 * Whether this device offers a native keyboard panel at all, whether
 * or not one is on screen right now. Menu code uses this to decide
 * if offering the choice between the two keyboards makes sense.
 *
 * @return true if a native keyboard panel could be shown.
 **/
bool input_osk_native_available(void);

void osk_update_last_codepoint(
      unsigned *last_codepoint,
      unsigned *last_codepoint_len,
      const char *word);

RETRO_END_DECLS

#endif
