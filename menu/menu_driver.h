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
#include <gfx/math/matrix_4x4.h>
#include <formats/image.h>
#include <queues/task_queue.h>

#include "menu_defines.h"
#include "menu_input.h"
#include "menu_entries.h"

#include "../retroarch.h"
#include "../file_path_special.h"
#include "../gfx/font_driver.h"
#include "../gfx/video_coord_array.h"

RETRO_BEGIN_DECLS

#ifndef MAX_COUNTERS
#define MAX_COUNTERS 64
#endif

#ifndef MAX_CHEAT_COUNTERS
#define MAX_CHEAT_COUNTERS 6000
#endif

#define MENU_SETTINGS_CORE_INFO_NONE             0xffff
#define MENU_SETTINGS_CORE_OPTION_NONE           0xffff
#define MENU_SETTINGS_CHEEVOS_NONE               0xffff
#define MENU_SETTINGS_CORE_OPTION_CREATE         0x05000
#define MENU_SETTINGS_CORE_OPTION_START          0x10000
#define MENU_SETTINGS_CHEEVOS_START              0x40000
#define MENU_SETTINGS_NETPLAY_ROOMS_START        0x80000

#define COLOR_TEXT_ALPHA(color, alpha) (color & 0xFFFFFF00) | alpha

#define HEX_R(hex) ((hex >> 16) & 0xFF) * (1.0f / 255.0f)
#define HEX_G(hex) ((hex >> 8 ) & 0xFF) * (1.0f / 255.0f)
#define HEX_B(hex) ((hex >> 0 ) & 0xFF) * (1.0f / 255.0f)

#define COLOR_HEX_TO_FLOAT(hex, alpha) { \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha  \
}

extern float osk_dark[16];

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
   MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_CORE_NAME,
   MENU_SETTING_DROPDOWN_ITEM_DISK_INDEX,
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
   MENU_SETTING_ACTION_CORE_DELETE,
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
   MENU_WIFI,
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

#ifdef HAVE_LAKKA_SWITCH
   MENU_SET_SWITCH_GPU_PROFILE,
   MENU_SET_SWITCH_BRIGHTNESS,
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
   MENU_SET_SWITCH_CPU_PROFILE,
#endif

   MENU_SET_CDROM_LIST,
   MENU_SET_LOAD_CDROM_LIST,
   MENU_SET_CDROM_INFO,
   MENU_SETTING_ACTION_DELETE_PLAYLIST,
   MENU_SETTING_ACTION_PLAYLIST_MANAGER_RESET_CORES,
   MENU_SETTING_ACTION_PLAYLIST_MANAGER_CLEAN_PLAYLIST,

   MENU_SETTING_MANUAL_CONTENT_SCAN_DIR,
   MENU_SETTING_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   MENU_SETTING_MANUAL_CONTENT_SCAN_CORE_NAME,
   MENU_SETTING_ACTION_MANUAL_CONTENT_SCAN_START,

   MENU_SETTINGS_LAST
};

typedef struct menu_display_ctx_driver
{
   /* Draw graphics to the screen. */
   void (*draw)(menu_display_ctx_draw_t *draw, video_frame_info_t *video_info);
   /* Draw one of the menu pipeline shaders. */
   void (*draw_pipeline)(menu_display_ctx_draw_t *draw,
         video_frame_info_t *video_info);
   void (*viewport)(menu_display_ctx_draw_t *draw,
         video_frame_info_t *video_info);
   /* Start blending operation. */
   void (*blend_begin)(video_frame_info_t *video_info);
   /* Finish blending operation. */
   void (*blend_end)(video_frame_info_t *video_info);
   /* Set the clear color back to its default values. */
   void (*restore_clear_color)(void);
   /* Set the color to be used when clearing the screen */
   void (*clear_color)(menu_display_ctx_clearcolor_t *clearcolor,
         video_frame_info_t *video_info);
   /* Get the default Model-View-Projection matrix */
   void *(*get_default_mvp)(video_frame_info_t *video_info);
   /* Get the default vertices matrix */
   const float *(*get_default_vertices)(void);
   /* Get the default texture coordinates matrix */
   const float *(*get_default_tex_coords)(void);
   /* Initialize the first compatible font driver for this menu driver. */
   bool (*font_init_first)(
         void **font_handle, void *video_data,
         const char *font_path, float font_size,
         bool is_threaded);
   enum menu_display_driver_type type;
   const char *ident;
   bool handles_transform;
   /* Enables and disables scissoring */
   void (*scissor_begin)(video_frame_info_t *video_info, int x, int y, unsigned width, unsigned height);
   void (*scissor_end)(video_frame_info_t *video_info);
} menu_display_ctx_driver_t;

typedef struct menu_ctx_driver
{
   /* Set a framebuffer texture. This is used for instance by RGUI. */
   void  (*set_texture)(void);
   /* Render a messagebox to the screen. */
   void  (*render_messagebox)(void *data, const char *msg);
   int   (*iterate)(void *data, void *userdata, enum menu_action action);
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
   bool (*get_load_content_animation_data)(void *userdata, menu_texture_item *icon, char **playlist_name);
   /* This will be invoked whenever a menu entry action
    * (menu_entry_action()) is performed */
   int (*entry_action)(void *userdata, menu_entry_t *entry, size_t i, enum menu_action action);
} menu_ctx_driver_t;


typedef struct
{
   unsigned rpl_entry_selection_ptr;
   size_t                     core_len;
   uint64_t state;

   char *core_buf;
   char menu_state_msg[1024];
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

   /* This is used for storing intermediary variables
    * that get used later on during menu actions -
    * for instance, selecting a shader pass for a shader
    * slot */
   struct
   {
      unsigned                unsigned_var;
   } scratchpad;
   const menu_ctx_driver_t *driver_ctx;
   void *userdata;
} menu_handle_t;

struct menu_display_ctx_draw
{
   float x;
   float y;
   float *color;
   const float *vertex;
   const float *tex_coord;
   unsigned width;
   unsigned height;
   uintptr_t texture;
   size_t vertex_count;
   struct video_coords *coords;
   void *matrix_data;
   enum menu_display_prim_type prim_type;
   struct
   {
      unsigned id;
      const void *backend_data;
      size_t backend_data_size;
      bool active;
   } pipeline;
   float rotation;
   float scale_factor;
};

typedef struct menu_display_ctx_rotate_draw
{
   bool scale_enable;
   float rotation;
   float scale_x;
   float scale_y;
   float scale_z;
   math_matrix_4x4 *matrix;
} menu_display_ctx_rotate_draw_t;

typedef struct menu_display_ctx_coord_draw
{
   const float *ptr;
} menu_display_ctx_coord_draw_t;

typedef struct menu_display_ctx_datetime
{
   char *s;
   size_t len;
   unsigned time_mode;
} menu_display_ctx_datetime_t;

typedef struct menu_display_ctx_powerstate
{
   char *s;
   size_t len;
   unsigned percent;
   bool battery_enabled;
   bool charging;
} menu_display_ctx_powerstate_t;

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
   unsigned action;
   enum menu_input_pointer_gesture gesture;
   int retcode;
   menu_file_list_cbs_t *cbs;
   menu_entry_t *entry;
} menu_ctx_pointer_t;

typedef struct menu_ctx_bind
{
   const char *path;
   const char *label;
   unsigned type;
   uint32_t label_hash;
   size_t idx;
   int retcode;
   menu_file_list_cbs_t *cbs;
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

const char *menu_driver_ident(void);

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data);

void menu_driver_frame(video_frame_info_t *video_info);

bool menu_driver_get_load_content_animation_data(menu_texture_item *icon, char **playlist_name);

bool menu_driver_iterate(menu_ctx_iterate_t *iterate);

bool menu_driver_list_clear(file_list_t *list);

bool menu_driver_list_cache(menu_ctx_list_t *list);

void menu_driver_navigation_set(bool scroll);

void menu_driver_populate_entries(menu_displaylist_info_t *info);

bool menu_driver_push_list(menu_ctx_displaylist_t *disp_list);

bool menu_driver_init(bool video_is_threaded);

void menu_driver_set_thumbnail_system(char *s, size_t len);

void menu_driver_get_thumbnail_system(char *s, size_t len);

void menu_driver_set_thumbnail_content(char *s, size_t len);

bool menu_driver_list_insert(menu_ctx_list_t *list);

bool menu_driver_list_set_selection(file_list_t *list);

bool menu_driver_list_get_selection(menu_ctx_list_t *list);

bool menu_driver_list_get_entry(menu_ctx_list_t *list);

bool menu_driver_list_get_size(menu_ctx_list_t *list);

size_t menu_navigation_get_selection(void);

void menu_navigation_set_selection(size_t val);

void menu_display_blend_begin(video_frame_info_t *video_info);
void menu_display_blend_end(video_frame_info_t *video_info);

void menu_display_scissor_begin(video_frame_info_t *video_info, int x, int y, unsigned width, unsigned height);
void menu_display_scissor_end(video_frame_info_t *video_info);

void menu_display_font_free(font_data_t *font);

void menu_display_coords_array_reset(void);
video_coord_array_t *menu_display_get_coords_array(void);

void menu_display_set_width(unsigned width);
void menu_display_get_fb_size(unsigned *fb_width, unsigned *fb_height,
      size_t *fb_pitch);
void menu_display_set_height(unsigned height);
void menu_display_set_header_height(unsigned height);
unsigned menu_display_get_header_height(void);
size_t menu_display_get_framebuffer_pitch(void);
void menu_display_set_framebuffer_pitch(size_t pitch);

bool menu_display_get_msg_force(void);
void menu_display_set_msg_force(bool state);
bool menu_display_get_update_pending(void);
void menu_display_set_viewport(unsigned width, unsigned height);
void menu_display_unset_viewport(unsigned width, unsigned height);
bool menu_display_get_framebuffer_dirty_flag(void);
void menu_display_set_framebuffer_dirty_flag(void);
void menu_display_unset_framebuffer_dirty_flag(void);
float menu_display_get_pixel_scale(unsigned width, unsigned height);
float menu_display_get_dpi_scale(unsigned width, unsigned height);
bool menu_display_init_first_driver(bool video_is_threaded);
bool menu_display_restore_clear_color(void);
void menu_display_clear_color(menu_display_ctx_clearcolor_t *color,
      video_frame_info_t *video_info);
void menu_display_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info);
void menu_display_draw_blend(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info);
void menu_display_draw_keyboard(
      uintptr_t hover_texture,
      const font_data_t *font,
      video_frame_info_t *video_info,
      char *grid[], unsigned id,
      unsigned text_color);

void menu_display_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info);
void menu_display_draw_bg(
      menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info,
      bool add_opacity, float opacity_override);
void menu_display_draw_gradient(
      menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info);
void menu_display_draw_quad(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color);
void menu_display_draw_polygon(
      video_frame_info_t *video_info,
      int x1, int y1,
      int x2, int y2,
      int x3, int y3,
      int x4, int y4,
      unsigned width, unsigned height,
      float *color);
void menu_display_draw_texture(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color, uintptr_t texture);
void menu_display_draw_texture_slice(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned new_w, unsigned new_h, unsigned width, unsigned height,
      float *color, unsigned offset, float scale_factor, uintptr_t texture);

void menu_display_rotate_z(menu_display_ctx_rotate_draw_t *draw,
      video_frame_info_t *video_info);

void menu_display_timedate(menu_display_ctx_datetime_t *datetime);
void menu_display_powerstate(menu_display_ctx_powerstate_t *powerstate);

void menu_display_handle_wallpaper_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

void menu_display_handle_thumbnail_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

void menu_display_handle_left_thumbnail_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

void menu_display_handle_savestate_thumbnail_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

void menu_display_push_quad(
      unsigned width, unsigned height,
      const float *colors, int x1, int y1,
      int x2, int y2);

void menu_display_snow(int width, int height);

void menu_display_allocate_white_texture(void);

void menu_display_draw_cursor(
      video_frame_info_t *video_info,
      float *color, float cursor_size, uintptr_t texture,
      float x, float y, unsigned width, unsigned height);

void menu_display_draw_text(
      const font_data_t *font, const char *text,
      float x, float y, int width, int height,
      uint32_t color, enum text_alignment text_align,
      float scale_factor, bool shadows_enable, float shadow_offset,
      bool draw_outside);

#define menu_display_set_alpha(color, alpha_value) (color[3] = color[7] = color[11] = color[15] = (alpha_value))

font_data_t *menu_display_font(
      enum application_special_type type,
      float font_size,
      bool video_is_threaded);

font_data_t *menu_display_font_file(char* fontpath, float font_size, bool is_threaded);

bool menu_display_reset_textures_list(
      const char *texture_path, const char *iconpath,
      uintptr_t *item, enum texture_filter_type filter_type,
      unsigned *width, unsigned *height);

bool menu_display_reset_textures_list_buffer(
        uintptr_t *item, enum texture_filter_type filter_type,
        void* buffer, unsigned buffer_len, enum image_type_enum image_type,
        unsigned *width, unsigned *height);

/* Returns the OSK key at a given position */
int menu_display_osk_ptr_at_pos(void *data, int x, int y,
      unsigned width, unsigned height);

bool menu_display_driver_exists(const char *s);

void menu_driver_destroy(void);

void hex32_to_rgba_normalized(uint32_t hex, float* rgba, float alpha);

menu_handle_t *menu_driver_get_ptr(void);

extern uintptr_t menu_display_white_texture;

extern menu_display_ctx_driver_t menu_display_ctx_gl;
extern menu_display_ctx_driver_t menu_display_ctx_gl_core;
extern menu_display_ctx_driver_t menu_display_ctx_gl1;
extern menu_display_ctx_driver_t menu_display_ctx_vulkan;
extern menu_display_ctx_driver_t menu_display_ctx_metal;
extern menu_display_ctx_driver_t menu_display_ctx_d3d8;
extern menu_display_ctx_driver_t menu_display_ctx_d3d9;
extern menu_display_ctx_driver_t menu_display_ctx_d3d10;
extern menu_display_ctx_driver_t menu_display_ctx_d3d11;
extern menu_display_ctx_driver_t menu_display_ctx_d3d12;
extern menu_display_ctx_driver_t menu_display_ctx_vita2d;
extern menu_display_ctx_driver_t menu_display_ctx_ctr;
extern menu_display_ctx_driver_t menu_display_ctx_wiiu;
extern menu_display_ctx_driver_t menu_display_ctx_caca;
extern menu_display_ctx_driver_t menu_display_ctx_gdi;
extern menu_display_ctx_driver_t menu_display_ctx_vga;
extern menu_display_ctx_driver_t menu_display_ctx_fpga;
extern menu_display_ctx_driver_t menu_display_ctx_switch;
extern menu_display_ctx_driver_t menu_display_ctx_sixel;

extern menu_ctx_driver_t menu_ctx_ozone;
extern menu_ctx_driver_t menu_ctx_xui;
extern menu_ctx_driver_t menu_ctx_rgui;
extern menu_ctx_driver_t menu_ctx_mui;
extern menu_ctx_driver_t menu_ctx_xmb;
extern menu_ctx_driver_t menu_ctx_stripes;

RETRO_END_DECLS

#endif
