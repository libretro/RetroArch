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
#include <gfx/math/matrix_4x4.h>

#include "widgets/menu_entry.h"
#include "menu_input.h"
#include "menu_entries.h"

#include "../gfx/video_driver.h"
#include "../file_path_special.h"
#include "../gfx/font_driver.h"
#include "../gfx/video_coord_array.h"

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
   RARCH_MENU_CTL_SET_PENDING_QUICK_MENU,
   RARCH_MENU_CTL_SET_PENDING_QUIT,
   RARCH_MENU_CTL_SET_PENDING_SHUTDOWN,
   RARCH_MENU_CTL_DEINIT,
   RARCH_MENU_CTL_SET_PREVENT_POPULATE,
   RARCH_MENU_CTL_UNSET_PREVENT_POPULATE,
   RARCH_MENU_CTL_IS_PREVENT_POPULATE,
   RARCH_MENU_CTL_IS_TOGGLE,
   RARCH_MENU_CTL_SET_TOGGLE,
   RARCH_MENU_CTL_UNSET_TOGGLE,
   RARCH_MENU_CTL_SET_OWN_DRIVER,
   RARCH_MENU_CTL_UNSET_OWN_DRIVER,
   RARCH_MENU_CTL_OWNS_DRIVER,
   RARCH_MENU_CTL_PLAYLIST_FREE,
   RARCH_MENU_CTL_PLAYLIST_INIT,
   RARCH_MENU_CTL_PLAYLIST_GET,
   RARCH_MENU_CTL_FIND_DRIVER,
   RARCH_MENU_CTL_LIST_FREE,
   RARCH_MENU_CTL_LIST_SET_SELECTION,
   RARCH_MENU_CTL_LIST_GET_SELECTION,
   RARCH_MENU_CTL_LIST_GET_SIZE,
   RARCH_MENU_CTL_LIST_GET_ENTRY,
   RARCH_MENU_CTL_LIST_CACHE,
   RARCH_MENU_CTL_LIST_INSERT,
   RARCH_MENU_CTL_ENVIRONMENT,
   RARCH_MENU_CTL_DRIVER_DATA_GET,
   RARCH_MENU_CTL_POINTER_TAP,
   RARCH_MENU_CTL_POINTER_DOWN,
   RARCH_MENU_CTL_POINTER_UP,
   RARCH_MENU_CTL_OSK_PTR_AT_POS,
   RARCH_MENU_CTL_BIND_INIT,
   RARCH_MENU_CTL_UPDATE_THUMBNAIL_PATH,
   RARCH_MENU_CTL_UPDATE_THUMBNAIL_IMAGE,
   RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH,
   RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE,
   MENU_NAVIGATION_CTL_CLEAR,
   MENU_NAVIGATION_CTL_INCREMENT,
   MENU_NAVIGATION_CTL_DECREMENT,
   MENU_NAVIGATION_CTL_SET_LAST,
   MENU_NAVIGATION_CTL_DESCEND_ALPHABET,
   MENU_NAVIGATION_CTL_ASCEND_ALPHABET,
   MENU_NAVIGATION_CTL_CLEAR_SCROLL_INDICES,
   MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX,
   MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL,
   MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL
};

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
   MENU_WIFI,
   MENU_ROOM,
/*
   MENU_ROOM_LAN,
   MENU_ROOM_MITM,
*/
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

enum materialui_color_theme
{
   MATERIALUI_THEME_BLUE = 0,
   MATERIALUI_THEME_BLUE_GREY,
   MATERIALUI_THEME_DARK_BLUE,
   MATERIALUI_THEME_GREEN,
   MATERIALUI_THEME_RED,
   MATERIALUI_THEME_YELLOW,
   MATERIALUI_THEME_NVIDIA_SHIELD,
   MATERIALUI_THEME_LAST
};

enum xmb_color_theme
{
   XMB_THEME_LEGACY_RED  = 0,
   XMB_THEME_DARK_PURPLE,
   XMB_THEME_MIDNIGHT_BLUE,
   XMB_THEME_GOLDEN,
   XMB_THEME_ELECTRIC_BLUE,
   XMB_THEME_APPLE_GREEN,
   XMB_THEME_UNDERSEA,
   XMB_THEME_VOLCANIC_RED,
   XMB_THEME_DARK,
   XMB_THEME_WALLPAPER,
   XMB_THEME_LAST
};

enum xmb_icon_theme
{
   XMB_ICON_THEME_MONOCHROME = 0,
   XMB_ICON_THEME_FLATUI,
   XMB_ICON_THEME_RETROACTIVE,
   XMB_ICON_THEME_PIXEL,
   XMB_ICON_THEME_NEOACTIVE,
   XMB_ICON_THEME_SYSTEMATIC,
   XMB_ICON_THEME_DOTART,
   XMB_ICON_THEME_CUSTOM,
   XMB_ICON_THEME_LAST
};

enum xmb_shader_pipeline
{
   XMB_SHADER_PIPELINE_WALLPAPER = 0,
   XMB_SHADER_PIPELINE_SIMPLE_RIBBON,
   XMB_SHADER_PIPELINE_RIBBON,
   XMB_SHADER_PIPELINE_SIMPLE_SNOW,
   XMB_SHADER_PIPELINE_SNOW,
   XMB_SHADER_PIPELINE_BOKEH,
   XMB_SHADER_PIPELINE_LAST
};

enum menu_display_prim_type
{
   MENU_DISPLAY_PRIM_NONE = 0,
   MENU_DISPLAY_PRIM_TRIANGLESTRIP,
   MENU_DISPLAY_PRIM_TRIANGLES
};

enum menu_display_driver_type
{
   MENU_VIDEO_DRIVER_GENERIC = 0,
   MENU_VIDEO_DRIVER_OPENGL,
   MENU_VIDEO_DRIVER_VULKAN,
   MENU_VIDEO_DRIVER_DIRECT3D,
   MENU_VIDEO_DRIVER_VITA2D,
   MENU_VIDEO_DRIVER_CTR,
   MENU_VIDEO_DRIVER_WIIU,
   MENU_VIDEO_DRIVER_CACA,
   MENU_VIDEO_DRIVER_GDI,
   MENU_VIDEO_DRIVER_VGA
};

enum menu_toggle_reason
{
  MENU_TOGGLE_REASON_NONE = 0,
  MENU_TOGGLE_REASON_USER,
  MENU_TOGGLE_REASON_MESSAGE
};

typedef uintptr_t menu_texture_item;

typedef struct menu_display_ctx_clearcolor
{
   float r;
   float g;
   float b;
   float a;
} menu_display_ctx_clearcolor_t;

typedef struct menu_display_frame_info
{
   bool shadows_enable;
} menu_display_frame_info_t;

typedef struct menu_display_ctx_driver
{
   /* Draw graphics to the screen. */
   void (*draw)(void *data);
   /* Draw one of the menu pipeline shaders. */
   void (*draw_pipeline)(void *data);
   void (*viewport)(void *data);
   /* Start blending operation. */
   void (*blend_begin)(void);
   /* Finish blending operation. */
   void (*blend_end)(void);
   /* Set the clear color back to its default values. */
   void (*restore_clear_color)(void);
   /* Set the color to be used when clearing the screen */
   void (*clear_color)(menu_display_ctx_clearcolor_t *clearcolor);
   /* Get the default Model-View-Projection matrix */
   void *(*get_default_mvp)(void);
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
} menu_display_ctx_driver_t;


typedef struct
{
   /* Scratchpad variables. These are used for instance
    * by the filebrowser when having to store intermediary
    * paths (subdirs/previous dirs/current dir/path, etc).
    */
   char deferred_path[PATH_MAX_LENGTH];
   char scratch_buf[PATH_MAX_LENGTH];
   char scratch2_buf[PATH_MAX_LENGTH];

   uint64_t state;

   struct
   {
      char msg[1024];
   } menu_state;

   /* path to the currently loaded database playlist file. */
   char db_playlist_file[PATH_MAX_LENGTH];
} menu_handle_t;

typedef struct menu_display_ctx_draw
{
   float x;
   float y;
   unsigned width;
   unsigned height;
   struct video_coords *coords;
   void *matrix_data;
   uintptr_t texture;
   enum menu_display_prim_type prim_type;
   float *color;
   const float *vertex;
   const float *tex_coord;
   size_t vertex_count;
   struct
   {
      unsigned id;
      const void *backend_data;
      size_t backend_data_size;
      bool active;
   } pipeline;
} menu_display_ctx_draw_t;

typedef struct menu_display_ctx_rotate_draw
{
   math_matrix_4x4 *matrix;
   float rotation;
   float scale_x;
   float scale_y;
   float scale_z;
   bool scale_enable;
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

typedef struct menu_display_ctx_font
{
   const char *path;
   float size;
} menu_display_ctx_font_t;

typedef struct menu_ctx_driver
{
   /* Set a framebuffer texture. This is used for instance by RGUI. */
   void  (*set_texture)(void);
   /* Render a messagebox to the screen. */
   void  (*render_messagebox)(void *data, const char *msg);
   int   (*iterate)(void *data, void *userdata, enum menu_action action);
   void  (*render)(void *data, bool is_idle);
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
   int (*pointer_tap)(void *data, unsigned x, unsigned y, unsigned ptr,
         menu_file_list_cbs_t *cbs,
         menu_entry_t *entry, unsigned action);
   void (*update_thumbnail_path)(void *data, unsigned i);
   void (*update_thumbnail_image)(void *data);
   void (*set_thumbnail_system)(void *data, char* s, size_t len);
   void (*set_thumbnail_content)(void *data, char* s, size_t len);
   int  (*osk_ptr_at_pos)(void *data, int x, int y, unsigned width, unsigned height);
   void (*update_savestate_thumbnail_path)(void *data, unsigned i);
   void (*update_savestate_thumbnail_image)(void *data);
   int (*pointer_down)(void *data, unsigned x, unsigned y, unsigned ptr,
         menu_file_list_cbs_t *cbs,
         menu_entry_t *entry, unsigned action);
   int (*pointer_up)(void *data, unsigned x, unsigned y, unsigned ptr,
         menu_file_list_cbs_t *cbs,
         menu_entry_t *entry, unsigned action);
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

bool menu_driver_render(bool is_idle, bool is_inited, bool is_dummy);

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data);

bool menu_driver_is_binding_state(void);

void menu_driver_set_binding_state(bool on);

void menu_driver_frame(video_frame_info_t *video_info);

/* Is a background texture set for the current menu driver?  Should
 * return true for RGUI, for instance. */
bool menu_driver_is_texture_set(void);

bool menu_driver_is_alive(void);

bool menu_driver_iterate(menu_ctx_iterate_t *iterate);

bool menu_driver_list_clear(void *data);

void menu_driver_navigation_set(bool scroll);

void menu_driver_populate_entries(menu_displaylist_info_t *info);

bool menu_driver_load_image(menu_ctx_load_image_t *load_image_info);

bool menu_driver_push_list(menu_ctx_displaylist_t *disp_list);

bool menu_driver_init(bool video_is_threaded);

void menu_driver_set_thumbnail_system(char *s, size_t len);

void menu_driver_set_thumbnail_content(char *s, size_t len);

size_t menu_navigation_get_selection(void);

void menu_navigation_set_selection(size_t val);


enum menu_toggle_reason menu_display_toggle_get_reason(void);
void menu_display_toggle_set_reason(enum menu_toggle_reason reason);

void menu_display_blend_begin(void);
void menu_display_blend_end(void);

void menu_display_font_free(font_data_t *font);

void menu_display_coords_array_reset(void);
video_coord_array_t *menu_display_get_coords_array(void);
const uint8_t *menu_display_get_font_framebuffer(void);
void menu_display_set_font_framebuffer(const uint8_t *buffer);
bool menu_display_libretro(bool is_idle, bool is_inited, bool is_dummy);

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
bool menu_display_get_font_data_init(void);
void menu_display_set_font_data_init(bool state);
bool menu_display_get_update_pending(void);
void menu_display_set_viewport(unsigned width, unsigned height);
void menu_display_unset_viewport(unsigned width, unsigned height);
bool menu_display_get_framebuffer_dirty_flag(void);
void menu_display_set_framebuffer_dirty_flag(void);
void menu_display_unset_framebuffer_dirty_flag(void);
float menu_display_get_dpi(void);
bool menu_display_init_first_driver(bool video_is_threaded);
bool menu_display_restore_clear_color(void);
void menu_display_clear_color(menu_display_ctx_clearcolor_t *color);
void menu_display_draw(menu_display_ctx_draw_t *draw);

void menu_display_draw_pipeline(menu_display_ctx_draw_t *draw);
void menu_display_draw_bg(
      menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info,
      bool add_opacity, float opacity_override);
void menu_display_draw_gradient(
      menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info);
void menu_display_draw_quad(int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color);
void menu_display_draw_texture(int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color, uintptr_t texture);
void menu_display_draw_texture_slice(int x, int y, unsigned w, unsigned h,
      unsigned new_w, unsigned new_h, unsigned width, unsigned height,
      float *color, unsigned offset, float scale_factor, uintptr_t texture);
void menu_display_rotate_z(menu_display_ctx_rotate_draw_t *draw);
bool menu_display_get_tex_coords(menu_display_ctx_coord_draw_t *draw);

void menu_display_timedate(menu_display_ctx_datetime_t *datetime);

void menu_display_handle_wallpaper_upload(void *task_data,
      void *user_data, const char *err);

void menu_display_handle_thumbnail_upload(void *task_data,
      void *user_data, const char *err);

void menu_display_handle_savestate_thumbnail_upload(void *task_data,
      void *user_data, const char *err);

void menu_display_push_quad(
      unsigned width, unsigned height,
      const float *colors, int x1, int y1,
      int x2, int y2);

void menu_display_snow(int width, int height);

void menu_display_allocate_white_texture(void);

void menu_display_draw_cursor(
      float *color, float cursor_size, uintptr_t texture,
      float x, float y, unsigned width, unsigned height);

void menu_display_draw_text(
      const font_data_t *font, const char *text,
      float x, float y, int width, int height,
      uint32_t color, enum text_alignment text_align,
      float scale_factor, bool shadows_enable, float shadow_offset);

#define menu_display_set_alpha(color, alpha_value) (color[3] = color[7] = color[11] = color[15] = (alpha_value))

font_data_t *menu_display_font(enum application_special_type type, float font_size,
      bool video_is_threaded);

void menu_display_reset_textures_list(const char *texture_path, const char *iconpath,
      uintptr_t *item, enum texture_filter_type filter_type);

void menu_driver_destroy(void);

extern uintptr_t menu_display_white_texture;

extern menu_display_ctx_driver_t menu_display_ctx_gl;
extern menu_display_ctx_driver_t menu_display_ctx_vulkan;
extern menu_display_ctx_driver_t menu_display_ctx_d3d;
extern menu_display_ctx_driver_t menu_display_ctx_vita2d;
extern menu_display_ctx_driver_t menu_display_ctx_ctr;
extern menu_display_ctx_driver_t menu_display_ctx_wiiu;
extern menu_display_ctx_driver_t menu_display_ctx_caca;
extern menu_display_ctx_driver_t menu_display_ctx_gdi;
extern menu_display_ctx_driver_t menu_display_ctx_vga;
extern menu_display_ctx_driver_t menu_display_ctx_null;

extern menu_ctx_driver_t menu_ctx_xui;
extern menu_ctx_driver_t menu_ctx_rgui;
extern menu_ctx_driver_t menu_ctx_mui;
extern menu_ctx_driver_t menu_ctx_nuklear;
extern menu_ctx_driver_t menu_ctx_xmb;
extern menu_ctx_driver_t menu_ctx_zarch;
extern menu_ctx_driver_t menu_ctx_null;

RETRO_END_DECLS

#endif
