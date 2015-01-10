/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <boolean.h>
#include "../../general.h"
#include "menu_navigation.h"
#include "../../core_info.h"
#include "../../playlist.h"
#include "../../input/input_common.h"
#include "../../input/keyboard_line.h"
#include "../../gfx/shader/shader_context.h"

#ifdef HAVE_RGUI
#define MENU_TEXTURE_FULLSCREEN false
#else
#define MENU_TEXTURE_FULLSCREEN true
#endif

#ifndef GFX_MAX_SHADERS
#define GFX_MAX_SHADERS 16
#endif

#ifndef MAX_COUNTERS
#define MAX_COUNTERS 64
#endif

#ifndef MAX_CHEAT_COUNTERS
#define MAX_CHEAT_COUNTERS 100
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
   MENU_FILE_PATH,
   MENU_FILE_DEVICE,
   MENU_FILE_CORE,
   MENU_FILE_PLAYLIST_ENTRY,
   MENU_FILE_CONTENTLIST_ENTRY,
   MENU_FILE_SHADER_PRESET,
   MENU_FILE_SHADER,
   MENU_FILE_VIDEOFILTER,
   MENU_FILE_AUDIOFILTER,
   MENU_FILE_CHEAT,
   MENU_FILE_OVERLAY,
   MENU_FILE_FONT,
   MENU_FILE_CONFIG,
   MENU_FILE_USE_DIRECTORY,
   MENU_FILE_CARCHIVE,
   MENU_FILE_IN_CARCHIVE,
   MENU_SETTINGS,
   MENU_SETTING_DRIVER,
   MENU_SETTING_ACTION,
   MENU_SETTING_ACTION_RUN,
   MENU_SETTING_ACTION_SAVESTATE,
   MENU_SETTING_ACTION_LOADSTATE,
   MENU_SETTING_ACTION_SCREENSHOT,
   MENU_SETTING_ACTION_RESET,
   MENU_SETTING_GROUP,
   MENU_SETTING_SUBGROUP,
   MENU_SETTING_HORIZONTAL_MENU,
   MENU_FILE_TYPE_T_LAST,
} menu_file_type_t;

typedef enum
{
   MENU_ACTION_UP,
   MENU_ACTION_DOWN,
   MENU_ACTION_LEFT,
   MENU_ACTION_RIGHT,
   MENU_ACTION_OK,
   MENU_ACTION_Y,
   MENU_ACTION_CANCEL,
   MENU_ACTION_REFRESH,
   MENU_ACTION_SELECT,
   MENU_ACTION_START,
   MENU_ACTION_MESSAGE,
   MENU_ACTION_SCROLL_DOWN,
   MENU_ACTION_SCROLL_UP,
   MENU_ACTION_TOGGLE,
   MENU_ACTION_NOOP
} menu_action_t;

typedef enum
{
   MENU_SETTINGS_VIDEO_RESOLUTION = MENU_FILE_TYPE_T_LAST + 1,
   MENU_SETTINGS_CUSTOM_VIEWPORT,
   MENU_SETTINGS_SHADER_PARAMETER_0,
   MENU_SETTINGS_SHADER_PARAMETER_LAST = MENU_SETTINGS_SHADER_PARAMETER_0 + (GFX_MAX_PARAMETERS - 1),
   MENU_SETTINGS_SHADER_PRESET_PARAMETER_0,
   MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST = MENU_SETTINGS_SHADER_PRESET_PARAMETER_0 + (GFX_MAX_PARAMETERS - 1),
   MENU_SETTINGS_SHADER_PASS_0,
   MENU_SETTINGS_SHADER_PASS_LAST = MENU_SETTINGS_SHADER_PASS_0 + (GFX_MAX_SHADERS - 1),
   MENU_SETTINGS_SHADER_PASS_FILTER_0,
   MENU_SETTINGS_SHADER_PASS_FILTER_LAST = MENU_SETTINGS_SHADER_PASS_FILTER_0  + (GFX_MAX_SHADERS - 1),
   MENU_SETTINGS_SHADER_PASS_SCALE_0,
   MENU_SETTINGS_SHADER_PASS_SCALE_LAST = MENU_SETTINGS_SHADER_PASS_SCALE_0  + (GFX_MAX_SHADERS - 1),

   MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX,
   MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND,
   MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS,

   MENU_SETTINGS_BIND_BEGIN,
   MENU_SETTINGS_BIND_LAST = MENU_SETTINGS_BIND_BEGIN + RARCH_ANALOG_RIGHT_Y_MINUS,
   MENU_SETTINGS_BIND_ALL_LAST = MENU_SETTINGS_BIND_BEGIN + RARCH_MENU_TOGGLE,

   MENU_SETTINGS_CUSTOM_BIND,
   MENU_SETTINGS_CUSTOM_BIND_KEYBOARD,
   MENU_SETTINGS_CUSTOM_BIND_ALL,
   MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL,
   MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN,
   MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END = MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN + (MAX_COUNTERS - 1),
   MENU_SETTINGS_PERF_COUNTERS_BEGIN,
   MENU_SETTINGS_PERF_COUNTERS_END = MENU_SETTINGS_PERF_COUNTERS_BEGIN + (MAX_COUNTERS - 1),
   MENU_SETTINGS_CHEAT_BEGIN,
   MENU_SETTINGS_CHEAT_END = MENU_SETTINGS_CHEAT_BEGIN + (MAX_CHEAT_COUNTERS - 1),
   MENU_SETTINGS_INPUT_DESC_BEGIN,
   MENU_SETTINGS_INPUT_DESC_END = MENU_SETTINGS_INPUT_DESC_BEGIN + (MAX_USERS * RARCH_CUSTOM_BIND_LIST_END),
} menu_settings_t;

/**
 * menu_init:
 * @data                     : Menu context handle.
 *
 * Create and initialize menu handle.
 *
 * Returns: menu handle on success, otherwise NULL.
 **/
void *menu_init(const void *data);

void menu_free_list(void *data);

bool menu_init_list(void *data);

/**
 * menu_iterate:
 * @input                    : input sample for this frame
 * @old_input                : input sample of the previous frame
 * @trigger_input            : difference' input sample - difference
 *                             between 'input' and 'old_input'
 *
 * Runs RetroArch menu for one frame.
 *
 * Returns: 0 on success, -1 if we need to quit out of the loop. 
 **/
int menu_iterate(retro_input_t input,
      retro_input_t old_input, retro_input_t trigger_input);

/**
 * menu_free:
 * @info                     : Menu handle.
 *
 * Frees a menu handle
 **/
void menu_free(void *data);

/**
 * menu_ticker_line:
 * @buf                      : buffer to write new message line to.
 * @len                      : length of buffer @input.
 * @idx                      : Index. Will be used for ticker logic.
 * @str                      : Input string.
 * @selected                 : Is the item currently selected in the menu?
 *
 * Take the contents of @str and apply a ticker effect to it,
 * and write the results in @buf.
 **/
void menu_ticker_line(char *buf, size_t len, unsigned tick,
      const char *str, bool selected);

bool load_menu_content(void);

void menu_update_system_info(menu_handle_t *menu, bool *load_no_content);

/**
 * menu_update_libretro_info:
 * @info                     : Pointer to system info
 *
 * Update menu state which depends on config.
 **/
void menu_update_libretro_info(struct retro_system_info *info);

unsigned menu_common_type_is(const char *label, unsigned type);

void apply_deferred_settings(void);


#ifdef __cplusplus
}
#endif

#endif
