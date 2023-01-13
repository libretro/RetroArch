/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - natinusala
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

#ifndef _GFX_WIDGETS_H
#define _GFX_WIDGETS_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <retro_common_api.h>
#include <formats/image.h>
#include <queues/task_queue.h>
#include <queues/message_queue.h>
#include <queues/fifo_queue.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "gfx_animation.h"
#include "gfx_display.h"

#define DEFAULT_BACKDROP               0.75f

#define MSG_QUEUE_PENDING_MAX          32
#define MSG_QUEUE_ONSCREEN_MAX         4

#define MSG_QUEUE_ANIMATION_DURATION      330
#define TASK_FINISHED_DURATION            3000
#define HOURGLASS_INTERVAL                5000
#define HOURGLASS_DURATION                1000

/* TODO: Colors for warning, error and success */

#define TEXT_COLOR_INFO 0xD8EEFFFF
#if 0
#define TEXT_COLOR_SUCCESS 0x22B14CFF
#define TEXT_COLOR_ERROR 0xC23B22FF
#endif
#define TEXT_COLOR_FAINT 0x878787FF

RETRO_BEGIN_DECLS

enum gfx_widgets_icon
{
   MENU_WIDGETS_ICON_PAUSED = 0,
   MENU_WIDGETS_ICON_FAST_FORWARD,
   MENU_WIDGETS_ICON_REWIND,
   MENU_WIDGETS_ICON_SLOW_MOTION,

   MENU_WIDGETS_ICON_HOURGLASS,
   MENU_WIDGETS_ICON_CHECK,

   MENU_WIDGETS_ICON_INFO,

   MENU_WIDGETS_ICON_ACHIEVEMENT,

   MENU_WIDGETS_ICON_LAST
};

enum notification_show_screenshot_duration
{
   NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL = 0,
   NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   NOTIFICATION_SHOW_SCREENSHOT_DURATION_LAST
};

enum notification_show_screenshot_flash
{
   NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL = 0,
   NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   NOTIFICATION_SHOW_SCREENSHOT_FLASH_OFF,
   NOTIFICATION_SHOW_SCREENSHOT_FLASH_LAST
};

enum cheevos_appearance_anchor
{
   CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT = 0,
   CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   CHEEVOS_APPEARANCE_ANCHOR_LAST
};

/* This structure holds all objects + metadata
 * corresponding to a particular font */
typedef struct
{
   font_data_t *font;
   video_font_raster_block_t raster_block; /* ptr alignment */
   size_t usage_count;
   unsigned glyph_width;
   float line_height;
   float line_ascender;
   float line_descender;
   float line_centre_offset;
} gfx_widget_font_data_t;

/* Font data */
typedef struct
{
   gfx_widget_font_data_t regular;
   gfx_widget_font_data_t bold;
   gfx_widget_font_data_t msg_queue;
} gfx_widget_fonts_t;

enum disp_widget_flags_enum
{
   DISPWIDG_FLAG_TASK_FINISHED             = (1 << 0),
   DISPWIDG_FLAG_TASK_ERROR                = (1 << 1),
   DISPWIDG_FLAG_TASK_CANCELLED            = (1 << 2),
   DISPWIDG_FLAG_EXPIRATION_TIMER_STARTED  = (1 << 3),
   /* Is it currently doing the fade out animation ? */
   DISPWIDG_FLAG_DYING                     = (1 << 4),
   /* Has the timer expired ? if so, should be set to dying */
   DISPWIDG_FLAG_EXPIRED                   = (1 << 5),
   /* Unfold animation */
   DISPWIDG_FLAG_UNFOLDED                  = (1 << 6),
   DISPWIDG_FLAG_UNFOLDING                 = (1 << 7)
};

typedef struct disp_widget_msg
{
   char *msg;
   char *msg_new;
   retro_task_t *task_ptr;

   uint32_t task_ident;
   size_t   msg_len;
   unsigned duration;
   unsigned text_height;
   unsigned width;

   float msg_transition_animation;
   float offset_y;
   float alpha;
   float unfold;
   float hourglass_rotation;
   float hourglass_timer; /* float alignment */
   float expiration_timer; /* float alignment */

   uint16_t flags;
   int8_t task_progress;
   /* How many tasks have used this notification? */
   uint8_t task_count;
} disp_widget_msg_t;

/* There can only be one message animation at a time to 
 * avoid confusing users */
enum dispgfx_widget_flags
{
   DISPGFX_WIDGET_FLAG_MSG_QUEUE_HAS_ICONS = (1 << 0),
   DISPGFX_WIDGET_FLAG_PERSISTING          = (1 << 1),
   DISPGFX_WIDGET_FLAG_MOVING              = (1 << 2),
   DISPGFX_WIDGET_FLAG_INITED              = (1 << 3)
};

typedef struct dispgfx_widget
{
   uint64_t gfx_widgets_frame_count;

#ifdef HAVE_THREADS
   slock_t* current_msgs_lock;
#endif
   fifo_buffer_t msg_queue;
   disp_widget_msg_t* current_msgs[MSG_QUEUE_ONSCREEN_MAX];
   gfx_widget_fonts_t gfx_widget_fonts; /* ptr alignment */

#ifdef HAVE_TRANSLATE
   uintptr_t ai_service_overlay_texture;
#endif
   uintptr_t msg_queue_icon;
   uintptr_t msg_queue_icon_outline;
   uintptr_t msg_queue_icon_rect;
   uintptr_t gfx_widgets_icons_textures[
   MENU_WIDGETS_ICON_LAST];
   uintptr_t gfx_widgets_generic_tag;

   size_t current_msgs_size;

#ifdef HAVE_TRANSLATE
   int ai_service_overlay_state;
#endif

   unsigned last_video_width;
   unsigned last_video_height;
   unsigned msg_queue_kill;
   /* Count of messages bound to a task in current_msgs */
   unsigned msg_queue_tasks_count;

   unsigned simple_widget_padding;
   unsigned simple_widget_height;

   /* Used for both generic and libretro messages */
   unsigned generic_message_height;

   unsigned msg_queue_height;
   unsigned msg_queue_spacing;
   unsigned msg_queue_rect_start_x;
   unsigned msg_queue_internal_icon_size;
   unsigned msg_queue_internal_icon_offset;
   unsigned msg_queue_icon_size_x;
   unsigned msg_queue_icon_size_y;
   unsigned msg_queue_icon_offset_y;
   unsigned msg_queue_scissor_start_x;
   unsigned msg_queue_default_rect_width_menu_alive;
   unsigned msg_queue_default_rect_width;
   unsigned msg_queue_regular_padding_x;
   unsigned msg_queue_regular_text_start;
   unsigned msg_queue_task_text_start_x;
   unsigned msg_queue_task_rect_start_x;
   unsigned msg_queue_task_hourglass_x;
   unsigned divider_width_1px;

   float last_scale_factor;
   float backdrop_orig[16];
   float msg_queue_bg[16];
   float pure_white[16];
#ifdef HAVE_TRANSLATE
   unsigned ai_service_overlay_width;
   unsigned ai_service_overlay_height;
#endif

   uint8_t flags;

   char assets_pkg_dir[PATH_MAX_LENGTH];
   char xmb_path[PATH_MAX_LENGTH];                /* TODO/FIXME - decouple from XMB */
   char ozone_path[PATH_MAX_LENGTH];              /* TODO/FIXME - decouple from Ozone */
   char ozone_regular_font_path[PATH_MAX_LENGTH]; /* TODO/FIXME - decouple from Ozone */
   char ozone_bold_font_path[PATH_MAX_LENGTH];    /* TODO/FIXME - decouple from Ozone */

   char monochrome_png_path[PATH_MAX_LENGTH];
   char gfx_widgets_path[PATH_MAX_LENGTH];
   char gfx_widgets_status_text[255];

   bool active;
} dispgfx_widget_t;


/* A widget */
/* TODO/FIXME: cleanup all unused parameters */
struct gfx_widget
{
   /* called when the widgets system is initialized
    * -> initialize the widget here */
   bool (*init)(gfx_display_t *p_disp,
         gfx_animation_t *p_anim,
         bool video_is_threaded, bool fullscreen);

   /* called when the widgets system is freed
    * -> free the widget here */
   void (*free)(void);

   /* called when the graphics context is reset
    * -> (re)load the textures here */
   void (*context_reset)(bool is_threaded,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      char* menu_png_path,
      char* widgets_png_path);

   /* called when the graphics context is destroyed
    * -> release the textures here */
   void (*context_destroy)(void);

   /* called when the window resolution changes
    * -> (re)layout the widget here */
   void (*layout)(void *data,
         bool is_threaded, const char *dir_assets, char *font_path);

   /* called every frame on the main thread
    * -> update the widget logic here */
   void (*iterate)(void *user_data,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded);

   /* called every frame
    * (on the video thread if threaded video is on)
    * -- data is a video_frame_info_t
    * -- userdata is a dispgfx_widget_t
    * -> draw the widget here */
   void (*frame)(void* data, void *userdata);
};

float gfx_widgets_get_thumbnail_scale_factor(
      const float dst_width, const float dst_height,
      const float image_width, const float image_height);

void gfx_widgets_draw_icon(
      void *userdata,
      void *data_disp,
      unsigned video_width,
      unsigned video_height,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      float radians,
      float cosine,
      float sine,
      float *color);

void gfx_widgets_draw_text(
      gfx_widget_font_data_t* font_data,
      const char *text,
      float x, float y,
      int width, int height,
      uint32_t color,
      enum text_alignment text_align,
      bool draw_outside);

void gfx_widgets_flush_text(
      unsigned video_width, unsigned video_height,
      gfx_widget_font_data_t* font_data);

typedef struct gfx_widget gfx_widget_t;

bool gfx_widgets_init(
      void *data_disp,
      void *data_anim,
      void *settings_data,
      uintptr_t widgets_active_ptr,
      bool video_is_threaded,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path);

void gfx_widgets_deinit(bool widgets_persisting);

void gfx_widgets_msg_queue_push(
      retro_task_t *task, const char *msg,
      unsigned duration,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category,
      unsigned prio, bool flush,
      bool menu_is_alive);

void gfx_widget_volume_update_and_show(float new_volume,
      bool mute);

void gfx_widgets_iterate(
      void *data_disp,
      void *settings_data,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded);

void gfx_widget_screenshot_taken(void *data,
      const char *shotname, const char *filename);

/* AI Service functions */
#ifdef HAVE_TRANSLATE
bool gfx_widgets_ai_service_overlay_load(
      char* buffer, unsigned buffer_len,
      enum image_type_enum image_type);

void gfx_widgets_ai_service_overlay_unload(void);
#endif

#ifdef HAVE_CHEEVOS
void gfx_widgets_update_cheevos_appearance(void);
void gfx_widgets_push_achievement(const char *title, const char* subtitle, const char *badge);
void gfx_widgets_set_leaderboard_display(unsigned id, const char* value);
void gfx_widgets_set_challenge_display(unsigned id, const char* badge);
#endif

/* TODO/FIXME/WARNING: Not thread safe! */
void gfx_widget_set_generic_message(
      const char *message, unsigned duration);
void gfx_widget_set_libretro_message(
      const char *message, unsigned duration);
void gfx_widget_set_progress_message(
      const char *message, unsigned duration,
      unsigned priority, int8_t progress);
bool gfx_widget_start_load_content_animation(void);

/* All the functions below should be called in
 * the video driver - once they are all added, set
 * enable_menu_widgets to true for that driver */
void gfx_widgets_frame(void *data);

bool gfx_widgets_ready(void);

dispgfx_widget_t *dispwidget_get_ptr(void);

extern const gfx_widget_t gfx_widget_screenshot;
extern const gfx_widget_t gfx_widget_volume;
extern const gfx_widget_t gfx_widget_generic_message;
extern const gfx_widget_t gfx_widget_libretro_message;
extern const gfx_widget_t gfx_widget_progress_message;
extern const gfx_widget_t gfx_widget_load_content_animation;

#ifdef HAVE_NETWORKING
extern const gfx_widget_t gfx_widget_netplay_chat;
extern const gfx_widget_t gfx_widget_netplay_ping;
#endif

#ifdef HAVE_CHEEVOS
extern const gfx_widget_t gfx_widget_achievement_popup;
extern const gfx_widget_t gfx_widget_leaderboard_display;
#endif

RETRO_END_DECLS

#endif
