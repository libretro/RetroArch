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

#include "../gfx_display.h"
#include "../gfx_widgets.h"

#include "../cheevos/cheevos.h"

#define CHEEVO_NOTIFICATION_DURATION      4000

#define CHEEVO_QUEUE_SIZE 8

typedef struct cheevo_popup
{
   char* title;
   char* subtitle;
   uintptr_t badge;
} cheevo_popup;

struct gfx_widget_achievement_popup_state
{
#ifdef HAVE_THREADS
   slock_t* queue_lock;
#endif
   cheevo_popup queue[CHEEVO_QUEUE_SIZE]; /* ptr alignment */
   const dispgfx_widget_t *dispwidget_ptr;
   int queue_read_index;
   int queue_write_index;
   unsigned width;
   unsigned height;
   float timer;   /* float alignment */
   float unfold;
   float y;
};

typedef struct gfx_widget_achievement_popup_state gfx_widget_achievement_popup_state_t;

static gfx_widget_achievement_popup_state_t p_w_achievement_popup_st;

/* Forward declarations */
static void gfx_widget_achievement_popup_start(
   gfx_widget_achievement_popup_state_t* state);
static void gfx_widget_achievement_popup_free_current(
   gfx_widget_achievement_popup_state_t* state);

static bool gfx_widget_achievement_popup_init(
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      bool video_is_threaded, bool fullscreen)
{
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;
   memset(state, 0, sizeof(*state));
   state->dispwidget_ptr   = (const dispgfx_widget_t*)
      dispwidget_get_ptr();

   state->queue_read_index = -1;

   return true;
}

static void gfx_widget_achievement_popup_free_all(gfx_widget_achievement_popup_state_t* state)
{
   if (state->queue_read_index >= 0)
   {
#ifdef HAVE_THREADS
      slock_lock(state->queue_lock);
#endif
      while (state->queue[state->queue_read_index].title)
         gfx_widget_achievement_popup_free_current(state);
#ifdef HAVE_THREADS
      slock_unlock(state->queue_lock);
#endif
   }
}

static void gfx_widget_achievement_popup_free(void)
{
   gfx_widget_achievement_popup_state_t *state = &p_w_achievement_popup_st;

   gfx_widget_achievement_popup_free_all(state);

#ifdef HAVE_THREADS
   slock_free(state->queue_lock);
   state->queue_lock     = NULL;
#endif
   state->dispwidget_ptr = NULL;
}

static void gfx_widget_achievement_popup_context_destroy(void)
{
   gfx_widget_achievement_popup_state_t *state = &p_w_achievement_popup_st;

   gfx_widget_achievement_popup_free_all(state);
}

static void gfx_widget_achievement_popup_frame(void* data, void* userdata)
{
   gfx_widget_achievement_popup_state_t *state = &p_w_achievement_popup_st;

   /* if there's nothing in the queue, just bail */
   if (
             state->queue_read_index < 0 
         || !state->queue[state->queue_read_index].title)
      return;

#ifdef HAVE_THREADS
   slock_lock(state->queue_lock);
#endif

   {
      static float pure_white[16]          = {
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
      };
      const video_frame_info_t* video_info = (const video_frame_info_t*)data;
      const unsigned video_width           = video_info->width;
      const unsigned video_height          = video_info->height;
      gfx_display_t            *p_disp     = (gfx_display_t*)video_info->disp_userdata;
      gfx_display_ctx_driver_t *dispctx    = p_disp->dispctx;
      dispgfx_widget_t* p_dispwidget       = (dispgfx_widget_t*)userdata;
      const unsigned unfold_offet          = ((1.0f - state->unfold) * 
            state->width / 2);
      int scissor_me_timbers               = 0;

      gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);
      gfx_display_set_alpha(pure_white, 1.0f);

      /* Default icon */
      if (!state->queue[state->queue_read_index].badge)
      {
         /* Backdrop */
         gfx_display_draw_quad(
               p_disp,
               video_info->userdata,
               video_width, video_height,
               0, (int)state->y,
               state->height,
               state->height,
               video_width, video_height,
               p_dispwidget->backdrop_orig,
               NULL);

         /* Icon */
         if (p_dispwidget->gfx_widgets_icons_textures[MENU_WIDGETS_ICON_ACHIEVEMENT])
         {
            if (dispctx && dispctx->blend_begin)
               dispctx->blend_begin(video_info->userdata);
            gfx_widgets_draw_icon(
                  video_info->userdata,
                  p_disp,
                  video_width,
                  video_height,
                  state->height,
                  state->height,
                  p_dispwidget->gfx_widgets_icons_textures[
                  MENU_WIDGETS_ICON_ACHIEVEMENT],
                  0,
                  state->y,
                  0.0f, /* rad */
                  1.0f, /* cos(rad)   = cos(0)  = 1.0f */
                  0.0f, /* sine(rad)  = sine(0) = 0.0f */
                  pure_white);
            if (dispctx && dispctx->blend_end)
               dispctx->blend_end(video_info->userdata);
         }
      }
      /* Badge */
      else
      {
         gfx_widgets_draw_icon(
               video_info->userdata,
               p_disp,
               video_width,
               video_height,
               state->height,
               state->height,
               state->queue[state->queue_read_index].badge,
               0,
               state->y,
               0.0f, /* rad */
               1.0f, /* cos(rad)   = cos(0)  = 1.0f */
               0.0f, /* sine(rad)  = sine(0) = 0.0f */
               pure_white);
      }

      /* I _think_ state->unfold changes in another thread */
      scissor_me_timbers = (fabs(state->unfold - 1.0f) > 0.01);
      if (scissor_me_timbers)
         gfx_display_scissor_begin(
               p_disp,
               video_info->userdata,
               video_width,
               video_height,
               state->height,
               0,
               (unsigned)((float)(state->width) * state->unfold),
               state->height);

      /* Backdrop */
      gfx_display_draw_quad(
            p_disp,
            video_info->userdata,
            video_width,
            video_height,
            state->height,
            (int)state->y,
            state->width,
            state->height,
            video_width,
            video_height,
            p_dispwidget->backdrop_orig,
	    NULL);

      /* Title */
      gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
            state->queue[state->queue_read_index].title,
            state->height + p_dispwidget->simple_widget_padding - unfold_offet,
            state->y + p_dispwidget->gfx_widget_fonts.regular.line_height
            + p_dispwidget->gfx_widget_fonts.regular.line_ascender,
            video_width, video_height,
            TEXT_COLOR_FAINT,
            TEXT_ALIGN_LEFT,
            true);

      /* Cheevo name */

      /* TODO: is a ticker necessary ? */
      gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
            state->queue[state->queue_read_index].subtitle,
            state->height + p_dispwidget->simple_widget_padding - unfold_offet,
            state->y + state->height
            - p_dispwidget->gfx_widget_fonts.regular.line_height
            - p_dispwidget->gfx_widget_fonts.regular.line_descender,
            video_width, video_height,
            TEXT_COLOR_INFO,
            TEXT_ALIGN_LEFT,
            true);

      if (scissor_me_timbers)
      {
         gfx_widgets_flush_text(video_width, video_height,
               &p_dispwidget->gfx_widget_fonts.regular);
         if (dispctx && dispctx->scissor_end)
            dispctx->scissor_end(video_info->userdata,
                  video_width, video_height);
      }
   }

#ifdef HAVE_THREADS
   slock_unlock(state->queue_lock);
#endif
}

static void gfx_widget_achievement_popup_free_current(
      gfx_widget_achievement_popup_state_t* state)
{
   if (state->queue[state->queue_read_index].title)
   {
      free(state->queue[state->queue_read_index].title);
      state->queue[state->queue_read_index].title = NULL;
   }

   if (state->queue[state->queue_read_index].subtitle)
   {
      free(state->queue[state->queue_read_index].subtitle);
      state->queue[state->queue_read_index].subtitle = NULL;
   }

   if (state->queue[state->queue_read_index].badge)
   {
      video_driver_texture_unload(&state->queue[state->queue_read_index].badge);
      state->queue[state->queue_read_index].badge = 0;
   }

   state->queue_read_index = (state->queue_read_index + 1) % ARRAY_SIZE(state->queue);
}

static void gfx_widget_achievement_popup_next(void* userdata)
{
   gfx_widget_achievement_popup_state_t *state = &p_w_achievement_popup_st;

#ifdef HAVE_THREADS
   slock_lock(state->queue_lock);
#endif

   if (state->queue_read_index >= 0)
   {
      if (state->queue[state->queue_read_index].title)
         gfx_widget_achievement_popup_free_current(state);

      /* start the next popup (if present) */
      if (state->queue[state->queue_read_index].title)
         gfx_widget_achievement_popup_start(state);
   }

#ifdef HAVE_THREADS
   slock_unlock(state->queue_lock);
#endif
}

static void gfx_widget_achievement_popup_dismiss(void *userdata)
{
   gfx_animation_ctx_entry_t entry;
   gfx_widget_achievement_popup_state_t *state = &p_w_achievement_popup_st;
   const dispgfx_widget_t        *p_dispwidget = (const dispgfx_widget_t*)
      state->dispwidget_ptr;

   /* Slide up animation */
   entry.cb             = gfx_widget_achievement_popup_next;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->y;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = (float)(-(int)(state->height));
   entry.userdata       = NULL;

   gfx_animation_push(&entry);
}

static void gfx_widget_achievement_popup_fold(void *userdata)
{
   gfx_animation_ctx_entry_t entry;
   gfx_widget_achievement_popup_state_t *state = &p_w_achievement_popup_st;
   const dispgfx_widget_t        *p_dispwidget = (const dispgfx_widget_t*)
      state->dispwidget_ptr;

   /* Fold */
   entry.cb             = gfx_widget_achievement_popup_dismiss;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->unfold;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   gfx_animation_push(&entry);
}

static void gfx_widget_achievement_popup_unfold(void *userdata)
{
   gfx_timer_ctx_entry_t timer;
   gfx_animation_ctx_entry_t entry;
   gfx_widget_achievement_popup_state_t *state = &p_w_achievement_popup_st;
   const dispgfx_widget_t        *p_dispwidget = (const dispgfx_widget_t*)
      state->dispwidget_ptr;

   /* Unfold */
   entry.cb             = NULL;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->unfold;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = 1.0f;
   entry.userdata       = NULL;

   gfx_animation_push(&entry);

   /* Wait before dismissing */
   timer.cb       = gfx_widget_achievement_popup_fold;
   timer.duration = MSG_QUEUE_ANIMATION_DURATION + CHEEVO_NOTIFICATION_DURATION;
   timer.userdata = NULL;

   gfx_animation_timer_start(&state->timer, &timer);
}

static void gfx_widget_achievement_popup_start(
   gfx_widget_achievement_popup_state_t* state)
{
   gfx_animation_ctx_entry_t entry;
   const dispgfx_widget_t *p_dispwidget = (const dispgfx_widget_t*)
      state->dispwidget_ptr;
   size_t title_len  = strlen(state->queue[state->queue_read_index].title);
   size_t stitle_len = strlen(state->queue[state->queue_read_index].subtitle);
   state->height     = p_dispwidget->gfx_widget_fonts.regular.line_height * 4;
   state->width      = MAX(
         font_driver_get_message_width(
            p_dispwidget->gfx_widget_fonts.regular.font,
            state->queue[state->queue_read_index].title, title_len,
            1.0f),
         font_driver_get_message_width(
            p_dispwidget->gfx_widget_fonts.regular.font,
            state->queue[state->queue_read_index].subtitle, stitle_len,
            1.0f)
   );
   state->width += p_dispwidget->simple_widget_padding * 2;
   state->y      = (float)(-(int)state->height);
   state->unfold = 0.0f;

   /* Slide down animation */
   entry.cb             = gfx_widget_achievement_popup_unfold;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->y;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   gfx_animation_push(&entry);
}

void gfx_widgets_push_achievement(const char *title, const char* subtitle, const char *badge)
{
   gfx_widget_achievement_popup_state_t *state = &p_w_achievement_popup_st;
   int start_notification = 1;

   /* important - this must be done outside the lock because it has the potential to need to
    * lock the video thread, which may be waiting for the popup queue lock to render popups */
   uintptr_t badge_id = rcheevos_get_badge_texture(badge, 0);

   if (state->queue_read_index < 0)
   {
      /* queue uninitialized */
      memset(&state->queue, 0, sizeof(state->queue));
      state->queue_read_index = 0;

#ifdef HAVE_THREADS
      state->queue_lock = slock_new();
#endif
   }

#ifdef HAVE_THREADS
   slock_lock(state->queue_lock);
#endif

   if (state->queue_write_index == state->queue_read_index)
   {
      if (state->queue[state->queue_write_index].title)
      {
         /* queue full */
#ifdef HAVE_THREADS
         slock_unlock(state->queue_lock);
#endif
         return;
      }

      /* queue empty */
   }
   else
      start_notification = 0; /* notification already being displayed */

   state->queue[state->queue_write_index].badge = badge_id;
   state->queue[state->queue_write_index].title = strdup(title);
   state->queue[state->queue_write_index].subtitle = strdup(subtitle);

   state->queue_write_index = (state->queue_write_index + 1) % ARRAY_SIZE(state->queue);

   if (start_notification)
      gfx_widget_achievement_popup_start(state);

#ifdef HAVE_THREADS
   slock_unlock(state->queue_lock);
#endif
}

const gfx_widget_t gfx_widget_achievement_popup = {
   &gfx_widget_achievement_popup_init,
   &gfx_widget_achievement_popup_free,
   NULL, /* context_reset*/
   &gfx_widget_achievement_popup_context_destroy,
   NULL, /* layout */
   NULL, /* iterate */
   &gfx_widget_achievement_popup_frame
};
