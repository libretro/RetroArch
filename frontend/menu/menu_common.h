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

#include "../../general.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../performance.h"
#include "../../core_info.h"
#include "menu_context.h"

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

#include "../../file_list.h"

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#define HAVE_SHADER_MANAGER
#include "../../gfx/shader_parse.h"
#endif

#include "history.h"

#define RGUI_MAX_SHADERS 8

typedef enum
{
   RGUI_FILE_PLAIN,
   RGUI_FILE_DIRECTORY,
   RGUI_FILE_DEVICE,
   RGUI_FILE_USE_DIRECTORY,
   RGUI_SETTINGS,
   RGUI_START_SCREEN,

   // Shader stuff
   RGUI_SETTINGS_VIDEO_OPTIONS,
   RGUI_SETTINGS_VIDEO_OPTIONS_FIRST,
   RGUI_SETTINGS_VIDEO_RESOLUTION,
   RGUI_SETTINGS_VIDEO_PAL60,
   RGUI_SETTINGS_VIDEO_FILTER,
   RGUI_SETTINGS_VIDEO_SOFT_FILTER,
   RGUI_SETTINGS_FLICKER_FILTER,
   RGUI_SETTINGS_SOFT_DISPLAY_FILTER,
   RGUI_SETTINGS_VIDEO_GAMMA,
   RGUI_SETTINGS_VIDEO_INTEGER_SCALE,
   RGUI_SETTINGS_VIDEO_ASPECT_RATIO,
   RGUI_SETTINGS_CUSTOM_VIEWPORT,
   RGUI_SETTINGS_CUSTOM_VIEWPORT_2,
   RGUI_SETTINGS_TOGGLE_FULLSCREEN,
   RGUI_SETTINGS_VIDEO_THREADED,
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
   RGUI_SETTINGS_SHADER_OPTIONS,
   RGUI_SETTINGS_SHADER_FILTER,
   RGUI_SETTINGS_SHADER_PRESET,
   RGUI_SETTINGS_SHADER_APPLY,
   RGUI_SETTINGS_SHADER_PASSES,
   RGUI_SETTINGS_SHADER_0,
   RGUI_SETTINGS_SHADER_0_FILTER,
   RGUI_SETTINGS_SHADER_0_SCALE,
   RGUI_SETTINGS_SHADER_LAST = RGUI_SETTINGS_SHADER_0_SCALE + (3 * (RGUI_MAX_SHADERS - 1)),
   RGUI_SETTINGS_SHADER_PRESET_SAVE,

   // settings options are done here too
   RGUI_SETTINGS_OPEN_FILEBROWSER,
   RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE,
   RGUI_SETTINGS_OPEN_HISTORY,
   RGUI_SETTINGS_CORE,
   RGUI_SETTINGS_DEFERRED_CORE,
   RGUI_SETTINGS_CONFIG,
   RGUI_SETTINGS_SAVE_CONFIG,
   RGUI_SETTINGS_CORE_OPTIONS,
   RGUI_SETTINGS_AUDIO_OPTIONS,
   RGUI_SETTINGS_INPUT_OPTIONS,
   RGUI_SETTINGS_PATH_OPTIONS,
   RGUI_SETTINGS_OPTIONS,
   RGUI_SETTINGS_DRIVERS,
   RGUI_SETTINGS_REWIND_ENABLE,
   RGUI_SETTINGS_REWIND_GRANULARITY,
   RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT,
   RGUI_SETTINGS_SRAM_AUTOSAVE,
   RGUI_SETTINGS_SAVESTATE_SAVE,
   RGUI_SETTINGS_SAVESTATE_LOAD,
   RGUI_SETTINGS_DISK_OPTIONS,
   RGUI_SETTINGS_DISK_INDEX,
   RGUI_SETTINGS_DISK_APPEND,
   RGUI_SETTINGS_DRIVER_VIDEO,
   RGUI_SETTINGS_DRIVER_AUDIO,
   RGUI_SETTINGS_DRIVER_INPUT,
   RGUI_SETTINGS_DRIVER_CAMERA,
   RGUI_SETTINGS_DRIVER_LOCATION,
   RGUI_SETTINGS_SCREENSHOT,
   RGUI_SETTINGS_GPU_SCREENSHOT,
   RGUI_SCREENSHOT_DIR_PATH,
   RGUI_BROWSER_DIR_PATH,
   RGUI_SHADER_DIR_PATH,
   RGUI_SAVESTATE_DIR_PATH,
   RGUI_SAVEFILE_DIR_PATH,
   RGUI_LIBRETRO_DIR_PATH,
   RGUI_LIBRETRO_INFO_DIR_PATH,
   RGUI_CONFIG_DIR_PATH,
   RGUI_OVERLAY_DIR_PATH,
   RGUI_SYSTEM_DIR_PATH,
   RGUI_SETTINGS_RESTART_GAME,
   RGUI_SETTINGS_AUDIO_MUTE,
   RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA,
   RGUI_SETTINGS_AUDIO_VOLUME_LEVEL, // XBOX1 only it seems. FIXME: Refactor this?
   RGUI_SETTINGS_AUDIO_VOLUME,
   RGUI_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE,
   RGUI_SETTINGS_RSOUND_SERVER_IP_ADDRESS,
   RGUI_SETTINGS_ZIP_EXTRACT,
   RGUI_SETTINGS_DEBUG_TEXT,
   RGUI_SETTINGS_RESTART_EMULATOR,
   RGUI_SETTINGS_RESUME_GAME,
   RGUI_SETTINGS_QUIT_RARCH,

   RGUI_SETTINGS_OVERLAY_PRESET,
   RGUI_SETTINGS_OVERLAY_OPACITY,
   RGUI_SETTINGS_OVERLAY_SCALE,
   RGUI_SETTINGS_BIND_PLAYER,
   RGUI_SETTINGS_BIND_DEVICE,
   RGUI_SETTINGS_BIND_DEVICE_TYPE,
   RGUI_SETTINGS_DEVICE_AUTODETECT_ENABLE,

   // Match up with libretro order for simplicity.
   RGUI_SETTINGS_BIND_BEGIN,
   RGUI_SETTINGS_BIND_B = RGUI_SETTINGS_BIND_BEGIN,
   RGUI_SETTINGS_BIND_Y,
   RGUI_SETTINGS_BIND_SELECT,
   RGUI_SETTINGS_BIND_START,
   RGUI_SETTINGS_BIND_UP,
   RGUI_SETTINGS_BIND_DOWN,
   RGUI_SETTINGS_BIND_LEFT,
   RGUI_SETTINGS_BIND_RIGHT,
   RGUI_SETTINGS_BIND_A,
   RGUI_SETTINGS_BIND_X,
   RGUI_SETTINGS_BIND_L,
   RGUI_SETTINGS_BIND_R,
   RGUI_SETTINGS_BIND_L2,
   RGUI_SETTINGS_BIND_R2,
   RGUI_SETTINGS_BIND_L3,
   RGUI_SETTINGS_BIND_R3,
   RGUI_SETTINGS_BIND_ANALOG_LEFT_X_PLUS,
   RGUI_SETTINGS_BIND_ANALOG_LEFT_X_MINUS,
   RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_PLUS,
   RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_MINUS,
   RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_PLUS,
   RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_MINUS,
   RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_PLUS,
   RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS,
   RGUI_SETTINGS_BIND_LAST = RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS,
   RGUI_SETTINGS_BIND_MENU_TOGGLE = RGUI_SETTINGS_BIND_BEGIN + RARCH_MENU_TOGGLE,
   RGUI_SETTINGS_CUSTOM_BIND,
   RGUI_SETTINGS_CUSTOM_BIND_ALL,
   RGUI_SETTINGS_CUSTOM_BIND_DEFAULT_ALL,

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
   RGUI_ACTION_START,
   RGUI_ACTION_MESSAGE,
   RGUI_ACTION_SCROLL_DOWN,
   RGUI_ACTION_SCROLL_UP,
   RGUI_ACTION_MAPPING_PREVIOUS,
   RGUI_ACTION_MAPPING_NEXT,
   RGUI_ACTION_NOOP
} rgui_action_t;

#define RGUI_MAX_BUTTONS 32
#define RGUI_MAX_AXES 32
#define RGUI_MAX_HATS 4
struct rgui_bind_state_port
{
   bool buttons[RGUI_MAX_BUTTONS];
   int16_t axes[RGUI_MAX_AXES];
   uint16_t hats[RGUI_MAX_HATS];
};

struct rgui_bind_axis_state
{
   // Default axis state.
   int16_t rested_axes[RGUI_MAX_AXES];
   // Locked axis state. If we configured an axis, avoid having the same axis state trigger something again right away.
   int16_t locked_axes[RGUI_MAX_AXES];
};

struct rgui_bind_state
{
   struct retro_keybind *target;
   unsigned begin;
   unsigned last;
   unsigned player;
   struct rgui_bind_state_port state[MAX_PLAYERS];
   struct rgui_bind_axis_state axis_state[MAX_PLAYERS];
   bool skip;
};

void menu_poll_bind_get_rested_axes(struct rgui_bind_state *state);
void menu_poll_bind_state(struct rgui_bind_state *state);
bool menu_poll_find_trigger(struct rgui_bind_state *state, struct rgui_bind_state *new_state);

#ifdef GEKKO
enum
{
   GX_RESOLUTIONS_512_192 = 0,
   GX_RESOLUTIONS_598_200,
   GX_RESOLUTIONS_640_200,
   GX_RESOLUTIONS_384_224,
   GX_RESOLUTIONS_448_224,
   GX_RESOLUTIONS_480_224,
   GX_RESOLUTIONS_512_224,
   GX_RESOLUTIONS_340_232,
   GX_RESOLUTIONS_512_232,
   GX_RESOLUTIONS_512_236,
   GX_RESOLUTIONS_336_240,
   GX_RESOLUTIONS_384_240,
   GX_RESOLUTIONS_512_240,
   GX_RESOLUTIONS_576_224,
   GX_RESOLUTIONS_608_224,
   GX_RESOLUTIONS_640_224,
   GX_RESOLUTIONS_530_240,
   GX_RESOLUTIONS_640_240,
   GX_RESOLUTIONS_512_448,
   GX_RESOLUTIONS_640_448, 
   GX_RESOLUTIONS_640_480,
   GX_RESOLUTIONS_LAST,
};
#endif


typedef struct
{
   uint64_t old_input_state;
   uint64_t trigger_state;
   bool do_held;

   unsigned delay_timer;
   unsigned delay_count;

   unsigned width;
   unsigned height;

   uint16_t *frame_buf;
   size_t frame_buf_pitch;
   bool frame_buf_show;

   file_list_t *menu_stack;
   file_list_t *selection_buf;
   size_t selection_ptr;
   bool need_refresh;
   bool msg_force;
   bool push_start_screen;

   core_info_list_t *core_info;
   bool defer_core;
   char deferred_path[PATH_MAX];

   // Quick jumping indices with L/R.
   // Rebuilt when parsing directory.
   size_t scroll_indices[2 * (26 + 2) + 1];
   unsigned scroll_indices_size;
   unsigned scroll_accel;

   char base_path[PATH_MAX];
   char default_glslp[PATH_MAX];
   char default_cgp[PATH_MAX];

   const uint8_t *font;
   bool alloc_font;

   char libretro_dir[PATH_MAX];
   struct retro_system_info info;
   bool load_no_rom;

#ifdef HAVE_SHADER_MANAGER
   struct gfx_shader shader;
#endif
   unsigned current_pad;

   rom_history_t *history;
   retro_time_t last_time; // Used to throttle RGUI in case VSync is broken.

   struct rgui_bind_state binds;
   struct
   {
      const char **buffer;
      const char *label;
      bool display;
   } keyboard;
} rgui_handle_t;

extern rgui_handle_t *rgui;

void menu_init(void);
bool menu_iterate(void);
void menu_free(void);

int rgui_input_postprocess(void *data, uint64_t old_state);

#ifdef HAVE_SHADER_MANAGER
void shader_manager_init(void *data);
void shader_manager_get_str(struct gfx_shader *shader,
      char *type_str, size_t type_str_size, unsigned type);
void shader_manager_set_preset(struct gfx_shader *shader,
      enum rarch_shader_type type, const char *path);
#endif

void menu_ticker_line(char *buf, size_t len, unsigned tick, const char *str, bool selected);

void menu_parse_and_resolve(void *data, unsigned menu_type);

void menu_init_core_info(void *data);

void load_menu_game_prepare(void);
bool load_menu_game(void);
void load_menu_game_history(unsigned game_index);
extern void load_menu_game_new_core(void);
void menu_rom_history_push(const char *path, const char *core_path,
      const char *core_name);
void menu_rom_history_push_current(void);

bool menu_replace_config(const char *path);

bool menu_save_new_config(void);

int menu_settings_toggle_setting(void *data, unsigned setting, unsigned action, unsigned menu_type);
int menu_set_settings(void *data, unsigned setting, unsigned action);
void menu_set_settings_label(char *type_str, size_t type_str_size, unsigned *w, unsigned type);

void menu_populate_entries(void *data, unsigned menu_type);
unsigned menu_type_is(unsigned type);

void menu_key_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

extern const menu_ctx_driver_t *menu_ctx;

#ifdef __cplusplus
}
#endif

#endif
