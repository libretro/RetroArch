/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef CONSOLE_EXT_H__
#define CONSOLE_EXT_H__

enum {
   MENU_ITEM_LOAD_STATE = 0,
   MENU_ITEM_SAVE_STATE,
   MENU_ITEM_KEEP_ASPECT_RATIO,
   MENU_ITEM_OVERSCAN_AMOUNT,
   MENU_ITEM_ORIENTATION,
#ifdef __CELLOS_LV2__
   MENU_ITEM_SCALE_FACTOR,
#endif
   MENU_ITEM_RESIZE_MODE,
   MENU_ITEM_FRAME_ADVANCE,
   MENU_ITEM_SCREENSHOT_MODE,
   MENU_ITEM_RESET,
   MENU_ITEM_RETURN_TO_GAME,
#ifdef __CELLOS_LV2__
   MENU_ITEM_RETURN_TO_MENU,
   MENU_ITEM_CHANGE_LIBRETRO,
   MENU_ITEM_RETURN_TO_MULTIMAN,
#endif
   MENU_ITEM_RETURN_TO_DASHBOARD
};

enum
{
   MODE_EMULATION = 0,
   MODE_MENU,
   MODE_EXIT
};

typedef struct
{
   char menu_border_file[PATH_MAX];
   char border_file[PATH_MAX];
   char border_dir[PATH_MAX];
#ifdef HAVE_HDD_CACHE_PARTITION
   char cache_dir[PATH_MAX];
#endif
   char cgp_dir[PATH_MAX];
   char config_file[PATH_MAX];
   char core_dir[PATH_MAX];
   char executable_extension[PATH_MAX];
   char filesystem_root_dir[PATH_MAX];
   char input_presets_dir[PATH_MAX];
#ifdef HAVE_MULTIMAN
   char multiman_self_file[PATH_MAX];
#endif
   char port_dir[PATH_MAX];
   char savestate_dir[PATH_MAX];
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   char menu_shader_file[PATH_MAX];
   char shader_file[PATH_MAX];
   char shader_dir[PATH_MAX];
#endif
   char sram_dir[PATH_MAX];
   char system_dir[PATH_MAX];
} default_paths_t;

extern default_paths_t default_paths;

#define MENU_ITEM_LAST MENU_ITEM_RETURN_TO_DASHBOARD+1

void rarch_convert_char_to_wchar(wchar_t *buf, const char * str, size_t size);
const char * rarch_convert_wchar_to_const_char(const wchar_t * wstr);

#endif
