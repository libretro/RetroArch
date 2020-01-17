/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2015-2018 - Andre Leiradella
 *  Copyright (C) 2018-2019 - natinusala
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

#include <retro_miscellaneous.h>

#include <lists/file_list.h>
#include <queues/fifo_queue.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "menu_widgets.h"

#ifdef HAVE_ACCESSIBILITY
#include "../../accessibility.h"
#endif

#include "../../verbosity.h"
#include "../../retroarch.h"
#include "../../configuration.h"
#include "../../msg_hash.h"

#include "../../tasks/task_content.h"
#include "../../ui/ui_companion_driver.h"

#include "../menu_driver.h"
#include "../menu_animation.h"

#include "../../gfx/font_driver.h"

#ifndef PI
#define PI 3.14159265359f
#endif

/* TODO: Fix context reset freezing everything in place (probably kills animations when it shouldn't anymore) */

static float msg_queue_background[16]  = COLOR_HEX_TO_FLOAT(0x3A3A3A, 1.0f);
static float msg_queue_info[16]        = COLOR_HEX_TO_FLOAT(0x12ACF8, 1.0f);

/* TODO: Colors for warning, error and success */

static float msg_queue_task_progress_1[16] = COLOR_HEX_TO_FLOAT(0x397869, 1.0f); /* Color of first progress bar in a task message */
static float msg_queue_task_progress_2[16] = COLOR_HEX_TO_FLOAT(0x317198, 1.0f); /* Color of second progress bar in a task message (for multiple tasks with same message) */

static float color_task_progress_bar[16] = COLOR_HEX_TO_FLOAT(0x22B14C, 1.0f);

static unsigned text_color_info        = 0xD8EEFFFF;
static unsigned text_color_success     = 0x22B14CFF;
static unsigned text_color_error       = 0xC23B22FF;
static unsigned text_color_faint       = 0x878787FF;

static float volume_bar_background[16]    = COLOR_HEX_TO_FLOAT(0x1A1A1A, 1.0f);
static float volume_bar_normal[16]        = COLOR_HEX_TO_FLOAT(0x198AC6, 1.0f);
static float volume_bar_loud[16]          = COLOR_HEX_TO_FLOAT(0xF5DD19, 1.0f);
static float volume_bar_loudest[16]       = COLOR_HEX_TO_FLOAT(0xC23B22, 1.0f);

static uint64_t menu_widgets_frame_count     = 0;

/* Font data */
static font_data_t *font_regular;
static font_data_t *font_bold;

static video_font_raster_block_t font_raster_regular;
static video_font_raster_block_t font_raster_bold;

static float menu_widgets_pure_white[16] = {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
};

/* FPS */
static char menu_widgets_fps_text[255] = {0};

/* Achievement notification */
static char *cheevo_title              = NULL;
static menu_texture_item cheevo_badge  = 0;
static float cheevo_unfold             = 0.0f;

static menu_timer_t cheevo_timer;

static float cheevo_y           = 0.0f;
static unsigned cheevo_width    = 0;
static unsigned cheevo_height   = 0;

/* Load content animation */
#define ANIMATION_LOAD_CONTENT_DURATION            333

#define LOAD_CONTENT_ANIMATION_INITIAL_ICON_SIZE   320
#define LOAD_CONTENT_ANIMATION_TARGET_ICON_SIZE    240

static bool load_content_animation_running            = false;
static char *load_content_animation_content_name      = NULL;
static char *load_content_animation_playlist_name     = NULL;
static menu_texture_item load_content_animation_icon  = 0;

static float load_content_animation_icon_color[16];
static float load_content_animation_icon_size;
static float load_content_animation_icon_alpha;
static float load_content_animation_fade_alpha;
static float load_content_animation_final_fade_alpha;

static menu_timer_t load_content_animation_end_timer;

static float menu_widgets_backdrop_orig[16] = {
   0.00, 0.00, 0.00, 0.75,
   0.00, 0.00, 0.00, 0.75,
   0.00, 0.00, 0.00, 0.75,
   0.00, 0.00, 0.00, 0.75,
};

static float menu_widgets_backdrop[16] = {
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
};

/* Messages queue */

typedef struct menu_widget_msg
{
   char *msg;
   char *msg_new;
   float msg_transition_animation;
   unsigned msg_len;
   unsigned duration;

   unsigned text_height;

   float offset_y;

   float alpha;

   bool dying; /* is it currently doing the fade out animation ? */

   unsigned width;
   bool expired; /* has the timer expired ? if so, should be set to dying */

   menu_timer_t expiration_timer;
   bool expiration_timer_started;

   retro_task_t *task_ptr;
   char *task_title_ptr; /* used to detect title change */
   uint8_t task_count; /* how many tasks have used this notification? */

   int8_t task_progress;
   bool task_finished;
   bool task_error;
   bool task_cancelled;
   uint32_t task_ident;

   bool unfolded; /* unfold animation */
   bool unfolding;
   float unfold;

   float hourglass_rotation;
   menu_timer_t hourglass_timer;
} menu_widget_msg_t;

static fifo_buffer_t *msg_queue       = NULL;
static file_list_t *current_msgs      = NULL;
static unsigned msg_queue_kill        = 0;

static unsigned msg_queue_tasks_count = 0; /* count of messages bound to a taskin current_msgs */

/* TODO: Don't display icons if assets are missing */

static menu_texture_item msg_queue_icon          = 0;
static menu_texture_item msg_queue_icon_outline  = 0;
static menu_texture_item msg_queue_icon_rect     = 0;
static bool msg_queue_has_icons                  = false;

extern menu_animation_ctx_tag menu_widgets_generic_tag;

/* there can only be one message animation at a time to avoid confusing users */
static bool widgets_moving   = false;

/* Icons */
enum menu_widgets_icon
{
   MENU_WIDGETS_ICON_VOLUME_MED = 0,
   MENU_WIDGETS_ICON_VOLUME_MAX,
   MENU_WIDGETS_ICON_VOLUME_MIN,
   MENU_WIDGETS_ICON_VOLUME_MUTE,

   MENU_WIDGETS_ICON_PAUSED,
   MENU_WIDGETS_ICON_FAST_FORWARD,
   MENU_WIDGETS_ICON_REWIND,
   MENU_WIDGETS_ICON_SLOW_MOTION,

   MENU_WIDGETS_ICON_HOURGLASS,
   MENU_WIDGETS_ICON_CHECK,

   MENU_WIDGETS_ICON_INFO,

   MENU_WIDGETS_ICON_ACHIEVEMENT,

   MENU_WIDGETS_ICON_LAST
};

static const char *menu_widgets_icons_names[MENU_WIDGETS_ICON_LAST] = {
   "menu_volume_med.png",
   "menu_volume_max.png",
   "menu_volume_min.png",
   "menu_volume_mute.png",

   "menu_pause.png",
   "menu_frameskip.png",
   "menu_rewind.png",
   "resume.png",

   "menu_hourglass.png",
   "menu_check.png",

   "menu_info.png",

   "menu_achievements.png"
};

static menu_texture_item menu_widgets_icons_textures[MENU_WIDGETS_ICON_LAST] = {0};

/* Volume */
static float volume_db              = 0.0f;
static float volume_percent         = 1.0f;
static menu_timer_t volume_timer    = 0.0f;

static float volume_alpha                  = 0.0f;
static float volume_text_alpha             = 0.0f;
static menu_animation_ctx_tag volume_tag   = (uintptr_t) &volume_alpha;
static bool volume_mute                    = false;


/* Screenshot */
static float screenshot_alpha                = 0.0f;
static menu_texture_item screenshot_texture  = 0;
static unsigned screenshot_texture_width     = 0;
static unsigned screenshot_texture_height    = 0;
static char screenshot_shotname[256]         = {0};
static char screenshot_filename[256]         = {0};
static bool screenshot_loaded                = false;

static unsigned screenshot_height;
static unsigned screenshot_width;
static float screenshot_scale_factor;
static unsigned screenshot_thumbnail_width;
static unsigned screenshot_thumbnail_height;
static float screenshot_y;
static menu_timer_t screenshot_timer;

static unsigned screenshot_shotname_length;

/* AI Service Overlay */
static int ai_service_overlay_state                  = 0;
static unsigned ai_service_overlay_width             = 0;
static unsigned ai_service_overlay_height            = 0;
static menu_texture_item ai_service_overlay_texture  = 0;

/* Generic message */
static menu_timer_t generic_message_timer;
static float generic_message_alpha = 0.0f;
#define GENERIC_MESSAGE_SIZE 256
static char generic_message[GENERIC_MESSAGE_SIZE] = {'\0'};

/* Libretro message */
static menu_timer_t libretro_message_timer;
static float libretro_message_alpha = 0.0f;
static unsigned libretro_message_width = 0;
#define LIBRETRO_MESSAGE_SIZE 512
static char libretro_message[LIBRETRO_MESSAGE_SIZE] = {'\0'};

/* Metrics */
static unsigned simple_widget_padding = 0;
static unsigned simple_widget_height;
static unsigned glyph_width;
static unsigned line_height;

static unsigned msg_queue_height;
static unsigned msg_queue_icon_size_x;
static unsigned msg_queue_icon_size_y;
static float msg_queue_text_scale_factor;
static unsigned msg_queue_base_width;
static unsigned msg_queue_spacing;
static unsigned msg_queue_glyph_width;
static unsigned msg_queue_rect_start_x;
static unsigned msg_queue_internal_icon_size;
static unsigned msg_queue_internal_icon_offset;
static unsigned msg_queue_icon_offset_y;
static unsigned msg_queue_scissor_start_x;
static unsigned msg_queue_default_rect_width;
static unsigned msg_queue_task_text_start_x;
static unsigned msg_queue_regular_padding_x;
static unsigned msg_queue_regular_text_start;
static unsigned msg_queue_regular_text_base_y;
static unsigned msg_queue_task_rect_start_x;
static unsigned msg_queue_task_hourglass_x;

static unsigned generic_message_height; /* used for both generic and libretro messages */

static void msg_widget_msg_transition_animation_done(void *userdata)
{
   menu_widget_msg_t *msg = (menu_widget_msg_t*) userdata;

   if (msg->msg)
      free(msg->msg);
   msg->msg = NULL;

   if (msg->msg_new)
      msg->msg = strdup(msg->msg_new);

   msg->msg_transition_animation = 0.0f;
}

void menu_widgets_msg_queue_push(
      retro_task_t *task, const char *msg,
      unsigned duration,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category,
      unsigned prio, bool flush)
{
   menu_widget_msg_t* msg_widget = NULL;

#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled())
      accessibility_speak_priority((char*)msg, 0);
#endif
   if (fifo_write_avail(msg_queue) > 0)
   {
      /* Get current msg if it exists */
      if (task && task->frontend_userdata)
      {
         msg_widget           = (menu_widget_msg_t*) task->frontend_userdata;
         msg_widget->task_ptr = task; /* msg_widgets can be passed between tasks */
      }

      /* Spawn a new notification */
      if (!msg_widget)
      {
         const char *title                      = msg;

         msg_widget                             = (menu_widget_msg_t*)calloc(1, sizeof(*msg_widget));

         if (task)
            title                               = task->title;

         msg_widget->duration                   = duration;
         msg_widget->offset_y                   = 0;
         msg_widget->alpha                      = 1.0f;

         msg_widget->dying                      = false;
         msg_widget->expired                    = false;

         msg_widget->expiration_timer           = 0;
         msg_widget->task_ptr                   = task;
         msg_widget->expiration_timer_started   = false;

         msg_widget->msg_new                    = NULL;
         msg_widget->msg_transition_animation   = 0.0f;

         msg_widget->text_height                = 0;

         if (msg_queue_has_icons)
         {
            msg_widget->unfolded                = false;
            msg_widget->unfolding               = false;
            msg_widget->unfold                  = 0.0f;
         }
         else
         {
            msg_widget->unfolded                = true;
            msg_widget->unfolding               = false;
            msg_widget->unfold                  = 1.0f;
         }

         if (task)
         {
            msg_widget->msg                  = strdup(title);
            msg_widget->msg_new              = strdup(title);
            msg_widget->msg_len              = (unsigned)strlen(title);

            msg_widget->task_error           = task->error;
            msg_widget->task_cancelled       = task->cancelled;
            msg_widget->task_finished        = task->finished;
            msg_widget->task_progress        = task->progress;
            msg_widget->task_ident           = task->ident;
            msg_widget->task_title_ptr       = task->title;
            msg_widget->task_count           = 1;

            msg_widget->unfolded             = true;

            msg_widget->width                = font_driver_get_message_width(font_regular, title, msg_widget->msg_len, msg_queue_text_scale_factor) + simple_widget_padding/2;

            task->frontend_userdata          = msg_widget;

            msg_widget->hourglass_rotation   = 0;
         }
         else
         {
            /* Compute rect width, wrap if necessary */
            /* Single line text > two lines text > two lines text with expanded width */
            unsigned title_length   = (unsigned)strlen(title);
            char *msg               = strdup(title);
            unsigned width          = msg_queue_default_rect_width;
            unsigned text_width     = font_driver_get_message_width(font_regular, title, title_length, msg_queue_text_scale_factor);
            settings_t *settings    = config_get_ptr();

            msg_widget->text_height = msg_queue_text_scale_factor * settings->floats.video_font_size;

            /* Text is too wide, split it into two lines */
            if (text_width > width)
            {
               if (text_width/2 > width)
               {
                  width = text_width/2;
                  width += 10 * msg_queue_glyph_width;
               }

               word_wrap(msg, msg, title_length/2 + 10, false, 2);

               msg_widget->text_height *= 2.5f;
            }
            else
            {
               width                      = text_width;
               msg_widget->text_height    *= 1.35f;
            }

            msg_widget->msg         = msg;
            msg_widget->msg_len     = (unsigned)strlen(msg);
            msg_widget->width       = width + simple_widget_padding/2;
         }

         fifo_write(msg_queue, &msg_widget, sizeof(msg_widget));
      }
      /* Update task info */
      else
      {
         if (msg_widget->expiration_timer_started)
         {
            menu_timer_kill(&msg_widget->expiration_timer);
            msg_widget->expiration_timer_started = false;
         }

         if (!string_is_equal(task->title, msg_widget->msg_new))
         {
            unsigned len         = (unsigned)strlen(task->title);
            unsigned new_width   = font_driver_get_message_width(font_regular, task->title, len, msg_queue_text_scale_factor);

            if (msg_widget->msg_new)
            {
               free(msg_widget->msg_new);
               msg_widget->msg_new = NULL;
            }

            msg_widget->msg_new                    = strdup(task->title);
            msg_widget->msg_len                    = len;
            msg_widget->task_title_ptr             = task->title;
            msg_widget->msg_transition_animation   = 0;

            if (!task->alternative_look)
            {
               menu_animation_ctx_entry_t entry;

               entry.easing_enum    = EASING_OUT_QUAD;
               entry.tag            = (uintptr_t) msg_widget;
               entry.duration       = MSG_QUEUE_ANIMATION_DURATION*2;
               entry.target_value   = msg_queue_height/2.0f;
               entry.subject        = &msg_widget->msg_transition_animation;
               entry.cb             = msg_widget_msg_transition_animation_done;
               entry.userdata       = msg_widget;

               menu_animation_push(&entry);
            }
            else
            {
               msg_widget_msg_transition_animation_done(msg_widget);
            }

            msg_widget->task_count++;

            msg_widget->width = new_width;
         }

         msg_widget->task_error        = task->error;
         msg_widget->task_cancelled    = task->cancelled;
         msg_widget->task_finished     = task->finished;
         msg_widget->task_progress     = task->progress;
      }
   }
}

static void menu_widgets_unfold_end(void *userdata)
{
   menu_widget_msg_t *unfold = (menu_widget_msg_t*) userdata;

   unfold->unfolding = false;
   widgets_moving    = false;
}

static void menu_widgets_move_end(void *userdata)
{
   if (userdata)
   {
      menu_widget_msg_t *unfold = (menu_widget_msg_t*) userdata;

      menu_animation_ctx_entry_t entry;

      entry.cb             = menu_widgets_unfold_end;
      entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
      entry.easing_enum    = EASING_OUT_QUAD;
      entry.subject        = &unfold->unfold;
      entry.tag            = (uintptr_t) unfold;
      entry.target_value   = 1.0f;
      entry.userdata       = unfold;

      menu_animation_push(&entry);

      unfold->unfolded  = true;
      unfold->unfolding = true;
   }
   else
      widgets_moving = false;
}

static void menu_widgets_msg_queue_expired(void *userdata)
{
   menu_widget_msg_t *msg = (menu_widget_msg_t *) userdata;

   if (msg && !msg->expired)
      msg->expired = true;
}

static void menu_widgets_msg_queue_move(void)
{
   int i;
   float y = 0;

   menu_widget_msg_t *unfold  = NULL; /* there should always be one and only one unfolded message */

   if (current_msgs->size == 0)
      return;

   for (i = (int)(current_msgs->size-1); i >= 0; i--)
   {
      menu_widget_msg_t *msg = (menu_widget_msg_t*)
         file_list_get_userdata_at_offset(current_msgs, i);

      if (!msg || msg->dying)
         continue;

      y += msg_queue_height / (msg->task_ptr ? 2 : 1) + msg_queue_spacing;

      if (!msg->unfolded)
         unfold = msg;

      if (msg->offset_y != y)
      {
         menu_animation_ctx_entry_t entry;

         entry.cb             = i == 0 ? menu_widgets_move_end : NULL;
         entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
         entry.easing_enum    = EASING_OUT_QUAD;
         entry.subject        = &msg->offset_y;
         entry.tag            = (uintptr_t) msg;
         entry.target_value   = y;
         entry.userdata       = unfold;

         menu_animation_push(&entry);

         widgets_moving = true;
      }
   }
}

static void menu_widgets_msg_queue_free(menu_widget_msg_t *msg, bool touch_list)
{
   size_t i;
   menu_animation_ctx_tag tag = (uintptr_t) msg;

   if (msg->task_ptr)
   {
      /* remove the reference the task has of ourself
         only if the task is not finished already
         (finished tasks are freed before the widget) */
      if (!msg->task_finished && !msg->task_error && !msg->task_cancelled)
         msg->task_ptr->frontend_userdata = NULL;

      /* update tasks count */
      msg_queue_tasks_count--;
   }

   /* Kill all animations */
   menu_timer_kill(&msg->hourglass_timer);
   menu_animation_kill_by_tag(&tag);

   /* Kill all timers */
   if (msg->expiration_timer_started)
      menu_timer_kill(&msg->expiration_timer);

   /* Free it */
   if (msg->msg)
      free(msg->msg);

   if (msg->msg_new)
      free(msg->msg_new);

   /* Remove it from the list */
   if (touch_list)
   {
      file_list_free_userdata(current_msgs, msg_queue_kill);

      for (i = msg_queue_kill; i < current_msgs->size-1; i++)
         current_msgs->list[i] = current_msgs->list[i+1];

      current_msgs->size--;
   }

   widgets_moving = false;
}

static void menu_widgets_msg_queue_kill_end(void *userdata)
{
   menu_widget_msg_t *msg = (menu_widget_msg_t*)
      file_list_get_userdata_at_offset(current_msgs, msg_queue_kill);

   if (!msg)
      return;

   menu_widgets_msg_queue_free(msg, true);
}

static void menu_widgets_msg_queue_kill(unsigned idx)
{
   menu_animation_ctx_entry_t entry;

   menu_widget_msg_t *msg = (menu_widget_msg_t*)
      file_list_get_userdata_at_offset(current_msgs, idx);

   if (!msg)
      return;

   widgets_moving = true;
   msg->dying     = true;

   msg_queue_kill = idx;

   /* Drop down */
   entry.cb             = NULL;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.tag            = (uintptr_t) msg;
   entry.userdata       = NULL;
   entry.subject        = &msg->offset_y;
   entry.target_value   = msg->offset_y - msg_queue_height/4;

   menu_animation_push(&entry);

   /* Fade out */
   entry.cb             = menu_widgets_msg_queue_kill_end;
   entry.subject        = &msg->alpha;
   entry.target_value   = 0.0f;

   menu_animation_push(&entry);

   /* Move all messages back to their correct position */
   menu_widgets_msg_queue_move();
}

static void menu_widgets_draw_icon(
      video_frame_info_t *video_info,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color)
{
   menu_display_ctx_rotate_draw_t rotate_draw;
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;

   if (!texture)
      return;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   draw.x               = x;
   draw.y               = height - y - icon_height;
   draw.width           = icon_width;
   draw.height          = icon_height;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   menu_display_draw(&draw, video_info);
}


static void menu_widgets_draw_icon_blend(
      video_frame_info_t *video_info,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color)
{
   menu_display_ctx_rotate_draw_t rotate_draw;
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;

   if (!texture)
      return;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   draw.x               = x;
   draw.y               = height - y - icon_height;
   draw.width           = icon_width;
   draw.height          = icon_height;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   menu_display_draw_blend(&draw, video_info);
}

static float menu_widgets_get_thumbnail_scale_factor(const float dst_width, const float dst_height,
      const float image_width, const float image_height)
{
   float dst_ratio      = dst_width / dst_height;
   float image_ratio    = image_width / image_height;

   return (dst_ratio > image_ratio) ? dst_height / image_height : dst_width / image_width;
}

static void menu_widgets_screenshot_dispose(void *userdata)
{
   screenshot_loaded = false;
   video_driver_texture_unload(&screenshot_texture);
}

static void menu_widgets_screenshot_end(void *userdata)
{
   menu_animation_ctx_entry_t entry;

   entry.cb             = menu_widgets_screenshot_dispose;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &screenshot_y;
   entry.tag            = menu_widgets_generic_tag;
   entry.target_value   = -((float)screenshot_height);
   entry.userdata       = NULL;

   menu_animation_push(&entry);
}

static void menu_widgets_start_msg_expiration_timer(menu_widget_msg_t *msg_widget, unsigned duration)
{
   menu_timer_ctx_entry_t timer;
   if (msg_widget->expiration_timer_started)
      return;

   timer.cb       = menu_widgets_msg_queue_expired;
   timer.duration = duration;
   timer.userdata = msg_widget;

   menu_timer_start(&msg_widget->expiration_timer, &timer);

   msg_widget->expiration_timer_started = true;
}

static void menu_widgets_hourglass_tick(void *userdata);

static void menu_widgets_hourglass_end(void *userdata)
{
   menu_timer_ctx_entry_t timer;
   menu_widget_msg_t *msg  = (menu_widget_msg_t*) userdata;

   msg->hourglass_rotation = 0.0f;

   timer.cb                = menu_widgets_hourglass_tick;
   timer.duration          = HOURGLASS_INTERVAL;
   timer.userdata          = msg;

   menu_timer_start(&msg->hourglass_timer, &timer);
}

static void menu_widgets_hourglass_tick(void *userdata)
{
   menu_widget_msg_t *msg = (menu_widget_msg_t*) userdata;
   menu_animation_ctx_tag tag = (uintptr_t) msg;

   menu_animation_ctx_entry_t entry;

   entry.easing_enum    = EASING_OUT_QUAD;
   entry.tag            = tag;
   entry.duration       = HOURGLASS_DURATION;
   entry.target_value   = -(2 * PI);
   entry.subject        = &msg->hourglass_rotation;
   entry.cb             = menu_widgets_hourglass_end;
   entry.userdata       = msg;

   menu_animation_push(&entry);
}

void menu_widgets_iterate(unsigned width, unsigned height)
{
   size_t i;

   /* Messages queue */

   /* Consume one message if available */
   if (fifo_read_avail(msg_queue) > 0 && !widgets_moving && current_msgs->size < MSG_QUEUE_ONSCREEN_MAX)
   {
      menu_widget_msg_t *msg_widget;

      fifo_read(msg_queue, &msg_widget, sizeof(msg_widget));

      /* Task messages always appear from the bottom of the screen */
      if (msg_queue_tasks_count == 0 || msg_widget->task_ptr)
      {
         file_list_append(current_msgs,
            NULL,
            NULL,
            0,
            0,
            0
         );

         file_list_set_userdata(current_msgs, current_msgs->size-1, msg_widget);
      }
      /* Regular messages are always above tasks */
      else
      {
        unsigned idx = (unsigned)(current_msgs->size - msg_queue_tasks_count);
         file_list_insert(current_msgs,
            NULL,
            NULL,
            0,
            0,
            0,
            idx
         );

         file_list_set_userdata(current_msgs, idx, msg_widget);
      }

      /* Start expiration timer if not associated to a task */
      if (!msg_widget->task_ptr)
      {
         menu_widgets_start_msg_expiration_timer(msg_widget, MSG_QUEUE_ANIMATION_DURATION*2 + msg_widget->duration);
      }
      /* Else, start hourglass animation timer */
      else
      {
         msg_queue_tasks_count++;
         menu_widgets_hourglass_end(msg_widget);
      }

      menu_widgets_msg_queue_move();
   }

   /* Kill first expired message */
   /* Start expiration timer of dead tasks */
   for (i = 0; i < current_msgs->size ; i++)
   {
      menu_widget_msg_t *msg  = (menu_widget_msg_t*)
         file_list_get_userdata_at_offset(current_msgs, i);

      if (!msg)
         continue;

      if (msg->task_ptr && (msg->task_finished || msg->task_cancelled))
         menu_widgets_start_msg_expiration_timer(msg, TASK_FINISHED_DURATION);

      if (msg->expired && !widgets_moving)
      {
         menu_widgets_msg_queue_kill((unsigned)i);
         break;
      }
   }

   /* Load screenshot and start its animation */
   if (screenshot_filename[0] != '\0')
   {
      menu_timer_ctx_entry_t timer;
      settings_t *settings  = config_get_ptr();
      float video_font_size = settings->floats.video_font_size;

      video_driver_texture_unload(&screenshot_texture);

      menu_display_reset_textures_list(screenshot_filename,
            "", &screenshot_texture, TEXTURE_FILTER_MIPMAP_LINEAR,
            &screenshot_texture_width, &screenshot_texture_height);

      screenshot_height = video_font_size * 4;
      screenshot_width  = width;

      screenshot_scale_factor = menu_widgets_get_thumbnail_scale_factor(
         width, screenshot_height,
         screenshot_texture_width, screenshot_texture_height
      );

      screenshot_thumbnail_width  = screenshot_texture_width * screenshot_scale_factor;
      screenshot_thumbnail_height = screenshot_texture_height * screenshot_scale_factor;

      screenshot_shotname_length  = (width - screenshot_thumbnail_width - simple_widget_padding*2) / glyph_width;

      screenshot_y = 0.0f;

      timer.cb       = menu_widgets_screenshot_end;
      timer.duration = SCREENSHOT_NOTIFICATION_DURATION;
      timer.userdata = NULL;

      menu_timer_start(&screenshot_timer, &timer);

      screenshot_loaded       = true;
      screenshot_filename[0]  = '\0';
   }
}

static int menu_widgets_draw_indicator(video_frame_info_t *video_info, 
      menu_texture_item icon, int y, int top_right_x_advance, 
      enum msg_hash_enums msg)
{
   unsigned width;

   menu_display_set_alpha(menu_widgets_backdrop_orig, DEFAULT_BACKDROP);

   if (icon)
   {
      unsigned height = simple_widget_height * 2;
      width  = height;

      menu_display_draw_quad(video_info,
         top_right_x_advance - width, y,
         width, height,
         video_info->width, video_info->height,
         menu_widgets_backdrop_orig
      );

      menu_display_set_alpha(menu_widgets_pure_white, 1.0f);

      menu_display_blend_begin(video_info);
      menu_widgets_draw_icon(video_info, width, height,
         icon, top_right_x_advance - width, y,
         video_info->width, video_info->height,
         0, 1, menu_widgets_pure_white
      );
      menu_display_blend_end(video_info);
   }
   else
   {
      unsigned height       = simple_widget_height;
      const char *txt       = msg_hash_to_str(msg);
      settings_t *settings  = config_get_ptr();
      float video_font_size = settings->floats.video_font_size;

      width = font_driver_get_message_width(font_regular, txt, (unsigned)strlen(txt), 1) + simple_widget_padding*2;

      menu_display_draw_quad(video_info,
         top_right_x_advance - width, y,
         width, height,
         video_info->width, video_info->height,
         menu_widgets_backdrop_orig
      );

      menu_display_draw_text(font_regular,
         txt,
         top_right_x_advance - width + simple_widget_padding, video_font_size + simple_widget_padding/4,
         video_info->width, video_info->height,
         0xFFFFFFFF, TEXT_ALIGN_LEFT,
         1.0f,
         false, 0, false
      );
   }

   return width;
}

static void menu_widgets_draw_task_msg(menu_widget_msg_t *msg, video_frame_info_t *video_info)
{
   unsigned text_color;
   unsigned bar_width;

   unsigned rect_x;
   unsigned rect_y;
   unsigned rect_width;
   unsigned rect_height;

   float *msg_queue_current_background;
   float *msg_queue_current_bar;

   bool draw_msg_new               = false;
   unsigned task_percentage_offset = 0;
   char task_percentage[256]       = {0};
   settings_t *settings            = config_get_ptr();
   float video_font_size           = settings->floats.video_font_size;

   if (msg->msg_new)
      draw_msg_new = !string_is_equal(msg->msg_new, msg->msg);

   task_percentage_offset = glyph_width * (msg->task_error ? 12 : 5) + simple_widget_padding * 1.25f; /*11 = strlen("Task failed")+1 */

   if (msg->task_finished)
   {
      if (msg->task_error)
         strlcpy(task_percentage, "Task failed", sizeof(task_percentage));
      else
         strlcpy(task_percentage, " ", sizeof(task_percentage));
   }
   else if (msg->task_progress >= 0 && msg->task_progress <= 100)
      snprintf(task_percentage, sizeof(task_percentage),
            "%i%%", msg->task_progress);

   rect_width = simple_widget_padding + msg->width + task_percentage_offset;
   bar_width  = rect_width * msg->task_progress/100.0f;
   text_color = COLOR_TEXT_ALPHA(0xFFFFFF00, (unsigned)(msg->alpha*255.0f));

   /* Rect */
   if (msg->task_finished)
      if (msg->task_count == 1)
         msg_queue_current_background = msg_queue_task_progress_1;
      else
         msg_queue_current_background = msg_queue_task_progress_2;
   else
      if (msg->task_count == 1)
         msg_queue_current_background = msg_queue_background;
      else
         msg_queue_current_background = msg_queue_task_progress_1;

   rect_x      = msg_queue_rect_start_x - msg_queue_icon_size_x;
   rect_y      = video_info->height - msg->offset_y;
   rect_height = msg_queue_height/2;

   menu_display_set_alpha(msg_queue_current_background, msg->alpha);
   menu_display_draw_quad(video_info,
      rect_x, rect_y,
      rect_width, rect_height,
      video_info->width, video_info->height,
      msg_queue_current_background
   );

   /* Progress bar */
   if (!msg->task_finished && msg->task_progress >= 0 && msg->task_progress <= 100)
   {
      if (msg->task_count == 1)
         msg_queue_current_bar = msg_queue_task_progress_1;
      else
         msg_queue_current_bar = msg_queue_task_progress_2;

      menu_display_set_alpha(msg_queue_current_bar, 1.0f);
      menu_display_draw_quad(video_info,
         msg_queue_task_rect_start_x, video_info->height - msg->offset_y,
         bar_width, rect_height,
         video_info->width, video_info->height,
         msg_queue_current_bar
      );
   }

   /* Icon */
   menu_display_set_alpha(menu_widgets_pure_white, msg->alpha);
   menu_display_blend_begin(video_info);
   menu_widgets_draw_icon(video_info,
      msg_queue_height/2,
      msg_queue_height/2,
      menu_widgets_icons_textures[msg->task_finished ? MENU_WIDGETS_ICON_CHECK : MENU_WIDGETS_ICON_HOURGLASS],
      msg_queue_task_hourglass_x,
      video_info->height - msg->offset_y,
      video_info->width,
      video_info->height,
      msg->task_finished ? 0 : msg->hourglass_rotation,
      1, menu_widgets_pure_white);
   menu_display_blend_end(video_info);

   /* Text */
   if (draw_msg_new)
   {
      font_driver_flush(video_info->width, video_info->height, font_regular, video_info);
      font_raster_regular.carr.coords.vertices  = 0;

      menu_display_scissor_begin(video_info, rect_x, rect_y, rect_width, rect_height);
      menu_display_draw_text(font_regular,
         msg->msg_new,
         msg_queue_task_text_start_x,
         video_info->height - msg->offset_y + msg_queue_text_scale_factor * video_font_size + msg_queue_height/4 - video_font_size/2.25f - msg_queue_height/2 + msg->msg_transition_animation,
         video_info->width, video_info->height,
         text_color,
         TEXT_ALIGN_LEFT,
         msg_queue_text_scale_factor,
         false,
         0,
         true
      );
   }

   menu_display_draw_text(font_regular,
      msg->msg,
      msg_queue_task_text_start_x,
      video_info->height - msg->offset_y + msg_queue_text_scale_factor * video_font_size + msg_queue_height/4 - video_font_size/2.25f + msg->msg_transition_animation,
      video_info->width, video_info->height,
      text_color,
      TEXT_ALIGN_LEFT,
      msg_queue_text_scale_factor,
      false,
      0,
      true
   );

   if (draw_msg_new)
   {
      font_driver_flush(video_info->width, video_info->height, font_regular, video_info);
      font_raster_regular.carr.coords.vertices  = 0;

      menu_display_scissor_end(video_info);
   }

   /* Progress text */
   text_color = COLOR_TEXT_ALPHA(0xFFFFFF00, (unsigned)(msg->alpha/2*255.0f));
   menu_display_draw_text(font_regular,
      task_percentage,
      msg_queue_rect_start_x - msg_queue_icon_size_x + rect_width - msg_queue_glyph_width,
      video_info->height - msg->offset_y + msg_queue_text_scale_factor * video_font_size + msg_queue_height/4 - video_font_size/2.25f,
      video_info->width, video_info->height,
      text_color,
      TEXT_ALIGN_RIGHT,
      msg_queue_text_scale_factor,
      false,
      0,
      true
   );
}

static void menu_widgets_draw_regular_msg(menu_widget_msg_t *msg, video_frame_info_t *video_info)
{
   menu_texture_item icon     = 0;

   unsigned bar_width;
   unsigned text_color;

   if (!icon)
      icon = menu_widgets_icons_textures[MENU_WIDGETS_ICON_INFO]; /* TODO: Real icon logic here */

   /* Icon */
   menu_display_set_alpha(msg_queue_info, msg->alpha);
   menu_display_set_alpha(menu_widgets_pure_white, msg->alpha);
   menu_display_set_alpha(msg_queue_background, msg->alpha);

   if (!msg->unfolded || msg->unfolding)
   {
      font_driver_flush(video_info->width, video_info->height, font_regular, video_info);
      font_driver_flush(video_info->width, video_info->height, font_bold, video_info);

      font_raster_regular.carr.coords.vertices  = 0;
      font_raster_bold.carr.coords.vertices     = 0;

      menu_display_scissor_begin(video_info, msg_queue_scissor_start_x, 0,
         (msg_queue_scissor_start_x + msg->width - simple_widget_padding*2) * msg->unfold, video_info->height);
   }

   if (msg_queue_has_icons)
   {
      menu_display_blend_begin(video_info);
      /* (int) cast is to be consistent with the rect drawing and prevent alignment
      * issues, don't remove it */
      menu_widgets_draw_icon(video_info,
         msg_queue_icon_size_x, msg_queue_icon_size_y,
         msg_queue_icon_rect, msg_queue_spacing, (int)(video_info->height - msg->offset_y - msg_queue_icon_offset_y),
         video_info->width, video_info->height, 
         0, 1, msg_queue_background);

      menu_display_blend_end(video_info);
   }

   /* Background */
   bar_width = simple_widget_padding + msg->width;

   menu_display_draw_quad(video_info,
      msg_queue_rect_start_x, video_info->height - msg->offset_y,
      bar_width, msg_queue_height,
      video_info->width, video_info->height,
      msg_queue_background
   );

   /* Text */
   text_color = COLOR_TEXT_ALPHA(0xFFFFFF00, (unsigned)(msg->alpha*255.0f));

   menu_display_draw_text(font_regular,
      msg->msg,
      msg_queue_regular_text_start - ((1.0f-msg->unfold) * msg->width/2),
      video_info->height - msg->offset_y + msg_queue_regular_text_base_y - msg->text_height/2,
      video_info->width, video_info->height,
      text_color,
      TEXT_ALIGN_LEFT,
      msg_queue_text_scale_factor, false, 0, true
   );

   if (!msg->unfolded || msg->unfolding)
   {
      font_driver_flush(video_info->width, video_info->height, font_regular, video_info);
      font_driver_flush(video_info->width, video_info->height, font_bold, video_info);

      font_raster_regular.carr.coords.vertices  = 0;
      font_raster_bold.carr.coords.vertices     = 0;

      menu_display_scissor_end(video_info);
   }

   if (msg_queue_has_icons)
   {
      menu_display_blend_begin(video_info);

      menu_widgets_draw_icon(video_info,
         msg_queue_icon_size_x, msg_queue_icon_size_y,
         msg_queue_icon, msg_queue_spacing, video_info->height - msg->offset_y - msg_queue_icon_offset_y, 
         video_info->width, video_info->height,
         0, 1, msg_queue_info);

      menu_widgets_draw_icon(video_info,
         msg_queue_icon_size_x, msg_queue_icon_size_y,
         msg_queue_icon_outline, msg_queue_spacing, video_info->height - msg->offset_y - msg_queue_icon_offset_y, 
         video_info->width, video_info->height,
         0, 1, menu_widgets_pure_white);

      menu_widgets_draw_icon(video_info,
         msg_queue_internal_icon_size, msg_queue_internal_icon_size,
         icon, msg_queue_spacing + msg_queue_internal_icon_offset, video_info->height - msg->offset_y - msg_queue_icon_offset_y + msg_queue_internal_icon_offset, 
         video_info->width, video_info->height,
         0, 1, menu_widgets_pure_white);

      menu_display_blend_end(video_info);
   }
}

static void menu_widgets_draw_backdrop(video_frame_info_t *video_info, float alpha)
{
   menu_display_set_alpha(menu_widgets_backdrop, alpha);
   menu_display_draw_quad(video_info, 0, 0, video_info->width, video_info->height, video_info->width, video_info->height, menu_widgets_backdrop);
}

static void menu_widgets_draw_load_content_animation(video_frame_info_t *video_info)
{
   /* TODO: scale this right ? (change metrics) */

   int icon_size        = (int) load_content_animation_icon_size;
   uint32_t text_alpha  = load_content_animation_fade_alpha * 255.0f;
   uint32_t text_color  = COLOR_TEXT_ALPHA(0xB8B8B800, text_alpha);
   unsigned text_offset = -25 * load_content_animation_fade_alpha;
   float *icon_color    = load_content_animation_icon_color;

   /* Fade out */
   menu_widgets_draw_backdrop(video_info, load_content_animation_fade_alpha);

   /* Icon */
   menu_display_set_alpha(icon_color, load_content_animation_icon_alpha);
   menu_display_blend_begin(video_info);
   menu_widgets_draw_icon(video_info, icon_size,
      icon_size, load_content_animation_icon,
      video_info->width/2 - icon_size/2,
      video_info->height/2 - icon_size/2,
      video_info->width,
      video_info->height,
      0, 1, icon_color
   );
   menu_display_blend_end(video_info);

   /* Text */
   menu_display_draw_text(font_bold,
      load_content_animation_content_name,
      video_info->width/2,
      video_info->height/2 + 175 + 25 + text_offset,
      video_info->width,
      video_info->height,
      text_color,
      TEXT_ALIGN_CENTER,
      1,
      false,
      0,
      false
   );

   /* Flush text layer */
   font_driver_flush(video_info->width, video_info->height, font_regular, video_info);
   font_driver_flush(video_info->width, video_info->height, font_bold, video_info);

   font_raster_regular.carr.coords.vertices = 0;
   font_raster_bold.carr.coords.vertices = 0;

   /* Everything disappears */
   menu_widgets_draw_backdrop(video_info, load_content_animation_final_fade_alpha);
}

void menu_widgets_frame(void *data)
{
   size_t i;
   video_frame_info_t *video_info = (video_frame_info_t*)data;
   int top_right_x_advance        = video_info->width;
   int scissor_me_timbers         = 0;
   settings_t *settings           = config_get_ptr();
   float video_font_size          = settings->floats.video_font_size;

   menu_widgets_frame_count++;

   menu_display_set_viewport(video_info->width, video_info->height);

   /* Font setup */
   font_driver_bind_block(font_regular, &font_raster_regular);
   font_driver_bind_block(font_bold, &font_raster_bold);

   font_raster_regular.carr.coords.vertices = 0;
   font_raster_bold.carr.coords.vertices    = 0;

   /* AI Service overlay */
   if (ai_service_overlay_state > 0)
   {
      float outline_color[16] = {
      0.00, 1.00, 0.00, 1.00,
      0.00, 1.00, 0.00, 1.00,
      0.00, 1.00, 0.00, 1.00,
      0.00, 1.00, 0.00, 1.00,
      };
      menu_display_set_alpha(menu_widgets_pure_white, 1.0f);

      menu_widgets_draw_icon_blend(video_info,
         video_info->width, video_info->height,
         ai_service_overlay_texture,
         0, 0,
         video_info->width, video_info->height,
         0, 1, menu_widgets_pure_white
      );
      /* top line */
      menu_display_draw_quad(video_info,
         0, 0,
         video_info->width, 1,
         video_info->width, video_info->height,
         outline_color
      );
      /* bottom line */
      menu_display_draw_quad(video_info,
         0, video_info->height-1,
         video_info->width, 1,
         video_info->width, video_info->height,
         outline_color
      );
      /* left line */
      menu_display_draw_quad(video_info,
         0, 0,
         1, video_info->height,
         video_info->width, video_info->height,
         outline_color
      );
      /* right line */
      menu_display_draw_quad(video_info,
         video_info->width-1, 0,
         1, video_info->height,
         video_info->width, video_info->height,
         outline_color
      );

      if (ai_service_overlay_state == 2)
          ai_service_overlay_state = 3;
   }


   /* Libretro message */
   if (libretro_message_alpha > 0.0f)
   {
      unsigned text_color = COLOR_TEXT_ALPHA(0xffffffff, (unsigned)(libretro_message_alpha*255.0f));
      menu_display_set_alpha(menu_widgets_backdrop_orig, libretro_message_alpha);

      menu_display_draw_quad(video_info,
         0, video_info->height - generic_message_height,
         libretro_message_width, generic_message_height,
         video_info->width, video_info->height,
         menu_widgets_backdrop_orig);

      menu_display_draw_text(font_regular, libretro_message,
         simple_widget_padding,
         video_info->height - generic_message_height/2 + line_height/4,
         video_info->width, video_info->height,
         text_color, TEXT_ALIGN_LEFT,
         1, false, 0, false);
   }

   /* Generic message */
   if (generic_message_alpha > 0.0f)
   {
      unsigned text_color = COLOR_TEXT_ALPHA(0xffffffff, (unsigned)(generic_message_alpha*255.0f));
      menu_display_set_alpha(menu_widgets_backdrop_orig, generic_message_alpha);

      menu_display_draw_quad(video_info,
         0, video_info->height - generic_message_height,
         video_info->width, generic_message_height,
         video_info->width, video_info->height,
         menu_widgets_backdrop_orig);

      menu_display_draw_text(font_regular, generic_message,
         video_info->width/2,
         video_info->height - generic_message_height/2 + line_height/4,
         video_info->width, video_info->height,
         text_color, TEXT_ALIGN_CENTER,
         1, false, 0, false);
   }

   /* Screenshot */
   if (screenshot_loaded)
   {
      char shotname[256];
      menu_animation_ctx_ticker_t ticker;

      menu_display_set_alpha(menu_widgets_backdrop_orig, DEFAULT_BACKDROP);

      menu_display_draw_quad(video_info,
         0, screenshot_y,
         screenshot_width, screenshot_height,
         video_info->width, video_info->height,
         menu_widgets_backdrop_orig
      );

      menu_display_set_alpha(menu_widgets_pure_white, 1.0f);
      menu_widgets_draw_icon(video_info,
         screenshot_thumbnail_width, screenshot_thumbnail_height,
         screenshot_texture,
         0, screenshot_y,
         video_info->width, video_info->height,
         0, 1, menu_widgets_pure_white
      );

      menu_display_draw_text(font_regular,
         msg_hash_to_str(MSG_SCREENSHOT_SAVED),
         screenshot_thumbnail_width + simple_widget_padding, video_font_size * 1.9f + screenshot_y,
         video_info->width, video_info->height,
         text_color_faint,
         TEXT_ALIGN_LEFT,
         1, false, 0, true
      );

      ticker.idx        = menu_animation_get_ticker_idx();
      ticker.len        = screenshot_shotname_length;
      ticker.s          = shotname;
      ticker.selected   = true;
      ticker.str        = screenshot_shotname;

      menu_animation_ticker(&ticker);

      menu_display_draw_text(font_regular,
         shotname,
         screenshot_thumbnail_width + simple_widget_padding, video_font_size * 2.9f + screenshot_y,
         video_info->width, video_info->height,
         text_color_info,
         TEXT_ALIGN_LEFT,
         1, false, 0, true
      );
   }

   /* Achievement notification */
   if (cheevo_title)
   {
      unsigned unfold_offet = ((1.0f-cheevo_unfold) * cheevo_width/2);

      menu_display_set_alpha(menu_widgets_backdrop_orig, DEFAULT_BACKDROP);
      menu_display_set_alpha(menu_widgets_pure_white, 1.0f);

      /* Default icon */
      if (!cheevo_badge)
      {
         /* Backdrop */
         menu_display_draw_quad(video_info,
            0, (int)cheevo_y,
            cheevo_height, cheevo_height,
            video_info->width, video_info->height,
            menu_widgets_backdrop_orig);

         /* Icon */
         if (menu_widgets_icons_textures[MENU_WIDGETS_ICON_ACHIEVEMENT])
         {
            menu_display_blend_begin(video_info);
            menu_widgets_draw_icon(video_info,
               cheevo_height, cheevo_height,
               menu_widgets_icons_textures[MENU_WIDGETS_ICON_ACHIEVEMENT], 0, cheevo_y,
               video_info->width, video_info->height, 0, 1, menu_widgets_pure_white);
            menu_display_blend_end(video_info);
         }
      }
      /* Badge */
      else
      {
         menu_widgets_draw_icon(video_info,
               cheevo_height, cheevo_height,
               cheevo_badge, 0, cheevo_y,
               video_info->width, video_info->height, 0, 1, menu_widgets_pure_white);
      }

      scissor_me_timbers = (fabs(cheevo_unfold - 1.0f) > 0.01); /* I _think_ cheevo_unfold changes in another thread */
      if (scissor_me_timbers)
      {
         menu_display_scissor_begin(video_info,
            cheevo_height, 0,
            (unsigned)((float)(cheevo_width) * cheevo_unfold), cheevo_height);
      }

      /* Backdrop */
      menu_display_draw_quad(video_info,
            cheevo_height, (int)cheevo_y,
            cheevo_width, cheevo_height,
            video_info->width, video_info->height,
            menu_widgets_backdrop_orig);

      /* Title */
      menu_display_draw_text(font_regular,
         msg_hash_to_str(MSG_ACHIEVEMENT_UNLOCKED),
         cheevo_height + simple_widget_padding - unfold_offet, video_font_size * 1.9f + cheevo_y,
         video_info->width, video_info->height,
         text_color_faint,
         TEXT_ALIGN_LEFT,
         1, false, 0, true
      );

      /* Title */

      /* TODO: is a ticker necessary ? */

      menu_display_draw_text(font_regular,
         cheevo_title,
         cheevo_height + simple_widget_padding - unfold_offet, video_font_size * 2.9f + cheevo_y,
         video_info->width, video_info->height,
         text_color_info,
         TEXT_ALIGN_LEFT,
         1, false, 0, true
      );

      if (scissor_me_timbers)
      {
         font_driver_flush(video_info->width, video_info->height, font_regular, video_info);
         font_raster_regular.carr.coords.vertices  = 0;
         menu_display_scissor_end(video_info);
      }
   }

   /* Volume */
   if (volume_alpha > 0.0f)
   {
      char msg[255];
      char percentage_msg[255];

      menu_texture_item volume_icon = 0;

      unsigned volume_width         = video_info->width / 3;
      unsigned volume_height        = video_font_size * 4;
      unsigned icon_size            = menu_widgets_icons_textures[MENU_WIDGETS_ICON_VOLUME_MED] ? volume_height : simple_widget_padding;
      unsigned text_color           = COLOR_TEXT_ALPHA(0xffffffff, (unsigned)(volume_text_alpha*255.0f));
      unsigned text_color_db        = COLOR_TEXT_ALPHA(text_color_faint, (unsigned)(volume_text_alpha*255.0f));

      unsigned bar_x                = icon_size;
      unsigned bar_height           = video_font_size / 2;
      unsigned bar_width            = volume_width - bar_x - simple_widget_padding;
      unsigned bar_y                = volume_height / 2 + bar_height/2;

      float *bar_background         = NULL;
      float *bar_foreground         = NULL;
      float bar_percentage          = 0.0f;

      if (volume_mute)
         volume_icon = menu_widgets_icons_textures[MENU_WIDGETS_ICON_VOLUME_MUTE];
      else if (volume_percent <= 1.0f)
      {
         if (volume_percent <= 0.5f)
            volume_icon = menu_widgets_icons_textures[MENU_WIDGETS_ICON_VOLUME_MIN];
         else
            volume_icon = menu_widgets_icons_textures[MENU_WIDGETS_ICON_VOLUME_MED];

         bar_background = volume_bar_background;
         bar_foreground = volume_bar_normal;
         bar_percentage = volume_percent;
      }
      else if (volume_percent > 1.0f && volume_percent <= 2.0f)
      {
         volume_icon = menu_widgets_icons_textures[MENU_WIDGETS_ICON_VOLUME_MAX];

         bar_background = volume_bar_normal;
         bar_foreground = volume_bar_loud;
         bar_percentage = volume_percent - 1.0f;
      }
      else
      {
         volume_icon = menu_widgets_icons_textures[MENU_WIDGETS_ICON_VOLUME_MAX];

         bar_background = volume_bar_loud;
         bar_foreground = volume_bar_loudest;
         bar_percentage = volume_percent - 2.0f;
      }

      if (bar_percentage > 1.0f)
         bar_percentage = 1.0f;

      /* Backdrop */
      menu_display_set_alpha(menu_widgets_backdrop_orig, volume_alpha);

      menu_display_draw_quad(video_info,
         0, 0,
         volume_width,
         volume_height,
         video_info->width,
         video_info->height,
         menu_widgets_backdrop_orig
      );

      /* Icon */
      if (volume_icon)
      {
         menu_display_set_alpha(menu_widgets_pure_white, volume_text_alpha);

         menu_display_blend_begin(video_info);
         menu_widgets_draw_icon(video_info,
            icon_size, icon_size,
            volume_icon,
            0, 0,
            video_info->width, video_info->height,
            0, 1, menu_widgets_pure_white
         );
         menu_display_blend_end(video_info);
      }

      if (volume_mute)
      {
         if (!menu_widgets_icons_textures[MENU_WIDGETS_ICON_VOLUME_MUTE])
         {
            const char *text  = msg_hash_to_str(MSG_AUDIO_MUTED);
            menu_display_draw_text(font_regular,
               text,
               volume_width/2, volume_height/2 + video_font_size / 3,
               video_info->width, video_info->height,
               text_color, TEXT_ALIGN_CENTER,
               1, false, 0, false
            );
         }
      }
      else
      {
         /* Bar */
         menu_display_set_alpha(bar_background, volume_text_alpha);
         menu_display_set_alpha(bar_foreground, volume_text_alpha);

         menu_display_draw_quad(video_info,
            bar_x + bar_percentage * bar_width, bar_y,
            bar_width - bar_percentage * bar_width, bar_height,
            video_info->width, video_info->height,
            bar_background
         );

         menu_display_draw_quad(video_info,
            bar_x, bar_y,
            bar_percentage * bar_width, bar_height,
            video_info->width, video_info->height,
            bar_foreground
         );

         /* Text */
         snprintf(msg, sizeof(msg), (volume_db >= 0 ? "+%.1f dB" : "%.1f dB"),
            volume_db);

         snprintf(percentage_msg, sizeof(percentage_msg), "%d%%",
            (int)(volume_percent * 100.0f));

         menu_display_draw_text(font_regular,
            msg,
            volume_width - simple_widget_padding, video_font_size * 2,
            video_info->width, video_info->height,
            text_color_db,
            TEXT_ALIGN_RIGHT,
            1, false, 0, false
         );

         menu_display_draw_text(font_regular,
            percentage_msg,
            icon_size, video_font_size * 2,
            video_info->width, video_info->height,
            text_color,
            TEXT_ALIGN_LEFT,
            1, false, 0, false
         );
      }
   }

   /* Draw all messages */
   for (i = 0; i < current_msgs->size; i++)
   {
      menu_widget_msg_t *msg = (menu_widget_msg_t*)
         file_list_get_userdata_at_offset(current_msgs, i);

      if (!msg)
         continue;

      if (msg->task_ptr)
         menu_widgets_draw_task_msg(msg, video_info);
      else
         menu_widgets_draw_regular_msg(msg, video_info);
   }

   /* FPS Counter */
   if (     video_info->fps_show 
         || video_info->framecount_show
         || video_info->memory_show
         )
   {
      const char *text      = *menu_widgets_fps_text == '\0' ? "N/A" : menu_widgets_fps_text;

      int text_width        = font_driver_get_message_width(font_regular, text, (unsigned)strlen(text), 1.0f);
      int total_width       = text_width + simple_widget_padding * 2;

      menu_display_set_alpha(menu_widgets_backdrop_orig, DEFAULT_BACKDROP);

      menu_display_draw_quad(video_info,
         top_right_x_advance - total_width, 0,
         total_width, simple_widget_height,
         video_info->width, video_info->height,
         menu_widgets_backdrop_orig
      );

      menu_display_draw_text(font_regular,
         text,
         top_right_x_advance - simple_widget_padding - text_width, video_font_size + simple_widget_padding/4,
         video_info->width, video_info->height,
         0xFFFFFFFF,
         TEXT_ALIGN_LEFT,
         1, false,0, true
      );
   }

   /* Indicators */
   if (video_info->widgets_is_paused)
      top_right_x_advance -= menu_widgets_draw_indicator(video_info,
         menu_widgets_icons_textures[MENU_WIDGETS_ICON_PAUSED], (video_info->fps_show ? simple_widget_height : 0), top_right_x_advance,
         MSG_PAUSED);

   if (video_info->widgets_is_fast_forwarding)
      top_right_x_advance -= menu_widgets_draw_indicator(video_info,
         menu_widgets_icons_textures[MENU_WIDGETS_ICON_FAST_FORWARD], (video_info->fps_show ? simple_widget_height : 0), top_right_x_advance,
         MSG_PAUSED);

   if (video_info->widgets_is_rewinding)
      top_right_x_advance -= menu_widgets_draw_indicator(video_info,
         menu_widgets_icons_textures[MENU_WIDGETS_ICON_REWIND], (video_info->fps_show ? simple_widget_height : 0), top_right_x_advance,
         MSG_REWINDING);

   if (video_info->runloop_is_slowmotion)
      top_right_x_advance -= menu_widgets_draw_indicator(video_info,
         menu_widgets_icons_textures[MENU_WIDGETS_ICON_SLOW_MOTION], (video_info->fps_show ? simple_widget_height : 0), top_right_x_advance,
         MSG_SLOW_MOTION);

   /* Screenshot */
   if (screenshot_alpha > 0.0f)
   {
      menu_display_set_alpha(menu_widgets_pure_white, screenshot_alpha);
      menu_display_draw_quad(video_info,
         0, 0,
         video_info->width, video_info->height,
         video_info->width, video_info->height,
         menu_widgets_pure_white
      );
   }

   /* Load content animation */
   if (load_content_animation_running)
      menu_widgets_draw_load_content_animation(video_info);
   else
   {
      font_driver_flush(video_info->width, video_info->height, font_regular, video_info);
      font_driver_flush(video_info->width, video_info->height, font_bold, video_info);

      font_raster_regular.carr.coords.vertices = 0;
      font_raster_bold.carr.coords.vertices = 0;
   }

   menu_display_unset_viewport(video_info->width, video_info->height);
}

bool menu_widgets_init(bool video_is_threaded)
{
   if (!menu_display_init_first_driver(video_is_threaded))
      goto error;

   menu_widgets_frame_count = 0;

   msg_queue = fifo_new(MSG_QUEUE_PENDING_MAX * sizeof(menu_widget_msg_t*));

   if (!msg_queue)
      goto error;

   current_msgs = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (!current_msgs)
      goto error;

   if (!file_list_reserve(current_msgs, MSG_QUEUE_ONSCREEN_MAX))
      goto error;

   return true;

error:
   if (menu_widgets_ready())
      menu_widgets_free();
   return false;
}

void menu_widgets_context_reset(bool is_threaded,
      unsigned width, unsigned height)
{
   int i;
   char xmb_path[PATH_MAX_LENGTH];
   char monochrome_png_path[PATH_MAX_LENGTH];
   char menu_widgets_path[PATH_MAX_LENGTH];
   char theme_path[PATH_MAX_LENGTH];
   char ozone_path[PATH_MAX_LENGTH];
   char font_path[PATH_MAX_LENGTH];
   settings_t *settings  = config_get_ptr();
   float video_font_size = settings->floats.video_font_size;

   /* Textures paths */
   fill_pathname_join(
      menu_widgets_path,
      settings->paths.directory_assets,
      "menu_widgets",
      sizeof(menu_widgets_path)
   );

   fill_pathname_join(
      xmb_path,
      settings->paths.directory_assets,
      "xmb",
      sizeof(xmb_path)
   );

   /* Monochrome */
   fill_pathname_join(
      theme_path,
      xmb_path,
      "monochrome",
      sizeof(theme_path)
   );

   fill_pathname_join(
      monochrome_png_path,
      theme_path,
      "png",
      sizeof(monochrome_png_path)
   );

   /* Load textures */
   /* Icons */
   for (i = 0; i < MENU_WIDGETS_ICON_LAST; i++)
   {
      menu_display_reset_textures_list(menu_widgets_icons_names[i], monochrome_png_path, &menu_widgets_icons_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL);
   }

   /* Message queue */
   menu_display_reset_textures_list("msg_queue_icon.png", menu_widgets_path, &msg_queue_icon, TEXTURE_FILTER_LINEAR, NULL, NULL);
   menu_display_reset_textures_list("msg_queue_icon_outline.png", menu_widgets_path, &msg_queue_icon_outline, TEXTURE_FILTER_LINEAR, NULL, NULL);
   menu_display_reset_textures_list("msg_queue_icon_rect.png", menu_widgets_path, &msg_queue_icon_rect, TEXTURE_FILTER_NEAREST, NULL, NULL);

   msg_queue_has_icons = msg_queue_icon && msg_queue_icon_outline && msg_queue_icon_rect;

   /* Fonts paths */
      fill_pathname_join(
      ozone_path,
      settings->paths.directory_assets,
      "ozone",
      sizeof(ozone_path)
   );

   /* Fonts */
   if (settings->paths.path_font[0] == '\0')
   {
      fill_pathname_join(font_path, ozone_path, "regular.ttf", sizeof(font_path));
      font_regular = menu_display_font_file(font_path, video_font_size, is_threaded);

      fill_pathname_join(font_path, ozone_path, "bold.ttf", sizeof(font_path));
      font_bold = menu_display_font_file(font_path, video_font_size, is_threaded);
   }
   else
   {
      font_regular = menu_display_font_file(settings->paths.path_font, video_font_size, is_threaded);
      font_bold = menu_display_font_file(settings->paths.path_font, video_font_size, is_threaded);
   }

   /* Metrics */
   simple_widget_padding            = video_font_size * 2/3;
   simple_widget_height             = video_font_size + simple_widget_padding;
   glyph_width                      = font_driver_get_message_width(font_regular, "a", 1, 1);
   line_height                      = font_driver_get_line_height(font_regular, 1);

   msg_queue_height                 = video_font_size * 2.5f;

   if (msg_queue_has_icons)
   {
      msg_queue_icon_size_y         = msg_queue_height * 1.2347826087f; /* original image is 280x284 */
      msg_queue_icon_size_x         = 0.98591549295f * msg_queue_icon_size_y;
   }
   else
   {
      msg_queue_icon_size_x         = 0;
      msg_queue_icon_size_y         = 0;
   }

   msg_queue_text_scale_factor      = 0.69f;
   msg_queue_base_width             = width / 4;
   msg_queue_spacing                = msg_queue_height / 3;
   msg_queue_glyph_width            = glyph_width * msg_queue_text_scale_factor;
   msg_queue_rect_start_x           = msg_queue_spacing + msg_queue_icon_size_x;
   msg_queue_internal_icon_size     = msg_queue_icon_size_y;
   msg_queue_internal_icon_offset   = (msg_queue_icon_size_y - msg_queue_internal_icon_size)/2;
   msg_queue_icon_offset_y          = (msg_queue_icon_size_y - msg_queue_height)/2;
   msg_queue_scissor_start_x        = msg_queue_spacing + msg_queue_icon_size_x - (msg_queue_icon_size_x * 0.28928571428f);
   msg_queue_default_rect_width     = msg_queue_glyph_width * 40;

   if (msg_queue_has_icons)
      msg_queue_regular_padding_x   = simple_widget_padding/2;
   else
      msg_queue_regular_padding_x   = simple_widget_padding;

   msg_queue_task_rect_start_x      = msg_queue_rect_start_x - msg_queue_icon_size_x;

   msg_queue_task_text_start_x      = msg_queue_task_rect_start_x + msg_queue_height/2;

   if (!menu_widgets_icons_textures[MENU_WIDGETS_ICON_HOURGLASS])
      msg_queue_task_text_start_x   -= msg_queue_glyph_width*2;

   msg_queue_regular_text_start     = msg_queue_rect_start_x + msg_queue_regular_padding_x;
   msg_queue_regular_text_base_y    = video_font_size * msg_queue_text_scale_factor + msg_queue_height/2;

   msg_queue_task_hourglass_x       = msg_queue_rect_start_x - msg_queue_icon_size_x;

   generic_message_height           = video_font_size * 2;
}

void menu_widgets_context_destroy(void)
{
   int i;

   /* TODO: Dismiss onscreen notifications that have been freed */

   /* Textures */
   for (i = 0; i < MENU_WIDGETS_ICON_LAST; i++)
      video_driver_texture_unload(&menu_widgets_icons_textures[i]);

   video_driver_texture_unload(&msg_queue_icon);
   video_driver_texture_unload(&msg_queue_icon_outline);
   video_driver_texture_unload(&msg_queue_icon_rect);

   /* Fonts */
   menu_display_font_free(font_regular);
   menu_display_font_free(font_bold);

   font_regular = NULL;
   font_bold = NULL;
}

static void menu_widgets_achievement_free(void *userdata)
{
   if (cheevo_title)
   {
      free(cheevo_title);
      cheevo_title = NULL;
   }

   if (cheevo_badge)
   {
      video_driver_texture_unload(&cheevo_badge);
      cheevo_badge = 0;
   }
}

void menu_widgets_free(void)
{
   size_t i;
   menu_animation_ctx_tag libretro_tag;

   /* Kill any pending animation */
   menu_animation_kill_by_tag(&volume_tag);
   menu_animation_kill_by_tag(&menu_widgets_generic_tag);

   /* Purge everything from the fifo */
   if (msg_queue)
   {
      while (fifo_read_avail(msg_queue) > 0)
      {
         menu_widget_msg_t *msg_widget;

         fifo_read(msg_queue, &msg_widget, sizeof(msg_widget));

         menu_widgets_msg_queue_free(msg_widget, false);
         free(msg_widget);
      }

      fifo_free(msg_queue);
      msg_queue = NULL;
   }

   /* Purge everything from the list */
   if (current_msgs)
   {
      for (i = 0; i < current_msgs->size; i++)
      {
         menu_widget_msg_t *msg = (menu_widget_msg_t*)
            file_list_get_userdata_at_offset(current_msgs, i);

         menu_widgets_msg_queue_free(msg, false);
      }
      file_list_free(current_msgs);
      current_msgs = NULL;
   }

   msg_queue_tasks_count = 0;

   /* Achievement notification */
   menu_widgets_achievement_free(NULL);

   /* Font */
   video_coord_array_free(&font_raster_regular.carr);
   video_coord_array_free(&font_raster_bold.carr);

   font_driver_bind_block(NULL, NULL);

   /* Reset state of all other widgets */
   /* Generic message*/
   generic_message_alpha = 0.0f;

   /* Libretro message */
   libretro_tag = (uintptr_t) &libretro_message_timer;
   libretro_message_alpha = 0.0f;
   menu_timer_kill(&libretro_message_timer);
   menu_animation_kill_by_tag(&libretro_tag);

   /* AI Service overlay */
   /* ... */

   /* Volume */
   volume_alpha = 0.0f;

   /* Screenshot */
   screenshot_alpha = 0.0f;
   menu_widgets_screenshot_dispose(NULL);
}

static void menu_widgets_volume_timer_end(void *userdata)
{
   menu_animation_ctx_entry_t entry;

   entry.cb             = NULL;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &volume_alpha;
   entry.tag            = volume_tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   menu_animation_push(&entry);

   entry.subject        = &volume_text_alpha;

   menu_animation_push(&entry);
}

void menu_widgets_volume_update_and_show(void)
{
   menu_timer_ctx_entry_t entry;
   settings_t *settings = config_get_ptr();
   bool mute            = *(audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE));
   float new_volume     = settings->floats.audio_volume;

   menu_animation_kill_by_tag(&volume_tag);

   volume_db         = new_volume;
   volume_percent    = pow(10, new_volume/20);
   volume_alpha      = DEFAULT_BACKDROP;
   volume_text_alpha = 1.0f;
   volume_mute       = mute;

   entry.cb          = menu_widgets_volume_timer_end;
   entry.duration    = VOLUME_DURATION;
   entry.userdata    = NULL;

   menu_timer_start(&volume_timer, &entry);
}

bool menu_widgets_set_fps_text(const char *new_fps_text)
{
   strlcpy(menu_widgets_fps_text,
         new_fps_text, sizeof(menu_widgets_fps_text));

   return true;
}

int menu_widgets_ai_service_overlay_get_state()
{
   return ai_service_overlay_state;
}

bool menu_widgets_ai_service_overlay_set_state(int state)
{
   ai_service_overlay_state = state;
   return true;
}



bool menu_widgets_ai_service_overlay_load(
        char* buffer, unsigned buffer_len, enum image_type_enum image_type)
{
   if (ai_service_overlay_state == 0)
   {
      bool res;
      res = menu_display_reset_textures_list_buffer(
               &ai_service_overlay_texture, 
               TEXTURE_FILTER_MIPMAP_LINEAR, 
               (void *) buffer, buffer_len, image_type,
               &ai_service_overlay_width, &ai_service_overlay_height);
      if (res)
         ai_service_overlay_state = 1;
      return res;
   }
   return true;
}

void menu_widgets_ai_service_overlay_unload()
{
   if (ai_service_overlay_state == 1)
   {
      video_driver_texture_unload(&ai_service_overlay_texture);
      ai_service_overlay_state = 0;
   }
}

static void menu_widgets_screenshot_fadeout(void *userdata)
{
   menu_animation_ctx_entry_t entry;

   entry.cb             = NULL;
   entry.duration       = SCREENSHOT_DURATION_OUT;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &screenshot_alpha;
   entry.tag            = menu_widgets_generic_tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   menu_animation_push(&entry);
}

static void menu_widgets_play_screenshot_flash(void)
{
   menu_animation_ctx_entry_t entry;

   entry.cb             = menu_widgets_screenshot_fadeout;
   entry.duration       = SCREENSHOT_DURATION_IN;
   entry.easing_enum    = EASING_IN_QUAD;
   entry.subject        = &screenshot_alpha;
   entry.tag            = menu_widgets_generic_tag;
   entry.target_value   = 1.0f;
   entry.userdata       = NULL;

   menu_animation_push(&entry);
}

void menu_widgets_screenshot_taken(const char *shotname, const char *filename)
{
   menu_widgets_play_screenshot_flash();
   strlcpy(screenshot_filename, filename, sizeof(screenshot_filename));
   strlcpy(screenshot_shotname, shotname, sizeof(screenshot_shotname));
}

static void menu_widgets_end_load_content_animation(void *userdata)
{
#if 0
   task_load_content_resume(); /* TODO: Restore that */
#endif
}

void menu_widgets_cleanup_load_content_animation(void)
{
   load_content_animation_running = false;
   free(load_content_animation_content_name);
}

void menu_widgets_start_load_content_animation(const char *content_name, bool remove_extension)
{
   /* TODO: finish the animation based on design, correct all timings */
   /* TODO: scale the icon correctly */
   menu_animation_ctx_entry_t entry;
   menu_timer_ctx_entry_t timer_entry;
   int i;

   float icon_color[16] = COLOR_HEX_TO_FLOAT(0x0473C9, 1.0f); /* TODO: random color */
   unsigned timing      = 0;

   /* Prepare data */
   load_content_animation_icon         = 0;

   /* Abort animation if we don't have an icon */
   if (!menu_driver_get_load_content_animation_data(&load_content_animation_icon,
      &load_content_animation_playlist_name) || !load_content_animation_icon)
   {
      menu_widgets_end_load_content_animation(NULL);
      return;
   }

   load_content_animation_content_name = strdup(content_name);

   if (remove_extension)
      path_remove_extension(load_content_animation_content_name);

   /* Reset animation state */
   load_content_animation_icon_size          = LOAD_CONTENT_ANIMATION_INITIAL_ICON_SIZE;
   load_content_animation_icon_alpha         = 0.0f;
   load_content_animation_fade_alpha         = 0.0f;
   load_content_animation_final_fade_alpha   = 0.0f;

   memcpy(load_content_animation_icon_color, icon_color, sizeof(load_content_animation_icon_color));

   /* Setup the animation */
   entry.cb          = NULL;
   entry.easing_enum = EASING_OUT_QUAD;
   entry.tag         = menu_widgets_generic_tag;
   entry.userdata    = NULL;

   /* Stage one: icon animation */
   /* Position */
   entry.duration       = ANIMATION_LOAD_CONTENT_DURATION;
   entry.subject        = &load_content_animation_icon_size;
   entry.target_value   = LOAD_CONTENT_ANIMATION_TARGET_ICON_SIZE;

   menu_animation_push(&entry);

   /* Alpha */
   entry.subject        = &load_content_animation_icon_alpha;
   entry.target_value   = 1.0f;

   menu_animation_push(&entry);
   timing += entry.duration;

   /* Stage two: backdrop + text */
   entry.duration       = ANIMATION_LOAD_CONTENT_DURATION*1.5;
   entry.subject        = &load_content_animation_fade_alpha;
   entry.target_value   = 1.0f;

   menu_animation_push_delayed(timing, &entry);
   timing += entry.duration;

   /* Stage three: wait then color transition */
   timing += ANIMATION_LOAD_CONTENT_DURATION*1.5;

   entry.duration = ANIMATION_LOAD_CONTENT_DURATION*3;

   for (i = 0; i < 16; i++)
   {
      if (i == 3 || i == 7 || i == 11 || i == 15)
         continue;

      entry.subject        = &load_content_animation_icon_color[i];
      entry.target_value   = menu_widgets_pure_white[i];

      menu_animation_push_delayed(timing, &entry);
   }

   timing += entry.duration;

   /* Stage four: wait then make everything disappear */
   timing += ANIMATION_LOAD_CONTENT_DURATION*2;

   entry.duration       = ANIMATION_LOAD_CONTENT_DURATION*1.5;
   entry.subject        = &load_content_animation_final_fade_alpha;
   entry.target_value   = 1.0f;

   menu_animation_push_delayed(timing, &entry);
   timing += entry.duration;

   /* Setup end */
   timer_entry.cb       = menu_widgets_end_load_content_animation;
   timer_entry.duration = timing;
   timer_entry.userdata = NULL;

   menu_timer_start(&load_content_animation_end_timer, &timer_entry);

   /* Draw all the things */
   load_content_animation_running = true;
}

static void menu_widgets_achievement_dismiss(void *userdata)
{
   menu_animation_ctx_entry_t entry;

   /* Slide up animation */
   entry.cb             = menu_widgets_achievement_free;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &cheevo_y;
   entry.tag            = menu_widgets_generic_tag;
   entry.target_value   = (float)(-(int)(cheevo_height));
   entry.userdata       = NULL;

   menu_animation_push(&entry);
}

static void menu_widgets_achievement_fold(void *userdata)
{
   menu_animation_ctx_entry_t entry;

   /* Fold */
   entry.cb             = menu_widgets_achievement_dismiss;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &cheevo_unfold;
   entry.tag            = menu_widgets_generic_tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   menu_animation_push(&entry);
}

static void menu_widgets_achievement_unfold(void *userdata)
{
   menu_animation_ctx_entry_t entry;
   menu_timer_ctx_entry_t timer;

   /* Unfold */
   entry.cb             = NULL;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &cheevo_unfold;
   entry.tag            = menu_widgets_generic_tag;
   entry.target_value   = 1.0f;
   entry.userdata       = NULL;

   menu_animation_push(&entry);

   /* Wait before dismissing */
   timer.cb       = menu_widgets_achievement_fold;
   timer.duration = MSG_QUEUE_ANIMATION_DURATION + CHEEVO_NOTIFICATION_DURATION;
   timer.userdata = NULL;

   menu_timer_start(&cheevo_timer, &timer);
}

static void menu_widgets_start_achievement_notification(void)
{
   menu_animation_ctx_entry_t entry;
   settings_t *settings = config_get_ptr();
   cheevo_height        = settings->floats.video_font_size * 4;
   cheevo_width         = MAX(
         font_driver_get_message_width(font_regular, msg_hash_to_str(MSG_ACHIEVEMENT_UNLOCKED), 0, 1),
         font_driver_get_message_width(font_regular, cheevo_title, 0, 1)
   );
   cheevo_width        += simple_widget_padding * 2;
   cheevo_y             = (float)(-(int)cheevo_height);
   cheevo_unfold        = 0.0f;

   /* Slide down animation */
   entry.cb             = menu_widgets_achievement_unfold;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &cheevo_y;
   entry.tag            = menu_widgets_generic_tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   menu_animation_push(&entry);
}

static void menu_widgets_get_badge_texture(menu_texture_item *tex, const char *badge)
{
   char badge_file[16];
   char fullpath[PATH_MAX_LENGTH];

   if (!badge)
   {
      *tex = 0;
      return;
   }

   strlcpy(badge_file, badge, sizeof(badge_file));
   strlcat(badge_file, ".png", sizeof(badge_file));
   fill_pathname_application_special(fullpath,
         PATH_MAX_LENGTH * sizeof(char),
         APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

   menu_display_reset_textures_list(badge_file, fullpath,
         tex, TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL);
}

void menu_widgets_push_achievement(const char *title, const char *badge)
{
   menu_widgets_achievement_free(NULL);

   /* TODO: Make a queue of notifications to display */

   cheevo_title = strdup(title);
   menu_widgets_get_badge_texture(&cheevo_badge, badge);

   menu_widgets_start_achievement_notification();
}

static void menu_widgets_generic_message_fadeout(void *userdata)
{
   menu_animation_ctx_entry_t entry;
   menu_animation_ctx_tag tag = (uintptr_t) &generic_message_timer;

   /* Start fade out animation */
   entry.cb             = NULL;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &generic_message_alpha;
   entry.tag            = tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   menu_animation_push(&entry);
}

void menu_widgets_set_message(char *msg)
{
   menu_timer_ctx_entry_t timer;
   menu_animation_ctx_tag tag = (uintptr_t) &generic_message_timer;

   strlcpy(generic_message, msg, GENERIC_MESSAGE_SIZE);

   generic_message_alpha = DEFAULT_BACKDROP;

   /* Kill and restart the timer / animation */
   menu_timer_kill(&generic_message_timer);
   menu_animation_kill_by_tag(&tag);

   timer.cb       = menu_widgets_generic_message_fadeout;
   timer.duration = GENERIC_MESSAGE_DURATION;
   timer.userdata = NULL;

   menu_timer_start(&generic_message_timer, &timer);
}

static void menu_widgets_libretro_message_fadeout(void *userdata)
{
   menu_animation_ctx_entry_t entry;
   menu_animation_ctx_tag tag = (uintptr_t) &libretro_message_timer;

   /* Start fade out animation */
   entry.cb             = NULL;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &libretro_message_alpha;
   entry.tag            = tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   menu_animation_push(&entry);
}

void menu_widgets_set_libretro_message(const char *msg, unsigned duration)
{
   menu_timer_ctx_entry_t timer;
   menu_animation_ctx_tag tag = (uintptr_t) &libretro_message_timer;

   strlcpy(libretro_message, msg, LIBRETRO_MESSAGE_SIZE);
   
   libretro_message_alpha = DEFAULT_BACKDROP;

   /* Kill and restart the timer / animation */
   menu_timer_kill(&libretro_message_timer);
   menu_animation_kill_by_tag(&tag);

   timer.cb       = menu_widgets_libretro_message_fadeout;
   timer.duration = duration;
   timer.userdata = NULL;

   menu_timer_start(&libretro_message_timer, &timer);

   /* Compute text width */
   libretro_message_width = font_driver_get_message_width(font_regular, msg, (unsigned)strlen(msg), 1) + simple_widget_padding * 2;
}
