/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef _RMENU_STACK_H_
#define _RMENU_STACK_H_

typedef struct
{
   char title[32];
   unsigned char enum_id;
   unsigned char selected;
   unsigned char page;
   unsigned char first_setting;
   unsigned char max_settings;
   unsigned char category_id;
   int (*entry)(void *data, void *state);
} menu;

typedef struct
{
   uint64_t input;
   uint64_t old_state;
#ifdef HAVE_OSKUTIL
   unsigned osk_param;
   bool (*osk_init)(void *data);
   bool (*osk_callback)(void *data);
#endif
} rmenu_state_t;

// iterate forward declarations
int select_file(void *data, void *state);
int select_directory(void *data, void *state);
int select_setting(void *data, void *state);
int select_rom(void *data, void *state);
int ingame_menu_resize(void *data, void *state);
int ingame_menu_screenshot(void *data, void *state);
int ingame_menu(void *data, void *state);

#endif
