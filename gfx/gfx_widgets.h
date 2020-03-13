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

#include <formats/image.h>
#include <queues/task_queue.h>
#include <queues/message_queue.h>

#include "gfx_animation.h"

#define DEFAULT_BACKDROP               0.75f

#define MSG_QUEUE_PENDING_MAX          32
#define MSG_QUEUE_ONSCREEN_MAX         4

#define MSG_QUEUE_ANIMATION_DURATION      330
#define CHEEVO_NOTIFICATION_DURATION      4000
#define TASK_FINISHED_DURATION            3000
#define HOURGLASS_INTERVAL                5000
#define HOURGLASS_DURATION                1000
#define GENERIC_MESSAGE_DURATION          3000

/* TODO: Colors for warning, error and success */

#define TEXT_COLOR_INFO 0xD8EEFFFF
#if 0
#define TEXT_COLOR_SUCCESS 0x22B14CFF
#define TEXT_COLOR_ERROR 0xC23B22FF
#endif
#define TEXT_COLOR_FAINT 0x878787FF

/* A widget */
/* TODO: cleanup all unused parameters */
struct gfx_widget
{
   /* called when the widgets system is initialized
    * -> initialize the widget here */
   bool (*init)(bool video_is_threaded, bool fullscreen);

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
   void (*layout)(bool is_threaded, const char *dir_assets, char *font_path);

   /* called every frame on the main thread
    * -> update the widget logic here */
   void (*iterate)(
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded);

   /* called every frame
    * (on the video thread if threaded video is on)
    * -> draw the widget here */
   void (*frame)(void* data);
};

gfx_animation_ctx_tag gfx_widgets_get_generic_tag(void);
float* gfx_widgets_get_pure_white(void);
unsigned gfx_widgets_get_padding(void);
unsigned gfx_widgets_get_height(void);
unsigned gfx_widgets_get_glyph_width(void);
float gfx_widgets_get_font_size(void);
font_data_t* gfx_widgets_get_font_regular(void);
font_data_t* gfx_widgets_get_font_bold(void);
float* gfx_widgets_get_backdrop_orig(void);
unsigned gfx_widgets_get_last_video_width(void);
unsigned gfx_widgets_get_last_video_height(void);
unsigned gfx_widgets_get_generic_message_height(void);

float gfx_widgets_get_thumbnail_scale_factor(
      const float dst_width, const float dst_height,
      const float image_width, const float image_height);

void gfx_widgets_draw_icon(
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color);

typedef struct gfx_widget gfx_widget_t;

extern const gfx_widget_t gfx_widget_screenshot;
extern const gfx_widget_t gfx_widget_volume;
extern const gfx_widget_t gfx_widget_generic_message;

bool gfx_widgets_active(void);
void gfx_widgets_set_persistence(bool persist);

bool gfx_widgets_init(
      bool video_is_threaded,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path);
void gfx_widgets_deinit(void);

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
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded);

void gfx_widget_screenshot_taken(const char *shotname, const char *filename);

/* AI Service functions */
#ifdef HAVE_TRANSLATE
int gfx_widgets_ai_service_overlay_get_state(void);
bool gfx_widgets_ai_service_overlay_set_state(int state);

bool gfx_widgets_ai_service_overlay_load(
        char* buffer, unsigned buffer_len,
        enum image_type_enum image_type);

void gfx_widgets_ai_service_overlay_unload(void);
#endif

void gfx_widgets_start_load_content_animation(
      const char *content_name, bool remove_extension);

void gfx_widgets_cleanup_load_content_animation(void);

void gfx_widgets_push_achievement(const char *title, const char *badge);

/* Warning: not thread safe! */
void gfx_widget_set_message(char *message);

/* Warning: not thread safe! */
void gfx_widgets_set_libretro_message(const char *message, unsigned duration);

/* All the functions below should be called in
 * the video driver - once they are all added, set
 * enable_menu_widgets to true for that driver */
void gfx_widgets_frame(void *data);

bool gfx_widgets_set_fps_text(const char *new_fps_text);

#endif
