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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "menu_common.h"
#include "../../input/keyboard_line.h"
#include "menu_input_line_cb.h"

void menu_key_start_line(rgui_handle_t *rgui, const char *label, input_keyboard_line_complete_t cb)
{
   g_extern.system.key_event = NULL;
   rgui->keyboard.display = true;
   rgui->keyboard.label = label;
   rgui->keyboard.buffer = input_keyboard_start_line(rgui, cb);
}

static void menu_key_end_line(rgui_handle_t *rgui)
{
   rgui->keyboard.display = false;
   rgui->keyboard.label = NULL;
   rgui->old_input_state = -1ULL; // Avoid triggering states on pressing return.
   g_extern.system.key_event = menu_key_event;
}

static void menu_search_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      file_list_search(rgui->selection_buf, str, &rgui->selection_ptr);
   menu_key_end_line(rgui);
}

#ifdef HAVE_NETPLAY
void netplay_port_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      g_extern.netplay_port = strtoul(str, NULL, 0);
   menu_key_end_line(rgui);
}

void netplay_ipaddress_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      strlcpy(g_extern.netplay_server, str, sizeof(g_extern.netplay_server));
   menu_key_end_line(rgui);
}

void netplay_nickname_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      strlcpy(g_extern.netplay_nick, str, sizeof(g_extern.netplay_nick));
   menu_key_end_line(rgui);
}
#endif

#ifdef HAVE_RSOUND
void rsound_ipaddress_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      strlcpy(g_settings.audio.device, str, sizeof(g_settings.audio.device));
   menu_key_end_line(rgui);
}
#endif

#ifdef HAVE_SHADER_MANAGER
void preset_filename_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   shader_manager_save_preset(rgui, str && *str ? str : NULL, false);
   menu_key_end_line(rgui);
}
#endif

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

