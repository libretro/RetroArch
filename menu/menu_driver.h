/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <formats/image.h>
#include <queues/task_queue.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "menu_defines.h"
#include "menu_dialog.h"
#include "menu_input.h"
#include "../input/input_osk.h"
#include "menu_input_bind_dialog.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "../gfx/gfx_animation.h"
#include "../gfx/gfx_display.h"

#include "../gfx/font_driver.h"
#include "../performance_counters.h"


RETRO_BEGIN_DECLS

#ifndef MAX_COUNTERS
#define MAX_COUNTERS 64
#endif

#ifndef MAX_CHEAT_COUNTERS
#define MAX_CHEAT_COUNTERS 6000
#endif

#define SCROLL_INDEX_SIZE          (2 * (26 + 2) + 1)

#define POWERSTATE_CHECK_INTERVAL  (30 * 1000000)
#define DATETIME_CHECK_INTERVAL    1000000

#define MENU_LIST_GET(list, idx) ((list) ? ((list)->menu_stack[(idx)]) : NULL)

#define MENU_LIST_GET_SELECTION(list, idx) ((list) ? ((list)->selection_buf[(idx)]) : NULL)

#define MENU_LIST_GET_STACK_SIZE(list, idx) ((list)->menu_stack[(idx)]->size)

#define MENU_ENTRIES_GET_SELECTION_BUF_PTR_INTERNAL(menu_st, idx) ((menu_st->entries.list) ? MENU_LIST_GET_SELECTION(menu_st->entries.list, (unsigned)idx) : NULL)
#define MENU_ENTRIES_NEEDS_REFRESH(menu_st) (!(menu_st->entries_nonblocking_refresh || !menu_st->entries_need_refresh))

#define MENU_SETTINGS_CORE_INFO_NONE             0xffff
#define MENU_SETTINGS_CORE_OPTION_NONE           0xffff
#define MENU_SETTINGS_CHEEVOS_NONE               0xffff
#define MENU_SETTINGS_CORE_OPTION_START          0x10000
#define MENU_SETTINGS_CHEEVOS_START              0x40000
#define MENU_SETTINGS_NETPLAY_ROOMS_START        0x80000

enum menu_settings_type
{
   MENU_SETTINGS_NONE       = FILE_TYPE_LAST + 1,
   MENU_SETTINGS,
   MENU_SETTINGS_TAB,
   MENU_HISTORY_TAB,
   MENU_FAVORITES_TAB,
   MENU_MUSIC_TAB,
   MENU_VIDEO_TAB,
   MENU_IMAGES_TAB,
   MENU_NETPLAY_TAB,
   MENU_EXPLORE_TAB,
   MENU_CONTENTLESS_CORES_TAB,
   MENU_ADD_TAB,
   MENU_PLAYLISTS_TAB,
   MENU_SETTING_DROPDOWN_ITEM,
   MENU_SETTING_DROPDOWN_ITEM_RESOLUTION,
   MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_PARAM,
   MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_PRESET_PARAM,
   MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_NUM_PASS,
   MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_DEFAULT_CORE,
   MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_LABEL_DISPLAY_MODE,
   MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_RIGHT_THUMBNAIL_MODE,
   MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_LEFT_THUMBNAIL_MODE,
   MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_SORT_MODE,
   MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_CORE_NAME,
   MENU_SETTING_DROPDOWN_ITEM_DISK_INDEX,
   MENU_SETTING_DROPDOWN_ITEM_INPUT_DEVICE_TYPE,
   MENU_SETTING_DROPDOWN_ITEM_INPUT_DEVICE_INDEX,
   MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION,
   MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION_KBD,
   MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM,
   MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM,
   MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM,
   MENU_SETTING_DROPDOWN_SETTING_INT_ITEM,
   MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM,
   MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM_SPECIAL,
   MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM_SPECIAL,
   MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM_SPECIAL,
   MENU_SETTING_DROPDOWN_SETTING_INT_ITEM_SPECIAL,
   MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL,
   MENU_SETTING_NO_ITEM,
   MENU_SETTING_DRIVER,
   MENU_SETTING_ACTION,
   MENU_SETTING_ACTION_RUN,
   MENU_SETTING_ACTION_CLOSE,
   MENU_SETTING_ACTION_CLOSE_HORIZONTAL,
   MENU_SETTING_ACTION_CORE_OPTIONS,
   MENU_SETTING_ACTION_CORE_OPTION_OVERRIDE_LIST,
   MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS,
   MENU_SETTING_ACTION_REMAP_FILE_MANAGER_LIST,
   MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS,
   MENU_SETTING_ACTION_CORE_MANAGER_OPTIONS,
#ifdef HAVE_MIST
   MENU_SETTING_ACTION_CORE_MANAGER_STEAM_OPTIONS,
   MENU_SETTING_ACTION_CORE_STEAM_INSTALL,
   MENU_SETTING_ACTION_CORE_STEAM_UNINSTALL,
#endif
   MENU_SETTING_ACTION_CORE_DISK_OPTIONS,
   MENU_SETTING_ACTION_CORE_SHADER_OPTIONS,
   MENU_SETTING_ACTION_SAVESTATE,
   MENU_SETTING_ACTION_LOADSTATE,
   MENU_SETTING_ACTION_SCREENSHOT,
   MENU_SETTING_ACTION_DELETE_ENTRY,
   MENU_SETTING_ACTION_RESET,
   MENU_SETTING_ACTION_CORE_LOCK,
   MENU_SETTING_ACTION_CORE_SET_STANDALONE_EXEMPT,
   MENU_SETTING_ACTION_CORE_DELETE,
   MENU_SETTING_ACTION_FAVORITES_DIR, /* "Start Directory" */
   MENU_SETTING_STRING_OPTIONS,
   MENU_SETTING_GROUP,
   MENU_SETTING_SUBGROUP,
   MENU_SETTING_HORIZONTAL_MENU,
   MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS,
   MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS,
   MENU_SETTING_PLAYLIST_MANAGER_DEFAULT_CORE,
   MENU_SETTING_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   MENU_SETTING_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE,
   MENU_SETTING_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE,
   MENU_SETTING_PLAYLIST_MANAGER_SORT_MODE,
   MENU_BLUETOOTH,
   MENU_WIFI,
   MENU_WIFI_DISCONNECT,
   MENU_ROOM,
   MENU_ROOM_LAN,
   MENU_ROOM_RELAY,
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

   MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_END = MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN + MENU_SETTINGS_AUDIO_MIXER_MAX_STREAMS,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_BEGIN,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_END = MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_BEGIN + MENU_SETTINGS_AUDIO_MIXER_MAX_STREAMS,

   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_END = MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN + MENU_SETTINGS_AUDIO_MIXER_MAX_STREAMS,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_END = MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN + MENU_SETTINGS_AUDIO_MIXER_MAX_STREAMS,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_END = MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN + MENU_SETTINGS_AUDIO_MIXER_MAX_STREAMS,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_END = MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN + MENU_SETTINGS_AUDIO_MIXER_MAX_STREAMS,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_END = MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN + MENU_SETTINGS_AUDIO_MIXER_MAX_STREAMS,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN,
   MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_END = MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN + MENU_SETTINGS_AUDIO_MIXER_MAX_STREAMS,
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

   MENU_SETTINGS_INPUT_LIBRETRO_DEVICE,
   MENU_SETTINGS_INPUT_ANALOG_DPAD_MODE,
   MENU_SETTINGS_INPUT_INPUT_REMAP_PORT,
   MENU_SETTINGS_INPUT_BEGIN,
   MENU_SETTINGS_INPUT_END = MENU_SETTINGS_INPUT_BEGIN + RARCH_CUSTOM_BIND_LIST_END + 6,
   MENU_SETTINGS_INPUT_DESC_BEGIN,
   MENU_SETTINGS_INPUT_DESC_END = MENU_SETTINGS_INPUT_DESC_BEGIN + ((RARCH_FIRST_CUSTOM_BIND + 8) * MAX_USERS),
   MENU_SETTINGS_INPUT_DESC_KBD_BEGIN,
   MENU_SETTINGS_INPUT_DESC_KBD_END = MENU_SETTINGS_INPUT_DESC_KBD_BEGIN + (RARCH_MAX_KEYS * MAX_USERS),
   MENU_SETTINGS_REMAPPING_PORT_BEGIN,
   MENU_SETTINGS_REMAPPING_PORT_END = MENU_SETTINGS_REMAPPING_PORT_BEGIN + (MAX_USERS),

   MENU_SETTINGS_SUBSYSTEM_LOAD,
   MENU_SETTINGS_SUBSYSTEM_ADD,
   MENU_SETTINGS_SUBSYSTEM_LAST = MENU_SETTINGS_SUBSYSTEM_ADD + RARCH_MAX_SUBSYSTEMS,
   MENU_SETTINGS_CHEAT_MATCH,

   MENU_SET_SCREEN_BRIGHTNESS,

#ifdef HAVE_LAKKA_SWITCH
   MENU_SET_SWITCH_GPU_PROFILE,
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
   MENU_SET_SWITCH_CPU_PROFILE,
#endif

   MENU_SETTINGS_CPU_POLICY_SET_MINFREQ,
   MENU_SETTINGS_CPU_POLICY_SET_MAXFREQ,
   MENU_SETTINGS_CPU_POLICY_SET_GOVERNOR,
   MENU_SETTINGS_CPU_MANAGED_SET_MINFREQ,
   MENU_SETTINGS_CPU_MANAGED_SET_MAXFREQ,

   MENU_SET_CDROM_LIST,
   MENU_SET_LOAD_CDROM_LIST,
   MENU_SET_EJECT_DISC,
   MENU_SET_CDROM_INFO,
   MENU_SETTING_ACTION_DELETE_PLAYLIST,
   MENU_SETTING_ACTION_PLAYLIST_MANAGER_RESET_CORES,
   MENU_SETTING_ACTION_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   MENU_SETTING_ACTION_PLAYLIST_MANAGER_REFRESH_PLAYLIST,

   MENU_SETTING_MANUAL_CONTENT_SCAN_DIR,
   MENU_SETTING_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   MENU_SETTING_MANUAL_CONTENT_SCAN_CORE_NAME,
   MENU_SETTING_ACTION_MANUAL_CONTENT_SCAN_START,

   MENU_SETTING_ACTION_CORE_CREATE_BACKUP,
   MENU_SETTING_ACTION_CORE_RESTORE_BACKUP,
   MENU_SETTING_ITEM_CORE_RESTORE_BACKUP,
   MENU_SETTING_ACTION_CORE_DELETE_BACKUP,
   MENU_SETTING_ITEM_CORE_DELETE_BACKUP,

   MENU_SETTING_ACTION_VIDEO_FILTER_REMOVE,
   MENU_SETTING_ACTION_AUDIO_DSP_PLUGIN_REMOVE,

   MENU_SETTING_ACTION_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   MENU_SETTING_ACTION_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   MENU_SETTING_ACTION_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   MENU_SETTING_ACTION_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   MENU_SETTING_ACTION_CORE_OPTIONS_RESET,
   MENU_SETTING_ACTION_CORE_OPTIONS_FLUSH,

   MENU_SETTING_ACTION_REMAP_FILE_LOAD,
   MENU_SETTING_ACTION_REMAP_FILE_SAVE_CORE,
   MENU_SETTING_ACTION_REMAP_FILE_SAVE_CONTENT_DIR,
   MENU_SETTING_ACTION_REMAP_FILE_SAVE_GAME,
   MENU_SETTING_ACTION_REMAP_FILE_REMOVE_CORE,
   MENU_SETTING_ACTION_REMAP_FILE_REMOVE_CONTENT_DIR,
   MENU_SETTING_ACTION_REMAP_FILE_REMOVE_GAME,
   MENU_SETTING_ACTION_REMAP_FILE_RESET,

   MENU_SETTING_ACTION_CONTENTLESS_CORE_RUN,

   MENU_SETTINGS_LAST
};

struct menu_list
{
   file_list_t **menu_stack;
   size_t menu_stack_size;
   file_list_t **selection_buf;
   size_t selection_buf_size;
};

typedef struct menu_list menu_list_t;

typedef struct menu_ctx_load_image
{
   void *data;
   enum menu_image_type type;
} menu_ctx_load_image_t;

typedef struct menu_ctx_driver
{
   /* Set a framebuffer texture. This is used for instance by RGUI. */
   void  (*set_texture)(void *data);
   /* Render a messagebox to the screen. */
   void  (*render_messagebox)(void *data, const char *msg);
   void  (*render)(void *data, unsigned width, unsigned height, bool is_idle);
   void  (*frame)(void *data, video_frame_info_t *video_info);
   /* Initializes the menu driver. (setup) */
   void* (*init)(void**, bool);
   /* Frees the menu driver. (teardown) */
   void  (*free)(void*);
   /* This will be invoked when we are running a hardware context
    * and we have just flushed the state. For instance - we have
    * just toggled fullscreen, the GL driver did a teardown/setup -
    * we now need to rebuild all of our textures and state for the
    * menu driver. */
   void  (*context_reset)(void *data, bool video_is_threaded);
   /* This will be invoked when we are running a hardware context
    * and the context in question wants to tear itself down. All
    * textures and related state on the menu driver will also
    * be freed up then. */
   void  (*context_destroy)(void *data);
   void  (*populate_entries)(void *data,
         const char *path, const char *label,
         unsigned k);
   void  (*toggle)(void *userdata, bool);
   /* This will clear the navigation position. */
   void  (*navigation_clear)(void *, bool);
   /* This will decrement the navigation position by one. */
   void  (*navigation_decrement)(void *data);
   /* This will increment the navigation position by one. */
   void  (*navigation_increment)(void *data);
   void  (*navigation_set)(void *data, bool);
   void  (*navigation_set_last)(void *data);
   /* This will descend the navigation position by one alphabet letter. */
   void  (*navigation_descend_alphabet)(void *, size_t *);
   /* This will ascend the navigation position by one alphabet letter. */
   void  (*navigation_ascend_alphabet)(void *, size_t *);
   /* Initializes a new menu list. */
   bool  (*lists_init)(void*);
   void  (*list_insert)(void *userdata,
         file_list_t *list, const char *, const char *, const char *, size_t,
         unsigned);
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
   /* Load an image for use by the menu driver */
   bool  (*load_image)(void *userdata, void *data, enum menu_image_type type);
   const char *ident;
   int (*environ_cb)(enum menu_environ_cb type, void *data, void *userdata);
   void (*update_thumbnail_path)(void *data, unsigned i, char pos);
   void (*update_thumbnail_image)(void *data);
   void (*refresh_thumbnail_image)(void *data, unsigned i);
   void (*set_thumbnail_system)(void *data, char* s, size_t len);
   void (*get_thumbnail_system)(void *data, char* s, size_t len);
   void (*set_thumbnail_content)(void *data, const char *s);
   int  (*osk_ptr_at_pos)(void *data, int x, int y, unsigned width, unsigned height);
   void (*update_savestate_thumbnail_path)(void *data, unsigned i);
   void (*update_savestate_thumbnail_image)(void *data);
   int (*pointer_down)(void *data, unsigned x, unsigned y, unsigned ptr,
         menu_file_list_cbs_t *cbs,
         menu_entry_t *entry, unsigned action);
   int (*pointer_up)(void *data, unsigned x, unsigned y, unsigned ptr,
         enum menu_input_pointer_gesture gesture,
         menu_file_list_cbs_t *cbs,
         menu_entry_t *entry, unsigned action);
   /* This will be invoked whenever a menu entry action
    * (menu_entry_action()) is performed */
   int (*entry_action)(void *userdata, menu_entry_t *entry, size_t i, enum menu_action action);
} menu_ctx_driver_t;

typedef struct
{
   uint64_t state;

   const menu_ctx_driver_t *driver_ctx;
   void *userdata;
   char *core_buf;

   size_t                     core_len;
   /* This is used for storing intermediary variables
    * that get used later on during menu actions -
    * for instance, selecting a shader pass for a shader
    * slot */
   struct
   {
      unsigned                unsigned_var;
   } scratchpad;
   unsigned rpl_entry_selection_ptr;

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   /* Used to cache the type and directory
    * of the last shader preset/pass loaded
    * via the menu file browser */
   struct
   {
      enum rarch_shader_type preset_type;
      enum rarch_shader_type pass_type;

      char preset_dir[PATH_MAX_LENGTH];
      char preset_file_name[PATH_MAX_LENGTH];

      char pass_dir[PATH_MAX_LENGTH];
      char pass_file_name[PATH_MAX_LENGTH];
   } last_shader_selection;
#endif

   /* Used to cache the last start content
    * loaded via the menu file browser */
   struct
   {
      char directory[PATH_MAX_LENGTH];
      char file_name[PATH_MAX_LENGTH];
   } last_start_content;

   char menu_state_msg[8192];
   /* Scratchpad variables. These are used for instance
    * by the filebrowser when having to store intermediary
    * paths (subdirs/previous dirs/current dir/path, etc).
    */
   char deferred_path[PATH_MAX_LENGTH];
   char scratch_buf[PATH_MAX_LENGTH];
   char scratch2_buf[PATH_MAX_LENGTH];
   char db_playlist_file[PATH_MAX_LENGTH];
   char filebrowser_label[PATH_MAX_LENGTH];
   char detect_content_path[PATH_MAX_LENGTH];
} menu_handle_t;

struct menu_state
{
   /* Timers */
   retro_time_t current_time_us;
   retro_time_t powerstate_last_time_us;
   retro_time_t datetime_last_time_us;
   retro_time_t input_last_time_us;
   menu_input_t input_state;               /* retro_time_t alignment */

   retro_time_t prev_start_time;
   retro_time_t noop_press_time;
   retro_time_t noop_start_time;
   retro_time_t action_start_time;
   retro_time_t action_press_time;

   struct menu_bind_state input_binds;     /* uint64_t alignment */

   menu_handle_t *driver_data;
   void *userdata;
   const menu_ctx_driver_t *driver_ctx;
   const char **input_dialog_keyboard_buffer;

   struct
   {
      rarch_setting_t *list_settings;
      menu_list_t *list;
      size_t begin;
   } entries;
   size_t   selection_ptr;
   size_t   contentless_core_ptr;

   /* Quick jumping indices with L/R.
    * Rebuilt when parsing directory. */
   struct
   {
      size_t   index_list[SCROLL_INDEX_SIZE];
      unsigned index_size;
      unsigned acceleration;
   } scroll;

   /* unsigned alignment */
   unsigned input_dialog_kb_type;
   unsigned input_dialog_kb_idx;
   unsigned input_driver_flushing_input;
   menu_dialog_t dialog_st;

   /* int16_t alignment */
   menu_input_pointer_hw_state_t input_pointer_hw_state;

   enum menu_action prev_action;

   /* When generating a menu list in menu_displaylist_build_list(),
    * the entry with a label matching 'pending_selection' will
    * be selected automatically */
   char pending_selection[PATH_MAX_LENGTH];
   /* Storage container for current menu datetime
    * representation string */
   char datetime_cache[255];
   /* Filled with current content path when a core calls
    * RETRO_ENVIRONMENT_SHUTDOWN. Value is required in
    * generic_menu_entry_action(), and must be cached
    * since RETRO_ENVIRONMENT_SHUTDOWN will cause
    * RARCH_PATH_CONTENT to be cleared */
   char pending_env_shutdown_content_path[PATH_MAX_LENGTH];

#ifdef HAVE_MENU
   char input_dialog_kb_label_setting[256];
   char input_dialog_kb_label[256];
#endif
   unsigned char kb_key_state[RETROK_LAST];

   bool input_dialog_kb_display;
   /* when enabled, on next iteration the 'Quick Menu' list will
    * be pushed onto the stack */
   bool pending_quick_menu;
   bool prevent_populate;
   /* The menu driver owns the userdata */
   bool data_own;
   /* Flagged when menu entries need to be refreshed */
   bool entries_need_refresh;
   bool entries_nonblocking_refresh;
   /* 'Close Content'-hotkey menu resetting */
   bool pending_close_content;
   /* Flagged when a core calls RETRO_ENVIRONMENT_SHUTDOWN,
    * requiring the menu to be flushed on the next iteration */
   bool pending_env_shutdown_flush;
   /* Screensaver status
    * - Does menu driver support screensaver functionality?
    * - Is screensaver currently active? */
   bool screensaver_supported;
   bool screensaver_active;
   bool is_binding;
   bool alive;
};


typedef struct menu_content_ctx_defer_info
{
   void *data;
   const char *dir;
   const char *path;
   const char *menu_label;
   char *s;
   size_t len;
} menu_content_ctx_defer_info_t;

typedef struct menu_ctx_displaylist
{
   menu_displaylist_info_t *info;
   unsigned type;
} menu_ctx_displaylist_t;

typedef struct menu_ctx_environment
{
   void *data;
   enum menu_environ_cb type;
} menu_ctx_environment_t;

typedef struct menu_ctx_pointer
{
   menu_file_list_cbs_t *cbs;
   menu_entry_t *entry;
   unsigned x;
   unsigned y;
   unsigned ptr;
   unsigned action;
   int retcode;
   enum menu_input_pointer_gesture gesture;
} menu_ctx_pointer_t;

typedef struct menu_ctx_bind
{
   menu_file_list_cbs_t *cbs;
   const char *path;
   const char *label;
   size_t idx;
   unsigned type;
} menu_ctx_bind_t;

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

const char *menu_driver_ident(void);

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data);

void menu_driver_frame(bool menu_is_alive, video_frame_info_t *video_info);

int menu_driver_deferred_push_content_list(file_list_t *list);

bool menu_driver_list_cache(menu_ctx_list_t *list);

void menu_driver_navigation_set(bool scroll);

void menu_driver_populate_entries(menu_displaylist_info_t *info);

bool menu_driver_push_list(menu_ctx_displaylist_t *disp_list);

bool menu_driver_init(bool video_is_threaded);

void menu_driver_set_thumbnail_system(char *s, size_t len);

void menu_driver_get_thumbnail_system(char *s, size_t len);

void menu_driver_set_thumbnail_content(char *s, size_t len);

bool menu_driver_list_get_selection(menu_ctx_list_t *list);

bool menu_driver_list_get_entry(menu_ctx_list_t *list);

bool menu_driver_list_get_size(menu_ctx_list_t *list);

bool menu_driver_screensaver_supported(void);

retro_time_t menu_driver_get_current_time(void);

size_t menu_navigation_get_selection(void);

void menu_navigation_set_selection(size_t val);

void menu_display_handle_thumbnail_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

void menu_display_handle_left_thumbnail_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

void menu_display_handle_savestate_thumbnail_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

void menu_display_timedate(gfx_display_ctx_datetime_t *datetime);

void menu_display_powerstate(gfx_display_ctx_powerstate_t *powerstate);

void menu_display_handle_wallpaper_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

#if defined(HAVE_LIBRETRODB)
typedef struct explore_state explore_state_t;
explore_state_t *menu_explore_build_list(const char *directory_playlist,
      const char *directory_database);
uintptr_t menu_explore_get_entry_icon(unsigned type);
void menu_explore_context_init(void);
void menu_explore_context_deinit(void);
void menu_explore_free_state(explore_state_t *state);
void menu_explore_free(void);
void menu_explore_set_state(explore_state_t *state);
#endif

/* Contentless cores START */
enum contentless_core_runtime_status
{
   CONTENTLESS_CORE_RUNTIME_UNKNOWN = 0,
   CONTENTLESS_CORE_RUNTIME_MISSING,
   CONTENTLESS_CORE_RUNTIME_VALID
};

typedef struct
{
   char *runtime_str;
   char *last_played_str;
   enum contentless_core_runtime_status status;
} contentless_core_runtime_info_t;

typedef struct
{
   char *licenses_str;
   contentless_core_runtime_info_t runtime;
} contentless_core_info_entry_t;

uintptr_t menu_contentless_cores_get_entry_icon(const char *core_id);
void menu_contentless_cores_context_init(void);
void menu_contentless_cores_context_deinit(void);
void menu_contentless_cores_free(void);
void menu_contentless_cores_set_runtime(const char *core_id,
      const contentless_core_runtime_info_t *runtime_info);
void menu_contentless_cores_get_info(const char *core_id,
      const contentless_core_info_entry_t **info);
void menu_contentless_cores_flush_runtime(void);
/* Contentless cores END */

/* Returns true if search filter is enabled
 * for the specified menu list */
bool menu_driver_search_filter_enabled(const char *label, unsigned type);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
void menu_driver_set_last_shader_preset_path(const char *path);
void menu_driver_set_last_shader_pass_path(const char *path);
enum rarch_shader_type menu_driver_get_last_shader_preset_type(void);
enum rarch_shader_type menu_driver_get_last_shader_pass_type(void);
void menu_driver_get_last_shader_preset_path(
      const char **directory, const char **file_name);
void menu_driver_get_last_shader_pass_path(
      const char **directory, const char **file_name);
#endif

const char *menu_driver_get_last_start_directory(void);
const char *menu_driver_get_last_start_file_name(void);
void menu_driver_set_last_start_content(const char *start_content_path);
const char *menu_driver_get_pending_selection(void);
void menu_driver_set_pending_selection(const char *pending_selection);

struct menu_state *menu_state_get_ptr(void);

enum action_iterate_type
{
   ITERATE_TYPE_DEFAULT = 0,
   ITERATE_TYPE_HELP,
   ITERATE_TYPE_INFO,
   ITERATE_TYPE_BIND
};

int menu_dialog_iterate(
      menu_dialog_t *p_dialog,
      settings_t *settings,
      char *s, size_t len,
      retro_time_t current_time);

void menu_entries_settings_deinit(struct menu_state *menu_st);

int menu_input_key_bind_set_mode_common(
      struct menu_state *menu_st,
      struct menu_bind_state *binds,
      enum menu_input_binds_ctl_state state,
      rarch_setting_t  *setting,
      settings_t *settings);

void menu_input_key_bind_poll_bind_get_rested_axes(
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      struct menu_bind_state *state);

#ifdef ANDROID
bool menu_input_key_bind_poll_find_hold_pad(
      struct menu_bind_state *new_state,
      struct retro_keybind * output,
      unsigned p);
#endif

bool menu_input_key_bind_poll_find_trigger_pad(
      struct menu_bind_state *state,
      struct menu_bind_state *new_state,
      struct retro_keybind * output,
      unsigned p);

bool menu_input_key_bind_poll_find_trigger(
      unsigned max_users,
      struct menu_bind_state *state,
      struct menu_bind_state *new_state,
      struct retro_keybind * output);

void input_event_osk_iterate(
      void *osk_grid,
      enum osk_type osk_idx);

void menu_input_get_mouse_hw_state(
      gfx_display_t *p_disp,
      menu_handle_t *menu,
      input_driver_state_t *input_driver_st,
      input_driver_t *current_input,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      bool keyboard_mapping_blocked,
      bool menu_mouse_enable,
      bool input_overlay_enable,
      bool overlay_active,
      menu_input_pointer_hw_state_t *hw_state);

void menu_input_get_touchscreen_hw_state(
      gfx_display_t *p_disp,
      menu_handle_t *menu,
      input_driver_state_t *input_driver_st,
      input_driver_t *current_input,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      bool keyboard_mapping_blocked,
      bool overlay_active,
      bool pointer_enabled,
      unsigned input_touch_scale,
      menu_input_pointer_hw_state_t *hw_state);

bool menu_entries_init(
      struct menu_state *menu_st,
      const menu_ctx_driver_t *menu_driver_ctx);

void menu_entries_list_deinit(
      const menu_ctx_driver_t *menu_driver_ctx,
      struct menu_state *menu_st);

void menu_list_flush_stack(
      const menu_ctx_driver_t *menu_driver_ctx,
      void *menu_userdata,
      struct menu_state *menu_st,
      menu_list_t *list,
      size_t idx, const char *needle, unsigned final_type);

bool menu_list_pop_stack(
      const menu_ctx_driver_t *menu_driver_ctx,
      void *menu_userdata,
      menu_list_t *list,
      size_t idx,
      size_t *directory_ptr);

bool input_event_osk_show_symbol_pages(
      menu_handle_t *menu);

float menu_input_get_dpi(
      menu_handle_t *menu,
      gfx_display_t *p_disp,
      unsigned video_width,
      unsigned video_height);

void menu_input_pointer_close_messagebox(struct menu_state *menu_st);

void menu_input_key_bind_poll_bind_state(
      input_driver_state_t *input_driver_st,
      const retro_keybind_set *binds,
      float input_axis_threshold,
      unsigned joy_idx,
      struct menu_bind_state *state,
      bool timed_out,
      bool keyboard_mapping_blocked);

enum action_iterate_type action_iterate_type(const char *label);

void menu_cbs_init(
      struct menu_state *menu_st,
      const menu_ctx_driver_t *menu_driver_ctx,
      file_list_t *list,
      menu_file_list_cbs_t *cbs,
      const char *path, const char *label,
      unsigned type, size_t idx);

bool menu_driver_displaylist_push(
      struct menu_state *menu_st,
      settings_t *settings,
      file_list_t *entry_list,
      file_list_t *entry_stack);

int generic_menu_entry_action(void *userdata, menu_entry_t *entry, size_t i, enum menu_action action);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
void menu_driver_get_last_shader_path_int(
      settings_t *settings, enum rarch_shader_type type,
      const char *shader_dir, const char *shader_file_name,
      const char **dir_out, const char **file_name_out);
#endif

int menu_entries_elem_get_first_char(
      file_list_t *list, unsigned offset);

void menu_entries_build_scroll_indices(
      struct menu_state *menu_st,
      file_list_t *list);

void get_current_menu_value(struct menu_state *menu_st,
      char *s, size_t len);
void get_current_menu_label(struct menu_state *menu_st,
      char *s, size_t len);
void get_current_menu_sublabel(struct menu_state *menu_st,
      char *s, size_t len);

void menu_display_common_image_upload(
      const menu_ctx_driver_t *menu_driver_ctx,
      void *menu_userdata,
      struct texture_image *img,
      void *user_data,
      unsigned type);

enum menu_driver_id_type menu_driver_set_id(
      const char *driver_name);

/**
 * config_get_menu_driver_options:
 *
 * Get an enumerated list of all menu driver names,
 * separated by '|'.
 *
 * Returns: string listing of all menu driver names,
 * separated by '|'.
 **/
const char *config_get_menu_driver_options(void);

bool generic_menu_init_list(struct menu_state *menu_st,
      settings_t *settings);

/* Teardown function for the menu driver. */
void menu_driver_destroy(
      struct menu_state *menu_st);

bool rarch_menu_init(
      struct menu_state *menu_st,
      menu_dialog_t        *p_dialog,
      const menu_ctx_driver_t *menu_driver_ctx,
      menu_input_t *menu_input,
      menu_input_pointer_hw_state_t *pointer_hw_state,
      settings_t *settings
      );

extern menu_ctx_driver_t menu_ctx_ozone;
extern menu_ctx_driver_t menu_ctx_xui;
extern menu_ctx_driver_t menu_ctx_rgui;
extern menu_ctx_driver_t menu_ctx_mui;
extern menu_ctx_driver_t menu_ctx_xmb;
extern menu_ctx_driver_t menu_ctx_stripes;

void menu_input_search_cb(void *userdata, const char *str);
bool menu_input_key_bind_custom_bind_keyboard_cb(
      void *data, unsigned code);
/* This callback gets triggered by the keyboard whenever
 * we press or release a keyboard key. When a keyboard
 * key is being pressed down, 'down' will be true. If it
 * is being released, 'down' will be false.
 */
void menu_input_key_event(bool down, unsigned keycode,
      uint32_t character, uint16_t mod);

const menu_ctx_driver_t *menu_driver_find_driver(
      settings_t *settings,
      const char *prefix,
      bool verbosity_enabled);

bool menu_input_key_bind_iterate(
      settings_t *settings,
      menu_input_ctx_bind_t *bind,
      retro_time_t current_time);

/*
 * This function gets called in order to process all input events
 * for the current frame.
 *
 * Sends input code to menu for one frame.
 *
 * It uses as input the local variables 'input' and 'trigger_input'.
 *
 * Mouse and touch input events get processed inside this function.
 *
 * NOTE: 'input' and 'trigger_input' is sourced from the keyboard and/or
 * the gamepad. It does not contain input state derived from the mouse
 * and/or touch - this gets dealt with separately within this function.
 *
 * TODO/FIXME - maybe needs to be overhauled so we can send multiple
 * events per frame if we want to, and we shouldn't send the
 * entire button state either but do a separate event per button
 * state.
 */
unsigned menu_event(
      settings_t *settings,
      input_bits_t *p_input,
      input_bits_t *p_trigger_input,
      bool display_kb);

int menu_input_post_iterate(
      gfx_display_t *p_disp,
      struct menu_state *menu_st,
      unsigned action,
      retro_time_t current_time);

/* Gets called when we want to toggle the menu.
 * If the menu is already running, it will be turned off.
 * If the menu is off, then the menu will be started.
 */
void menu_driver_toggle(
      void *curr_video_data,
      void *video_driver_data,
      menu_handle_t *menu,
      menu_input_t *menu_input,
      settings_t *settings,
      bool menu_driver_alive,
      bool overlay_alive,
      retro_keyboard_event_t *key_event,
      retro_keyboard_event_t *frontend_key_event,
      bool on);

/* Iterate the menu driver for one frame. */
bool menu_driver_iterate(
      struct menu_state *menu_st,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      enum menu_action action,
      retro_time_t current_time);

extern const menu_ctx_driver_t *menu_ctx_drivers[];

RETRO_END_DECLS

#endif
