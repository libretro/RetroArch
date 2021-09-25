/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2015-2018 - Andre Leiradella
 *  Copyright (C) 2018-2020 - natinusala
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
#include <retro_inline.h>

#include <queues/fifo_queue.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <retro_math.h>

#include "gfx_display.h"
#include "gfx_widgets.h"
#include "font_driver.h"

#include "../configuration.h"
#include "../msg_hash.h"

#include "../tasks/task_content.h"

#ifdef HAVE_THREADS
#define SLOCK_LOCK(x) slock_lock(x)
#define SLOCK_UNLOCK(x) slock_unlock(x)
#else
#define SLOCK_LOCK(x)
#define SLOCK_UNLOCK(x)
#endif

#define BASE_FONT_SIZE 32.0f

#define MSG_QUEUE_FONT_SIZE (BASE_FONT_SIZE * 0.69f)

/* Icons */
static const char 
*gfx_widgets_icons_names[MENU_WIDGETS_ICON_LAST]         = {
   "menu_pause.png",
   "menu_frameskip.png",
   "menu_rewind.png",
   "resume.png",

   "menu_hourglass.png",
   "menu_check.png",

   "menu_info.png",

   "menu_achievements.png"
};

static void INLINE gfx_widgets_font_free(gfx_widget_font_data_t *font_data)
{
   if (font_data->font)
      gfx_display_font_free(font_data->font);

   font_data->font        = NULL;
   font_data->usage_count = 0;
}

/* Widgets list */
const static gfx_widget_t* const widgets[] = {
#ifdef HAVE_SCREENSHOTS
   &gfx_widget_screenshot,
#endif
   &gfx_widget_volume,
#ifdef HAVE_CHEEVOS
   &gfx_widget_achievement_popup,
   &gfx_widget_leaderboard_display,
#endif
   &gfx_widget_generic_message,
   &gfx_widget_libretro_message,
   &gfx_widget_progress_message,
   &gfx_widget_load_content_animation
};

#if defined(HAVE_MENU) && defined(HAVE_XMB)
static float gfx_display_get_widget_pixel_scale(
      gfx_display_t *p_disp,
      settings_t *settings,
      unsigned width, unsigned height, bool fullscreen)
{
   static unsigned last_width                          = 0;
   static unsigned last_height                         = 0;
   static float scale                                  = 0.0f;
   static bool scale_cached                            = false;
   bool scale_updated                                  = false;
   static float last_menu_scale_factor                 = 0.0f;
   static enum menu_driver_id_type last_menu_driver_id = MENU_DRIVER_ID_UNKNOWN;
   static float adjusted_scale                         = 1.0f;
   bool gfx_widget_scale_auto                          = settings->bools.menu_widget_scale_auto;
#if (defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   float menu_widget_scale_factor                      = settings->floats.menu_widget_scale_factor;
#else
   float menu_widget_scale_factor_fullscreen           = settings->floats.menu_widget_scale_factor;
   float menu_widget_scale_factor_windowed             = settings->floats.menu_widget_scale_factor_windowed;
   float menu_widget_scale_factor                      = fullscreen ?
         menu_widget_scale_factor_fullscreen : menu_widget_scale_factor_windowed;
#endif
   float menu_scale_factor                             = menu_widget_scale_factor;

   if (gfx_widget_scale_auto)
   {
#ifdef HAVE_RGUI
      /* When using RGUI, _menu_scale_factor
       * is ignored
       * > If we are not using a widget scale factor override,
       *   just set menu_scale_factor to 1.0 */
      if (p_disp->menu_driver_id == MENU_DRIVER_ID_RGUI)
         menu_scale_factor                             = 1.0f;
      else
#endif
      {
         float _menu_scale_factor                      = 
            settings->floats.menu_scale_factor;
         menu_scale_factor                             = _menu_scale_factor;
      }
   }

   /* We need to perform a square root here, which
    * can be slow on some platforms (not *slow*, but
    * it involves enough work that it's worth trying
    * to optimise). We therefore cache the pixel scale,
    * and only update on first run or when the video
    * size changes */
   if (!scale_cached ||
       (width  != last_width) ||
       (height != last_height))
   {
      /* Baseline reference is a 1080p display */
      scale = (float)(
            sqrt((double)((width * width) + (height * height))) /
            DIAGONAL_PIXELS_1080P);

      scale_cached  = true;
      scale_updated = true;
      last_width    = width;
      last_height   = height;
   }

   /* Adjusted scale calculation may also be slow, so
    * only update if something changes */
   if (scale_updated ||
       (menu_scale_factor != last_menu_scale_factor) ||
       (p_disp->menu_driver_id != last_menu_driver_id))
   {
      adjusted_scale         = gfx_display_get_adjusted_scale(
            p_disp,
            scale, menu_scale_factor, width);
      last_menu_scale_factor = menu_scale_factor;
      last_menu_driver_id    = p_disp->menu_driver_id;
   }

   return adjusted_scale;
}
#endif

static void msg_widget_msg_transition_animation_done(void *userdata)
{
   disp_widget_msg_t *msg = (disp_widget_msg_t*)userdata;

   if (msg->msg)
      free(msg->msg);
   msg->msg = NULL;

   if (msg->msg_new)
      msg->msg = strdup(msg->msg_new);

   msg->msg_transition_animation = 0.0f;
}

void gfx_widgets_msg_queue_push(
      void *data,
      retro_task_t *task,
      const char *msg,
      unsigned duration,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category,
      unsigned prio, bool flush,
      bool menu_is_alive)
{
   disp_widget_msg_t    *msg_widget = NULL;
   dispgfx_widget_t *p_dispwidget   = (dispgfx_widget_t*)data;

   if (FIFO_WRITE_AVAIL_NONPTR(p_dispwidget->msg_queue) > 0)
   {
      /* Get current msg if it exists */
      if (task && task->frontend_userdata)
      {
         msg_widget           = (disp_widget_msg_t*)task->frontend_userdata;
         /* msg_widgets can be passed between tasks */
         msg_widget->task_ptr = task;
      }

      /* Spawn a new notification */
      if (!msg_widget)
      {
         const char *title                      = msg;

         msg_widget                             = (disp_widget_msg_t*)malloc(sizeof(*msg_widget));

         if (task)
            title                               = task->title;

         msg_widget->msg                        = NULL;
         msg_widget->msg_new                    = NULL;
         msg_widget->msg_transition_animation   = 0.0f;
         msg_widget->msg_len                    = 0;
         msg_widget->duration                   = duration;

         msg_widget->text_height                = 0;

         msg_widget->offset_y                   = 0;
         msg_widget->alpha                      = 1.0f;

         msg_widget->dying                      = false;
         msg_widget->expired                    = false;
         msg_widget->width                      = 0;

         msg_widget->expiration_timer           = 0;
         msg_widget->expiration_timer_started   = false;

         msg_widget->task_ptr                   = task;
         msg_widget->task_title_ptr             = NULL;
         msg_widget->task_count                 = 0;

         msg_widget->task_progress              = 0;
         msg_widget->task_finished              = false;
         msg_widget->task_error                 = false;
         msg_widget->task_cancelled             = false;
         msg_widget->task_ident                 = 0;

         msg_widget->unfolded                   = false;
         msg_widget->unfolding                  = false;
         msg_widget->unfold                     = 0.0f;

         msg_widget->hourglass_rotation         = 0.0f;
         msg_widget->hourglass_timer            = 0.0f;

         if (!p_dispwidget->msg_queue_has_icons)
         {
            msg_widget->unfolded                = true;
            msg_widget->unfolding               = false;
            msg_widget->unfold                  = 1.0f;
         }

         if (task)
         {
            msg_widget->msg                     = strdup(title);
            msg_widget->msg_new                 = strdup(title);
            msg_widget->msg_len                 = (unsigned)strlen(title);

            msg_widget->task_error              = !string_is_empty(task->error);
            msg_widget->task_cancelled          = task->cancelled;
            msg_widget->task_finished           = task->finished;
            msg_widget->task_progress           = task->progress;
            msg_widget->task_ident              = task->ident;
            msg_widget->task_title_ptr          = task->title;
            msg_widget->task_count              = 1;

            msg_widget->unfolded                = true;

            msg_widget->width                   = font_driver_get_message_width(
                  p_dispwidget->gfx_widget_fonts.msg_queue.font,
                  title,
                  msg_widget->msg_len, 1) +
                  p_dispwidget->simple_widget_padding / 2;

            task->frontend_userdata             = msg_widget;

            msg_widget->hourglass_rotation      = 0;
         }
         else
         {
            /* Compute rect width, wrap if necessary */
            /* Single line text > two lines text > two lines 
             * text with expanded width */
            unsigned title_length               = (unsigned)strlen(title);
            char *msg                           = NULL;
            size_t msg_len                      = 0;
            unsigned width                      = menu_is_alive 
               ? p_dispwidget->msg_queue_default_rect_width_menu_alive 
               : p_dispwidget->msg_queue_default_rect_width;
            unsigned text_width                 = font_driver_get_message_width(
                  p_dispwidget->gfx_widget_fonts.msg_queue.font,
                  title,
                  title_length,
                  1);
            msg_widget->text_height             = p_dispwidget->gfx_widget_fonts.msg_queue.line_height;

            msg_len = title_length + 1 + 1; /* 1 byte uses for inserting '\n' */
            msg = (char *)malloc(msg_len);
            if (!msg)
               return;
            msg[0] = '\0';

            /* Text is too wide, split it into two lines */
            if (text_width > width)
            {
               /* If the second line is too short, the widget may
                * look unappealing - ensure that second line is at
                * least 25% of the total width */
               if ((text_width - (text_width >> 2)) < width)
                  width = text_width - (text_width >> 2);

               word_wrap(msg, msg_len, title, (title_length * width) / text_width,
                     100, 2);

               msg_widget->text_height *= 2;
            }
            else
            {
               width                            = text_width;
               strlcpy(msg, title, msg_len);
            }

            msg_widget->msg                     = msg;
            msg_widget->msg_len                 = (unsigned)strlen(msg);
            msg_widget->width                   = width + 
               p_dispwidget->simple_widget_padding / 2;
         }

         fifo_write(&p_dispwidget->msg_queue,
               &msg_widget, sizeof(msg_widget));
      }
      /* Update task info */
      else
      {
         if (msg_widget->expiration_timer_started)
         {
            uintptr_t _tag = (uintptr_t)&msg_widget->expiration_timer;
            gfx_animation_kill_by_tag(&_tag);
            msg_widget->expiration_timer_started = false;
         }

         if (!string_is_equal(task->title, msg_widget->msg_new))
         {
            unsigned len         = (unsigned)strlen(task->title);
            unsigned new_width   = font_driver_get_message_width(
                  p_dispwidget->gfx_widget_fonts.msg_queue.font,
                  task->title,
                  len,
                  1);

            if (msg_widget->msg_new)
            {
               free(msg_widget->msg_new);
               msg_widget->msg_new                 = NULL;
            }

            msg_widget->msg_new                    = strdup(task->title);
            msg_widget->msg_len                    = len;
            msg_widget->task_title_ptr             = task->title;
            msg_widget->msg_transition_animation   = 0;

            if (!task->alternative_look)
            {
               gfx_animation_ctx_entry_t entry;

               entry.easing_enum    = EASING_OUT_QUAD;
               entry.tag            = (uintptr_t)msg_widget;
               entry.duration       = MSG_QUEUE_ANIMATION_DURATION*2;
               entry.target_value   = p_dispwidget->msg_queue_height / 2.0f;
               entry.subject        = &msg_widget->msg_transition_animation;
               entry.cb             = msg_widget_msg_transition_animation_done;
               entry.userdata       = msg_widget;

               gfx_animation_push(&entry);
            }
            else
               msg_widget_msg_transition_animation_done(msg_widget);

            msg_widget->task_count++;

            msg_widget->width = new_width;
         }

         msg_widget->task_error        = !string_is_empty(task->error);
         msg_widget->task_cancelled    = task->cancelled;
         msg_widget->task_finished     = task->finished;
         msg_widget->task_progress     = task->progress;
      }
   }
}

static void gfx_widgets_unfold_end(void *userdata)
{
   disp_widget_msg_t *unfold        = (disp_widget_msg_t*)userdata;
   dispgfx_widget_t *p_dispwidget   = (dispgfx_widget_t*)dispwidget_get_ptr();

   unfold->unfolding                = false;
   p_dispwidget->widgets_moving     = false;
}

static void gfx_widgets_move_end(void *userdata)
{
   dispgfx_widget_t *p_dispwidget   = (dispgfx_widget_t*)dispwidget_get_ptr();

   if (userdata)
   {
      gfx_animation_ctx_entry_t entry;
      disp_widget_msg_t *unfold    = (disp_widget_msg_t*)userdata;

      entry.cb                     = gfx_widgets_unfold_end;
      entry.duration               = MSG_QUEUE_ANIMATION_DURATION;
      entry.easing_enum            = EASING_OUT_QUAD;
      entry.subject                = &unfold->unfold;
      entry.tag                    = (uintptr_t)unfold;
      entry.target_value           = 1.0f;
      entry.userdata               = unfold;

      gfx_animation_push(&entry);

      unfold->unfolded             = true;
      unfold->unfolding            = true;
   }
   else
      p_dispwidget->widgets_moving = false;
}

static void gfx_widgets_msg_queue_expired(void *userdata)
{
   disp_widget_msg_t *msg = (disp_widget_msg_t *)userdata;

   if (msg && !msg->expired)
      msg->expired = true;
}

static void gfx_widgets_msg_queue_move(dispgfx_widget_t *p_dispwidget)
{
   int i;
   float y = 0;
   /* there should always be one and only one unfolded message */
   disp_widget_msg_t *unfold        = NULL; 

   SLOCK_LOCK(p_dispwidget->current_msgs_lock);

   for (i = (int)(p_dispwidget->current_msgs_size - 1); i >= 0; i--)
   {
      disp_widget_msg_t* msg = p_dispwidget->current_msgs[i];

      if (!msg || msg->dying)
         continue;

      y += p_dispwidget->msg_queue_height 
         / (msg->task_ptr ? 2 : 1) + p_dispwidget->msg_queue_spacing;

      if (!msg->unfolded)
         unfold = msg;

      if (msg->offset_y != y)
      {
         gfx_animation_ctx_entry_t entry;

         entry.cb             = (i == 0) ? gfx_widgets_move_end : NULL;
         entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
         entry.easing_enum    = EASING_OUT_QUAD;
         entry.subject        = &msg->offset_y;
         entry.tag            = (uintptr_t)msg;
         entry.target_value   = y;
         entry.userdata       = unfold;

         gfx_animation_push(&entry);

         p_dispwidget->widgets_moving = true;
      }
   }

   SLOCK_UNLOCK(p_dispwidget->current_msgs_lock);
}

static void gfx_widgets_msg_queue_free(
      dispgfx_widget_t *p_dispwidget,
      disp_widget_msg_t *msg)
{
   uintptr_t tag = (uintptr_t)msg;
   uintptr_t hourglass_timer_tag = (uintptr_t)&msg->hourglass_timer;

   if (msg->task_ptr)
   {
      /* remove the reference the task has of ourself
         only if the task is not finished already
         (finished tasks are freed before the widget) */
      if (!msg->task_finished && !msg->task_error && !msg->task_cancelled)
         msg->task_ptr->frontend_userdata = NULL;

      /* update tasks count */
      p_dispwidget->msg_queue_tasks_count--;
   }

   /* Kill all animations */
   gfx_animation_kill_by_tag(&hourglass_timer_tag);
   gfx_animation_kill_by_tag(&tag);

   /* Kill all timers */
   if (msg->expiration_timer_started)
   {
      uintptr_t _tag = (uintptr_t)&msg->expiration_timer;
      gfx_animation_kill_by_tag(&_tag);
   }

   /* Free it */
   if (msg->msg)
      free(msg->msg);

   if (msg->msg_new)
      free(msg->msg_new);

   p_dispwidget->widgets_moving = false;
}

static void gfx_widgets_msg_queue_kill_end(void *userdata)
{
   unsigned i;
   disp_widget_msg_t* msg;
   dispgfx_widget_t *p_dispwidget   = (dispgfx_widget_t*)dispwidget_get_ptr();

   SLOCK_LOCK(p_dispwidget->current_msgs_lock);

   msg = p_dispwidget->current_msgs[p_dispwidget->msg_queue_kill];
   if (msg)
   {
      /* Remove it from the list */
      for (i = p_dispwidget->msg_queue_kill; i < p_dispwidget->current_msgs_size - 1; i++)
         p_dispwidget->current_msgs[i] = p_dispwidget->current_msgs[i + 1];

      p_dispwidget->current_msgs_size--;
      p_dispwidget->current_msgs[p_dispwidget->current_msgs_size] = NULL;

      /* clean up the item */
      gfx_widgets_msg_queue_free(p_dispwidget, msg);

      /* free the associated memory */
      free(msg);
   }

   SLOCK_UNLOCK(p_dispwidget->current_msgs_lock);
}

static void gfx_widgets_msg_queue_kill(
      dispgfx_widget_t *p_dispwidget,
      unsigned idx)
{
   gfx_animation_ctx_entry_t entry;
   disp_widget_msg_t *msg = p_dispwidget->current_msgs[idx];

   if (!msg)
      return;

   p_dispwidget->widgets_moving = true;
   msg->dying                   = true;

   p_dispwidget->msg_queue_kill = idx;

   /* Drop down */
   entry.cb                     = NULL;
   entry.duration               = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum            = EASING_OUT_QUAD;
   entry.tag                    = (uintptr_t)msg;
   entry.userdata               = NULL;
   entry.subject                = &msg->offset_y;
   entry.target_value           = msg->offset_y - 
      p_dispwidget->msg_queue_height / 4;

   gfx_animation_push(&entry);

   /* Fade out */
   entry.cb                     = gfx_widgets_msg_queue_kill_end;
   entry.subject                = &msg->alpha;
   entry.target_value           = 0.0f;

   gfx_animation_push(&entry);

   /* Move all messages back to their correct position */
   if (p_dispwidget->current_msgs_size != 0)
      gfx_widgets_msg_queue_move(p_dispwidget);
}

void gfx_widgets_draw_icon(
      void *userdata,
      void *data_disp,
      unsigned video_width,
      unsigned video_height,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      float rotation, float scale_factor,
      float *color)
{
   gfx_display_ctx_rotate_draw_t rotate_draw;
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;
   gfx_display_t            *p_disp  = (gfx_display_t*)data_disp;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   if (!texture)
      return;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   gfx_display_rotate_z(p_disp, &rotate_draw, userdata);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   draw.x               = x;
   draw.y               = video_height - y - icon_height;
   draw.width           = icon_width;
   draw.height          = icon_height;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline_id     = 0;

   if (draw.height > 0 && draw.width > 0)
      if (dispctx->draw)
         dispctx->draw(&draw, userdata, video_width, video_height);
}

void gfx_widgets_draw_text(
      gfx_widget_font_data_t* font_data,
      const char *text,
      float x, float y,
      int width, int height,
      uint32_t color,
      enum text_alignment text_align,
      bool draw_outside)
{
   if (!font_data || string_is_empty(text))
      return;

   gfx_display_draw_text(
         font_data->font,
         text,
         x, y,
         width, height,
         color,
         text_align,
         1.0f,
         false,
         0.0f,
         draw_outside);

   font_data->usage_count++;
}

void gfx_widgets_flush_text(
      unsigned video_width, unsigned video_height,
      gfx_widget_font_data_t* font_data)
{
   /* Flushing is slow - only do it if font
    * has actually been used */
   if (!font_data || (font_data->usage_count == 0))
      return;

   font_driver_flush(video_width, video_height, font_data->font);
   font_data->raster_block.carr.coords.vertices = 0;
   font_data->usage_count                       = 0;
}

float gfx_widgets_get_thumbnail_scale_factor(
      const float dst_width, const float dst_height,
      const float image_width, const float image_height)
{
   float dst_ratio      = dst_width   / dst_height;
   float image_ratio    = image_width / image_height;

   if (dst_ratio > image_ratio)
      return (dst_height / image_height);
   return (dst_width / image_width);
}

static void gfx_widgets_start_msg_expiration_timer(
      disp_widget_msg_t *msg_widget, unsigned duration)
{
   gfx_timer_ctx_entry_t timer;

   timer.cb       = gfx_widgets_msg_queue_expired;
   timer.duration = duration;
   timer.userdata = msg_widget;

   gfx_animation_timer_start(&msg_widget->expiration_timer, &timer);

   msg_widget->expiration_timer_started = true;
}

static void gfx_widgets_hourglass_tick(void *userdata);

static void gfx_widgets_hourglass_end(void *userdata)
{
   gfx_timer_ctx_entry_t timer;
   disp_widget_msg_t *msg  = (disp_widget_msg_t*)userdata;

   msg->hourglass_rotation = 0.0f;

   timer.cb                = gfx_widgets_hourglass_tick;
   timer.duration          = HOURGLASS_INTERVAL;
   timer.userdata          = msg;

   gfx_animation_timer_start(&msg->hourglass_timer, &timer);
}

static void gfx_widgets_hourglass_tick(void *userdata)
{
   gfx_animation_ctx_entry_t entry;
   disp_widget_msg_t *msg = (disp_widget_msg_t*)userdata;
   uintptr_t          tag = (uintptr_t)msg;

   entry.easing_enum      = EASING_OUT_QUAD;
   entry.tag              = tag;
   entry.duration         = HOURGLASS_DURATION;
   entry.target_value     = -(2 * M_PI);
   entry.subject          = &msg->hourglass_rotation;
   entry.cb               = gfx_widgets_hourglass_end;
   entry.userdata         = msg;

   gfx_animation_push(&entry);
}

static void gfx_widgets_font_init(
      gfx_display_t *p_disp,
      dispgfx_widget_t *p_dispwidget,
      gfx_widget_font_data_t *font_data,
      bool is_threaded, char *font_path, float font_size)
{
   int                glyph_width   = 0;
   float                scaled_size = font_size * 
      p_dispwidget->last_scale_factor;

   /* Free existing font */
   if (font_data->font)
   {
      gfx_display_font_free(font_data->font);
      font_data->font = NULL;
   }

   /* Get approximate glyph width */
   font_data->glyph_width = scaled_size * (3.0f / 4.0f);

   /* Create font */
   font_data->font = gfx_display_font_file(p_disp,
         font_path, scaled_size, is_threaded);

   /* Get font metadata */
   glyph_width = font_driver_get_message_width(font_data->font, "a", 1, 1.0f);
   if (glyph_width > 0)
      font_data->glyph_width     = (float)glyph_width;
   font_data->line_height        = (float)font_driver_get_line_height(font_data->font, 1.0f);
   font_data->line_ascender      = (float)font_driver_get_line_ascender(font_data->font, 1.0f);
   font_data->line_descender     = (float)font_driver_get_line_descender(font_data->font, 1.0f);
   font_data->line_centre_offset = (float)font_driver_get_line_centre_offset(font_data->font, 1.0f);

   font_data->usage_count        = 0;
}


static void gfx_widgets_layout(
      gfx_display_t *p_disp,
      dispgfx_widget_t *p_dispwidget,
      bool is_threaded, const char *dir_assets, char *font_path)
{
   size_t i;

   /* Initialise fonts */
   if (string_is_empty(font_path))
   {
      char ozone_path[PATH_MAX_LENGTH];
      char font_file[PATH_MAX_LENGTH];

      ozone_path[0] = '\0';
      font_file[0]  = '\0';

      /* Base path */
      fill_pathname_join(ozone_path, dir_assets, "ozone", sizeof(ozone_path));

      /* Create regular font */
      fill_pathname_join(font_file, ozone_path, "regular.ttf", sizeof(font_file));
      gfx_widgets_font_init(p_disp, p_dispwidget,
            &p_dispwidget->gfx_widget_fonts.regular,
            is_threaded, font_file, BASE_FONT_SIZE);

      /* Create bold font */
      fill_pathname_join(font_file, ozone_path, "bold.ttf", sizeof(font_file));
      gfx_widgets_font_init(p_disp, p_dispwidget,
            &p_dispwidget->gfx_widget_fonts.bold,
            is_threaded, font_file, BASE_FONT_SIZE);

      /* Create msg_queue font */
      fill_pathname_join(font_file, ozone_path, "regular.ttf", sizeof(font_file));
      gfx_widgets_font_init(p_disp, p_dispwidget,
            &p_dispwidget->gfx_widget_fonts.msg_queue,
            is_threaded, font_file, MSG_QUEUE_FONT_SIZE);
   }
   else
   {
      /* Load fonts from user-supplied path */
      gfx_widgets_font_init(p_disp, p_dispwidget,
            &p_dispwidget->gfx_widget_fonts.regular,
            is_threaded, font_path, BASE_FONT_SIZE);
      gfx_widgets_font_init(p_disp, p_dispwidget,
            &p_dispwidget->gfx_widget_fonts.bold,
            is_threaded, font_path, BASE_FONT_SIZE);
      gfx_widgets_font_init(p_disp, p_dispwidget,
            &p_dispwidget->gfx_widget_fonts.msg_queue,
            is_threaded, font_path, MSG_QUEUE_FONT_SIZE);
   }

   /* Calculate dimensions */
   p_dispwidget->simple_widget_padding            = p_dispwidget->gfx_widget_fonts.regular.line_height * 2.0f/3.0f;
   p_dispwidget->simple_widget_height             = p_dispwidget->gfx_widget_fonts.regular.line_height + p_dispwidget->simple_widget_padding;

   p_dispwidget->msg_queue_height                 = p_dispwidget->gfx_widget_fonts.msg_queue.line_height * 2.5f * (BASE_FONT_SIZE / MSG_QUEUE_FONT_SIZE);

   if (p_dispwidget->msg_queue_has_icons)
   {
      p_dispwidget->msg_queue_icon_size_y         = p_dispwidget->msg_queue_height 
         * 1.2347826087f; /* original image is 280x284 */
      p_dispwidget->msg_queue_icon_size_x         = 0.98591549295f * p_dispwidget->msg_queue_icon_size_y;
   }
   else
   {
      p_dispwidget->msg_queue_icon_size_x         = 0;
      p_dispwidget->msg_queue_icon_size_y         = 0;
   }

   p_dispwidget->msg_queue_spacing                = p_dispwidget->msg_queue_height / 3;
   p_dispwidget->msg_queue_rect_start_x           = p_dispwidget->msg_queue_spacing + p_dispwidget->msg_queue_icon_size_x;
   p_dispwidget->msg_queue_internal_icon_size     = p_dispwidget->msg_queue_icon_size_y;
   p_dispwidget->msg_queue_internal_icon_offset   = (p_dispwidget->msg_queue_icon_size_y - p_dispwidget->msg_queue_internal_icon_size) / 2;
   p_dispwidget->msg_queue_icon_offset_y          = (p_dispwidget->msg_queue_icon_size_y - p_dispwidget->msg_queue_height) / 2;
   p_dispwidget->msg_queue_scissor_start_x        = p_dispwidget->msg_queue_spacing + p_dispwidget->msg_queue_icon_size_x - (p_dispwidget->msg_queue_icon_size_x * 0.28928571428f);

   if (p_dispwidget->msg_queue_has_icons)
      p_dispwidget->msg_queue_regular_padding_x   = p_dispwidget->simple_widget_padding / 2;
   else
      p_dispwidget->msg_queue_regular_padding_x   = p_dispwidget->simple_widget_padding;

   p_dispwidget->msg_queue_task_rect_start_x      = p_dispwidget->msg_queue_rect_start_x - p_dispwidget->msg_queue_icon_size_x;

   p_dispwidget->msg_queue_task_text_start_x      = p_dispwidget->msg_queue_task_rect_start_x + p_dispwidget->msg_queue_height / 2;

   if (!p_dispwidget->gfx_widgets_icons_textures[MENU_WIDGETS_ICON_HOURGLASS])
      p_dispwidget->msg_queue_task_text_start_x         -= 
         p_dispwidget->gfx_widget_fonts.msg_queue.glyph_width * 2.0f;

   p_dispwidget->msg_queue_regular_text_start            = p_dispwidget->msg_queue_rect_start_x;

   p_dispwidget->msg_queue_task_hourglass_x              = p_dispwidget->msg_queue_rect_start_x - p_dispwidget->msg_queue_icon_size_x;

   p_dispwidget->generic_message_height    = p_dispwidget->gfx_widget_fonts.regular.line_height * 2.0f;

   p_dispwidget->msg_queue_default_rect_width_menu_alive = p_dispwidget
      ->gfx_widget_fonts.msg_queue.glyph_width * 40.0f;
   p_dispwidget->msg_queue_default_rect_width            = p_dispwidget->last_video_width 
      - p_dispwidget->msg_queue_regular_text_start - (2 * p_dispwidget->simple_widget_padding);

   p_dispwidget->divider_width_1px    = 1;
   if (p_dispwidget->last_scale_factor > 1.0f)
      p_dispwidget->divider_width_1px = (unsigned)(p_dispwidget->last_scale_factor + 0.5f);

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->layout)
         widget->layout(p_dispwidget,
               is_threaded, dir_assets, font_path);
   }
}


void gfx_widgets_iterate(
      void *data,
      void *data_disp,
      void *settings_data,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded)
{
   size_t i;
   dispgfx_widget_t *p_dispwidget   = (dispgfx_widget_t*)data;
   /* c.f. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323
    * On some platforms (e.g. 32-bit x86 without SSE),
    * gcc can produce inconsistent floating point results
    * depending upon optimisation level. This can break
    * floating point variable comparisons. A workaround is
    * to declare the affected variable as 'volatile', which
    * disables optimisations and removes excess precision
    * (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323#c87) */
   volatile float scale_factor      = 0.0f;
   gfx_display_t *p_disp            = (gfx_display_t*)data_disp;
   settings_t *settings             = (settings_t*)settings_data;
#ifdef HAVE_XMB
   enum menu_driver_id_type type    = p_disp->menu_driver_id;
   if (type == MENU_DRIVER_ID_XMB)
      scale_factor                  = gfx_display_get_widget_pixel_scale(p_disp, settings, width, height, fullscreen);
   else
#endif
      scale_factor                  = gfx_display_get_dpi_scale(
            p_disp,
            settings, width, height, fullscreen, true);

   /* Check whether screen dimensions or menu scale
    * factor have changed */
   if ((scale_factor != p_dispwidget->last_scale_factor) ||
       (width        != p_dispwidget->last_video_width) ||
       (height       != p_dispwidget->last_video_height))
   {
      p_dispwidget->last_scale_factor = scale_factor;
      p_dispwidget->last_video_width  = width;
      p_dispwidget->last_video_height = height;

      /* Note: We don't need a full context reset here
       * > Just rescale layout, and reset frame time counter */
      gfx_widgets_layout(p_disp, p_dispwidget,
            is_threaded, dir_assets, font_path);
      video_driver_monitor_reset();
   }

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->iterate)
         widget->iterate(p_dispwidget,
               width, height, fullscreen,
               dir_assets, font_path, is_threaded);
   }

   /* Messages queue */

   /* Consume one message if available */
   if ((FIFO_READ_AVAIL_NONPTR(p_dispwidget->msg_queue) > 0)
         && !p_dispwidget->widgets_moving 
         && (p_dispwidget->current_msgs_size < ARRAY_SIZE(p_dispwidget->current_msgs)))
   {
      disp_widget_msg_t *msg_widget = NULL;

      SLOCK_LOCK(p_dispwidget->current_msgs_lock);

      if (p_dispwidget->current_msgs_size < ARRAY_SIZE(p_dispwidget->current_msgs))
      {
         if (FIFO_READ_AVAIL_NONPTR(p_dispwidget->msg_queue) > 0)
            fifo_read(&p_dispwidget->msg_queue,
                  &msg_widget, sizeof(msg_widget));

         if (msg_widget)
         {
            /* Task messages always appear from the bottom of the screen, append it */
            if (p_dispwidget->msg_queue_tasks_count == 0 || msg_widget->task_ptr)
               p_dispwidget->current_msgs[p_dispwidget->current_msgs_size] = msg_widget;
            /* Regular messages are always above tasks, make room and insert it */
            else
            {
               unsigned idx = (unsigned)(p_dispwidget->current_msgs_size -
                  p_dispwidget->msg_queue_tasks_count);
               for (i = p_dispwidget->current_msgs_size; i > idx; i--)
                  p_dispwidget->current_msgs[i] = p_dispwidget->current_msgs[i - 1];
               p_dispwidget->current_msgs[idx] = msg_widget;
            }

            p_dispwidget->current_msgs_size++;
         }
      }

      SLOCK_UNLOCK(p_dispwidget->current_msgs_lock);

      if (msg_widget)
      {
         /* Start expiration timer if not associated to a task */
         if (!msg_widget->task_ptr)
         {
            if (!msg_widget->expiration_timer_started)
               gfx_widgets_start_msg_expiration_timer(
                  msg_widget, MSG_QUEUE_ANIMATION_DURATION * 2
                  + msg_widget->duration);
         }
         /* Else, start hourglass animation timer */
         else
         {
            p_dispwidget->msg_queue_tasks_count++;
            gfx_widgets_hourglass_end(msg_widget);
         }

         if (p_dispwidget->current_msgs_size != 0)
            gfx_widgets_msg_queue_move(p_dispwidget);
      }
   }

   /* Kill first expired message */
   /* Start expiration timer of dead tasks */
   for (i = 0; i < p_dispwidget->current_msgs_size; i++)
   {
      disp_widget_msg_t *msg_widget = p_dispwidget->current_msgs[i];

      if (!msg_widget)
         continue;

      if (msg_widget->task_ptr && (msg_widget->task_finished 
               || msg_widget->task_cancelled))
         if (!msg_widget->expiration_timer_started)
            gfx_widgets_start_msg_expiration_timer(msg_widget, TASK_FINISHED_DURATION);

      if (msg_widget->expired && !p_dispwidget->widgets_moving)
      {
         gfx_widgets_msg_queue_kill(p_dispwidget,
               (unsigned)i);
         break;
      }
   }
}

static int gfx_widgets_draw_indicator(
      dispgfx_widget_t *p_dispwidget,
      gfx_display_t            *p_disp,
      gfx_display_ctx_driver_t *dispctx,
      void *userdata, 
      unsigned video_width,
      unsigned video_height,
      uintptr_t icon, int y, int top_right_x_advance, 
      enum msg_hash_enums msg)
{
   unsigned width;

   gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);

   if (icon)
   {
      unsigned height = p_dispwidget->simple_widget_height * 2;
      width           = height;

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width, video_height,
            top_right_x_advance - width, y,
            width, height,
            video_width, video_height,
            p_dispwidget->backdrop_orig,
            NULL
      );

      gfx_display_set_alpha(p_dispwidget->pure_white, 1.0f);

      if (dispctx && dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      gfx_widgets_draw_icon(
            userdata,
            p_disp,
            video_width,
            video_height,
            width, height,
            icon, top_right_x_advance - width, y,
            0, 1,
            p_dispwidget->pure_white
            );
      if (dispctx && dispctx->blend_end)
         dispctx->blend_end(userdata);
   }
   else
   {
      unsigned height       = p_dispwidget->simple_widget_height;
      const char *txt       = msg_hash_to_str(msg);

      width = font_driver_get_message_width(p_dispwidget->gfx_widget_fonts.regular.font,
            txt,
            (unsigned)strlen(txt), 1) + p_dispwidget->simple_widget_padding * 2;

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width, video_height,
            top_right_x_advance - width, y,
            width, height,
            video_width, video_height,
            p_dispwidget->backdrop_orig,
            NULL
      );

      gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
            txt,
            top_right_x_advance - width 
            + p_dispwidget->simple_widget_padding,
            y + (height / 2.0f) + 
            p_dispwidget->gfx_widget_fonts.regular.line_centre_offset,
            video_width, video_height,
            0xFFFFFFFF, TEXT_ALIGN_LEFT,
            false);
   }

   return width;
}

static void gfx_widgets_draw_task_msg(
      dispgfx_widget_t *p_dispwidget,
      gfx_display_t            *p_disp,
      gfx_display_ctx_driver_t *dispctx,
      disp_widget_msg_t *msg,
      void *userdata,
      unsigned video_width,
      unsigned video_height)
{
   /* Color of first progress bar in a task message */
   static float msg_queue_task_progress_1[16]               = 
      COLOR_HEX_TO_FLOAT(0x397869, 1.0f);
   /* Color of second progress bar in a task message 
    * (for multiple tasks with same message) */
   static float msg_queue_task_progress_2[16]               = 
      COLOR_HEX_TO_FLOAT(0x317198, 1.0f);
   unsigned text_color;
   unsigned bar_width;

   unsigned rect_x;
   unsigned rect_y;
   unsigned rect_width;
   unsigned rect_height;
   float text_y_base;

   float *msg_queue_current_background;
   float *msg_queue_current_bar;

   bool draw_msg_new                 = false;
   unsigned task_percentage_offset   = 0;
   char task_percentage[256]         = {0};

   if (msg->msg_new)
      draw_msg_new                   = !string_is_equal(msg->msg_new, msg->msg);

   task_percentage_offset            = 
      p_dispwidget->gfx_widget_fonts.msg_queue.glyph_width 
      * (msg->task_error ? 12 : 5) 
      + p_dispwidget->simple_widget_padding * 1.25f; /*11 = STRLEN_CONST("Task failed") + 1 */

   if (msg->task_finished)
   {
      if (msg->task_error)
         strcpy_literal(task_percentage, "Task failed");
      else
         strcpy_literal(task_percentage, " ");
   }
   else if (msg->task_progress >= 0 && msg->task_progress <= 100)
      snprintf(task_percentage, sizeof(task_percentage),
            "%i%%", msg->task_progress);

   rect_width = p_dispwidget->simple_widget_padding 
      + msg->width 
      + task_percentage_offset;
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
         msg_queue_current_background = p_dispwidget->msg_queue_bg;
      else
         msg_queue_current_background = msg_queue_task_progress_1;

   rect_x      = p_dispwidget->msg_queue_rect_start_x - p_dispwidget->msg_queue_icon_size_x;
   rect_y      = video_height - msg->offset_y;
   rect_height = p_dispwidget->msg_queue_height / 2;

   gfx_display_set_alpha(msg_queue_current_background, msg->alpha);
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width, video_height,
         rect_x, rect_y,
         rect_width, rect_height,
         video_width, video_height,
         msg_queue_current_background,
	 NULL
         );

   /* Progress bar */
   if (!msg->task_finished && msg->task_progress >= 0 && msg->task_progress <= 100)
   {
      if (msg->task_count == 1)
         msg_queue_current_bar = msg_queue_task_progress_1;
      else
         msg_queue_current_bar = msg_queue_task_progress_2;

      gfx_display_set_alpha(msg_queue_current_bar, 1.0f);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width, video_height,
            p_dispwidget->msg_queue_task_rect_start_x, video_height - msg->offset_y,
            bar_width, rect_height,
            video_width, video_height,
            msg_queue_current_bar,
	    NULL
            );
   }

   /* Icon */
   gfx_display_set_alpha(p_dispwidget->pure_white, msg->alpha);
   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);
   gfx_widgets_draw_icon(
         userdata,
         p_disp,
         video_width,
         video_height,
         p_dispwidget->msg_queue_height / 2,
         p_dispwidget->msg_queue_height / 2,
         p_dispwidget->gfx_widgets_icons_textures[
         msg->task_finished 
         ? MENU_WIDGETS_ICON_CHECK 
         : MENU_WIDGETS_ICON_HOURGLASS],
         p_dispwidget->msg_queue_task_hourglass_x,
         video_height - msg->offset_y,
         msg->task_finished ? 0 : msg->hourglass_rotation,
         1,
         p_dispwidget->pure_white);
   if (dispctx && dispctx->blend_end)
      dispctx->blend_end(userdata);

   /* Text */
   text_y_base = video_height 
      - msg->offset_y 
      + p_dispwidget->msg_queue_height / 4.0f 
      + p_dispwidget->gfx_widget_fonts.msg_queue.line_centre_offset;

   if (draw_msg_new)
   {
      gfx_widgets_flush_text(video_width, video_height,
            &p_dispwidget->gfx_widget_fonts.msg_queue);

      if (p_disp->dispctx && p_disp->dispctx->scissor_begin)
         gfx_display_scissor_begin(p_disp,
               userdata,
               video_width, video_height,
               rect_x, rect_y, rect_width, rect_height);

      gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
            msg->msg_new,
            p_dispwidget->msg_queue_task_text_start_x,
            text_y_base 
            - p_dispwidget->msg_queue_height / 2.0f 
            + msg->msg_transition_animation,
            video_width, video_height,
            text_color,
            TEXT_ALIGN_LEFT,
            true);
   }

   gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
         msg->msg,
         p_dispwidget->msg_queue_task_text_start_x,
         text_y_base + msg->msg_transition_animation,
         video_width, video_height,
         text_color,
         TEXT_ALIGN_LEFT,
         true);

   if (draw_msg_new)
   {
      gfx_widgets_flush_text(video_width, video_height,
            &p_dispwidget->gfx_widget_fonts.msg_queue);
      if (dispctx && dispctx->scissor_end)
         dispctx->scissor_end(userdata,
               video_width, video_height);
   }

   /* Progress text */
   text_color = COLOR_TEXT_ALPHA(0xFFFFFF00, (unsigned)(msg->alpha/2*255.0f));
   gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
      task_percentage,
      p_dispwidget->msg_queue_rect_start_x - p_dispwidget->msg_queue_icon_size_x + rect_width - 
      p_dispwidget->gfx_widget_fonts.msg_queue.glyph_width,
      text_y_base,
      video_width, video_height,
      text_color,
      TEXT_ALIGN_RIGHT,
      true);
}

static void gfx_widgets_draw_regular_msg(
      dispgfx_widget_t *p_dispwidget,
      gfx_display_t *p_disp,
      gfx_display_ctx_driver_t *dispctx,
      disp_widget_msg_t *msg,
      void *userdata,
      unsigned video_width,
      unsigned video_height)
{
   static float msg_queue_info[16]                          =
      COLOR_HEX_TO_FLOAT(0x12ACF8, 1.0f);
   unsigned bar_width;
   unsigned bar_margin;
   unsigned text_color;
   static float last_alpha = 0.0f;

   msg->unfolding          = false;
   msg->unfolded           = true;

   if (last_alpha != msg->alpha)
   {
      /* Icon */
      gfx_display_set_alpha(msg_queue_info, msg->alpha);
      gfx_display_set_alpha(p_dispwidget->pure_white, msg->alpha);
      gfx_display_set_alpha(p_dispwidget->msg_queue_bg, msg->alpha);
      last_alpha = msg->alpha;
   }

   if (!msg->unfolded || msg->unfolding)
   {
      gfx_widgets_flush_text(video_width, video_height,
            &p_dispwidget->gfx_widget_fonts.regular);
      gfx_widgets_flush_text(video_width, video_height,
            &p_dispwidget->gfx_widget_fonts.bold);
      gfx_widgets_flush_text(video_width, video_height,
            &p_dispwidget->gfx_widget_fonts.msg_queue);

      if (p_disp->dispctx && p_disp->dispctx->scissor_begin)
         gfx_display_scissor_begin(p_disp,
               userdata,
               video_width, video_height,
               p_dispwidget->msg_queue_scissor_start_x, 0,
               (p_dispwidget->msg_queue_scissor_start_x + msg->width - 
                p_dispwidget->simple_widget_padding * 2) 
               * msg->unfold, video_height);
   }

   /* Background */
   bar_width  = p_dispwidget->simple_widget_padding + msg->width + p_dispwidget->msg_queue_icon_size_x;
   bar_margin = 4;

   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         p_dispwidget->msg_queue_rect_start_x - p_dispwidget->msg_queue_icon_size_x + bar_margin,
         video_height - msg->offset_y,
         bar_width - bar_margin,
         p_dispwidget->msg_queue_height,
         video_width,
         video_height,
         p_dispwidget->msg_queue_bg,
	 NULL
         );

   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         p_dispwidget->msg_queue_rect_start_x - p_dispwidget->msg_queue_icon_size_x,
         video_height - msg->offset_y,
         bar_margin,
         p_dispwidget->msg_queue_height,
         video_width,
         video_height,
         p_dispwidget->pure_white,
         NULL
         );

   /* Text */
   text_color = COLOR_TEXT_ALPHA(0xFFFFFF00, (unsigned)(msg->alpha*255.0f));

   gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
      msg->msg,
      p_dispwidget->msg_queue_regular_text_start,
      video_height - msg->offset_y + (p_dispwidget->msg_queue_height - msg->text_height)/2.0f + p_dispwidget->gfx_widget_fonts.msg_queue.line_ascender,
      video_width, video_height,
      text_color,
      TEXT_ALIGN_LEFT,
      true);

   if (!msg->unfolded || msg->unfolding)
   {
      gfx_widgets_flush_text(video_width, video_height, &p_dispwidget->gfx_widget_fonts.regular);
      gfx_widgets_flush_text(video_width, video_height, &p_dispwidget->gfx_widget_fonts.bold);
      gfx_widgets_flush_text(video_width, video_height, &p_dispwidget->gfx_widget_fonts.msg_queue);

      if (dispctx && dispctx->scissor_end)
         dispctx->scissor_end(userdata,
               video_width, video_height);
   }

   if (p_dispwidget->msg_queue_has_icons)
   {
      if (dispctx && dispctx->blend_begin)
         dispctx->blend_begin(userdata);

      gfx_widgets_draw_icon(
            userdata,
            p_disp,
            video_width,
            video_height,
            p_dispwidget->msg_queue_icon_size_x,
            p_dispwidget->msg_queue_icon_size_y,
            p_dispwidget->gfx_widgets_icons_textures[MENU_WIDGETS_ICON_INFO],
            p_dispwidget->msg_queue_spacing,
            video_height - msg->offset_y  - p_dispwidget->msg_queue_icon_offset_y,
            0,
            1,
            msg_queue_info);

      if (dispctx && dispctx->blend_end)
         dispctx->blend_end(userdata);
   }
}

static void INLINE gfx_widgets_font_bind(gfx_widget_font_data_t *font_data)
{
   font_driver_bind_block(font_data->font, &font_data->raster_block);
   font_data->raster_block.carr.coords.vertices = 0;
   font_data->usage_count                       = 0;
}

static void INLINE gfx_widgets_font_unbind(gfx_widget_font_data_t *font_data)
{
   font_driver_bind_block(font_data->font, NULL);
}

void gfx_widgets_frame(void *data)
{
   size_t i;
   video_frame_info_t *video_info   = (video_frame_info_t*)data;
   gfx_display_t            *p_disp = (gfx_display_t*)video_info->disp_userdata;
   gfx_display_ctx_driver_t *dispctx= p_disp->dispctx;
   dispgfx_widget_t *p_dispwidget   = (dispgfx_widget_t*)video_info->widgets_userdata;
   bool framecount_show             = video_info->framecount_show;
   bool memory_show                 = video_info->memory_show;
   bool core_status_msg_show        = video_info->core_status_msg_show;
   void *userdata                   = video_info->userdata;
   unsigned video_width             = video_info->width;
   unsigned video_height            = video_info->height;
   bool widgets_is_paused           = video_info->widgets_is_paused;
   bool fps_show                    = video_info->fps_show;
   bool widgets_is_fastforwarding   = video_info->widgets_is_fast_forwarding;
   bool widgets_is_rewinding        = video_info->widgets_is_rewinding;
   bool runloop_is_slowmotion       = video_info->runloop_is_slowmotion;
   bool menu_screensaver_active     = video_info->menu_screensaver_active;
   int top_right_x_advance          = video_width;

   p_dispwidget->gfx_widgets_frame_count++;

   /* If menu screensaver is active, draw nothing */
   if (menu_screensaver_active)
      return;

   video_driver_set_viewport(video_width, video_height, true, false);

   /* Font setup */
   gfx_widgets_font_bind(&p_dispwidget->gfx_widget_fonts.regular);
   gfx_widgets_font_bind(&p_dispwidget->gfx_widget_fonts.bold);
   gfx_widgets_font_bind(&p_dispwidget->gfx_widget_fonts.msg_queue);

#ifdef HAVE_TRANSLATE
   /* AI Service overlay */
   if (p_dispwidget->ai_service_overlay_state > 0)
   {
      float outline_color[16] = {
      0.00, 1.00, 0.00, 1.00,
      0.00, 1.00, 0.00, 1.00,
      0.00, 1.00, 0.00, 1.00,
      0.00, 1.00, 0.00, 1.00,
      };
      gfx_display_set_alpha(p_dispwidget->pure_white, 1.0f);

      if (p_dispwidget->ai_service_overlay_texture)
      {
         if (dispctx->blend_begin)
            dispctx->blend_begin(userdata);
         gfx_widgets_draw_icon(
               userdata,
               p_disp,
               video_width,
               video_height,
               video_width,
               video_height,
               p_dispwidget->ai_service_overlay_texture,
               0,
               0,
               0,
               1,
               p_dispwidget->pure_white
               );
         if (dispctx->blend_end)
            dispctx->blend_end(userdata);
      }

      /* top line */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width, video_height,
            0, 0,
            video_width,
            p_dispwidget->divider_width_1px,
            video_width,
            video_height,
            outline_color,
	    NULL
            );
      /* bottom line */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width, video_height,
            0,
            video_height - p_dispwidget->divider_width_1px,
            video_width,
            p_dispwidget->divider_width_1px,
            video_width,
            video_height,
            outline_color,
	    NULL
            );
      /* left line */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0,
            0,
            p_dispwidget->divider_width_1px,
            video_height,
            video_width,
            video_height,
            outline_color,
	    NULL
            );
      /* right line */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width, video_height,
            video_width - p_dispwidget->divider_width_1px,
            0,
            p_dispwidget->divider_width_1px,
            video_height,
            video_width,
            video_height,
            outline_color,
	    NULL
            );

      if (p_dispwidget->ai_service_overlay_state == 2)
          p_dispwidget->ai_service_overlay_state = 3;
   }
#endif

   /* Status Text (fps, framecount, memory, core status message) */
   if (     fps_show
         || framecount_show
         || memory_show
         || core_status_msg_show
         )
   {
      const char *text      = *p_dispwidget->gfx_widgets_status_text == '\0'
         ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)
         : p_dispwidget->gfx_widgets_status_text;

      int text_width        = font_driver_get_message_width(
            p_dispwidget->gfx_widget_fonts.regular.font,
            text,
            (unsigned)strlen(text), 1.0f);
      int total_width       = text_width
         + p_dispwidget->simple_widget_padding * 2;

      int status_text_x     = top_right_x_advance
         - p_dispwidget->simple_widget_padding - text_width;
      /* Ensure that left hand side of text does
       * not bleed off the edge of the screen */
      status_text_x         = (status_text_x < 0) ? 0 : status_text_x;

      gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            top_right_x_advance - total_width, 0,
            total_width,
            p_dispwidget->simple_widget_height,
            video_width,
            video_height,
            p_dispwidget->backdrop_orig,
	    NULL
            );

      gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
            text,
            status_text_x,
            p_dispwidget->simple_widget_height / 2.0f
            + p_dispwidget->gfx_widget_fonts.regular.line_centre_offset,
            video_width, video_height,
            0xFFFFFFFF,
            TEXT_ALIGN_LEFT,
            true);
   }

   /* Indicators */
   if (widgets_is_paused)
      top_right_x_advance -= gfx_widgets_draw_indicator(
            p_dispwidget,
            p_disp,
            dispctx,
            userdata,
            video_width,
            video_height,
            p_dispwidget->gfx_widgets_icons_textures[
            MENU_WIDGETS_ICON_PAUSED],
            (fps_show 
             ? p_dispwidget->simple_widget_height 
             : 0),
            top_right_x_advance,
            MSG_PAUSED);

   if (widgets_is_fastforwarding)
      top_right_x_advance -= gfx_widgets_draw_indicator(
            p_dispwidget,
            p_disp,
            dispctx,
            userdata,
            video_width,
            video_height,
            p_dispwidget->gfx_widgets_icons_textures[
            MENU_WIDGETS_ICON_FAST_FORWARD],
            (fps_show ? p_dispwidget->simple_widget_height : 0),
            top_right_x_advance,
            MSG_FAST_FORWARD);

   if (widgets_is_rewinding)
      top_right_x_advance -= gfx_widgets_draw_indicator(
            p_dispwidget,
            p_disp,
            dispctx,
            userdata,
            video_width,
            video_height,
            p_dispwidget->gfx_widgets_icons_textures[
            MENU_WIDGETS_ICON_REWIND],
            (fps_show ? p_dispwidget->simple_widget_height : 0),
            top_right_x_advance,
            MSG_REWINDING);

   if (runloop_is_slowmotion)
   {
      top_right_x_advance -= gfx_widgets_draw_indicator(
            p_dispwidget,
            p_disp,
            dispctx,
            userdata,
            video_width,
            video_height,
            p_dispwidget->gfx_widgets_icons_textures[
            MENU_WIDGETS_ICON_SLOW_MOTION],
            (fps_show ? p_dispwidget->simple_widget_height : 0),
            top_right_x_advance,
            MSG_SLOW_MOTION);
      (void)top_right_x_advance;
   }

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->frame)
         widget->frame(data, p_dispwidget);
   }

   /* Draw all messages */
   if (p_dispwidget->current_msgs_size)
   {
      SLOCK_LOCK(p_dispwidget->current_msgs_lock);

      for (i = 0; i < p_dispwidget->current_msgs_size; i++)
      {
         disp_widget_msg_t* msg = p_dispwidget->current_msgs[i];

         if (!msg)
            continue;

         if (msg->task_ptr)
            gfx_widgets_draw_task_msg(
               p_dispwidget,
               p_disp,
               dispctx,
               msg, userdata,
               video_width, video_height);
         else
            gfx_widgets_draw_regular_msg(
               p_dispwidget,
               p_disp,
               dispctx,
               msg, userdata,
               video_width, video_height);
      }

      SLOCK_UNLOCK(p_dispwidget->current_msgs_lock);
   }

   /* Ensure all text is flushed */
   gfx_widgets_flush_text(video_width, video_height,
         &p_dispwidget->gfx_widget_fonts.regular);
   gfx_widgets_flush_text(video_width, video_height,
         &p_dispwidget->gfx_widget_fonts.bold);
   gfx_widgets_flush_text(video_width, video_height,
         &p_dispwidget->gfx_widget_fonts.msg_queue);

   /* Unbind fonts */
   gfx_widgets_font_unbind(&p_dispwidget->gfx_widget_fonts.regular);
   gfx_widgets_font_unbind(&p_dispwidget->gfx_widget_fonts.bold);
   gfx_widgets_font_unbind(&p_dispwidget->gfx_widget_fonts.msg_queue);

   video_driver_set_viewport(video_width, video_height, false, true);
}

static void gfx_widgets_free(dispgfx_widget_t *p_dispwidget)
{
   size_t i;

   p_dispwidget->widgets_inited     = false;

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->free)
         widget->free();
   }

   /* Kill all running animations */
   gfx_animation_kill_by_tag(
         &p_dispwidget->gfx_widgets_generic_tag);

   /* Purge everything from the fifo */
   while (FIFO_READ_AVAIL_NONPTR(p_dispwidget->msg_queue) > 0)
   {
      disp_widget_msg_t *msg_widget;

      fifo_read(&p_dispwidget->msg_queue,
            &msg_widget, sizeof(msg_widget));

      /* Note: gfx_widgets_free() is only called when
       * main_exit() is invoked. At this stage, we cannot
       * guarantee that any task pointers are valid (the
       * task may have been free()'d, but we can't know
       * that here) - so all we can do is unset the task
       * pointer associated with each message
       * > If we don't do this, gfx_widgets_msg_queue_free()
       *   will generate heap-use-after-free errors */
      msg_widget->task_ptr = NULL;

      gfx_widgets_msg_queue_free(p_dispwidget, msg_widget);
      free(msg_widget);
   }

   fifo_deinitialize(&p_dispwidget->msg_queue);

   /* Purge everything from the list */
   SLOCK_LOCK(p_dispwidget->current_msgs_lock);

   p_dispwidget->current_msgs_size = 0;
   for (i = 0; i < ARRAY_SIZE(p_dispwidget->current_msgs); i++)
   {
      disp_widget_msg_t *msg = p_dispwidget->current_msgs[i];
      if (!msg)
         continue;

      /* Note: gfx_widgets_free() is only called when
         * main_exit() is invoked. At this stage, we cannot
         * guarantee that any task pointers are valid (the
         * task may have been free()'d, but we can't know
         * that here) - so all we can do is unset the task
         * pointer associated with each message
         * > If we don't do this, gfx_widgets_msg_queue_free()
         *   will generate heap-use-after-free errors */
      msg->task_ptr = NULL;

      gfx_widgets_msg_queue_free(p_dispwidget, msg);
   }
   SLOCK_UNLOCK(p_dispwidget->current_msgs_lock);

#ifdef HAVE_THREADS
   slock_free(p_dispwidget->current_msgs_lock);
   p_dispwidget->current_msgs_lock = NULL;
#endif

   p_dispwidget->msg_queue_tasks_count = 0;

   /* Font */
   video_coord_array_free(
         &p_dispwidget->gfx_widget_fonts.regular.raster_block.carr);
   video_coord_array_free(
         &p_dispwidget->gfx_widget_fonts.bold.raster_block.carr);
   video_coord_array_free(
         &p_dispwidget->gfx_widget_fonts.msg_queue.raster_block.carr);

   font_driver_bind_block(NULL, NULL);
}

static void gfx_widgets_context_reset(
      dispgfx_widget_t *p_dispwidget,
      gfx_display_t *p_disp,
      settings_t *settings,
      bool is_threaded,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path)
{
   size_t i;
   char xmb_path[PATH_MAX_LENGTH];
   char monochrome_png_path[PATH_MAX_LENGTH];
   char gfx_widgets_path[PATH_MAX_LENGTH];
   char theme_path[PATH_MAX_LENGTH];

   xmb_path[0]            = '\0';
   monochrome_png_path[0] = '\0';
   gfx_widgets_path[0]    = '\0';
   theme_path[0]          = '\0';

   /* Textures paths */
   fill_pathname_join(
      gfx_widgets_path,
      dir_assets,
      "menu_widgets",
      sizeof(gfx_widgets_path)
   );

   fill_pathname_join(
      xmb_path,
      dir_assets,
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
      gfx_display_reset_textures_list(
            gfx_widgets_icons_names[i],
            monochrome_png_path,
            &p_dispwidget->gfx_widgets_icons_textures[i],
            TEXTURE_FILTER_MIPMAP_LINEAR,
            NULL,
            NULL);
   }

   /* Message queue */
   gfx_display_reset_textures_list(
         "msg_queue_icon.png",
         gfx_widgets_path,
         &p_dispwidget->msg_queue_icon,
         TEXTURE_FILTER_LINEAR,
         NULL,
         NULL);
   gfx_display_reset_textures_list(
         "msg_queue_icon_outline.png",
         gfx_widgets_path,
         &p_dispwidget->msg_queue_icon_outline,
         TEXTURE_FILTER_LINEAR,
         NULL,
         NULL);
   gfx_display_reset_textures_list(
         "msg_queue_icon_rect.png",
         gfx_widgets_path,
         &p_dispwidget->msg_queue_icon_rect,
         TEXTURE_FILTER_NEAREST,
         NULL,
         NULL);

   p_dispwidget->msg_queue_has_icons = 
      p_dispwidget->msg_queue_icon         && 
      p_dispwidget->msg_queue_icon_outline &&
      p_dispwidget->msg_queue_icon_rect;

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->context_reset)
         widget->context_reset(is_threaded, width, height,
               fullscreen, dir_assets, font_path,
               monochrome_png_path, gfx_widgets_path);
   }

   /* Update scaling/dimensions */
   p_dispwidget->last_video_width     = width;
   p_dispwidget->last_video_height    = height;
#ifdef HAVE_XMB
   if (p_disp->menu_driver_id == MENU_DRIVER_ID_XMB)
      p_dispwidget->last_scale_factor = gfx_display_get_widget_pixel_scale(
            p_disp, settings,
            p_dispwidget->last_video_width,
            p_dispwidget->last_video_height, fullscreen);
   else
#endif
      p_dispwidget->last_scale_factor = gfx_display_get_dpi_scale(
                     p_disp, settings,
                     p_dispwidget->last_video_width,
                     p_dispwidget->last_video_height,
                     fullscreen, true);

   gfx_widgets_layout(p_disp, p_dispwidget,
         is_threaded, dir_assets, font_path);
   video_driver_monitor_reset();
}

bool gfx_widgets_init(
      void *data,
      void *data_disp,
      void *data_anim,
      void *settings_data,
      uintptr_t widgets_active_ptr,
      bool video_is_threaded,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path)
{
   unsigned i;
   unsigned color                              = 0x222222;
   dispgfx_widget_t *p_dispwidget              = (dispgfx_widget_t*)data;
   gfx_display_t *p_disp                       = (gfx_display_t*)data_disp;
   gfx_animation_t *p_anim                     = (gfx_animation_t*)data_anim;
   settings_t *settings                        = (settings_t*)settings_data;
   p_dispwidget->divider_width_1px             = 1;
   p_dispwidget->gfx_widgets_generic_tag       = (uintptr_t)widgets_active_ptr;

   if (!gfx_display_init_first_driver(p_disp, video_is_threaded))
      goto error;
   gfx_display_set_alpha(p_dispwidget->backdrop_orig, 0.75f);
   for (i = 0; i < 16; i++)
      p_dispwidget->pure_white[i] = 1.00f;

   p_dispwidget->msg_queue_bg[0]  = HEX_R(color);
   p_dispwidget->msg_queue_bg[1]  = HEX_G(color);
   p_dispwidget->msg_queue_bg[2]  = HEX_B(color);
   p_dispwidget->msg_queue_bg[3]  = 1.0f;
   p_dispwidget->msg_queue_bg[4]  = HEX_R(color);
   p_dispwidget->msg_queue_bg[5]  = HEX_G(color);
   p_dispwidget->msg_queue_bg[6]  = HEX_B(color);
   p_dispwidget->msg_queue_bg[7]  = 1.0f;
   p_dispwidget->msg_queue_bg[8]  = HEX_R(color);
   p_dispwidget->msg_queue_bg[9]  = HEX_G(color);
   p_dispwidget->msg_queue_bg[10] = HEX_B(color);
   p_dispwidget->msg_queue_bg[11] = 1.0f;
   p_dispwidget->msg_queue_bg[12] = HEX_R(color);
   p_dispwidget->msg_queue_bg[13] = HEX_G(color);
   p_dispwidget->msg_queue_bg[14] = HEX_B(color);
   p_dispwidget->msg_queue_bg[15] = 1.0f;

   if (!p_dispwidget->widgets_inited)
   {
      size_t i;

      p_dispwidget->gfx_widgets_frame_count = 0;

      for (i = 0; i < ARRAY_SIZE(widgets); i++)
      {
         const gfx_widget_t* widget = widgets[i];

         if (widget->init)
            widget->init(p_disp, p_anim, video_is_threaded, fullscreen);
      }

      if (!fifo_initialize(&p_dispwidget->msg_queue,
            MSG_QUEUE_PENDING_MAX * sizeof(disp_widget_msg_t*)))
         goto error;

      memset(&p_dispwidget->current_msgs[0], 0, sizeof(p_dispwidget->current_msgs));
      p_dispwidget->current_msgs_size = 0;

#ifdef HAVE_THREADS
      p_dispwidget->current_msgs_lock = slock_new();
#endif

      p_dispwidget->widgets_inited = true;
   }

   gfx_widgets_context_reset(
         p_dispwidget,
         p_disp,
         settings,
         video_is_threaded,
         width, height, fullscreen,
         dir_assets, font_path);

   return true;

error:
   gfx_widgets_free(p_dispwidget);
   return false;
}

static void gfx_widgets_context_destroy(dispgfx_widget_t *p_dispwidget)
{
   size_t i;

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->context_destroy)
         widget->context_destroy();
   }

   /* TODO: Dismiss onscreen notifications that have been freed */

   /* Textures */
   for (i = 0; i < MENU_WIDGETS_ICON_LAST; i++)
      video_driver_texture_unload(&p_dispwidget->gfx_widgets_icons_textures[i]);

   video_driver_texture_unload(&p_dispwidget->msg_queue_icon);
   video_driver_texture_unload(&p_dispwidget->msg_queue_icon_outline);
   video_driver_texture_unload(&p_dispwidget->msg_queue_icon_rect);

   p_dispwidget->msg_queue_icon         = 0;
   p_dispwidget->msg_queue_icon_outline = 0;
   p_dispwidget->msg_queue_icon_rect    = 0;

   /* Fonts */
   gfx_widgets_font_free(&p_dispwidget->gfx_widget_fonts.regular);
   gfx_widgets_font_free(&p_dispwidget->gfx_widget_fonts.bold);
   gfx_widgets_font_free(&p_dispwidget->gfx_widget_fonts.msg_queue);
}


void gfx_widgets_deinit(void *data, bool widgets_persisting)
{
   dispgfx_widget_t *p_dispwidget = (dispgfx_widget_t*)data;

   gfx_widgets_context_destroy(p_dispwidget);

   if (!widgets_persisting)
      gfx_widgets_free(p_dispwidget);
}

#ifdef HAVE_TRANSLATE
/* NOTE: Reads image from memory buffer */
static bool gfx_widgets_reset_textures_list_buffer(
        uintptr_t *item, enum texture_filter_type filter_type,
        void* buffer, unsigned buffer_len, enum image_type_enum image_type,
        unsigned *width, unsigned *height)
{
   struct texture_image ti;

   ti.width                      = 0;
   ti.height                     = 0;
   ti.pixels                     = NULL;
   ti.supports_rgba              = video_driver_supports_rgba();

   if (!image_texture_load_buffer(&ti, image_type, buffer, buffer_len))
      return false;

   if (width)
      *width = ti.width;

   if (height)
      *height = ti.height;

   /* if the poke interface doesn't support texture load then return false */  
   if (!video_driver_texture_load(&ti, filter_type, item))
       return false;
   image_texture_free(&ti);
   return true;
}

bool gfx_widgets_ai_service_overlay_load(
      dispgfx_widget_t *p_dispwidget,
      char* buffer, unsigned buffer_len,
      enum image_type_enum image_type)
{
   if (p_dispwidget->ai_service_overlay_state == 0)
   {
      if (!gfx_widgets_reset_textures_list_buffer(
               &p_dispwidget->ai_service_overlay_texture, 
               TEXTURE_FILTER_MIPMAP_LINEAR, 
               (void *) buffer, buffer_len, image_type,
               &p_dispwidget->ai_service_overlay_width,
               &p_dispwidget->ai_service_overlay_height))
         return false;
      p_dispwidget->ai_service_overlay_state = 1;
   }
   return true;
}

void gfx_widgets_ai_service_overlay_unload(dispgfx_widget_t *p_dispwidget)
{
   if (p_dispwidget->ai_service_overlay_state == 1)
   {
      video_driver_texture_unload(&p_dispwidget->ai_service_overlay_texture);
      p_dispwidget->ai_service_overlay_texture = 0;
      p_dispwidget->ai_service_overlay_state   = 0;
   }
}
#endif
