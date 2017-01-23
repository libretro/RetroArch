/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

#ifndef __MENU_DRIVER_H__
#define __MENU_DRIVER_H__

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "widgets/menu_entry.h"
#include "menu_input.h"
#include "menu_entries.h"

#include "../gfx/video_shader_driver.h"

RETRO_BEGIN_DECLS

#ifndef MAX_COUNTERS
#define MAX_COUNTERS 64
#endif

#ifndef MAX_CHEAT_COUNTERS
#define MAX_CHEAT_COUNTERS 100
#endif

#define MENU_SETTINGS_CORE_INFO_NONE             0xffff
#define MENU_SETTINGS_CORE_OPTION_NONE           0xffff
#define MENU_SETTINGS_CHEEVOS_NONE               0xffff
#define MENU_SETTINGS_CORE_OPTION_CREATE         0x05000
#define MENU_SETTINGS_CORE_OPTION_START          0x10000
#define MENU_SETTINGS_PLAYLIST_ASSOCIATION_START 0x20000
#define MENU_SETTINGS_CHEEVOS_START              0x40000
#define MENU_SETTINGS_NETPLAY_ROOMS_START        0x80000

enum menu_image_type
{
   MENU_IMAGE_NONE = 0,
   MENU_IMAGE_WALLPAPER,
   MENU_IMAGE_THUMBNAIL,
   MENU_IMAGE_SAVESTATE_THUMBNAIL
};

enum menu_environ_cb
{
   MENU_ENVIRON_NONE = 0,
   MENU_ENVIRON_RESET_HORIZONTAL_LIST,
   MENU_ENVIRON_ENABLE_MOUSE_CURSOR,
   MENU_ENVIRON_DISABLE_MOUSE_CURSOR,
   MENU_ENVIRON_LAST
};

enum menu_state_changes
{
   MENU_STATE_RENDER_FRAMEBUFFER = 0,
   MENU_STATE_RENDER_MESSAGEBOX,
   MENU_STATE_BLIT,
   MENU_STATE_POP_STACK,
   MENU_STATE_POST_ITERATE
};

enum rarch_menu_ctl_state
{
   RARCH_MENU_CTL_NONE = 0,
   RARCH_MENU_CTL_REFRESH,
   RARCH_MENU_CTL_NAVIGATION_INCREMENT,
   RARCH_MENU_CTL_NAVIGATION_DECREMENT,
   RARCH_MENU_CTL_NAVIGATION_SET,
   RARCH_MENU_CTL_NAVIGATION_CLEAR,
   RARCH_MENU_CTL_NAVIGATION_SET_LAST,
   RARCH_MENU_CTL_NAVIGATION_ASCEND_ALPHABET,
   RARCH_MENU_CTL_NAVIGATION_DESCEND_ALPHABET,
   RARCH_MENU_CTL_IS_PENDING_QUICK_MENU,
   RARCH_MENU_CTL_SET_PENDING_QUICK_MENU,
   RARCH_MENU_CTL_IS_PENDING_QUIT,
   RARCH_MENU_CTL_SET_PENDING_QUIT,
   RARCH_MENU_CTL_IS_PENDING_SHUTDOWN,
   RARCH_MENU_CTL_SET_PENDING_SHUTDOWN,
   RARCH_MENU_CTL_DEINIT,
   RARCH_MENU_CTL_INIT,
   RARCH_MENU_CTL_SET_PREVENT_POPULATE,
   RARCH_MENU_CTL_UNSET_PREVENT_POPULATE,
   RARCH_MENU_CTL_IS_PREVENT_POPULATE,
   RARCH_MENU_CTL_SET_TEXTURE,
   RARCH_MENU_CTL_IS_TOGGLE,
   RARCH_MENU_CTL_SET_TOGGLE,
   RARCH_MENU_CTL_UNSET_TOGGLE,
   RARCH_MENU_CTL_DESTROY,
   RARCH_MENU_CTL_IS_SET_TEXTURE,
   RARCH_MENU_CTL_SET_OWN_DRIVER,
   RARCH_MENU_CTL_UNSET_OWN_DRIVER,
   RARCH_MENU_CTL_OWNS_DRIVER,
   RARCH_MENU_CTL_LOAD_NO_CONTENT_GET,
   RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT,
   RARCH_MENU_CTL_SET_LOAD_NO_CONTENT,
   RARCH_MENU_CTL_UNSET_LOAD_NO_CONTENT,
   RARCH_MENU_CTL_SYSTEM_INFO_DEINIT,
   RARCH_MENU_CTL_SYSTEM_INFO_GET,
   RARCH_MENU_CTL_PLAYLIST_FREE,
   RARCH_MENU_CTL_PLAYLIST_INIT,
   RARCH_MENU_CTL_PLAYLIST_GET,
   RARCH_MENU_CTL_TOGGLE,
   RARCH_MENU_CTL_CONTEXT_RESET,
   RARCH_MENU_CTL_CONTEXT_DESTROY,
   RARCH_MENU_CTL_POPULATE_ENTRIES,
   RARCH_MENU_CTL_FIND_DRIVER,
   RARCH_MENU_CTL_LOAD_IMAGE,
   RARCH_MENU_CTL_LIST_FREE,
   RARCH_MENU_CTL_LIST_CLEAR,
   RARCH_MENU_CTL_LIST_SET_SELECTION,
   RARCH_MENU_CTL_LIST_GET_SELECTION,
   RARCH_MENU_CTL_LIST_GET_SIZE,
   RARCH_MENU_CTL_LIST_GET_ENTRY,
   RARCH_MENU_CTL_LIST_CACHE,
   RARCH_MENU_CTL_LIST_INSERT,
   RARCH_MENU_CTL_LIST_PUSH,
   RARCH_MENU_CTL_ITERATE,
   RARCH_MENU_CTL_ENVIRONMENT,
   RARCH_MENU_CTL_DRIVER_DATA_GET,
   RARCH_MENU_CTL_POINTER_TAP,
   RARCH_MENU_CTL_OSK_PTR_AT_POS,
   RARCH_MENU_CTL_BIND_INIT,
   RARCH_MENU_CTL_UPDATE_THUMBNAIL_PATH,
   RARCH_MENU_CTL_UPDATE_THUMBNAIL_IMAGE,
   RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH,
   RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE
};

enum menu_settings_type
{
   MENU_SETTINGS_NONE       = FILE_TYPE_LAST + 1,
   MENU_SETTINGS,
   MENU_SETTINGS_TAB,
   MENU_HISTORY_TAB,
   MENU_MUSIC_TAB,
   MENU_VIDEO_TAB,
   MENU_IMAGES_TAB,
   MENU_NETPLAY_TAB,
   MENU_ADD_TAB,
   MENU_PLAYLISTS_TAB,
   MENU_SETTING_NO_ITEM,
   MENU_SETTING_DRIVER,
   MENU_SETTING_ACTION,
   MENU_SETTING_ACTION_RUN,
   MENU_SETTING_ACTION_CLOSE,
   MENU_SETTING_ACTION_CORE_OPTIONS,
   MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS,
   MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS,
   MENU_SETTING_ACTION_CORE_INFORMATION,
   MENU_SETTING_ACTION_CORE_DISK_OPTIONS,
   MENU_SETTING_ACTION_CORE_SHADER_OPTIONS,
   MENU_SETTING_ACTION_SAVESTATE,
   MENU_SETTING_ACTION_LOADSTATE,
   MENU_SETTING_ACTION_SCREENSHOT,
   MENU_SETTING_ACTION_DELETE_ENTRY,
   MENU_SETTING_ACTION_RESET,
   MENU_SETTING_STRING_OPTIONS,
   MENU_SETTING_GROUP,
   MENU_SETTING_SUBGROUP,
   MENU_SETTING_HORIZONTAL_MENU,
   MENU_WIFI,
   MENU_NETPLAY_LAN_SCAN,
   MENU_INFO_MESSAGE,
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
   MENU_SETTINGS_INPUT_DESC_END = MENU_SETTINGS_INPUT_DESC_BEGIN + (MAX_USERS * (RARCH_FIRST_CUSTOM_BIND + 4)),
   MENU_SETTINGS_LAST
};

typedef struct
{
   char deferred_path[PATH_MAX_LENGTH];
   char scratch_buf[PATH_MAX_LENGTH];
   char scratch2_buf[PATH_MAX_LENGTH];

   uint64_t state;
   struct
   {
      char msg[1024];
   } menu_state;

   char db_playlist_file[PATH_MAX_LENGTH];
} menu_handle_t;

typedef struct menu_ctx_driver
{
   void  (*set_texture)(void);
   void  (*render_messagebox)(void *data, const char *msg);
   int   (*iterate)(void *data, void *userdata, enum menu_action action);
   void  (*render)(void *data);
   void  (*frame)(void *data, video_frame_info_t *video_info);
   void* (*init)(void**);
   void  (*free)(void*);
   void  (*context_reset)(void *data);
   void  (*context_destroy)(void *data);
   void  (*populate_entries)(void *data,
         const char *path, const char *label,
         unsigned k);
   void  (*toggle)(void *userdata, bool);
   void  (*navigation_clear)(void *, bool);
   void  (*navigation_decrement)(void *data);
   void  (*navigation_increment)(void *data);
   void  (*navigation_set)(void *data, bool);
   void  (*navigation_set_last)(void *data);
   void  (*navigation_descend_alphabet)(void *, size_t *);
   void  (*navigation_ascend_alphabet)(void *, size_t *);
   bool  (*lists_init)(void*);
   void  (*list_insert)(void *userdata,
         file_list_t *list, const char *, const char *, const char *, size_t);
   int   (*list_prepend)(void *userdata,
         file_list_t *list, const char *, const char *, size_t);
   void  (*list_free)(file_list_t *list, size_t, size_t);
   void  (*list_clear)(file_list_t *list);
   void  (*list_cache)(void *data, enum menu_list_type, unsigned);
   int   (*list_push)(void *data, void *userdata, menu_displaylist_info_t*, unsigned);
   size_t(*list_get_selection)(void *data);
   size_t(*list_get_size)(void *data, enum menu_list_type type);
   void *(*list_get_entry)(void *data, enum menu_list_type type, unsigned i);
   void  (*list_set_selection)(void *data, file_list_t *list);
   int   (*bind_init)(menu_file_list_cbs_t *cbs,
         const char *path, const char *label, unsigned type, size_t idx);
   bool  (*load_image)(void *userdata, void *data, enum menu_image_type type);
   const char *ident;
   int (*environ_cb)(enum menu_environ_cb type, void *data, void *userdata);
   int (*pointer_tap)(void *data, unsigned x, unsigned y, unsigned ptr,
         menu_file_list_cbs_t *cbs,
         menu_entry_t *entry, unsigned action);
   void (*update_thumbnail_path)(void *data, unsigned i);
   void (*update_thumbnail_image)(void *data);
   int  (*osk_ptr_at_pos)(void *data, int x, int y, unsigned width, unsigned height);
   void (*update_savestate_thumbnail_path)(void *data, unsigned i);
   void (*update_savestate_thumbnail_image)(void *data);
} menu_ctx_driver_t;

typedef struct menu_ctx_load_image
{
   void *data;
   enum menu_image_type type;
} menu_ctx_load_image_t;

typedef struct menu_ctx_displaylist
{
   menu_displaylist_info_t *info;
   unsigned type;
} menu_ctx_displaylist_t;

typedef struct menu_ctx_iterate
{
   enum menu_action action;

   struct
   {
      int16_t x;
      int16_t y;
      bool touch;
   } pointer;

   struct
   {
      int16_t x;
      int16_t y;
      struct
      {
         bool left;
         bool right;
      } buttons;
      struct
      {
         bool up;
         bool down;
      } wheel;
   } mouse;
} menu_ctx_iterate_t;

typedef struct menu_ctx_environment
{
   enum menu_environ_cb type;
   void *data;
} menu_ctx_environment_t;

typedef struct menu_ctx_pointer
{
   unsigned x;
   unsigned y;
   unsigned ptr;
   menu_file_list_cbs_t *cbs;
   menu_entry_t *entry;
   unsigned action;
   int retcode;
} menu_ctx_pointer_t;

typedef struct menu_ctx_bind
{
   menu_file_list_cbs_t *cbs;
   const char *path;
   const char *label;
   unsigned type;
   size_t idx;
   uint32_t label_hash;
   int retcode;
} menu_ctx_bind_t;

/**
 * menu_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to menu driver at index. Can be NULL
 * if nothing found.
 **/
const void *menu_driver_find_handle(int index);

/**
 * menu_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of menu driver at index. Can be NULL
 * if nothing found.
 **/
const char *menu_driver_find_ident(int index);

/**
 * config_get_menu_driver_options:
 *
 * Get an enumerated list of all menu driver names,
 * separated by '|'.
 *
 * Returns: string listing of all menu driver names,
 * separated by '|'.
 **/
const char* config_get_menu_driver_options(void);

/* HACK */
extern unsigned int rdb_entry_start_game_selection_ptr;

const char *menu_driver_ident(void);

bool menu_driver_render(bool is_idle);

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data);

bool menu_driver_is_binding_state(void);

void menu_driver_set_binding_state(bool on);

void menu_driver_frame(video_frame_info_t *video_info);

bool menu_driver_is_alive(void);

extern menu_ctx_driver_t menu_ctx_xui;
extern menu_ctx_driver_t menu_ctx_rgui;
extern menu_ctx_driver_t menu_ctx_mui;
extern menu_ctx_driver_t menu_ctx_nuklear;
extern menu_ctx_driver_t menu_ctx_xmb;
extern menu_ctx_driver_t menu_ctx_zarch;
extern menu_ctx_driver_t menu_ctx_null;

RETRO_END_DECLS

#endif
