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
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "menu_defines.h"
#include "menu_input.h"
#include "../input/input_osk.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "../gfx/gfx_animation.h"
#include "../gfx/gfx_display.h"
#include "../gfx/gfx_thumbnail_path.h"
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
#define MENU_ENTRIES_NEEDS_REFRESH(menu_st) (!((menu_st->flags & MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH) || !(menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH)))

#define MENU_SETTINGS_CORE_INFO_NONE             0xffff
#define MENU_SETTINGS_CORE_OPTION_NONE           0xffff
#define MENU_SETTINGS_CHEEVOS_NONE               0xffff
#define MENU_SETTINGS_CORE_OPTION_START          0x10000
#define MENU_SETTINGS_CHEEVOS_START              0x40000
#define MENU_SETTINGS_NETPLAY_ROOMS_START        0x80000

/* "Normalize" non-alphabetical entries so they
 * are lumped together for purposes of jumping. */
#define ELEM_GET_FIRST_CHAR(ret) ((ret < 'a') ? ('a' - 1) : (ret > 'z') ? ('z' + 1) : ret)

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
   MENU_SETTING_DROPDOWN_ITEM_INPUT_SELECT_RESERVED_DEVICE,
#ifdef ANDROID
    MENU_SETTING_DROPDOWN_ITEM_INPUT_SELECT_PHYSICAL_KEYBOARD,
#endif
   MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION,
   MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION_KBD,
   MENU_SETTING_DROPDOWN_ITEM_AUDIO_DEVICE,
#ifdef HAVE_MICROPHONE
   MENU_SETTING_DROPDOWN_ITEM_MICROPHONE_DEVICE,
#endif
#ifdef HAVE_NETWORKING
   MENU_SETTING_DROPDOWN_ITEM_NETPLAY_MITM_SERVER,
#endif
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
   MENU_SETTING_ACTION_PLAYREPLAY,
   MENU_SETTING_ACTION_RECORDREPLAY,
   MENU_SETTING_ACTION_HALTREPLAY,
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
   MENU_INFO_ACHIEVEMENTS_SERVER_UNREACHABLE,
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
   MENU_NETPLAY_KICK,
   MENU_NETPLAY_BAN,
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

#if defined(HAVE_LIBNX)
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
   MENU_SETTING_ACTION_REMAP_FILE_SAVE_AS,
   MENU_SETTING_ACTION_REMAP_FILE_SAVE_CORE,
   MENU_SETTING_ACTION_REMAP_FILE_SAVE_CONTENT_DIR,
   MENU_SETTING_ACTION_REMAP_FILE_SAVE_GAME,
   MENU_SETTING_ACTION_REMAP_FILE_REMOVE_CORE,
   MENU_SETTING_ACTION_REMAP_FILE_REMOVE_CONTENT_DIR,
   MENU_SETTING_ACTION_REMAP_FILE_REMOVE_GAME,
   MENU_SETTING_ACTION_REMAP_FILE_RESET,
   MENU_SETTING_ACTION_REMAP_FILE_FLUSH,

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

      char pass_dir[DIR_MAX_LENGTH];
      char preset_dir[DIR_MAX_LENGTH];
      char preset_file_name[NAME_MAX_LENGTH];
      char pass_file_name[NAME_MAX_LENGTH];
   } last_shader_selection;
#endif

   /* Used to cache the last start content
    * loaded via the menu file browser */
   struct
   {
      char directory[DIR_MAX_LENGTH];
      char file_name[NAME_MAX_LENGTH];
   } last_start_content;

   char menu_state_msg[PATH_MAX_LENGTH * 2];
   /* Scratchpad variables. These are used for instance
    * by the filebrowser when having to store intermediary
    * paths (subdirs/previous dirs/current dir/path, etc).
    */
   char deferred_path[PATH_MAX_LENGTH];
   char scratch_buf[PATH_MAX_LENGTH];
   char scratch2_buf[PATH_MAX_LENGTH];
   char db_playlist_file[PATH_MAX_LENGTH];
   char filebrowser_label[NAME_MAX_LENGTH];
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

   gfx_thumbnail_path_data_t *thumbnail_path_data;
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
      enum menu_scroll_mode mode;
   } scroll;

   /* unsigned alignment */
   unsigned input_dialog_kb_type;
   unsigned input_dialog_kb_idx;
   unsigned input_driver_flushing_input;
   menu_dialog_t dialog_st;
   enum menu_action prev_action;
#ifdef HAVE_RUNAHEAD
   enum menu_runahead_mode runahead_mode;
#endif

   /* int16_t alignment */
   menu_input_pointer_hw_state_t input_pointer_hw_state;

   uint16_t flags;
#ifdef HAVE_OVERLAY
   uint16_t overlay_types;
#endif

   /* When generating a menu list in menu_displaylist_build_list(),
    * the entry with a label matching 'pending_selection' will
    * be selected automatically */
   char pending_selection[PATH_MAX_LENGTH];
   /* Storage container for current menu datetime
    * representation string */
   char datetime_cache[NAME_MAX_LENGTH];
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

#if defined(HAVE_LIBRETRODB)
typedef struct explore_state explore_state_t;
#endif

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

#if defined(HAVE_LIBRETRODB)
explore_state_t *menu_explore_build_list(const char *directory_playlist,
      const char *directory_database);
uintptr_t menu_explore_get_entry_icon(unsigned type);
ssize_t menu_explore_get_entry_playlist_index(unsigned type,
      playlist_t **playlist, const struct playlist_entry **entry,
      file_list_t *list, size_t *list_pos, size_t *list_size);
ssize_t menu_explore_set_playlist_thumbnail(unsigned type,
      gfx_thumbnail_path_data_t *thumbnail_path_data); /* returns list index */
bool menu_explore_is_content_list(void);
void menu_explore_context_init(void);
void menu_explore_context_deinit(void);
void menu_explore_free_state(explore_state_t *state);
void menu_explore_free(void);
void menu_explore_set_state(explore_state_t *state);
#endif

const char *menu_driver_ident(void);

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data);

void menu_driver_frame(bool menu_is_alive, video_frame_info_t *video_info);

int menu_driver_deferred_push_content_list(file_list_t *list);

bool menu_driver_init(bool video_is_threaded);

retro_time_t menu_driver_get_current_time(void);

size_t menu_display_timedate(gfx_display_ctx_datetime_t *datetime);

void menu_display_powerstate(gfx_display_ctx_powerstate_t *powerstate);

void menu_display_handle_wallpaper_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

uintptr_t menu_contentless_cores_get_entry_icon(const char *core_id);
void menu_contentless_cores_context_init(void);
void menu_contentless_cores_context_deinit(void);
void menu_contentless_cores_free(void);
void menu_contentless_cores_set_runtime(const char *core_id,
      const contentless_core_runtime_info_t *runtime_info);
void menu_contentless_cores_get_info(const char *core_id,
      const contentless_core_info_entry_t **info);
void menu_contentless_cores_flush_runtime(void);

/* Returns true if search filter is enabled
 * for the specified menu list */
bool menu_driver_search_filter_enabled(const char *label, unsigned type);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
void menu_driver_set_last_shader_preset_path(const char *path);
void menu_driver_set_last_shader_pass_path(const char *path);
enum rarch_shader_type menu_driver_get_last_shader_preset_type(void);
void menu_driver_get_last_shader_preset_path(
      const char **directory, const char **file_name);
void menu_driver_get_last_shader_pass_path(
      const char **directory, const char **file_name);
#endif

void menu_driver_set_pending_selection(const char *pending_selection);

struct menu_state *menu_state_get_ptr(void);

int generic_menu_entry_action(void *userdata, menu_entry_t *entry, size_t i, enum menu_action action);

void menu_entries_build_scroll_indices(
      struct menu_state *menu_st,
      file_list_t *list);

void get_current_menu_value(struct menu_state *menu_st,
      char *s, size_t len);

/* Teardown function for the menu driver. */
void menu_driver_destroy(
      struct menu_state *menu_st);

const menu_ctx_driver_t *menu_driver_find_driver(
      settings_t *settings,
      const char *prefix,
      bool verbosity_enabled);

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

void menu_display_common_image_upload(void *data,
      void *user_data, unsigned type);

size_t menu_update_fullscreen_thumbnail_label(
      char *s, size_t len,
      bool is_quick_menu, const char *title);

bool menu_is_running_quick_menu(void);
bool menu_is_nonrunning_quick_menu(void);

bool menu_input_key_bind_set_mode(
      enum menu_input_binds_ctl_state state, void *data);

void menu_driver_set_thumbnail_system(void *data, char *s, size_t len);

size_t menu_driver_get_thumbnail_system(void *data, char *s, size_t len);

#ifdef HAVE_RUNAHEAD
void menu_update_runahead_mode(void);
#endif

extern const menu_ctx_driver_t *menu_ctx_drivers[];

extern menu_ctx_driver_t menu_ctx_ozone;
extern menu_ctx_driver_t menu_ctx_rgui;
extern menu_ctx_driver_t menu_ctx_mui;
extern menu_ctx_driver_t menu_ctx_xmb;

RETRO_END_DECLS

#endif
