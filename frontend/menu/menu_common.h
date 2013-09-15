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

#ifndef MENU_COMMON_H__
#define MENU_COMMON_H__

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../performance.h"

#ifdef HAVE_RGUI
#define MENU_TEXTURE_FULLSCREEN false
#else
#define MENU_TEXTURE_FULLSCREEN true
#endif

#ifndef __cplusplus
#include <stdbool.h>
#else
extern "C" {
#endif

#ifdef HAVE_FILEBROWSER
#include "utils/file_browser.h"
#else
#include "utils/file_list.h"
#endif

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#define HAVE_SHADER_MANAGER
#include "../../gfx/shader_parse.h"
#endif

#include "history.h"

#define RGUI_MAX_SHADERS 8

enum
{
   DEVICE_NAV_UP = 0,
   DEVICE_NAV_DOWN,
   DEVICE_NAV_LEFT,
   DEVICE_NAV_RIGHT,
#if defined(HAVE_RMENU) || defined(HAVE_RMENU_XUI)
   DEVICE_NAV_UP_ANALOG_L,
   DEVICE_NAV_DOWN_ANALOG_L,
   DEVICE_NAV_LEFT_ANALOG_L,
   DEVICE_NAV_RIGHT_ANALOG_L,
   DEVICE_NAV_UP_ANALOG_R,
   DEVICE_NAV_DOWN_ANALOG_R,
   DEVICE_NAV_LEFT_ANALOG_R,
   DEVICE_NAV_RIGHT_ANALOG_R,
#endif
   DEVICE_NAV_A,
   DEVICE_NAV_B,
   DEVICE_NAV_L,
   DEVICE_NAV_R,
   DEVICE_NAV_L2,
   DEVICE_NAV_R2,
   DEVICE_NAV_START,
   DEVICE_NAV_SELECT,
   DEVICE_NAV_MENU,
   DEVICE_NAV_X,
   DEVICE_NAV_Y,
   DEVICE_NAV_LAST
};

typedef enum
{
   RGUI_FILE_PLAIN,
   RGUI_FILE_DIRECTORY,
   RGUI_FILE_DEVICE,
   RGUI_FILE_USE_DIRECTORY,
   RGUI_SETTINGS,

   // Shader stuff
   RGUI_SETTINGS_VIDEO_OPTIONS,
   RGUI_SETTINGS_VIDEO_OPTIONS_FIRST,
#ifdef GEKKO
   RGUI_SETTINGS_VIDEO_RESOLUTION,
#endif
   RGUI_SETTINGS_VIDEO_FILTER,
   RGUI_SETTINGS_VIDEO_SOFT_FILTER,
   RGUI_SETTINGS_VIDEO_GAMMA,
   RGUI_SETTINGS_VIDEO_INTEGER_SCALE,
   RGUI_SETTINGS_VIDEO_ASPECT_RATIO,
   RGUI_SETTINGS_CUSTOM_VIEWPORT,
   RGUI_SETTINGS_CUSTOM_VIEWPORT_2,
   RGUI_SETTINGS_TOGGLE_FULLSCREEN,
   RGUI_SETTINGS_VIDEO_ROTATION,
   RGUI_SETTINGS_VIDEO_VSYNC,
   RGUI_SETTINGS_VIDEO_HARD_SYNC,
   RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES,
   RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION,
   RGUI_SETTINGS_VIDEO_SWAP_INTERVAL,
   RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X,
   RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y,
   RGUI_SETTINGS_VIDEO_CROP_OVERSCAN,
   RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO,
   RGUI_SETTINGS_VIDEO_OPTIONS_LAST,
#ifdef HAVE_SHADER_MANAGER
   RGUI_SETTINGS_SHADER_OPTIONS,
   RGUI_SETTINGS_SHADER_FILTER,
   RGUI_SETTINGS_SHADER_PRESET,
   RGUI_SETTINGS_SHADER_APPLY,
   RGUI_SETTINGS_SHADER_PASSES,
   RGUI_SETTINGS_SHADER_0,
   RGUI_SETTINGS_SHADER_0_FILTER,
   RGUI_SETTINGS_SHADER_0_SCALE,
   RGUI_SETTINGS_SHADER_LAST = RGUI_SETTINGS_SHADER_0_SCALE + (3 * (RGUI_MAX_SHADERS - 1)),
#endif

   // settings options are done here too
   RGUI_SETTINGS_OPEN_FILEBROWSER,
   RGUI_SETTINGS_OPEN_HISTORY,
   RGUI_SETTINGS_CORE,
   RGUI_SETTINGS_CONFIG,
   RGUI_SETTINGS_CORE_OPTIONS,
   RGUI_SETTINGS_AUDIO_OPTIONS,
   RGUI_SETTINGS_INPUT_OPTIONS,
   RGUI_SETTINGS_PATH_OPTIONS,
   RGUI_SETTINGS_OPTIONS,
   RGUI_SETTINGS_REWIND_ENABLE,
   RGUI_SETTINGS_REWIND_GRANULARITY,
   RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT,
   RGUI_SETTINGS_SRAM_AUTOSAVE,
   RGUI_SETTINGS_SAVESTATE_SAVE,
   RGUI_SETTINGS_SAVESTATE_LOAD,
   RGUI_SETTINGS_DISK_OPTIONS,
   RGUI_SETTINGS_DISK_INDEX,
   RGUI_SETTINGS_DISK_APPEND,
#ifdef HAVE_SCREENSHOTS
   RGUI_SETTINGS_SCREENSHOT,
   RGUI_SETTINGS_GPU_SCREENSHOT,
   RGUI_SCREENSHOT_DIR_PATH,
#endif
   RGUI_BROWSER_DIR_PATH,
   RGUI_SHADER_DIR_PATH,
   RGUI_SAVESTATE_DIR_PATH,
   RGUI_SAVEFILE_DIR_PATH,
   RGUI_LIBRETRO_DIR_PATH,
#ifdef HAVE_OVERLAY
   RGUI_OVERLAY_DIR_PATH,
#endif
   RGUI_SYSTEM_DIR_PATH,
   RGUI_SETTINGS_RESTART_GAME,
   RGUI_SETTINGS_AUDIO_MUTE,
   RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA,
   RGUI_SETTINGS_ZIP_EXTRACT,
   RGUI_SETTINGS_DEBUG_TEXT,
   RGUI_SETTINGS_RESTART_EMULATOR,
   RGUI_SETTINGS_RESUME_GAME,
   RGUI_SETTINGS_QUIT_RARCH,

#ifdef HAVE_OVERLAY
   RGUI_SETTINGS_OVERLAY_PRESET,
   RGUI_SETTINGS_OVERLAY_OPACITY,
   RGUI_SETTINGS_OVERLAY_SCALE,
#endif
   RGUI_SETTINGS_BIND_PLAYER,
   RGUI_SETTINGS_BIND_DEVICE,
   RGUI_SETTINGS_BIND_DEVICE_TYPE,
   RGUI_SETTINGS_BIND_DPAD_EMULATION,
   RGUI_SETTINGS_BIND_UP,
   RGUI_SETTINGS_BIND_DOWN,
   RGUI_SETTINGS_BIND_LEFT,
   RGUI_SETTINGS_BIND_RIGHT,
   RGUI_SETTINGS_BIND_A,
   RGUI_SETTINGS_BIND_B,
   RGUI_SETTINGS_BIND_X,
   RGUI_SETTINGS_BIND_Y,
   RGUI_SETTINGS_BIND_START,
   RGUI_SETTINGS_BIND_SELECT,
   RGUI_SETTINGS_BIND_L,
   RGUI_SETTINGS_BIND_R,
   RGUI_SETTINGS_BIND_L2,
   RGUI_SETTINGS_BIND_R2,
   RGUI_SETTINGS_BIND_L3,
   RGUI_SETTINGS_BIND_R3,

   RGUI_SETTINGS_CORE_OPTION_NONE = 0xffff,
   RGUI_SETTINGS_CORE_OPTION_START = 0x10000
} rgui_file_type_t;

typedef enum
{
   RGUI_ACTION_UP,
   RGUI_ACTION_DOWN,
   RGUI_ACTION_LEFT,
   RGUI_ACTION_RIGHT,
   RGUI_ACTION_OK,
   RGUI_ACTION_CANCEL,
   RGUI_ACTION_REFRESH,
   RGUI_ACTION_SETTINGS,
   RGUI_ACTION_START,
   RGUI_ACTION_MESSAGE,
   RGUI_ACTION_SCROLL_DOWN,
   RGUI_ACTION_SCROLL_UP,
   RGUI_ACTION_MAPPING_PREVIOUS,
   RGUI_ACTION_MAPPING_NEXT,
   RGUI_ACTION_NOOP
} rgui_action_t;

typedef struct
{
   uint64_t old_input_state;
   uint64_t trigger_state;
   bool do_held;

   unsigned delay_timer;
   unsigned delay_count;

   uint16_t *frame_buf;
   size_t frame_buf_pitch;
   bool frame_buf_show;

#ifdef HAVE_FILEBROWSER
   filebrowser_t *browser;
   unsigned menu_type;
#else
   rgui_list_t *menu_stack;
   rgui_list_t *selection_buf;
#endif
   size_t selection_ptr;
   bool need_refresh;
   bool msg_force;

   char base_path[PATH_MAX];

   const uint8_t *font;
   bool alloc_font;

#ifdef HAVE_DYNAMIC
   char libretro_dir[PATH_MAX];
#endif
   struct retro_system_info info;
   bool load_no_rom;

#ifdef HAVE_OSKUTIL
   unsigned osk_param;
   oskutil_params oskutil_handle;
   bool (*osk_init)(void *data);
   bool (*osk_callback)(void *data);
#endif
#ifdef HAVE_SHADER_MANAGER
   struct gfx_shader shader;
#endif
   unsigned current_pad;

   rom_history_t *history;
   rarch_time_t last_time; // Used to throttle RGUI in case VSync is broken.
} rgui_handle_t;

extern rgui_handle_t *rgui;

void menu_init(void);
bool menu_iterate(void);
void menu_free(void);

#ifndef HAVE_RMENU_XUI
#if defined(HAVE_RGUI) || defined(HAVE_RMENU)
int rgui_input_postprocess(void *data, uint64_t old_state);
#endif
#endif

#ifdef HAVE_SHADER_MANAGER
void shader_manager_init(rgui_handle_t *rgui);
void shader_manager_get_str(struct gfx_shader *shader,
      char *type_str, size_t type_str_size, unsigned type);
#endif

void menu_ticker_line(char *buf, size_t len, unsigned tick, const char *str, bool selected);

void load_menu_game_prepare(void);
bool load_menu_game(void);
void load_menu_game_history(unsigned game_index);
void menu_rom_history_push(const char *path, const char *core_path,
      const char *core_name);
void menu_rom_history_push_current(void);

bool menu_replace_config(const char *path);

#ifdef __cplusplus
}
#endif

#endif
