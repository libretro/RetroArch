/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "menu_common.h"
#include "../../input/keyboard_line.h"

static void menu_search_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      file_list_search(rgui->selection_buf, str, &rgui->selection_ptr);
   rgui->keyboard.display = false;
   rgui->keyboard.label = NULL;
   rgui->old_input_state = -1ULL; // Avoid triggering states on pressing return.
}

void menu_key_event(bool down, unsigned keycode, uint32_t character, uint16_t mod)
{
   (void)down;
   (void)keycode;
   (void)mod;

   if (character == '/')
   {
      rgui->keyboard.display = true;
      rgui->keyboard.label = "Search:";
      rgui->keyboard.buffer = input_keyboard_start_line(rgui, menu_search_callback);
   }
}
