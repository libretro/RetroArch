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

#ifndef CONSOLE_EXT_H__
#define CONSOLE_EXT_H__

#if defined(__CELLOS_LV2__)
#define EXT_EXECUTABLES "self|SELF|bin|BIN"
#elif defined(_XBOX1)
#define EXT_EXECUTABLES "xbe|XBE"
#elif defined(_XBOX360)
#define EXT_EXECUTABLES "xex|XEX"
#elif defined(GEKKO)
#define EXT_EXECUTABLES "dol|DOL"
#endif

#define EXT_IMAGES "png|PNG|jpg|JPG|JPEG|jpeg"
#define EXT_SHADERS "cg|CG"
#define EXT_CGP_PRESETS "cgp|CGP"
#define EXT_INPUT_PRESETS "cfg|CFG"

enum
{
   EXTERN_LAUNCHER_SALAMANDER,
#ifdef HAVE_MULTIMAN
   EXTERN_LAUNCHER_MULTIMAN,
#endif
#ifdef GEKKO
   EXTERN_LAUNCHER_CHANNEL,
#endif
};

enum {
   MENU_ITEM_LOAD_STATE = 0,
   MENU_ITEM_SAVE_STATE,
   MENU_ITEM_KEEP_ASPECT_RATIO,
   MENU_ITEM_OVERSCAN_AMOUNT,
   MENU_ITEM_ORIENTATION,
#ifdef HAVE_FBO
   MENU_ITEM_SCALE_FACTOR,
#endif
   MENU_ITEM_RESIZE_MODE,
   MENU_ITEM_FRAME_ADVANCE,
   MENU_ITEM_SCREENSHOT_MODE,
   MENU_ITEM_RESET,
   MENU_ITEM_RETURN_TO_GAME,
   MENU_ITEM_RETURN_TO_MENU,
   MENU_ITEM_CHANGE_LIBRETRO,
#ifdef HAVE_MULTIMAN
   MENU_ITEM_RETURN_TO_MULTIMAN,
#endif
   MENU_ITEM_QUIT_RARCH,
   MENU_ITEM_LAST
};

enum
{
   MODE_EMULATION = 0,
   MODE_LOAD_GAME,
   MODE_INIT,
   MODE_MENU,
   MODE_MENU_PREINIT,
   MODE_MENU_INGAME,
   MODE_MENU_DRAW,
   MODE_EXIT,
   MODE_LOAD_FIRST_SHADER,
   MODE_LOAD_SECOND_SHADER,
};

enum
{
   SOUND_MODE_NORMAL,
#ifdef HAVE_RSOUND
   SOUND_MODE_RSOUND,
#endif
#ifdef HAVE_HEADSET
   SOUND_MODE_HEADSET,
#endif
   SOUND_MODE_LAST
};

#ifdef _WIN32
#define MAXIMUM_PATH 260
#else
#define MAXIMUM_PATH 512
#endif

typedef struct
{
   char menu_border_file[MAXIMUM_PATH];
   char border_file[MAXIMUM_PATH];
   char border_dir[MAXIMUM_PATH];
#ifdef HAVE_HDD_CACHE_PARTITION
   char cache_dir[MAXIMUM_PATH];
#endif
   char cgp_dir[MAXIMUM_PATH];
   char core_dir[MAXIMUM_PATH];
   char config_path[MAXIMUM_PATH];
   char libretro_path[MAXIMUM_PATH];
   char executable_extension[MAXIMUM_PATH];
   char filebrowser_startup_dir[MAXIMUM_PATH];
   char filesystem_root_dir[MAXIMUM_PATH];
   char input_presets_dir[MAXIMUM_PATH];
   char screenshots_dir[MAXIMUM_PATH];
#ifdef HAVE_MULTIMAN
   char multiman_self_file[MAXIMUM_PATH];
#endif
   char port_dir[MAXIMUM_PATH];
   char savestate_dir[MAXIMUM_PATH];
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   char menu_shader_file[MAXIMUM_PATH];
   char shader_file[MAXIMUM_PATH];
   char shader_dir[MAXIMUM_PATH];
#endif
   char salamander_file[MAXIMUM_PATH];
   char sram_dir[MAXIMUM_PATH];
   char system_dir[MAXIMUM_PATH];
} default_paths_t;

extern default_paths_t default_paths;

#endif
