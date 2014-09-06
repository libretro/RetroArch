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

#ifndef MENU_COMMON_H__
#define MENU_COMMON_H__

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../boolean.h"
#include "../../general.h"
#include "menu_navigation.h"
#include "../../core_info.h"
#include "../../file_list.h"
#include "../../playlist.h"
#include "../../input/input_common.h"
#include "../../input/keyboard_line.h"
#include "../../performance.h"
#include "../../gfx/shader_common.h"

#ifdef HAVE_RGUI
#define MENU_TEXTURE_FULLSCREEN false
#else
#define MENU_TEXTURE_FULLSCREEN true
#endif

#ifndef GFX_MAX_SHADERS
#define GFX_MAX_SHADERS 16
#endif

#define MENU_SETTINGS_CORE_INFO_NONE    0xffff
#define MENU_SETTINGS_CORE_OPTION_NONE  0xffff
#define MENU_SETTINGS_CORE_OPTION_START 0x10000


#define MENU_KEYBOARD_BIND_TIMEOUT_SECONDS 5

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   MENU_FILE_NONE,
   MENU_FILE_PLAIN,
   MENU_FILE_DIRECTORY,
   MENU_FILE_DEVICE,
   MENU_FILE_LINEFEED,
   MENU_FILE_LINEFEED_SWITCH,
   MENU_FILE_CORE,
   MENU_FILE_PLAYLIST_ENTRY,
   MENU_FILE_USE_DIRECTORY,
   MENU_FILE_SWITCH,
   MENU_FILE_CARCHIVE,
   MENU_FILE_IN_CARCHIVE,
   MENU_SETTINGS,
} menu_file_type_t;

typedef enum
{
   MENU_ACTION_UP,
   MENU_ACTION_DOWN,
   MENU_ACTION_LEFT,
   MENU_ACTION_RIGHT,
   MENU_ACTION_OK,
   MENU_ACTION_CANCEL,
   MENU_ACTION_REFRESH,
   MENU_ACTION_SELECT,
   MENU_ACTION_START,
   MENU_ACTION_MESSAGE,
   MENU_ACTION_SCROLL_DOWN,
   MENU_ACTION_SCROLL_UP,
   MENU_ACTION_NOOP
} menu_action_t;

typedef enum
{
   // Shader stuff
   MENU_SETTINGS_VIDEO_RESOLUTION = MENU_SETTINGS + 1,
   MENU_SETTINGS_CUSTOM_VIEWPORT,
   MENU_SETTINGS_SHADER_OPTIONS,
   MENU_SETTINGS_SHADER_PARAMETER_0,
   MENU_SETTINGS_SHADER_PARAMETER_LAST = MENU_SETTINGS_SHADER_PARAMETER_0 + (GFX_MAX_PARAMETERS - 1),
   MENU_SETTINGS_SHADER_PASS_0,
   MENU_SETTINGS_SHADER_PASS_LAST = MENU_SETTINGS_SHADER_PASS_0 + (GFX_MAX_SHADERS - 1),
   MENU_SETTINGS_SHADER_PASS_FILTER_0,
   MENU_SETTINGS_SHADER_PASS_FILTER_LAST = MENU_SETTINGS_SHADER_PASS_FILTER_0  + (GFX_MAX_SHADERS - 1),
   MENU_SETTINGS_SHADER_PASS_SCALE_0,
   MENU_SETTINGS_SHADER_PASS_SCALE_LAST = MENU_SETTINGS_SHADER_PASS_SCALE_0  + (GFX_MAX_SHADERS - 1),

   MENU_SETTINGS_BIND_PLAYER,
   MENU_SETTINGS_BIND_DEVICE,
   MENU_SETTINGS_BIND_DEVICE_TYPE,
   MENU_SETTINGS_BIND_ANALOG_MODE,

   // Match up with libretro order for simplicity.
   MENU_SETTINGS_BIND_BEGIN,
   MENU_SETTINGS_BIND_LAST = MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_RIGHT_Y_MINUS,
   MENU_SETTINGS_BIND_ALL_LAST = MENU_SETTINGS_BIND_BEGIN + RARCH_MENU_TOGGLE,

   MENU_SETTINGS_CUSTOM_BIND_MODE,
   MENU_SETTINGS_CUSTOM_BIND,
   MENU_SETTINGS_CUSTOM_BIND_KEYBOARD,
   MENU_SETTINGS_CUSTOM_BIND_ALL,
   MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL,
   MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN,
   MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END = MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN + (MAX_COUNTERS - 1),
   MENU_SETTINGS_PERF_COUNTERS_BEGIN,
   MENU_SETTINGS_PERF_COUNTERS_END = MENU_SETTINGS_PERF_COUNTERS_BEGIN + (MAX_COUNTERS - 1)
} menu_settings_t;

void *menu_init(const void *data);

bool menu_iterate(void);

void menu_free(void *data);

void menu_ticker_line(char *buf, size_t len, unsigned tick,
      const char *str, bool selected);

bool load_menu_content(void);

void load_menu_content_history(unsigned game_index);

void menu_update_system_info(menu_handle_t *menu, bool *load_no_content);

unsigned menu_common_type_is(const char *label, unsigned type);

#ifdef __cplusplus
}
#endif

#endif
