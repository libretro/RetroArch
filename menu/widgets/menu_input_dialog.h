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

#ifndef _MENU_INPUT_DIALOG_H
#define _MENU_INPUT_DIALOG_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>

#include <retro_common_api.h>

#include "../../input/input_driver.h"

RETRO_BEGIN_DECLS

typedef struct menu_input_ctx_line
{
   const char *label;
   const char *label_setting;
   unsigned type;
   unsigned idx;
   input_keyboard_line_complete_t cb;
} menu_input_ctx_line_t;

const char *menu_input_dialog_get_label_setting_buffer(void);

const char *menu_input_dialog_get_label_buffer(void);

const char *menu_input_dialog_get_buffer(void);

unsigned menu_input_dialog_get_kb_type(void);

unsigned menu_input_dialog_get_kb_idx(void);

bool menu_input_dialog_start_search(void);

void menu_input_dialog_hide_kb(void);

void menu_input_dialog_display_kb(void);

bool menu_input_dialog_get_display_kb(void);

bool menu_input_dialog_start(menu_input_ctx_line_t *line);

void menu_input_dialog_end(void);

RETRO_END_DECLS

#endif
