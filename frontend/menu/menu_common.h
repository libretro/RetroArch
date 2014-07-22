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
#include "../info/core_info.h"
#include "file_list.h"
#include "history.h"
#include "../../input/input_common.h"
#include "../../input/keyboard_line.h"
#include "../../performance.h"

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)

#ifndef HAVE_SHADER_MANAGER
#define HAVE_SHADER_MANAGER
#endif

#include "../../gfx/shader_parse.h"
#endif

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
   MENU_FILE_PLAIN,
   MENU_FILE_DIRECTORY,
   MENU_FILE_DEVICE,
   MENU_FILE_USE_DIRECTORY,
   MENU_SETTINGS,
   MENU_INFO_SCREEN,
   MENU_START_SCREEN,
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
   MENU_ACTION_MAPPING_PREVIOUS,
   MENU_ACTION_MAPPING_NEXT,
   MENU_ACTION_NOOP
} menu_action_t;

void menu_poll_bind_get_rested_axes(struct menu_bind_state *state);
void menu_poll_bind_state(struct menu_bind_state *state);
bool menu_poll_find_trigger(struct menu_bind_state *state, struct menu_bind_state *new_state);
bool menu_custom_bind_keyboard_cb(void *data, unsigned code);

void *menu_init(const void *data);
bool menu_iterate(void);
void menu_free(void *data);

void menu_ticker_line(char *buf, size_t len, unsigned tick, const char *str, bool selected);

void load_menu_game_prepare(void);
void load_menu_game_prepare_dummy(void);
bool load_menu_game(void);
void load_menu_game_history(unsigned game_index);
void menu_content_history_push_current(void);

bool menu_replace_config(const char *path);

bool menu_save_new_config(void);

int menu_defer_core(core_info_list_t *data, const char *dir, const char *path, char *deferred_path, size_t sizeof_deferred_path);

uint64_t menu_input(void);

void menu_flush_stack_type(unsigned final_type);
void menu_update_system_info(menu_handle_t *menu, bool *load_no_rom);
void menu_build_scroll_indices(file_list_t *buf);

#ifdef __cplusplus
}
#endif

#endif
