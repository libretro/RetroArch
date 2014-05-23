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
#include "../info/core_info.h"

#ifdef HAVE_RGUI
#define MENU_TEXTURE_FULLSCREEN false
#else
#define MENU_TEXTURE_FULLSCREEN true
#endif

#include "../../boolean.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "file_list.h"

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#define HAVE_SHADER_MANAGER
#include "../../gfx/shader_parse.h"
#endif

#include "history.h"

#ifndef GFX_MAX_SHADERS
#define GFX_MAX_SHADERS 16
#endif

#define RGUI_SETTINGS_CORE_INFO_NONE    0xffff
#define RGUI_SETTINGS_CORE_OPTION_NONE  0xffff
#define RGUI_SETTINGS_CORE_OPTION_START 0x10000

typedef enum
{
   RGUI_FILE_PLAIN,
   RGUI_FILE_DIRECTORY,
   RGUI_FILE_DEVICE,
   RGUI_FILE_USE_DIRECTORY,
   RGUI_SETTINGS,
   RGUI_START_SCREEN,
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

#define RGUI_KEYBOARD_BIND_TIMEOUT_SECONDS 5
struct rgui_bind_state
{
   struct retro_keybind *target;
   int64_t timeout_end; // For keyboard binding.
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
bool menu_custom_bind_keyboard_cb(void *data, unsigned code);

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
   core_info_t core_info_current;
   bool defer_core;
   char deferred_path[PATH_MAX];

   // Quick jumping indices with L/R.
   // Rebuilt when parsing directory.
   size_t scroll_indices[2 * (26 + 2) + 1];
   unsigned scroll_indices_size;
   unsigned scroll_accel;

   char default_glslp[PATH_MAX];
   char default_cgp[PATH_MAX];

   const uint8_t *font;
   bool alloc_font;

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

   bool bind_mode_keyboard;
   retro_time_t time;
   retro_time_t delta;
   retro_time_t target_msec;
   retro_time_t sleep_msec;
} rgui_handle_t;

void *menu_init(void);
bool menu_iterate(void *data);
void menu_free(void *data);

void menu_ticker_line(char *buf, size_t len, unsigned tick, const char *str, bool selected);

void load_menu_game_prepare(void *data);
void load_menu_game_prepare_dummy(void *data);
bool load_menu_game(void *data);
void load_menu_game_history(void *data, unsigned game_index);
extern void load_menu_game_new_core(void *data);
void menu_rom_history_push(void *data, const char *path, const char *core_path,
      const char *core_name);
void menu_rom_history_push_current(void *data);

bool menu_replace_config(void *data, const char *path);

bool menu_save_new_config(void);

int menu_defer_core(void *data, const char *dir, const char *path, char *deferred_path, size_t sizeof_deferred_path);

uint64_t menu_input(void *data);

void menu_flush_stack_type(void *data, unsigned final_type);
void menu_update_system_info(void *data, bool *load_no_rom);
void menu_build_scroll_indices(void *data, file_list_t *buf);

#ifdef __cplusplus
}
#endif

#endif
