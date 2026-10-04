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

#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <retro_inline.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <retro_math.h>
#include <features/features_cpu.h>

#include <compat/strl.h>

#include "gfx_display.h"
#include "gfx_widgets.h"
#include "font_driver.h"
#ifdef HAVE_THREADS
#include "video_thread_wrapper.h"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_defines.h"
#endif

#include "../configuration.h"
#include "../file_path_special.h"
#include "../msg_hash.h"

#include "../tasks/task_content.h"
#include "../tasks/tasks_internal.h"
#include "gfx_surface.h"

#define BASE_FONT_SIZE      32.0f
#define MSG_QUEUE_FONT_SIZE 20.0f

static dispgfx_widget_t dispwidget_st = {0};
static uint64_t widget_icon_load_gen  = 0;

/* Set by gfx_widgets_reload_assets() on the thread that asks; taken by
 * the next layout pass on the thread that drives the widgets, which
 * rebuilds every font even where path and size are unchanged - the
 * file behind the path is what changed. */
static retro_atomic_int_t widget_fonts_reload = RETRO_ATOMIC_INT_INITIALIZER(0);
static bool               widget_fonts_force  = false;

/* Recompute the layout variables that depend on whether widget icons
 * are loaded.  Called from gfx_widgets_layout() during context_reset,
 * and again from the async icon load callback when icons arrive. */
static void gfx_widgets_update_icon_layout(dispgfx_widget_t *p_dispwidget)
{
   bool has_icons = !!(p_dispwidget->flags & DISPGFX_WIDGET_FLAG_MSG_QUEUE_HAS_ICONS);

   if (has_icons)
   {
      p_dispwidget->msg_queue_icon_size_y         = p_dispwidget->msg_queue_height;
      p_dispwidget->msg_queue_icon_size_x         = p_dispwidget->msg_queue_icon_size_y;
   }
   else
   {
      p_dispwidget->msg_queue_icon_size_x         = p_dispwidget->msg_queue_height / 4.0f;
      p_dispwidget->msg_queue_icon_size_y         = p_dispwidget->msg_queue_icon_size_x;
   }

   p_dispwidget->msg_queue_internal_icon_size     = p_dispwidget->msg_queue_icon_size_y;
   p_dispwidget->msg_queue_icon_offset_y          = (p_dispwidget->msg_queue_icon_size_y - p_dispwidget->msg_queue_height) / 2;
   p_dispwidget->msg_queue_scissor_start_x        = p_dispwidget->msg_queue_spacing + p_dispwidget->msg_queue_icon_size_x - (p_dispwidget->msg_queue_icon_size_x * 0.28928571428f);

   p_dispwidget->msg_queue_regular_text_start     = p_dispwidget->msg_queue_rect_start_x + p_dispwidget->msg_queue_icon_size_x + (p_dispwidget->simple_widget_padding / 2.5f);
   p_dispwidget->msg_queue_task_text_start_x      = p_dispwidget->msg_queue_rect_start_x + (p_dispwidget->msg_queue_height / 2.0f) + (p_dispwidget->simple_widget_padding / 2.0f);

   if (!p_dispwidget->gfx_widgets_icons_textures[MENU_WIDGETS_ICON_HOURGLASS])
      p_dispwidget->msg_queue_task_text_start_x  -= p_dispwidget->gfx_widget_fonts.msg_queue.glyph_width * 2.0f;

   p_dispwidget->msg_queue_default_rect_width     = VIDEO_SCALE_W(p_dispwidget->last_video_dims)
         - p_dispwidget->msg_queue_regular_text_start - (2 * p_dispwidget->simple_widget_padding);
}

static void gfx_widgets_font_compute_metrics(
      gfx_widget_font_data_t *font_data);

/* Widgets list */
static const gfx_widget_t* const widgets[] = {
#ifdef HAVE_NETWORKING
   &gfx_widget_netplay_chat,
   &gfx_widget_netplay_ping,
#endif
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
      unsigned dims, bool fullscreen)
{
   unsigned width                                      = VIDEO_SCALE_W(dims);
   unsigned height                                     = VIDEO_SCALE_H(dims);
   static unsigned last_dims                           = 0;
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
   float menu_widget_scale_factor                      = fullscreen
         ? menu_widget_scale_factor_fullscreen
         : menu_widget_scale_factor_windowed;
#endif
   float menu_scale_factor                             = menu_widget_scale_factor;

   if (gfx_widget_scale_auto)
      menu_scale_factor                                = settings->floats.menu_scale_factor;

   /* We need to perform a square root here, which
    * can be slow on some platforms (not *slow*, but
    * it involves enough work that it's worth trying
    * to optimise). We therefore cache the pixel scale,
    * and only update on first run or when the video
    * size changes */
   if (!scale_cached || dims != last_dims)
   {
      /* Baseline reference is a 1080p display */
      scale = (float)(
            sqrt((double)((width * width) + (height * height))) /
            DIAGONAL_PIXELS_1080P);

      scale_cached  = true;
      scale_updated = true;
      last_dims     = dims;
   }

   /* Adjusted scale calculation may also be slow, so
    * only update if something changes */
   if (    scale_updated
       || (menu_scale_factor      != last_menu_scale_factor)
       || (p_disp->menu_driver_id != last_menu_driver_id))
   {
      adjusted_scale         = scale * menu_scale_factor;
      adjusted_scale         = (adjusted_scale > 0.0001f) ? adjusted_scale : 1.0f;
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
   {
      msg->msg     = msg->msg_new;
      msg->msg_new = NULL;
   }

   msg->msg_transition_animation = 0.0f;
}

/* Forward declaration: msg_queue_free is defined further below
 * (around line 550) but is needed by msg_queue_push for the
 * race-loss rollback path added in the producer-locking fix. */
static void gfx_widgets_msg_queue_free(
      dispgfx_widget_t *p_dispwidget,
      disp_widget_msg_t *msg);

/* The pending ring: one producer (the main thread), one consumer (the
 * widgets' owner), release/acquire on the two cursors, no lock.  Each
 * side reads its own cursor relaxed and the other's with an acquire
 * load, which is what carries the slot pointer across. */
static bool gfx_widgets_pending_push(dispgfx_widget_t *p_dispwidget,
      disp_widget_msg_t *msg_widget)
{
   int tail = retro_atomic_load_relaxed_int(&p_dispwidget->msg_queue_tail);
   int head = retro_atomic_load_acquire_int(&p_dispwidget->msg_queue_head);
   if (tail - head >= MSG_QUEUE_PENDING_MAX)
      return false;
   p_dispwidget->msg_queue[(unsigned)tail & (MSG_QUEUE_PENDING_MAX - 1)]
         = msg_widget;
   retro_atomic_store_release_int(&p_dispwidget->msg_queue_tail, tail + 1);
   return true;
}

static disp_widget_msg_t *gfx_widgets_pending_pop(dispgfx_widget_t *p_dispwidget)
{
   disp_widget_msg_t *msg_widget;
   int head = retro_atomic_load_relaxed_int(&p_dispwidget->msg_queue_head);
   int tail = retro_atomic_load_acquire_int(&p_dispwidget->msg_queue_tail);
   if (head == tail)
      return NULL;
   msg_widget = p_dispwidget->msg_queue[
         (unsigned)head & (MSG_QUEUE_PENDING_MAX - 1)];
   retro_atomic_store_release_int(&p_dispwidget->msg_queue_head, head + 1);
   return msg_widget;
}

static disp_widget_msg_t *gfx_widgets_pending_peek(dispgfx_widget_t *p_dispwidget)
{
   int head = retro_atomic_load_relaxed_int(&p_dispwidget->msg_queue_head);
   int tail = retro_atomic_load_acquire_int(&p_dispwidget->msg_queue_tail);
   if (head == tail)
      return NULL;
   return p_dispwidget->msg_queue[(unsigned)head & (MSG_QUEUE_PENDING_MAX - 1)];
}

/* A task's update on its way to the widgets' owner. The main thread
 * never touches a task's widget: it sends what the task looks like
 * now, and the owner finds the widget by the key the task carries in
 * frontend_userdata - a number, handed on when one task passes its
 * widget to another, and safe to hold after the widget or the task is
 * gone. */
typedef struct gfx_widgets_task_cmd
{
   struct gfx_widgets_task_cmd *next;
   char     *title;
   char     *error;
   uintptr_t key;
   uint32_t  ident;
   uint32_t  seq;
   unsigned  duration;
   uint8_t   category;
   uint8_t   task_flags;
   uint8_t   style;
   int8_t    progress;
   bool      rebind;      /* a transfer: the new owner and nothing else */
} gfx_widgets_task_cmd_t;

/* Progress is sent every frame and may be dropped past this many
 * updates in flight; a finished task's last update never is. */
#define TASK_CMDS_SOFT_MAX 64

static void gfx_widgets_task_cmd_push(dispgfx_widget_t *p_dispwidget,
      gfx_widgets_task_cmd_t *cmd)
{
   retro_atomic_fetch_add_int(&p_dispwidget->task_cmds_count, 1);
#ifdef RETRO_ATOMIC_HAS_PTR
   {
      void *head;
      do
      {
         head      = retro_atomic_load_acquire_ptr(&p_dispwidget->task_cmds);
         cmd->next = (gfx_widgets_task_cmd_t*)head;
      } while (!retro_atomic_cas_ptr(&p_dispwidget->task_cmds, head, cmd));
   }
#else
   gfx_widgets_state_lock();
   cmd->next               = (gfx_widgets_task_cmd_t*)p_dispwidget->task_cmds;
   p_dispwidget->task_cmds = cmd;
   gfx_widgets_state_unlock();
#endif
}

/* Everything sent since the last call, oldest first. */
static gfx_widgets_task_cmd_t *gfx_widgets_task_cmds_take(
      dispgfx_widget_t *p_dispwidget)
{
   gfx_widgets_task_cmd_t *cmd, *list = NULL;
#ifdef RETRO_ATOMIC_HAS_PTR
   if (!retro_atomic_load_relaxed_ptr(&p_dispwidget->task_cmds))
      return NULL;
   cmd = (gfx_widgets_task_cmd_t*)
      retro_atomic_exchange_ptr(&p_dispwidget->task_cmds, NULL);
#else
   gfx_widgets_state_lock();
   cmd                     = (gfx_widgets_task_cmd_t*)p_dispwidget->task_cmds;
   p_dispwidget->task_cmds = NULL;
   gfx_widgets_state_unlock();
#endif
   while (cmd)
   {
      gfx_widgets_task_cmd_t *next = cmd->next;
      cmd->next                    = list;
      list                         = cmd;
      cmd                          = next;
   }
   return list;
}

static void gfx_widgets_task_cmd_free(dispgfx_widget_t *p_dispwidget,
      gfx_widgets_task_cmd_t *cmd)
{
   retro_atomic_fetch_sub_int(&p_dispwidget->task_cmds_count, 1);
   free(cmd->title);
   free(cmd->error);
   free(cmd);
}

static void gfx_widgets_task_cmds_discard(dispgfx_widget_t *p_dispwidget)
{
   gfx_widgets_task_cmd_t *cmd = gfx_widgets_task_cmds_take(p_dispwidget);
   while (cmd)
   {
      gfx_widgets_task_cmd_t *next = cmd->next;
      gfx_widgets_task_cmd_free(p_dispwidget, cmd);
      cmd                          = next;
   }
}

/* Task widgets not yet on screen were never animated or counted. */
static void gfx_widgets_task_pending_discard(dispgfx_widget_t *p_dispwidget)
{
   unsigned i;
   for (i = 0; i < p_dispwidget->task_pending_size; i++)
   {
      disp_widget_msg_t *msg_widget = p_dispwidget->task_pending[i];
      free(msg_widget->msg);
      free(msg_widget->msg_new);
      free(msg_widget);
   }
   p_dispwidget->task_pending_size = 0;
}

/* The next message to go on screen: the older of the plain ring's
 * head and the first waiting task widget. */
static disp_widget_msg_t *gfx_widgets_pending_next(dispgfx_widget_t *p_dispwidget)
{
   disp_widget_msg_t *plain = gfx_widgets_pending_peek(p_dispwidget);
   if (     p_dispwidget->task_pending_size
         && (!plain || (int32_t)(p_dispwidget->task_pending[0]->seq
                  - plain->seq) < 0))
   {
      unsigned i;
      disp_widget_msg_t *msg_widget = p_dispwidget->task_pending[0];
      p_dispwidget->task_pending_size--;
      for (i = 0; i < p_dispwidget->task_pending_size; i++)
         p_dispwidget->task_pending[i] = p_dispwidget->task_pending[i + 1];
      return msg_widget;
   }
   return gfx_widgets_pending_pop(p_dispwidget);
}

/* Width, wrap and height of a plain message, from the font the widgets
 * draw with, on the thread that owns it: the consumer of the message
 * queue (gfx_widgets_iterate_frame()), not whoever pushed it. Task
 * messages are measured where the owner applies their updates. */
static void gfx_widgets_msg_measure(dispgfx_widget_t *p_dispwidget,
      disp_widget_msg_t *msg_widget)
{
   size_t len = msg_widget->msg_len;
   /* Compute rect width, wrap if necessary */
   /* Single line text > two lines text > two lines
    * text with expanded width */
   char *msg_new                       = NULL;
   size_t msg_len                      = 0;
   unsigned rect_width                 = p_dispwidget->msg_queue_default_rect_width;
   unsigned text_width                 = font_driver_get_message_width(
         p_dispwidget->gfx_widget_fonts.msg_queue.font,
         msg_widget->msg,
         len,
         1.0f);
   msg_widget->text_height             = p_dispwidget->gfx_widget_fonts.msg_queue.line_height;
   /* +1 for potential '\n' insertion, +1 for NUL */
   msg_len                             = len + 1 + 1;
   if (!(msg_new = (char*)malloc(msg_len)))
   {
      /* Unwrapped, at its full width */
      msg_widget->width        = text_width + (p_dispwidget->simple_widget_padding / 2);
      return;
   }
   msg_new[0] = '\0';

   /* Text is too wide, split it into two lines */
   if (text_width > rect_width)
   {
      int wrap_length          = 0;

      /* If the second line is too short, the widget may
       * look unappealing - ensure that second line is at
       * least 25% of the total width */
      if ((text_width - (text_width >> 2)) < rect_width)
         rect_width = text_width - (text_width >> 2);

      msg_widget->msg_len      = word_wrap(msg_new, msg_len, msg_widget->msg, len,
            (int)((len * rect_width) / text_width),
            100, 2);

      /* Recalculate widget width with longest wrapped line */
      wrap_length              = string_index_last_occurance(msg_new, '\n');
      if (wrap_length != -1)
      {
         len                  -= wrap_length;

         if ((int)len < wrap_length)
            len       = wrap_length;

         text_width            = font_driver_get_message_width(
            p_dispwidget->gfx_widget_fonts.msg_queue.font,
            msg_widget->msg, len, 1.0f);

         rect_width            = text_width;
      }

      msg_widget->text_height *= 2;
   }
   else
   {
      rect_width               = text_width;
      msg_widget->msg_len      = strlcpy(msg_new, msg_widget->msg, msg_len);
   }

   free(msg_widget->msg);
   msg_widget->msg             = msg_new;
   msg_widget->width           = rect_width + (p_dispwidget->simple_widget_padding / 2);

   /* Use big size only when needed */
   if (strchr(msg_widget->msg, '\n'))
   {
      msg_widget->flags &= ~DISPWIDG_FLAG_SMALL;
      if (msg_widget->text_height == p_dispwidget->gfx_widget_fonts.msg_queue.line_height)
         msg_widget->text_height *= 2;
   }
}

/* A widget can be passed between tasks - a download hands its widget
 * to the decompress task it spawns - and the per-task lifecycle flags
 * are sticky, so without re-keying they would describe the previous
 * owner: the widget would start dying on the strength of a task that
 * is no longer the one driving it. task->ident is unique per task, so
 * a mismatch is an exact test for "different owner". EXPIRED is left
 * alone: a widget already on its way out stays on its way out. */
static void gfx_widgets_task_rebind(disp_widget_msg_t *msg_widget,
      uint32_t ident)
{
   if (msg_widget->task_ident != ident)
   {
      if (msg_widget->flags & DISPWIDG_FLAG_EXPIRATION_TIMER_STARTED)
      {
         uintptr_t _tag     = (uintptr_t)&msg_widget->expiration_timer;
         gfx_animation_kill_widget_by_tag(&_tag);
         msg_widget->flags &= ~DISPWIDG_FLAG_EXPIRATION_TIMER_STARTED;
      }

      msg_widget->flags    &= ~(DISPWIDG_FLAG_TASK_FINISHED
                              | DISPWIDG_FLAG_TASK_ERROR
                              | DISPWIDG_FLAG_TASK_CANCELLED);
      msg_widget->task_ident = ident;
   }
   msg_widget->flags    |= DISPWIDG_FLAG_TASK;
}

void gfx_widgets_task_transfer(retro_task_t *from, retro_task_t *to)
{
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;
   uintptr_t key                  = (uintptr_t)from->frontend_userdata;
   gfx_widgets_task_cmd_t *cmd;

   to->frontend_userdata   = from->frontend_userdata;
   from->frontend_userdata = NULL;
   /* The owner re-keys the widget on its next frame. Should this not
    * get through, the new task's first update re-keys it instead. */
   if (     !key
         || retro_atomic_load_acquire_int(&p_dispwidget->task_cmds_count)
            >= TASK_CMDS_SOFT_MAX
         || !(cmd = (gfx_widgets_task_cmd_t*)calloc(1, sizeof(*cmd))))
      return;
   cmd->key    = key;
   cmd->ident  = to->ident;
   cmd->rebind = true;
   gfx_widgets_task_cmd_push(p_dispwidget, cmd);
}

static disp_widget_msg_t *gfx_widgets_task_widget_find(
      dispgfx_widget_t *p_dispwidget, uintptr_t key)
{
   size_t i;
   for (i = 0; i < p_dispwidget->current_msgs_size; i++)
   {
      disp_widget_msg_t *msg_widget = p_dispwidget->current_msgs[i];
      if (msg_widget && msg_widget->task_key == key)
         return msg_widget;
   }
   for (i = 0; i < p_dispwidget->task_pending_size; i++)
      if (p_dispwidget->task_pending[i]->task_key == key)
         return p_dispwidget->task_pending[i];
   return NULL;
}

/* On the widgets' owner: bring the task's widget up to date, or make
 * it one. Takes the update's strings. */
static void gfx_widgets_task_cmd_apply(dispgfx_widget_t *p_dispwidget,
      gfx_widgets_task_cmd_t *cmd)
{
   disp_widget_msg_t *msg_widget =
      gfx_widgets_task_widget_find(p_dispwidget, cmd->key);

   if (msg_widget)
      gfx_widgets_task_rebind(msg_widget, cmd->ident);
   if (cmd->rebind)
      return;

   if (!msg_widget)
   {
      char **text = (cmd->error && *cmd->error) ? &cmd->error : &cmd->title;

      /* A task with no title, or a muted one - whose only update is
       * its retirement, for a widget it may have had - spawns nothing,
       * and with no room to wait in the next update tries again */
      if (     !cmd->title
            || (cmd->task_flags & RETRO_TASK_FLG_MUTE)
            || p_dispwidget->task_pending_size
               >= ARRAY_SIZE(p_dispwidget->task_pending)
            || !(msg_widget = (disp_widget_msg_t*)calloc(1, sizeof(*msg_widget))))
         return;

      if (!(msg_widget->msg_new = strdup(*text)))
      {
         free(msg_widget);
         return;
      }
      msg_widget->msg              = *text;
      *text                        = NULL;
      msg_widget->msg_len          = strlen(msg_widget->msg);
      msg_widget->duration         = cmd->duration;
      msg_widget->alpha            = 1.0f;
      msg_widget->alternative_look =
         (cmd->task_flags & RETRO_TASK_FLG_ALTERNATIVE_LOOK) != 0;
      msg_widget->task_key         = cmd->key;
      msg_widget->task_ident       = cmd->ident;
      msg_widget->task_progress    = cmd->progress;
      msg_widget->seq              = cmd->seq;
      /* Default to small single line size and grow when necessary */
      msg_widget->flags            = DISPWIDG_FLAG_SMALL | DISPWIDG_FLAG_TASK;

      if (cmd->category == MESSAGE_QUEUE_CATEGORY_WARNING)
         msg_widget->flags        |= DISPWIDG_FLAG_CATEGORY_WARNING;
      else if (cmd->category == MESSAGE_QUEUE_CATEGORY_ERROR)
         msg_widget->flags        |= DISPWIDG_FLAG_CATEGORY_ERROR;
      else if (cmd->category == MESSAGE_QUEUE_CATEGORY_SUCCESS)
         msg_widget->flags        |= DISPWIDG_FLAG_CATEGORY_SUCCESS;

      if (text == &cmd->error)
         msg_widget->flags        |= DISPWIDG_FLAG_TASK_ERROR;
      if (cmd->task_flags & RETRO_TASK_FLG_CANCELLED)
         msg_widget->flags        |= DISPWIDG_FLAG_TASK_CANCELLED;
      if (cmd->task_flags & RETRO_TASK_FLG_FINISHED)
         msg_widget->flags        |= DISPWIDG_FLAG_TASK_FINISHED;

      if (cmd->style == TASK_STYLE_POSITIVE)
         msg_widget->flags        |= DISPWIDG_FLAG_POSITIVE;
      else if (cmd->style == TASK_STYLE_NEGATIVE)
         msg_widget->flags        |= DISPWIDG_FLAG_NEGATIVE;

      msg_widget->width            = font_driver_get_message_width(
            p_dispwidget->gfx_widget_fonts.msg_queue.font,
            msg_widget->msg, msg_widget->msg_len, 1.0f) +
            p_dispwidget->simple_widget_padding / 2;

      /* Use big size only when needed */
      if (strchr(msg_widget->msg, '\n'))
      {
         msg_widget->flags &= ~DISPWIDG_FLAG_SMALL;
         if (msg_widget->text_height == p_dispwidget->gfx_widget_fonts.msg_queue.line_height)
            msg_widget->text_height *= 2;
      }

      p_dispwidget->task_pending[p_dispwidget->task_pending_size++] = msg_widget;
      return;
   }

   /* Update task info */
   if (msg_widget->flags & DISPWIDG_FLAG_EXPIRATION_TIMER_STARTED)
   {
      uintptr_t _tag     = (uintptr_t)&msg_widget->expiration_timer;
      gfx_animation_kill_widget_by_tag(&_tag);
      msg_widget->flags &= ~DISPWIDG_FLAG_EXPIRATION_TIMER_STARTED;
   }

   if (cmd->title &&
         !string_is_equal(cmd->title, msg_widget->msg_new
            ? msg_widget->msg_new : msg_widget->msg))
   {
      uintptr_t title_tag = (uintptr_t)&msg_widget->msg_transition_animation;
      size_t _len;
      unsigned new_width;
      const char *new_title;

      gfx_animation_kill_widget_by_tag(&title_tag);

      if (msg_widget->msg_new)
      {
         free(msg_widget->msg_new);
         msg_widget->msg_new                 = NULL;
      }

      new_title   = msg_widget->msg_new      = cmd->title;
      cmd->title                             = NULL;

      _len        = strlen(new_title);
      new_width   = font_driver_get_message_width(
            p_dispwidget->gfx_widget_fonts.msg_queue.font,
            new_title,
            _len,
            1.0f);

      msg_widget->msg_len                    = _len;
      msg_widget->msg_transition_animation   = 0;

      if (!msg_widget->alternative_look)
      {
         gfx_animation_ctx_entry_t entry;

         entry.easing_enum    = EASING_OUT_QUAD;
         entry.tag            = title_tag;
         entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
         entry.target_value   = p_dispwidget->msg_queue_height / 2.0f;
         entry.subject        = &msg_widget->msg_transition_animation;
         entry.cb             = msg_widget_msg_transition_animation_done;
         entry.userdata       = msg_widget;

         gfx_animation_push_widget(&entry);
      }
      else
         msg_widget_msg_transition_animation_done(msg_widget);

      msg_widget->width = new_width;
   }

   if (cmd->error && *cmd->error)
      msg_widget->flags               |= DISPWIDG_FLAG_TASK_ERROR;
   if (cmd->task_flags & RETRO_TASK_FLG_CANCELLED)
      msg_widget->flags               |= DISPWIDG_FLAG_TASK_CANCELLED;
   if (cmd->task_flags & RETRO_TASK_FLG_FINISHED)
      msg_widget->flags               |= DISPWIDG_FLAG_TASK_FINISHED;
   msg_widget->task_progress     = cmd->progress;
}

/* On the widgets' owner, once a frame: the updates sent since the
 * last one, in the order they were sent. */
static void gfx_widgets_task_cmds_apply(dispgfx_widget_t *p_dispwidget)
{
   gfx_widgets_task_cmd_t *cmd = gfx_widgets_task_cmds_take(p_dispwidget);
   while (cmd)
   {
      gfx_widgets_task_cmd_t *next = cmd->next;
      gfx_widgets_task_cmd_apply(p_dispwidget, cmd);
      gfx_widgets_task_cmd_free(p_dispwidget, cmd);
      cmd                          = next;
   }
}

void gfx_widgets_msg_queue_push(
      retro_task_t *task,
      const char *msg,
      size_t len,
      unsigned duration,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category,
      unsigned prio, bool flush,
      bool menu_is_alive)
{
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;

   /* A plain message touches only the pending ring and the widget it
    * allocates, which the consumer measures */
   if (!task)
   {
      disp_widget_msg_t *msg_widget =
         (disp_widget_msg_t*)calloc(1, sizeof(*msg_widget));

      if (!msg_widget)
         return;

      msg_widget->duration = duration;
      msg_widget->alpha    = 1.0f;
      msg_widget->seq      = p_dispwidget->msg_seq;
      /* Default to small single line size and grow when necessary */
      msg_widget->flags    = DISPWIDG_FLAG_SMALL;

      if (category == MESSAGE_QUEUE_CATEGORY_WARNING)
         msg_widget->flags |= DISPWIDG_FLAG_CATEGORY_WARNING;
      else if (category == MESSAGE_QUEUE_CATEGORY_ERROR)
         msg_widget->flags |= DISPWIDG_FLAG_CATEGORY_ERROR;
      else if (category == MESSAGE_QUEUE_CATEGORY_SUCCESS)
         msg_widget->flags |= DISPWIDG_FLAG_CATEGORY_SUCCESS;

      /* Measured by the consumer, which owns the font */
      msg_widget->msg      = strdup(msg);
      msg_widget->msg_len  = len;

      /* Never animated and never shown: nothing to unwind when the
       * ring has no room */
      if (     !msg_widget->msg
            || !gfx_widgets_pending_push(p_dispwidget, msg_widget))
      {
         free(msg_widget->msg);
         free(msg_widget);
         return;
      }
      p_dispwidget->msg_seq++;
      return;
   }

   /* A task's goes to the widgets' owner as it stands now */
   {
      task_progress_snapshot_t snapshot;
      gfx_widgets_task_cmd_t *cmd;
      uintptr_t key = (uintptr_t)task->frontend_userdata;

      /* Terminal flags are published even if copying a string fails. */
      task_get_progress_snapshot(task, &snapshot);

      /* No widget to update and none to spawn; and progress, which is
       * sent again next frame, gives way when the owner is behind */
      if (     (!key && (!snapshot.title
                  || (snapshot.flags & RETRO_TASK_FLG_MUTE)))
            || (!(snapshot.flags & RETRO_TASK_FLG_FINISHED)
               && retro_atomic_load_acquire_int(&p_dispwidget->task_cmds_count)
                  >= TASK_CMDS_SOFT_MAX)
            || !(cmd = (gfx_widgets_task_cmd_t*)calloc(1, sizeof(*cmd))))
      {
         free(snapshot.title);
         free(snapshot.error);
         return;
      }

      /* A key of its own, never a task's again: a task that handed
       * its widget on and pushes once more must not reach it. */
      if (!key)
      {
         if (!++p_dispwidget->task_key_last)
            p_dispwidget->task_key_last = 1;
         key                     = p_dispwidget->task_key_last;
         task->frontend_userdata = (void*)key;
      }
      cmd->title      = snapshot.title;
      cmd->error      = snapshot.error;
      cmd->key        = key;
      cmd->ident      = task->ident;
      cmd->seq        = p_dispwidget->msg_seq++;
      cmd->duration   = duration;
      cmd->category   = (uint8_t)category;
      cmd->task_flags = snapshot.flags;
      cmd->style      = (uint8_t)task->style;
      cmd->progress   = snapshot.progress;
      gfx_widgets_task_cmd_push(p_dispwidget, cmd);
   }
}

static void gfx_widgets_move_end(void *userdata)
{
   dispgfx_widget_t *p_dispwidget   = &dispwidget_st;

   p_dispwidget->flags         &= ~DISPGFX_WIDGET_FLAG_MOVING;
}

static void gfx_widgets_msg_queue_expired(void *userdata)
{
   disp_widget_msg_t *msg = (disp_widget_msg_t *)userdata;

   if (msg && !(msg->flags & DISPWIDG_FLAG_EXPIRED))
      msg->flags  |= DISPWIDG_FLAG_EXPIRED;
}

static void gfx_widgets_msg_queue_move(dispgfx_widget_t *p_dispwidget)
{
   int i;
   float y = 0;
   bool size_small = false;

   for (i = (int)(p_dispwidget->current_msgs_size - 1); i >= 0; i--)
   {
      disp_widget_msg_t* msg = p_dispwidget->current_msgs[i];

      if (!msg || (msg->flags & DISPWIDG_FLAG_DYING))
         continue;

      size_small             = (   (msg->flags & DISPWIDG_FLAG_TASK)
                                || (msg->flags & DISPWIDG_FLAG_SMALL));

      if (y == 0)
         y += (p_dispwidget->msg_queue_padding * 4.0f);

      y +=    (p_dispwidget->msg_queue_height / 2.0f / (size_small ? 2.0f : 1.0f))
            + (p_dispwidget->msg_queue_spacing * (size_small ? 1.0f : 2.0f))
            + floor(p_dispwidget->divider_width_1px);

      if (msg->offset_y != y)
      {
         gfx_animation_ctx_entry_t entry;

         entry.cb             = (i == 0) ? gfx_widgets_move_end : NULL;
         entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
         entry.easing_enum    = EASING_OUT_QUAD;
         entry.subject        = &msg->offset_y;
         entry.tag            = (uintptr_t)msg;
         entry.target_value   = ceilf(y);
         entry.userdata       = msg;

         gfx_animation_push_widget(&entry);

         p_dispwidget->flags |= DISPGFX_WIDGET_FLAG_MOVING;
      }
   }

}

static void gfx_widgets_msg_queue_free(
      dispgfx_widget_t *p_dispwidget,
      disp_widget_msg_t *msg)
{
   uintptr_t tag = (uintptr_t)msg;
   uintptr_t hourglass_timer_tag = (uintptr_t)&msg->hourglass_timer;
   uintptr_t title_tag = (uintptr_t)&msg->msg_transition_animation;

   /* Update tasks count. Keyed off the sticky flag rather than
    * task_key, which is cleared when a widget is cut loose. */
   if (msg->flags & DISPWIDG_FLAG_TASK)
   {
      if (p_dispwidget->msg_queue_tasks_count > 0)
         p_dispwidget->msg_queue_tasks_count--;
   }

   /* Kill all animations */
   gfx_animation_kill_widget_by_tag(&title_tag);
   gfx_animation_kill_widget_by_tag(&hourglass_timer_tag);
   gfx_animation_kill_widget_by_tag(&tag);

   /* Kill all timers */
   if (msg->flags & DISPWIDG_FLAG_EXPIRATION_TIMER_STARTED)
   {
      uintptr_t _tag = (uintptr_t)&msg->expiration_timer;
      gfx_animation_kill_widget_by_tag(&_tag);
   }

   /* Free it */
   if (msg->msg)
      free(msg->msg);

   if (msg->msg_new)
      free(msg->msg_new);

   p_dispwidget->flags &= ~DISPGFX_WIDGET_FLAG_MOVING;
}

static void gfx_widgets_msg_queue_kill_end(void *userdata)
{
   disp_widget_msg_t* msg;
   dispgfx_widget_t *p_dispwidget   = &dispwidget_st;

   if ((msg = p_dispwidget->current_msgs[p_dispwidget->msg_queue_kill]))
   {
      int i;
      /* Remove it from the list */
      for (i = p_dispwidget->msg_queue_kill; i < (int)(p_dispwidget->current_msgs_size - 1); i++)
         p_dispwidget->current_msgs[i] = p_dispwidget->current_msgs[i + 1];

      p_dispwidget->current_msgs_size--;
      p_dispwidget->current_msgs[p_dispwidget->current_msgs_size] = NULL;

      /* clean up the item */
      gfx_widgets_msg_queue_free(p_dispwidget, msg);

      /* free the associated memory */
      free(msg);
   }

}

static void gfx_widgets_msg_queue_kill(
      dispgfx_widget_t *p_dispwidget,
      unsigned idx)
{
   gfx_animation_ctx_entry_t entry;
   disp_widget_msg_t *msg = p_dispwidget->current_msgs[idx];

   if (!msg)
      return;

   p_dispwidget->flags         |= DISPGFX_WIDGET_FLAG_MOVING;
   msg->flags                  |= DISPWIDG_FLAG_DYING;

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

   gfx_animation_push_widget(&entry);

   /* Fade out */
   entry.cb                     = gfx_widgets_msg_queue_kill_end;
   entry.subject                = &msg->alpha;
   entry.target_value           = 0.0f;

   gfx_animation_push_widget(&entry);

   /* Move all messages back to their correct position */
   if (p_dispwidget->current_msgs_size != 0)
      gfx_widgets_msg_queue_move(p_dispwidget);
}

void gfx_widgets_draw_icon(
      void *userdata,
      void *data_disp,
      unsigned video_dims,
      unsigned icon_dims,
      uintptr_t texture,
      float x, float y,
      float radians,
      float cosine,
      float sine,
      float *color)
{
   unsigned video_height = VIDEO_SCALE_H(video_dims);
   unsigned icon_height  = VIDEO_SCALE_H(icon_dims);
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;
   gfx_display_t            *p_disp  = (gfx_display_t*)data_disp;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   if (!texture)
      return;
   /* Every draw below goes through dispctx; without one there is
    * nothing to render (the context is torn down around a video
    * driver reinit while widget frames may still be submitted). */
   if (!dispctx)
      return;

   memset(&mymat, 0, sizeof(mymat));

   if (!dispctx->handles_transform)
      gfx_display_rotate_z(p_disp, &mymat, cosine, sine, userdata);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   draw.pos             = VIDEO_POS_PACK(VIDEO_PX(x),
         VIDEO_PX(video_height - y - icon_height));
   draw.dims            = icon_dims;
   draw.scale_factor    = 1.0f;
   draw.rotation        = radians;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.pipeline_id     = 0;

   if (VIDEO_SCALE_H(draw.dims) > 0 && VIDEO_SCALE_W(draw.dims) > 0)
      gfx_display_draw(dispctx, &draw, userdata,
            video_dims);
}

void gfx_widgets_draw_text(
      gfx_widget_font_data_t* font_data,
      const char *text,
      float x, float y,
      unsigned dims,
      uint32_t color,
      enum text_alignment text_align,
      bool draw_outside)
{
   if (!font_data || !text || !*text)
      return;

   gfx_display_draw_text(
         font_data->font,
         text,
         x, y,
         dims,
         color,
         text_align,
         1.0f,
         false,
         0.0f,
         draw_outside);

   font_data->usage_count++;
}

void gfx_widgets_flush_text(
      unsigned video_dims,
      gfx_widget_font_data_t* font_data)
{
   /* Flushing is slow - only do it if font
    * has actually been used */
   if (!font_data)
      return;

   /* A rebuilt font has different metrics; pick them up before
    * anything is drawn with the old ones. Done here rather than only
    * when there is something to flush, so a widget that drew nothing
    * this frame still lays out correctly on the next. */
   gfx_widgets_font_sync(font_data);

   if (font_data->usage_count == 0)
      return;

   if (font_data->font && font_data->font->renderer && font_data->font->renderer->flush)
      font_data->font->renderer->flush(video_dims,
            font_data->font->renderer_data);
   font_data->raster_block.carr.coords.vertices = 0;
   font_data->usage_count                       = 0;
}

float gfx_widgets_get_thumbnail_scale_factor(
      unsigned dst_dims, unsigned image_dims)
{
   float dst_ratio;
   float image_ratio;
   float dst_width    = (float)VIDEO_SCALE_W(dst_dims);
   float dst_height   = (float)VIDEO_SCALE_H(dst_dims);
   float image_width  = (float)VIDEO_SCALE_W(image_dims);
   float image_height = (float)VIDEO_SCALE_H(image_dims);

   if (   dst_height   == 0.0f || image_height == 0.0f
       || dst_width    == 0.0f || image_width  == 0.0f)
      return 1.0f;

   dst_ratio      = dst_width   / dst_height;
   image_ratio    = image_width / image_height;

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

   gfx_animation_timer_start_widget(&msg_widget->expiration_timer, &timer);

   msg_widget->flags                   |=
      DISPWIDG_FLAG_EXPIRATION_TIMER_STARTED;
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

   gfx_animation_timer_start_widget(&msg->hourglass_timer, &timer);
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

   gfx_animation_push_widget(&entry);
}

static void gfx_widgets_font_init(
      gfx_display_t *p_disp,
      dispgfx_widget_t *p_dispwidget,
      gfx_widget_font_data_t *font_data,
      bool is_threaded, char *font_path, float font_size)
{
   float scaled_size             = font_size * p_dispwidget->last_scale_factor;

   /* Limit minimum font size to keep it readable.
    * Before the match test below, so it compares the size that would
    * actually be built. */
   if (scaled_size < 9)
      scaled_size = 9;

   /* Nothing to do if this is already the font that was asked for.
    * gfx_widgets_layout() runs on any video dimension change, but the
    * size here derives from last_scale_factor alone, so a resize at
    * an unchanged scale asks for the six fonts already loaded.
    *
    * usage_count is still cleared: it counts draws against the font
    * since the last layout pass, and the callers below expect a
    * layout to have reset it whether or not a rebuild happened. */
   if (     !widget_fonts_force
         && font_driver_matches(font_data->font, font_path, scaled_size))
   {
      font_data->usage_count     = 0;
      return;
   }

   /* Get approximate glyph width */
   font_data->glyph_width        = scaled_size * (3.0f / 4.0f);

   /* Create font.
    *
    * Built before the old one is released, and the old one retired
    * rather than freed: gfx_widgets_layout() reaches here from
    * gfx_widgets_iterate(), which runs before the video driver's
    * frame function, so freeing the atlas outright can pull it out
    * from under a command list that still references it. */
   {
      font_data_t *old_font         = font_data->font;

      font_data->font               = gfx_display_font_file(p_disp,
            font_path, scaled_size, is_threaded);

      if (!font_data->font)
         font_data->font            = old_font;
      else if (old_font)
         font_driver_free_deferred(old_font);
   }

   /* Get font metadata. gfx_display_font_file() can fail, and there is
    * no implicit font to fall back on any more, so the approximate
    * glyph width set above has to stand on its own. */
   if (font_data->font)
      gfx_widgets_font_compute_metrics(font_data);

   font_data->usage_count        = 0;
}

/* Recompute the derived metrics if the font has been rebuilt since
 * they were last worked out. Cheap when nothing has changed. The menu
 * drivers get this from font_flush() via font_driver_sync_impl(), but
 * widgets keep their own font struct and flush directly. */
/* Work the derived metrics out. Unconditional: callers that know the
 * font is new must not be turned away by the generation check, since
 * a fresh font_data_t and a fresh font_driver both start at
 * generation 0 and would compare equal. */
static void gfx_widgets_font_compute_metrics(
      gfx_widget_font_data_t *font_data)
{
   int glyph_width;

   if (!font_data || !font_data->font)
      return;

   font_data->metrics_generation = font_driver_get_generation();

   {
      glyph_width                = font_driver_get_message_width(
            font_data->font, "a", 1, 1.0f);
      if (glyph_width > 0)
         font_data->glyph_width  = (float)glyph_width;

      font_data->line_height        = (float)(int)roundf(font_data->font->metrics.height);
      font_data->line_ascender      = (float)(int)roundf(font_data->font->metrics.ascender);
      font_data->line_descender     = (float)(int)roundf(font_data->font->metrics.descender);
      font_data->line_centre_offset = roundf((font_data->font->metrics.ascender
            - font_data->font->metrics.descender) * 0.5f);
   }
}

void gfx_widgets_font_sync(gfx_widget_font_data_t *font_data)
{
   if (!font_data || !font_data->font)
      return;
   /* Nothing has been rebuilt since these were worked out. */
   if (font_data->metrics_generation == font_driver_get_generation())
      return;

   gfx_widgets_font_compute_metrics(font_data);
}

static void gfx_widgets_layout(
      gfx_display_t *p_disp,
      dispgfx_widget_t *p_dispwidget,
      bool is_threaded, const char *dir_assets, char *font_path)
{
   size_t i;

   /* Recorded here rather than at the call sites, so context reset
    * and the in-place rebuild cannot disagree on what is loaded. */
   strlcpy(p_dispwidget->last_font_path, font_path ? font_path : "",
         sizeof(p_dispwidget->last_font_path));

   /* Initialise fonts */
   if (!font_path || !*font_path)
   {
      char font_file[PATH_MAX_LENGTH];
      /* Create regular font */
      gfx_widgets_font_init(p_disp, p_dispwidget,
            &p_dispwidget->gfx_widget_fonts.regular,
            is_threaded, p_dispwidget->ozone_regular_font_path, BASE_FONT_SIZE);
      /* Create bold font */
      gfx_widgets_font_init(p_disp, p_dispwidget,
            &p_dispwidget->gfx_widget_fonts.bold,
            is_threaded, p_dispwidget->ozone_bold_font_path, BASE_FONT_SIZE);

      /* Create msg_queue font */
      {
         const char *lang_font = font_driver_language_font_file();

         if (lang_font)
            fill_pathname_join_special(font_file,
                  p_dispwidget->assets_pkg_dir, lang_font, sizeof(font_file));
         else
            strlcpy(font_file, p_dispwidget->ozone_regular_font_path,
                  sizeof(font_file));
      }
      gfx_widgets_font_init(p_disp, p_dispwidget,
            &p_dispwidget->gfx_widget_fonts.msg_queue,
            is_threaded, font_file, MSG_QUEUE_FONT_SIZE);

      /* Only the message-queue font follows the language; the regular
       * and bold ones are always the ozone faces. Marking it lets a
       * language change rebuild it in place. */
      font_driver_set_language_font(
            p_dispwidget->gfx_widget_fonts.msg_queue.font,
            p_dispwidget->assets_pkg_dir,
            p_dispwidget->ozone_regular_font_path);
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
   p_dispwidget->simple_widget_padding            = p_dispwidget->gfx_widget_fonts.regular.line_height * (2.0f / 3.0f) + 0.5f;
   p_dispwidget->simple_widget_height             = p_dispwidget->gfx_widget_fonts.regular.line_height + p_dispwidget->simple_widget_padding;

   p_dispwidget->msg_queue_height                 = p_dispwidget->gfx_widget_fonts.msg_queue.line_height * 2.4f * (BASE_FONT_SIZE / MSG_QUEUE_FONT_SIZE);
   p_dispwidget->msg_queue_padding                = (unsigned)(((float)p_dispwidget->gfx_widget_fonts.msg_queue.line_height * (2.0f / 3.0f)) + 0.5f);
   p_dispwidget->msg_queue_spacing                = p_dispwidget->msg_queue_height / 4.0f;
   p_dispwidget->msg_queue_rect_start_x           = ceil(p_dispwidget->msg_queue_padding - (p_dispwidget->simple_widget_padding * 0.10f));

   gfx_widgets_update_icon_layout(p_dispwidget);

   p_dispwidget->divider_width_1px                = 1;
   if (p_dispwidget->last_scale_factor > 1.0f)
      p_dispwidget->divider_width_1px             = (unsigned)(p_dispwidget->last_scale_factor + 0.5f);

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->layout)
         widget->layout(p_dispwidget,
               is_threaded, dir_assets, font_path);
   }
}

/* Relayout when the screen, the scale factor or the notification font
 * changes. It builds and retires fonts, which the font driver keeps on
 * the main thread, so it stays there when the threaded video worker
 * runs the rest of the widgets; it is rare, and holds the widget state
 * lock only for the relayout itself. */
static INLINE void gfx_widgets_update_layout(
      void *data_disp,
      void *settings_data,
      unsigned dims, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded)
{
   dispgfx_widget_t *p_dispwidget   = &dispwidget_st;
   /* c.f. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323
    * On some platforms (e.g. 32-bit x86 without SSE),
    * GCC can produce inconsistent floating point results
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
      scale_factor                  = gfx_display_get_widget_pixel_scale(p_disp, settings, dims, fullscreen);
   else
#endif
      scale_factor                  = gfx_display_get_dpi_scale(
            p_disp,
            settings, dims, fullscreen, true);

   /* Check whether screen dimensions, menu scale factor or the
    * notification font have changed. The font is watched here rather
    * than from a settings callback because the rebuild needs the
    * video driver up and the thread that drives it, which is what
    * runs once a frame here. */
   if ((scale_factor != p_dispwidget->last_scale_factor) ||
       (dims         != p_dispwidget->last_video_dims) ||
       !string_is_equal(p_dispwidget->last_font_path,
             font_path ? font_path : "") ||
       retro_atomic_load_acquire_int(&widget_fonts_reload))
   {
      gfx_widgets_state_lock();
      p_dispwidget->last_scale_factor = scale_factor;
      p_dispwidget->last_video_dims   = dims;
      widget_fonts_force              = retro_atomic_cas_int(
            &widget_fonts_reload, 1, 0);

      /* Note: We don't need a full context reset here
       * > Just rescale layout, and reset frame time counter */
      gfx_widgets_layout(p_disp, p_dispwidget,
            is_threaded, dir_assets, font_path);
      widget_fonts_force              = false;
      video_driver_monitor_reset();
      gfx_widgets_state_unlock();
   }
}

/* Once a frame: the widgets' own iterate() and the message queue. On
 * the threaded video worker when it draws the widgets. */
static INLINE void gfx_widgets_iterate_frame(
      unsigned dims, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded)
{
   size_t i;
   dispgfx_widget_t *p_dispwidget   = &dispwidget_st;

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->iterate)
         widget->iterate(p_dispwidget,
               dims, fullscreen,
               dir_assets, font_path, is_threaded);
   }

   /* Messages queue */

   gfx_widgets_task_cmds_apply(p_dispwidget);

   /* Consume one message if available.  current_msgs[] and the MOVING
    * flag belong to this thread, the one that owns the widgets.  The
    * pending ring is shared with its producer and popped without a
    * lock; a push that lands just after the pop is taken on the next
    * frame. */
   if (    !(p_dispwidget->flags & DISPGFX_WIDGET_FLAG_MOVING)
         && (p_dispwidget->current_msgs_size < ARRAY_SIZE(p_dispwidget->current_msgs)))
   {
      disp_widget_msg_t *msg_widget = gfx_widgets_pending_next(p_dispwidget);

      if (msg_widget)
      {
         /* Plain messages arrive unmeasured; this thread owns the font */
         if (!(msg_widget->flags & DISPWIDG_FLAG_TASK) && !msg_widget->width)
            gfx_widgets_msg_measure(p_dispwidget, msg_widget);

         /* Task messages always appear from the bottom of the screen, append it */
         if (   p_dispwidget->msg_queue_tasks_count == 0
             || (msg_widget->flags & DISPWIDG_FLAG_TASK))
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

         /* Start expiration timer if not associated to a task */
         if (!(msg_widget->flags & DISPWIDG_FLAG_TASK))
         {
            if (!(msg_widget->flags & DISPWIDG_FLAG_EXPIRATION_TIMER_STARTED))
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

      if (      (msg_widget->flags & DISPWIDG_FLAG_TASK)
            &&   ((msg_widget->flags & DISPWIDG_FLAG_TASK_FINISHED)
               || (msg_widget->flags & DISPWIDG_FLAG_TASK_CANCELLED)))
         if (!(msg_widget->flags & DISPWIDG_FLAG_EXPIRATION_TIMER_STARTED))
            gfx_widgets_start_msg_expiration_timer(msg_widget, TASK_FINISHED_DURATION);

      if (      (msg_widget->flags   & DISPWIDG_FLAG_EXPIRED)
            && !(p_dispwidget->flags & DISPGFX_WIDGET_FLAG_MOVING))
      {
         gfx_widgets_msg_queue_kill(p_dispwidget,
               (unsigned)i);
         break;
      }
   }
}

void gfx_widgets_iterate(
      void *data_disp,
      void *settings_data,
      unsigned dims, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded)
{
   gfx_widgets_state_lock();
   gfx_widgets_update_layout(data_disp, settings_data, dims,
         fullscreen, dir_assets, font_path, is_threaded);
   gfx_widgets_iterate_frame(dims, fullscreen,
         dir_assets, font_path, is_threaded);
   gfx_widgets_state_unlock();
}

#ifdef HAVE_THREADS
void gfx_widgets_iterate_layout(
      void *data_disp,
      void *settings_data,
      unsigned dims, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded)
{
   gfx_widgets_update_layout(data_disp, settings_data, dims,
         fullscreen, dir_assets, font_path, is_threaded);
}
#endif

static int gfx_widgets_draw_indicator(
      dispgfx_widget_t *p_dispwidget,
      gfx_display_t            *p_disp,
      gfx_display_ctx_driver_t *dispctx,
      void *userdata,
      unsigned video_dims,
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
            video_dims,
            top_right_x_advance - width, y,
            VIDEO_SCALE_PACK(width, height),
            video_dims,
            p_dispwidget->backdrop_orig,
            NULL
      );

      gfx_display_set_alpha(p_dispwidget->pure_white, 1.0f);

      gfx_display_blend_begin(dispctx, userdata);
      gfx_widgets_draw_icon(
            userdata,
            p_disp,
            video_dims,
            VIDEO_SCALE_PACK(width, height),
            icon,
            top_right_x_advance - width, y,
            0.0f, /* rad */
            1.0f, /* cos(rad)   = cos(0)  = 1.0f */
            0.0f, /* sine(rad)  = sine(0) = 0.0f */
            p_dispwidget->pure_white
            );
      gfx_display_blend_end(dispctx, userdata);
   }
   else
   {
      char txt[NAME_MAX_LENGTH];
      unsigned height       = p_dispwidget->simple_widget_height;
      size_t _len = strlcpy(txt, msg_hash_to_str(msg), sizeof(txt));

      width = font_driver_get_message_width(
            p_dispwidget->gfx_widget_fonts.regular.font,
            txt, _len, 1.0f)
         + p_dispwidget->simple_widget_padding * 2;

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_dims,
            top_right_x_advance - width, y,
            VIDEO_SCALE_PACK(width, height),
            video_dims,
            p_dispwidget->backdrop_orig,
            NULL
      );

      gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
            txt,
            top_right_x_advance - width
            + p_dispwidget->simple_widget_padding,
            y + (height / 2.0f) +
            p_dispwidget->gfx_widget_fonts.regular.line_centre_offset,
            video_dims,
            TEXT_COLOR_INFO, TEXT_ALIGN_LEFT,
            false);
   }

   return width;
}

static void gfx_widgets_draw_task_msg(
      dispgfx_widget_t *p_dispwidget,
      gfx_display_t *p_disp,
      gfx_display_ctx_driver_t *dispctx,
      disp_widget_msg_t *msg,
      void *userdata,
      unsigned video_dims,
      unsigned alt_slot)
{
   unsigned video_width  = VIDEO_SCALE_W(video_dims);
   unsigned video_height = VIDEO_SCALE_H(video_dims);
   float msg_queue_background[16]    = COLOR_HEX_TO_FLOAT(BG_COLOR_DEFAULT, 1.0f);
   float msg_queue_bar[16]           = COLOR_HEX_TO_FLOAT(BG_COLOR_MARGIN, 1.0f);
   float msg_queue_task_progress[16] = COLOR_HEX_TO_FLOAT(BG_COLOR_PROGRESS, 1.0f);
   float msg_queue_task_negative[16] = COLOR_HEX_TO_FLOAT(ICON_COLOR_RED, 1.0f);
   float msg_queue_task_positive[16] = COLOR_HEX_TO_FLOAT(ICON_COLOR_GREEN, 1.0f);

   unsigned msg_queue_height         = p_dispwidget->msg_queue_height;
   unsigned text_color;
   unsigned bar_width;

   unsigned rect_margin;
   unsigned rect_x;
   unsigned rect_y;
   unsigned rect_width;
   unsigned rect_height;
   float text_y_base;

   float *msg_queue_current_background;
   float *msg_queue_current_progress;

   size_t _len                       = 0;
   size_t task_percentage_offset     = 0;
   char task_percentage[16]          = "";
   bool draw_msg_new                 = false;
   bool msg_alternative              = msg->alternative_look;

   if (msg->msg_new)
      draw_msg_new                   = !string_is_equal(msg->msg_new, msg->msg);

   if (msg->flags & DISPWIDG_FLAG_TASK_FINISHED)
   {
      if (msg->flags & DISPWIDG_FLAG_TASK_ERROR)
         _len = strlcpy(task_percentage, msg_hash_to_str(MSG_ERROR), sizeof(task_percentage));
   }
   else if (msg->task_progress >= 0 && msg->task_progress <= 100)
      _len = snprintf(task_percentage, sizeof(task_percentage),
            "%i%%", msg->task_progress);

   task_percentage_offset = p_dispwidget->gfx_widget_fonts.msg_queue.glyph_width * _len;
   rect_width             = (msg_alternative)
         ? video_width
         : p_dispwidget->simple_widget_padding + msg->width + (p_dispwidget->msg_queue_icon_size_x / 2) + task_percentage_offset;
   bar_width              = rect_width * msg->task_progress / 100.0f;
   text_color             = COLOR_TEXT_ALPHA(TEXT_COLOR_INFO, (unsigned)(msg->alpha * 255.0f));

   /* Rect */
   if (     msg->flags & DISPWIDG_FLAG_TASK_FINISHED
         && msg_alternative)
      msg_queue_current_background = msg_queue_task_progress;
   else
      msg_queue_current_background = msg_queue_background;

   if (msg_alternative)
      msg_queue_height = msg_queue_height + (msg_queue_height >> 1);

   rect_x      = p_dispwidget->msg_queue_rect_start_x;
   rect_y      = video_height - msg->offset_y;
   rect_height = msg_queue_height / 2;
   rect_margin = p_dispwidget->simple_widget_padding * 0.15f;

   if (msg_alternative)
   {
      rect_x      = 0;
      rect_y      = video_height - (rect_height * (int)(alt_slot + 1));
      rect_margin = 0;
   }

   if (rect_margin)
   {
      gfx_display_set_alpha(msg_queue_bar, msg->alpha);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_dims,
            rect_x, rect_y,
            VIDEO_SCALE_PACK(rect_margin, rect_height),
            video_dims,
            msg_queue_bar,
            NULL
            );
   }

   gfx_display_set_alpha(msg_queue_current_background, msg->alpha);
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_dims,
         rect_x + rect_margin, rect_y,
         VIDEO_SCALE_PACK(rect_width, rect_height),
         video_dims,
         msg_queue_current_background,
         NULL
         );

   /* Progress bar */
   if (    !(msg->flags & DISPWIDG_FLAG_TASK_FINISHED)
         && (msg->task_progress >= 0)
         && (msg->task_progress <= 100))
   {
      msg_queue_current_progress = msg_queue_task_progress;

      gfx_display_set_alpha(msg_queue_current_progress, msg->alpha);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_dims,
            rect_x + rect_margin, rect_y,
            VIDEO_SCALE_PACK(bar_width, rect_height),
            video_dims,
            msg_queue_current_progress,
            NULL
            );
   }

   /* Icon */
   gfx_display_blend_begin(dispctx, userdata);
   {
      float radians = 0.0f; /* rad                        */
      float cosine  = 1.0f; /* cos(rad)  = cos(0)  = 1.0f */
      float sine    = 0.0f; /* sine(rad) = sine(0) = 0.0f */
      int texture   = MENU_WIDGETS_ICON_CHECK;
      float *color  = msg_queue_task_positive;

      if (!(msg->flags & DISPWIDG_FLAG_TASK_FINISHED))
      {
         texture    = MENU_WIDGETS_ICON_HOURGLASS;
         color      = msg_queue_bar;
         radians    = msg->hourglass_rotation;
         /* The display drivers that consume this rotation via the
          * cosine/sine pair (gl, gl1, glcore, vulkan) need the
          * trig values recomputed to match the live rotation -
          * leaving them at the zero-rotation defaults above pins
          * the hourglass icon to its starting orientation. */
         cosine     = cosf(radians);
         sine       = sinf(radians);
      }
      else if (msg->flags & DISPWIDG_FLAG_POSITIVE)
      {
         texture    = MENU_WIDGETS_ICON_ADD;
         color      = msg_queue_task_positive;
      }
      else if (   msg->flags & DISPWIDG_FLAG_NEGATIVE
               || msg->flags & DISPWIDG_FLAG_TASK_ERROR)
      {
         texture    = MENU_WIDGETS_ICON_EXIT;
         color      = msg_queue_task_negative;
      }

      if (msg_alternative)
         gfx_display_set_alpha(color, msg->alpha * 0.5f);
      else
         gfx_display_set_alpha(color, msg->alpha);

      gfx_widgets_draw_icon(
            userdata,
            p_disp,
            video_dims,
            VIDEO_SCALE_PACK(msg_queue_height / 2.5f, msg_queue_height / 2.5f),
            p_dispwidget->gfx_widgets_icons_textures[texture],
            rect_x + (msg_queue_height / 12.0f) + (msg_queue_height / MSG_QUEUE_FONT_SIZE),
            rect_y + (msg_queue_height / MSG_QUEUE_FONT_SIZE),
            radians,
            cosine,
            sine,
            color);
   }
   gfx_display_blend_end(dispctx, userdata);

   /* Text */
   text_y_base = rect_y
      + (msg_queue_height / 4.25f)
      + p_dispwidget->gfx_widget_fonts.msg_queue.line_centre_offset;

   if (draw_msg_new)
   {
      gfx_widgets_flush_text(video_dims,
            &p_dispwidget->gfx_widget_fonts.msg_queue);

      gfx_display_scissor_begin(p_disp,
            userdata,
            video_dims,
            rect_x, rect_y, VIDEO_SCALE_PACK(rect_width, rect_height));

      gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
            msg->msg_new,
            p_dispwidget->msg_queue_task_text_start_x,
            text_y_base - msg_queue_height / 2.0f + msg->msg_transition_animation,
            video_dims,
            text_color,
            TEXT_ALIGN_LEFT,
            true);
   }

   gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
         msg->msg,
         p_dispwidget->msg_queue_task_text_start_x,
         text_y_base + msg->msg_transition_animation,
         video_dims,
         text_color,
         TEXT_ALIGN_LEFT,
         true);

   if (draw_msg_new)
   {
      gfx_widgets_flush_text(video_dims,
            &p_dispwidget->gfx_widget_fonts.msg_queue);
      if (dispctx && dispctx->scissor_end)
         dispctx->scissor_end(userdata,
               video_dims);
   }

   /* Progress text */
   text_color = COLOR_TEXT_ALPHA(TEXT_COLOR_INFO, (unsigned)(msg->alpha * 128));
   gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
      task_percentage,
      rect_x + rect_width - (msg_alternative
            ? p_dispwidget->gfx_widget_fonts.msg_queue.glyph_width * 3
            : p_dispwidget->gfx_widget_fonts.msg_queue.glyph_width),
      text_y_base,
      video_dims,
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
      unsigned video_dims)
{
   unsigned video_height = VIDEO_SCALE_H(video_dims);
   float msg_queue_info_blue[16]   = COLOR_HEX_TO_FLOAT(ICON_COLOR_BLUE, 1.0f);
   float msg_queue_info_yellow[16] = COLOR_HEX_TO_FLOAT(ICON_COLOR_YELLOW, 1.0f);
   float msg_queue_info_red[16]    = COLOR_HEX_TO_FLOAT(ICON_COLOR_RED, 1.0f);
   float msg_queue_info_green[16]  = COLOR_HEX_TO_FLOAT(ICON_COLOR_GREEN, 1.0f);
   float msg_queue_bar[16]         = COLOR_HEX_TO_FLOAT(BG_COLOR_MARGIN, 1.0f);
   float* msg_queue_info;
   float text_y_base;
   unsigned rect_width;
   unsigned rect_height;
   unsigned rect_margin;
   unsigned text_color;

   /* Tint icon yellow for warnings, red for errors,
    * green for success, and blue for info */
   if (msg->flags & DISPWIDG_FLAG_CATEGORY_WARNING)
      msg_queue_info = msg_queue_info_yellow;
   else if (msg->flags & DISPWIDG_FLAG_CATEGORY_ERROR)
      msg_queue_info = msg_queue_info_red;
   else if (msg->flags & DISPWIDG_FLAG_CATEGORY_SUCCESS)
      msg_queue_info = msg_queue_info_green;
   else
      msg_queue_info = msg_queue_info_blue;

   gfx_display_set_alpha(msg_queue_info, msg->alpha);
   gfx_display_set_alpha(p_dispwidget->pure_white, msg->alpha);
   gfx_display_set_alpha(p_dispwidget->msg_queue_bg, msg->alpha);

   /* Background */
   rect_width  = p_dispwidget->simple_widget_padding + msg->width + p_dispwidget->msg_queue_icon_size_x;
   rect_height = p_dispwidget->msg_queue_height;
   rect_margin = p_dispwidget->simple_widget_padding * 0.15f;
   gfx_display_set_alpha(msg_queue_bar, msg->alpha);

   if (msg->flags & DISPWIDG_FLAG_SMALL)
   {
      rect_width  = p_dispwidget->simple_widget_padding + msg->width + p_dispwidget->msg_queue_icon_size_x / 2;
      rect_height = p_dispwidget->msg_queue_height / 2;
   }

   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_dims,
         p_dispwidget->msg_queue_rect_start_x + rect_margin,
         video_height - msg->offset_y,
         VIDEO_SCALE_PACK(rect_width - rect_margin, rect_height),
         video_dims,
         p_dispwidget->msg_queue_bg,
         NULL
         );

   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_dims,
         p_dispwidget->msg_queue_rect_start_x,
         video_height - msg->offset_y,
         VIDEO_SCALE_PACK(rect_margin, rect_height),
         video_dims,
         msg_queue_bar,
         NULL
         );

   /* Text */
   text_color = COLOR_TEXT_ALPHA(TEXT_COLOR_INFO, (unsigned)(msg->alpha*255.0f));

   if (msg->flags & DISPWIDG_FLAG_SMALL)
      text_y_base = video_height
         - msg->offset_y
         + p_dispwidget->msg_queue_height / 4.25f
         + p_dispwidget->gfx_widget_fonts.msg_queue.line_centre_offset;
   else
      text_y_base = video_height
         - msg->offset_y
         + (rect_height - msg->text_height) / 2.0f
         + p_dispwidget->gfx_widget_fonts.msg_queue.line_ascender;

   gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
      msg->msg,
      (msg->flags & DISPWIDG_FLAG_SMALL)
         ? p_dispwidget->msg_queue_task_text_start_x
         : p_dispwidget->msg_queue_regular_text_start,
      text_y_base,
      video_dims,
      text_color,
      TEXT_ALIGN_LEFT,
      true);

   /* Icon */
   if (p_dispwidget->flags & DISPGFX_WIDGET_FLAG_MSG_QUEUE_HAS_ICONS)
   {
      float icon_size = p_dispwidget->msg_queue_icon_size_x / ((msg->flags & DISPWIDG_FLAG_SMALL) ? 2 : 1);
      /* For warnings and errors, flip the 'i' upside down so it becomes '!' */
      bool invert_y = (msg->flags & (  DISPWIDG_FLAG_CATEGORY_WARNING
                                     | DISPWIDG_FLAG_CATEGORY_ERROR)) != 0;
      float radians = (invert_y ? M_PI : 0.0f);
      float cosine  = cosf(radians);
      float sine    = sinf(radians);

      gfx_display_blend_begin(dispctx, userdata);

      gfx_widgets_draw_icon(
            userdata,
            p_disp,
            video_dims,
            VIDEO_SCALE_PACK(icon_size, icon_size),
            p_dispwidget->gfx_widgets_icons_textures[MENU_WIDGETS_ICON_INFO],
            p_dispwidget->msg_queue_rect_start_x
                  + (p_dispwidget->msg_queue_height / 10.0f),
            video_height - msg->offset_y - p_dispwidget->msg_queue_icon_offset_y,
            radians,
            cosine,
            sine,
            msg_queue_info);

      gfx_display_blend_end(dispctx, userdata);
   }
}

bool gfx_widgets_visible(void *data)
{
   size_t i;
   video_frame_info_t *video_info = (video_frame_info_t*)data;
   dispgfx_widget_t *p_dispwidget = (dispgfx_widget_t*)video_info->widgets_userdata;
   bool fps_show                  = video_info->fps_show;
   bool framecount_show           = video_info->framecount_show;
   bool memory_show               = video_info->memory_show;
   bool core_status_msg_show      = video_info->core_status_msg_show;
   bool time_show                 = video_info->time_show;
   uint32_t video_flags           = video_info->video_st_flags;
   bool widgets_is_paused         = (video_flags & VIDEO_FLAG_WIDGETS_PAUSED) != 0;
   bool widgets_is_fastmotion     = (video_flags & VIDEO_FLAG_WIDGETS_FASTMOTION) != 0;
   bool widgets_is_slowmotion     = (video_flags & VIDEO_FLAG_WIDGETS_SLOWMOTION) != 0;
   bool widgets_is_rewinding      = (video_flags & VIDEO_FLAG_WIDGETS_REWINDING) != 0;
   bool notifications_hidden      = video_info->notifications_hidden || video_info->msg_queue_delay;

#ifdef HAVE_MENU
   if ((video_info->menu_st_flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE))
      return false;
#endif

   if (notifications_hidden)
      return false;

#ifdef HAVE_TRANSLATE
   if (gfx_widgets_ai_service_overlay_get_state() > 0)
      return true;
#endif

   if (     fps_show
         || framecount_show
         || memory_show
         || core_status_msg_show
         || time_show)
      return true;

   if (     widgets_is_paused
         || widgets_is_fastmotion
         || widgets_is_slowmotion
         || widgets_is_rewinding)
      return true;

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];
      if (widget->visible && widget->visible())
         return true;
   }

   if (p_dispwidget->current_msgs_size)
      return true;

   return false;
}

static void gfx_widgets_frame_state(void *data)
{
   size_t i;
   video_frame_info_t *video_info   = (video_frame_info_t*)data;
   gfx_display_t            *p_disp = (gfx_display_t*)video_info->disp_userdata;
   gfx_display_ctx_driver_t *dispctx= p_disp->dispctx;
   video_driver_state_t *video_st   = video_state_get_ptr();
   dispgfx_widget_t *p_dispwidget   = (dispgfx_widget_t*)video_info->widgets_userdata;
   bool fps_show                    = video_info->fps_show;
   bool framecount_show             = video_info->framecount_show;
   bool memory_show                 = video_info->memory_show;
   bool core_status_msg_show        = video_info->core_status_msg_show;
   bool time_show                   = video_info->time_show;
   bool onscreen_panels             = fps_show || framecount_show || memory_show || core_status_msg_show || time_show;
   void *userdata                   = video_info->userdata;
   unsigned video_width             = VIDEO_SCALE_W(video_info->dims);
   uint32_t video_flags             = video_info->video_st_flags;
   bool widgets_is_paused           = (video_flags & VIDEO_FLAG_WIDGETS_PAUSED) != 0;
   bool widgets_is_fastmotion       = (video_flags & VIDEO_FLAG_WIDGETS_FASTMOTION) != 0;
   bool widgets_is_slowmotion       = (video_flags & VIDEO_FLAG_WIDGETS_SLOWMOTION) != 0;
   bool widgets_is_rewinding        = (video_flags & VIDEO_FLAG_WIDGETS_REWINDING) != 0;
#ifdef HAVE_MENU
   bool menu_screensaver_active     = (video_info->menu_st_flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE) != 0;
#endif
   bool notifications_hidden        = video_info->notifications_hidden || video_info->msg_queue_delay;
   int top_right_x_advance          = video_width;

   /* Second-pass icon layout: when async widget icons finish loading,
    * detect the transition and recompute icon-dependent layout.
    * Wait until ALL icons are loaded before flipping the flag —
    * the layout function checks individual textures (e.g. hourglass)
    * so partial loads produce wrong text offsets. */
   if (!(p_dispwidget->flags & DISPGFX_WIDGET_FLAG_MSG_QUEUE_HAS_ICONS))
   {
      size_t _i;
      bool all_loaded = true;
      for (_i = 0; _i < MENU_WIDGETS_ICON_LAST; _i++)
      {
         if (!p_dispwidget->gfx_widgets_icons_textures[_i])
         {
            all_loaded = false;
            break;
         }
      }
      if (all_loaded)
      {
         p_dispwidget->flags |= DISPGFX_WIDGET_FLAG_MSG_QUEUE_HAS_ICONS;
         gfx_widgets_update_icon_layout(p_dispwidget);
      }
   }

#ifdef HAVE_MENU
   /* If menu screensaver is active, draw nothing */
   if (menu_screensaver_active)
      return;
#endif
   /* If notifications are hidden, draw nothing */
   if (notifications_hidden)
      return;

   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(
            video_st->data, video_info->dims, true, false);

   /* Font setup */
   font_driver_bind_block(p_dispwidget->gfx_widget_fonts.regular.font,
         &p_dispwidget->gfx_widget_fonts.regular.raster_block);
   font_driver_bind_block(p_dispwidget->gfx_widget_fonts.bold.font,
         &p_dispwidget->gfx_widget_fonts.bold.raster_block);
   font_driver_bind_block(p_dispwidget->gfx_widget_fonts.msg_queue.font,
         &p_dispwidget->gfx_widget_fonts.msg_queue.raster_block);

   p_dispwidget->gfx_widget_fonts.regular.raster_block.carr.coords.vertices   = 0;
   p_dispwidget->gfx_widget_fonts.regular.usage_count                         = 0;
   p_dispwidget->gfx_widget_fonts.bold.raster_block.carr.coords.vertices      = 0;
   p_dispwidget->gfx_widget_fonts.bold.usage_count                            = 0;
   p_dispwidget->gfx_widget_fonts.msg_queue.raster_block.carr.coords.vertices = 0;
   p_dispwidget->gfx_widget_fonts.msg_queue.usage_count                       = 0;

#ifdef HAVE_TRANSLATE
   /* AI Service overlay */
   if (gfx_widgets_ai_service_overlay_get_state() > 0)
   {
      video_viewport_t content_vp;
      int overlay_x             = 0;
      int overlay_y             = 0;
      unsigned overlay_width    = video_width;
      unsigned overlay_height   = VIDEO_SCALE_H(video_info->dims);
      float outline_color[16] = {
      0.00, 1.00, 0.00, 1.00,
      0.00, 1.00, 0.00, 1.00,
      0.00, 1.00, 0.00, 1.00,
      0.00, 1.00, 0.00, 1.00,
      };

      if (video_driver_get_viewport_info(&content_vp) && VIDEO_SCALE_W(content_vp.dims) && VIDEO_SCALE_H(content_vp.dims))
      {
         overlay_x      = VIDEO_POS_X(content_vp.pos);
         overlay_y      = VIDEO_POS_Y(content_vp.pos);
         overlay_width  = VIDEO_SCALE_W(content_vp.dims);
         overlay_height = VIDEO_SCALE_H(content_vp.dims);
      }
      gfx_display_set_alpha(p_dispwidget->pure_white, 1.0f);

      if (p_dispwidget->ai_service_overlay_texture)
      {
         gfx_display_blend_begin(dispctx, userdata);
         gfx_widgets_draw_icon(
               userdata,
               p_disp,
               video_info->dims,
               VIDEO_SCALE_PACK(overlay_width, overlay_height),
               p_dispwidget->ai_service_overlay_texture,
               overlay_x,
               overlay_y,
               0.0f, /* rad                         */
               1.0f, /* cos(rad)   = cos(0)  = 1.0f */
               0.0f, /* sine(rad)  = sine(0) = 0.0f */
               p_dispwidget->pure_white
               );
         gfx_display_blend_end(dispctx, userdata);
      }

      /* top line */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_info->dims,
            overlay_x, overlay_y,
            VIDEO_SCALE_PACK(overlay_width, p_dispwidget->divider_width_1px),
            video_info->dims,
            outline_color,
            NULL
            );
      /* bottom line */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_info->dims,
            overlay_x,
            overlay_y + overlay_height - p_dispwidget->divider_width_1px,
            VIDEO_SCALE_PACK(overlay_width, p_dispwidget->divider_width_1px),
            video_info->dims,
            outline_color,
            NULL
            );
      /* left line */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_info->dims,
            overlay_x,
            overlay_y,
            VIDEO_SCALE_PACK(p_dispwidget->divider_width_1px, overlay_height),
            video_info->dims,
            outline_color,
            NULL
            );
      /* right line */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_info->dims,
            overlay_x + overlay_width - p_dispwidget->divider_width_1px,
            overlay_y,
            VIDEO_SCALE_PACK(p_dispwidget->divider_width_1px, overlay_height),
            video_info->dims,
            outline_color,
            NULL
            );
      if (gfx_widgets_ai_service_overlay_get_state() == 2)
          gfx_widgets_ai_service_overlay_set_state(3);
   }
#endif

   /* On-Screen Panels (FPS, framecount, memory, core status message, time) */
   if (onscreen_panels)
   {
      const char *txt;
      size_t      txt_len;
      int         txt_width;
      int         total_width;
      int         status_txt_x;

      if (*p_dispwidget->gfx_widgets_status_text == '\0')
      {
         /* Rare fallback when no panel content is set this frame. */
         txt     = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
         txt_len = strlen(txt);
      }
      else
      {
         /* Length was cached by the producer in video_driver.c —
          * no per-frame strlen on a string that can be up to 256
          * bytes long. */
         txt     = p_dispwidget->gfx_widgets_status_text;
         txt_len = p_dispwidget->gfx_widgets_status_text_len;
      }

      txt_width    = font_driver_get_message_width(
            p_dispwidget->gfx_widget_fonts.regular.font,
            txt, txt_len, 1.0f);
      total_width  = txt_width
         + p_dispwidget->simple_widget_padding * 2;

      status_txt_x = top_right_x_advance
         - p_dispwidget->simple_widget_padding - txt_width;
      /* Ensure that left hand side of text does
       * not bleed off the edge of the screen */
      if (status_txt_x < 0)
         status_txt_x      = 0;

      gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_info->dims,
            top_right_x_advance - total_width,
            0,
            VIDEO_SCALE_PACK(total_width, p_dispwidget->simple_widget_height),
            video_info->dims,
            p_dispwidget->backdrop_orig,
            NULL
            );

      gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
            txt,
            status_txt_x,
            p_dispwidget->simple_widget_height / 2.0f
            + p_dispwidget->gfx_widget_fonts.regular.line_centre_offset,
            video_info->dims,
            TEXT_COLOR_INFO,
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
            video_info->dims,
            p_dispwidget->gfx_widgets_icons_textures[
            MENU_WIDGETS_ICON_PAUSED],
            (onscreen_panels ? p_dispwidget->simple_widget_height : 0),
            top_right_x_advance,
            MSG_PAUSED);

   if (widgets_is_fastmotion)
      top_right_x_advance -= gfx_widgets_draw_indicator(
            p_dispwidget,
            p_disp,
            dispctx,
            userdata,
            video_info->dims,
            p_dispwidget->gfx_widgets_icons_textures[
            MENU_WIDGETS_ICON_FAST_FORWARD],
            (onscreen_panels ? p_dispwidget->simple_widget_height : 0),
            top_right_x_advance,
            MSG_FAST_FORWARD);

   if (widgets_is_rewinding)
      top_right_x_advance -= gfx_widgets_draw_indicator(
            p_dispwidget,
            p_disp,
            dispctx,
            userdata,
            video_info->dims,
            p_dispwidget->gfx_widgets_icons_textures[
            MENU_WIDGETS_ICON_REWIND],
            (onscreen_panels ? p_dispwidget->simple_widget_height : 0),
            top_right_x_advance,
            MSG_REWINDING);

   if (widgets_is_slowmotion)
   {
      top_right_x_advance -= gfx_widgets_draw_indicator(
            p_dispwidget,
            p_disp,
            dispctx,
            userdata,
            video_info->dims,
            p_dispwidget->gfx_widgets_icons_textures[
            MENU_WIDGETS_ICON_SLOW_MOTION],
            (onscreen_panels ? p_dispwidget->simple_widget_height : 0),
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
      unsigned alt_slot = 0;

      for (i = 0; i < p_dispwidget->current_msgs_size; i++)
      {
         disp_widget_msg_t* msg = p_dispwidget->current_msgs[i];

         if (!msg)
            continue;

         if (msg->flags & DISPWIDG_FLAG_TASK)
            gfx_widgets_draw_task_msg(
               p_dispwidget,
               p_disp,
               dispctx,
               msg, userdata,
               video_info->dims,
               msg->alternative_look ? alt_slot++ : 0);
         else
            gfx_widgets_draw_regular_msg(
               p_dispwidget,
               p_disp,
               dispctx,
               msg, userdata,
               video_info->dims);
      }

   }

   /* Ensure all text is flushed */
   gfx_widgets_flush_text(video_info->dims,
         &p_dispwidget->gfx_widget_fonts.regular);
   gfx_widgets_flush_text(video_info->dims,
         &p_dispwidget->gfx_widget_fonts.bold);
   gfx_widgets_flush_text(video_info->dims,
         &p_dispwidget->gfx_widget_fonts.msg_queue);

   /* Unbind fonts */
   font_driver_bind_block(p_dispwidget->gfx_widget_fonts.regular.font, NULL);
   font_driver_bind_block(p_dispwidget->gfx_widget_fonts.bold.font, NULL);
   font_driver_bind_block(p_dispwidget->gfx_widget_fonts.msg_queue.font, NULL);

   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(
            video_st->data, video_info->dims, false, true);
}

void gfx_widgets_frame(void *data)
{
#ifdef HAVE_THREADS
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;
#endif

   gfx_widgets_state_lock();
#ifdef HAVE_THREADS
   /* A frame the wrapper already had queued still says the widgets
    * are active. Once gfx_widgets_deinit() has taken the worker away
    * from them - under this lock, before it frees the fonts - the
    * fonts are not there to draw with; the frame's own flag is the
    * main thread's word from before that. */
   if (     !p_dispwidget->worker
         && p_dispwidget->video_st
         && ((video_driver_state_t*)p_dispwidget->video_st)->thread_wrapper_active)
   {
      gfx_widgets_state_unlock();
      return;
   }
#endif
   gfx_widgets_frame_state(data);
   gfx_widgets_state_unlock();

   /* Nothing gathered may still be waiting when the frame is over */
   gfx_display_flush_batch(disp_get_ptr());
}

static void gfx_widgets_free(dispgfx_widget_t *p_dispwidget)
{
   size_t i;

   p_dispwidget->flags     &= ~DISPGFX_WIDGET_FLAG_INITED;

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->free)
         widget->free();
   }

   /* Kill all running animations */
   gfx_animation_kill_widget_by_tag(
         &p_dispwidget->gfx_widgets_generic_tag);

   /* Purge everything still pending */
   for (;;)
   {
      disp_widget_msg_t *msg_widget = gfx_widgets_pending_pop(p_dispwidget);
      if (!msg_widget)
         break;

      gfx_widgets_msg_queue_free(p_dispwidget, msg_widget);
      free(msg_widget);
   }
   gfx_widgets_task_cmds_discard(p_dispwidget);
   gfx_widgets_task_pending_discard(p_dispwidget);

   /* Purge everything from the list */

   p_dispwidget->current_msgs_size = 0;
   for (i = 0; i < ARRAY_SIZE(p_dispwidget->current_msgs); i++)
   {
      disp_widget_msg_t *msg = p_dispwidget->current_msgs[i];
      if (!msg)
         continue;

      gfx_widgets_msg_queue_free(p_dispwidget, msg);
      free(msg);
      p_dispwidget->current_msgs[i] = NULL;
   }
#ifdef HAVE_THREADS
   slock_free(p_dispwidget->state_lock);
   p_dispwidget->state_lock = NULL;
#endif

   p_dispwidget->msg_queue_tasks_count = 0;

   /* Font */
   video_coord_array_free(
         &p_dispwidget->gfx_widget_fonts.regular.raster_block.carr);
   video_coord_array_free(
         &p_dispwidget->gfx_widget_fonts.bold.raster_block.carr);
   video_coord_array_free(
         &p_dispwidget->gfx_widget_fonts.msg_queue.raster_block.carr);
}

/* Loads the message queue icons from the assets directory. Invalidates
 * any load still in flight; the slots must be empty. */
static void gfx_widgets_load_icons(dispgfx_widget_t *p_dispwidget)
{
   /* Icons */
   static const char
      *gfx_widgets_icons_names[MENU_WIDGETS_ICON_LAST]         = {
         "menu_pause.png",
         "menu_frameskip.png",
         "menu_rewind.png",
         "resume.png",

         "menu_hourglass.png",
         "menu_check.png",
         "menu_add.png",
         "menu_exit.png",

         "menu_info.png",

         "menu_achievements.png"
      };
   size_t i;
   bool supports_rgba = gfx_surface_wants_rgba();

   /* Invalidate any in-flight async icon loads */
   widget_icon_load_gen++;

   /* Start with no-icons layout — text positions are correct for
    * text-only rendering.  When loads complete (immediately on sync
    * platforms, via callback on async), the frame-loop detects
    * non-zero textures and recomputes icon-dependent layout. */
   p_dispwidget->flags &= ~DISPGFX_WIDGET_FLAG_MSG_QUEUE_HAS_ICONS;

   for (i = 0; i < MENU_WIDGETS_ICON_LAST; i++)
   {
      char texpath[PATH_MAX_LENGTH];
      fill_pathname_join_special(texpath,
            p_dispwidget->monochrome_png_path,
            gfx_widgets_icons_names[i],
            sizeof(texpath));
      gfx_display_load_icon(texpath, supports_rgba,
            &p_dispwidget->gfx_widgets_icons_textures[i],
            widget_icon_load_gen, &widget_icon_load_gen);
   }
}

static void gfx_widgets_context_reset(
      dispgfx_widget_t *p_dispwidget,
      gfx_display_t *p_disp,
      settings_t *settings,
      bool is_threaded,
      unsigned dims, bool fullscreen,
      const char *dir_assets, char *font_path)
{
   size_t i;

   gfx_widgets_load_icons(p_dispwidget);

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->context_reset)
         widget->context_reset(is_threaded, dims,
               fullscreen, dir_assets, font_path,
               p_dispwidget->monochrome_png_path,
               p_dispwidget->gfx_widgets_path);
   }

   /* Update scaling/dimensions */
   p_dispwidget->last_video_dims      = dims;
#ifdef HAVE_XMB
   if (p_disp->menu_driver_id == MENU_DRIVER_ID_XMB)
      p_dispwidget->last_scale_factor = gfx_display_get_widget_pixel_scale(
            p_disp, settings, p_dispwidget->last_video_dims, fullscreen);
   else
#endif
      p_dispwidget->last_scale_factor = gfx_display_get_dpi_scale(
                     p_disp, settings, p_dispwidget->last_video_dims,
                     fullscreen, true);

   gfx_widgets_layout(p_disp, p_dispwidget,
         is_threaded, dir_assets, font_path);
   video_driver_monitor_reset();
}

void gfx_widgets_reload_assets(void)
{
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;
   settings_t *settings           = config_get_ptr();
   bool is_threaded               = false;
   size_t i;

   if (     !p_dispwidget->active
         || !(p_dispwidget->flags & DISPGFX_WIDGET_FLAG_INITED))
      return;

   /* The icons, and the widgets that load their own from the assets
    * directory, are reloaded here under the state lock, which a frame
    * drawing the widgets holds: none is mid-draw while a slot changes.
    * Under the wrapper an unload is retired behind the frames that may
    * still name the texture. */
#ifdef HAVE_THREADS
   if (p_dispwidget->video_st)
      is_threaded = ((video_driver_state_t*)
            p_dispwidget->video_st)->thread_wrapper_active;
#endif

   gfx_widgets_state_lock();
   for (i = 0; i < MENU_WIDGETS_ICON_LAST; i++)
      video_driver_texture_unload(
            &p_dispwidget->gfx_widgets_icons_textures[i]);
   gfx_widgets_load_icons(p_dispwidget);

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t *widget = widgets[i];

      /* The others hold live state (popups, badges, a screenshot)
       * that their context_destroy throws away. The load content
       * animation is left out too: its reset loads the core icon
       * synchronously, which under the wrapper waits on the video
       * thread while this one holds the lock that thread draws the
       * widgets under. It loads that icon again when it next runs. */
      if (widget != &gfx_widget_volume)
         continue;
      if (widget->context_destroy)
         widget->context_destroy();
      if (widget->context_reset)
         widget->context_reset(is_threaded,
               p_dispwidget->last_video_dims,
               settings->bools.video_fullscreen,
               settings->paths.directory_assets,
               settings->paths.path_font,
               p_dispwidget->monochrome_png_path,
               p_dispwidget->gfx_widgets_path);
   }
   gfx_widgets_state_unlock();

   /* Fonts are rebuilt by the next layout pass, on the thread that
    * drives the widgets, which retires the old ones */
   retro_atomic_store_release_int(&widget_fonts_reload, 1);
}

bool gfx_widgets_init(
      void *data_disp,
      void *data_anim,
      void *settings_data,
      uintptr_t widgets_active_ptr,
      bool video_is_threaded,
      unsigned dims, bool fullscreen,
      const char *dir_assets, char *font_path)
{
   size_t i;
   dispgfx_widget_t *p_dispwidget              = &dispwidget_st;
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

   for (i = 0; i < 16; i += 4)
   {
      p_dispwidget->msg_queue_bg[i]     = HEX_R(BG_COLOR_DEFAULT);
      p_dispwidget->msg_queue_bg[i + 1] = HEX_G(BG_COLOR_DEFAULT);
      p_dispwidget->msg_queue_bg[i + 2] = HEX_B(BG_COLOR_DEFAULT);
      p_dispwidget->msg_queue_bg[i + 3] = 1.0f;
   }

   if (!(p_dispwidget->flags & DISPGFX_WIDGET_FLAG_INITED))
   {
      char theme_path[PATH_MAX_LENGTH];

      for (i = 0; i < ARRAY_SIZE(widgets); i++)
      {
         const gfx_widget_t* widget = widgets[i];

         if (widget->init)
            widget->init(p_disp, p_anim, video_is_threaded, fullscreen);
      }

      retro_atomic_int_init(&p_dispwidget->msg_queue_head, 0);
      retro_atomic_int_init(&p_dispwidget->msg_queue_tail, 0);
      gfx_widgets_task_cmds_discard(p_dispwidget);
      p_dispwidget->task_pending_size = 0;

      memset(&p_dispwidget->current_msgs[0], 0, sizeof(p_dispwidget->current_msgs));
      p_dispwidget->current_msgs_size = 0;

#ifdef HAVE_THREADS
      retro_atomic_size_init(&p_dispwidget->state_owner, 0);
      p_dispwidget->state_depth       = 0;
      p_dispwidget->state_lock        = slock_new();
#endif

      fill_pathname_join_special(
            p_dispwidget->gfx_widgets_path,
            dir_assets,
            "menu_widgets",
            sizeof(p_dispwidget->gfx_widgets_path)
            );
      fill_pathname_join_special(
            p_dispwidget->xmb_path,
            dir_assets,
            "xmb",
            sizeof(p_dispwidget->xmb_path)
            );
      /* Base path */
      fill_pathname_join_special(p_dispwidget->ozone_path,
            dir_assets,
            "ozone",
            sizeof(p_dispwidget->ozone_path));
      fill_pathname_join_special(p_dispwidget->ozone_regular_font_path,
            p_dispwidget->ozone_path, "regular.ttf",
            sizeof(p_dispwidget->ozone_regular_font_path));
      fill_pathname_join_special(p_dispwidget->ozone_bold_font_path,
            p_dispwidget->ozone_path, "bold.ttf",
            sizeof(p_dispwidget->ozone_bold_font_path));
      fill_pathname_join_special(
            theme_path,
            p_dispwidget->xmb_path,
            "monochrome",
            sizeof(theme_path)
            );
      fill_pathname_join_special(
            p_dispwidget->monochrome_png_path,
            theme_path,
            "png",
            sizeof(p_dispwidget->monochrome_png_path)
            );
      fill_pathname_join_special(p_dispwidget->assets_pkg_dir,
            settings->paths.directory_assets, "pkg",
            sizeof(p_dispwidget->assets_pkg_dir));

      p_dispwidget->flags |= DISPGFX_WIDGET_FLAG_INITED;
   }

   gfx_widgets_context_reset(
         p_dispwidget,
         p_disp,
         settings,
         video_is_threaded,
         dims, fullscreen,
         dir_assets, font_path);

#ifdef HAVE_THREADS
   /* Under the threaded video wrapper the worker that draws the
    * widgets also animates and lays them out */
   p_dispwidget->video_st = video_state_get_ptr();
   p_dispwidget->worker   = ((video_driver_state_t*)
         p_dispwidget->video_st)->thread_wrapper_active;
   gfx_animation_widgets_own(p_dispwidget->worker);
#endif

   return true;

error:
   gfx_widgets_free(p_dispwidget);
   return false;
}

static void gfx_widgets_font_free(gfx_widget_font_data_t *font_data)
{
   if (font_data->font)
      font_driver_free(font_data->font);

   font_data->font        = NULL;
   font_data->usage_count = 0;
}

static void gfx_widgets_context_destroy(dispgfx_widget_t *p_dispwidget)
{
   size_t i;

   /* Icons still uploading land nowhere */
   gfx_display_texture_loads_cancel(p_dispwidget, sizeof(*p_dispwidget));

   for (i = 0; i < ARRAY_SIZE(widgets); i++)
   {
      const gfx_widget_t* widget = widgets[i];

      if (widget->context_destroy)
         widget->context_destroy();
   }

   /* TODO: Dismiss onscreen notifications that have been freed */

   /* Invalidate in-flight async widget icon loads */
   widget_icon_load_gen++;

   /* Textures */
   for (i = 0; i < MENU_WIDGETS_ICON_LAST; i++)
      video_driver_texture_unload(&p_dispwidget->gfx_widgets_icons_textures[i]);

   /* Fonts */
   gfx_widgets_font_free(&p_dispwidget->gfx_widget_fonts.regular);
   gfx_widgets_font_free(&p_dispwidget->gfx_widget_fonts.bold);
   gfx_widgets_font_free(&p_dispwidget->gfx_widget_fonts.msg_queue);
}

/* Cuts every notification widget loose from its task.
 *
 * Must run before p_dispwidget->active goes false and progress pushes
 * stop reaching us. Display widgets persist across driver reinits by
 * default (dispgfx_widget_t.persisting), and a task that finishes
 * while we are inactive is never seen to: its widget would wait for an
 * update that cannot come.
 *
 * Widgets cut loose from a task not seen to finish are marked expired,
 * so they do not linger next to the fresh widget the task spawns on
 * its first push after reinit. */
static void gfx_widgets_detach_tasks(dispgfx_widget_t *p_dispwidget)
{
   size_t i;

   /* Not displayed yet: discarded outright, with the updates nobody
    * will apply. */
   for (;;)
   {
      disp_widget_msg_t *msg_widget = gfx_widgets_pending_pop(p_dispwidget);

      if (!msg_widget)
         break;

      gfx_widgets_msg_queue_free(p_dispwidget, msg_widget);
      free(msg_widget);
   }
   gfx_widgets_task_cmds_discard(p_dispwidget);
   gfx_widgets_task_pending_discard(p_dispwidget);

   for (i = 0; i < p_dispwidget->current_msgs_size; i++)
   {
      disp_widget_msg_t *msg = p_dispwidget->current_msgs[i];

      if (!msg || !msg->task_key)
         continue;

      if (!(msg->flags & DISPWIDG_FLAG_TASK_FINISHED))
         msg->flags |= DISPWIDG_FLAG_EXPIRED;

      msg->task_key = 0;
   }
}

void gfx_widgets_deinit(bool widgets_persisting)
{
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;

#ifdef HAVE_THREADS
   /* Back to the main list: the tweens of widgets that persist carry
    * on under whichever video comes up next, and freeing kills the
    * rest where the widget code looks for them. Under the state lock:
    * a frame the worker is drawing finishes first, and every frame
    * after it sees the worker gone (gfx_widgets_frame()) and draws no
    * widgets, so the fonts freed below are read by nobody. The frames
    * the wrapper still holds were queued while the widgets were
    * active, and would otherwise draw with a font being freed. */
   gfx_widgets_state_lock();
   p_dispwidget->worker = false;
   gfx_animation_widgets_own(false);
   gfx_widgets_state_unlock();
#endif

   gfx_widgets_detach_tasks(p_dispwidget);

   gfx_widgets_context_destroy(p_dispwidget);

   if (!widgets_persisting)
      gfx_widgets_free(p_dispwidget);
}

#ifdef HAVE_TRANSLATE
static retro_atomic_int_t ai_service_overlay_state
   = RETRO_ATOMIC_INT_INITIALIZER(0);

int gfx_widgets_ai_service_overlay_get_state(void)
{
   return (int)retro_atomic_load_acquire_int(&ai_service_overlay_state);
}

void gfx_widgets_ai_service_overlay_set_state(int state)
{
   retro_atomic_store_release_int(&ai_service_overlay_state, state);
}

bool gfx_widgets_ai_service_overlay_load(
      char* buffer, unsigned buffer_len,
      enum image_type_enum image_type)
{
   dispgfx_widget_t *p_dispwidget   = &dispwidget_st;
   if (gfx_widgets_ai_service_overlay_get_state() == 0)
   {
      unsigned dims                 = 0;
      if (!gfx_display_reset_textures_list_buffer(
               &p_dispwidget->ai_service_overlay_texture,
               gfx_display_texture_filter(),
               (void *) buffer, buffer_len, image_type,
               &dims))
         return false;
      p_dispwidget->ai_service_overlay_dims = dims;
      gfx_widgets_ai_service_overlay_set_state(1);
   }
   return true;
}

void gfx_widgets_ai_service_overlay_unload(void)
{
   dispgfx_widget_t *p_dispwidget   = &dispwidget_st;
   if (gfx_widgets_ai_service_overlay_get_state() == 1)
   {
      gfx_display_texture_loads_cancel(&p_dispwidget->ai_service_overlay_texture, sizeof(p_dispwidget->ai_service_overlay_texture));
      video_driver_texture_unload(&p_dispwidget->ai_service_overlay_texture);
      p_dispwidget->ai_service_overlay_texture = 0;
      gfx_widgets_ai_service_overlay_set_state(0);
   }
}
#endif

#ifdef HAVE_THREADS
void gfx_widgets_status_text_to_frame(void *data, char *status_text)
{
   video_frame_info_t *video_info = (video_frame_info_t*)data;

   if (  (   video_info->fps_show
          || video_info->framecount_show
          || video_info->memory_show
          || video_info->core_status_msg_show
          || video_info->time_show
         )
#ifdef HAVE_MENU
       && !((video_info->menu_st_flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE))
#endif
       && !video_info->notifications_hidden
       && *status_text)
      video_thread_status_text(status_text);
   /* Taken: video_driver_frame() writes nothing into widget state */
   *status_text = '\0';
}

void gfx_widgets_worker_step(void *data,
      const char *status_text, size_t status_text_len)
{
   video_frame_info_t *video_info = (video_frame_info_t*)data;
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;

   if (!p_dispwidget->worker)
      return;

   gfx_widgets_state_lock();
   /* The panels' text, carried with the frame */
   if (status_text_len)
   {
      memcpy(p_dispwidget->gfx_widgets_status_text, status_text,
            status_text_len + 1);
      p_dispwidget->gfx_widgets_status_text_len = status_text_len;
   }
   /* What the frame carried, not what the setting says now: this runs
    * on the video thread under the threaded wrapper. */
   gfx_animation_update_widgets(cpu_features_get_time_usec(),
         video_info->menu_ticker_speed,
         video_info->dims);
   /* What the frame carried, not the settings the main thread writes:
    * this runs on the video thread under the threaded wrapper. */
   p_dispwidget->frame_menu_st_flags = (uint16_t)video_info->menu_st_flags;
   gfx_widgets_iterate_frame(
         video_info->dims, video_info->fullscreen,
         video_info->widget_dir_assets,
         (char*)video_info->widget_path_font,
         true);
   gfx_widgets_state_unlock();
}

void gfx_widgets_state_lock(void)
{
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;
   uintptr_t self;

   if (     !p_dispwidget->state_lock
         || !p_dispwidget->video_st
         || !((video_driver_state_t*)
               p_dispwidget->video_st)->thread_wrapper_active)
      return;

   self = sthread_get_current_thread_id();
   if ((uintptr_t)retro_atomic_load_acquire_size(
            &p_dispwidget->state_owner) == self)
   {
      p_dispwidget->state_depth++;
      return;
   }

   slock_lock(p_dispwidget->state_lock);
   retro_atomic_store_release_size(&p_dispwidget->state_owner, (size_t)self);
   p_dispwidget->state_depth = 1;
}

void gfx_widgets_state_unlock(void)
{
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;

   /* Not the owner: the matching lock found the wrapper inactive */
   if ((uintptr_t)retro_atomic_load_acquire_size(
            &p_dispwidget->state_owner) != sthread_get_current_thread_id())
      return;
   if (--p_dispwidget->state_depth)
      return;

   retro_atomic_store_release_size(&p_dispwidget->state_owner, 0);
   slock_unlock(p_dispwidget->state_lock);
}

/* For the threaded video wrapper, before this thread waits on the
 * worker: a draw blocked on the lock would never let the worker reach
 * the command. Returns the depth to hand back to resume, 0 if this
 * thread held nothing. */
unsigned gfx_widgets_state_yield(void)
{
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;
   unsigned depth;

   if (     !p_dispwidget->state_lock
         || (uintptr_t)retro_atomic_load_acquire_size(
            &p_dispwidget->state_owner) != sthread_get_current_thread_id())
      return 0;

   depth                     = p_dispwidget->state_depth;
   p_dispwidget->state_depth = 0;
   retro_atomic_store_release_size(&p_dispwidget->state_owner, 0);
   slock_unlock(p_dispwidget->state_lock);
   return depth;
}

void gfx_widgets_state_resume(unsigned depth)
{
   dispgfx_widget_t *p_dispwidget = &dispwidget_st;

   if (!depth || !p_dispwidget->state_lock)
      return;

   slock_lock(p_dispwidget->state_lock);
   retro_atomic_store_release_size(&p_dispwidget->state_owner,
         (size_t)sthread_get_current_thread_id());
   p_dispwidget->state_depth = depth;
}
#endif

dispgfx_widget_t *dispwidget_get_ptr(void)
{
   return &dispwidget_st;
}

bool gfx_widgets_ready(void)
{
#ifdef HAVE_GFX_WIDGETS
   return dispwidget_st.active;
#else
   return false;
#endif
}
