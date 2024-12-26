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
   char* badge_name;
   uintptr_t badge;
   retro_time_t badge_retry;
} cheevo_popup;

enum
{
   ANCHOR_LEFT = 0,
   ANCHOR_CENTER,
   ANCHOR_RIGHT
};

enum
{
   ANCHOR_TOP = 0,
   ANCHOR_BOTTOM
};

struct gfx_widget_achievement_popup_state
{
#ifdef HAVE_THREADS
   slock_t* queue_lock;
#endif
   cheevo_popup queue[CHEEVO_QUEUE_SIZE]; /* ptr alignment */
   const dispgfx_widget_t* dispwidget_ptr;
   int queue_read_index;
   int queue_write_index;
   unsigned width;         /* Width of popup (in pixels), set in _start (length of strings) */
   unsigned height;        /* Height of popup (in pixels), set in _start (4 x line height) */
   float timer;            /* For delay of CHEEVO_NOTIFICATION_DURATION before starting _fold */
   float unfold;           /* Progress of unfolding animation, changes in _unfold and _fold */
   float slide_h;          /* Progress of horizontal sliding animation, changes in _unfold and _fold (stay centered) */
   float slide_v;          /* Progress of vertical sliding animation, changes in _start and _dismiss (slide on/off screen) */

   /* Values copied from user config in _start to prevent accessing every _frame */
   float target_h;         /* Horizontal sliding target, 0.0 to 0.5, convert to screen-space before use */
   float target_v;         /* Vertical sliding target, 0.0 to 0.5, convert to screen-space before use */
   uint8_t anchor_h;       /* Horizontal anchor */
   uint8_t anchor_v;       /* Vertical anchor */
   bool padding_auto;      /* Should we use target h/v or grab pixel values from p_dispwidget? */
};

typedef struct gfx_widget_achievement_popup_state gfx_widget_achievement_popup_state_t;

static gfx_widget_achievement_popup_state_t p_w_achievement_popup_st;

/* Forward declarations */
static void gfx_widget_achievement_popup_start(
   gfx_widget_achievement_popup_state_t* state);
static void gfx_widget_achievement_popup_free_current(
   gfx_widget_achievement_popup_state_t* state);

static bool gfx_widget_achievement_popup_init(gfx_display_t* p_disp,
   gfx_animation_t* p_anim, bool video_is_threaded, bool fullscreen)
{
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;
   memset(state, 0, sizeof(*state));
   state->dispwidget_ptr = (const dispgfx_widget_t*)
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
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;

   gfx_widget_achievement_popup_free_all(state);

#ifdef HAVE_THREADS
   slock_free(state->queue_lock);
   state->queue_lock = NULL;
#endif
   state->dispwidget_ptr = NULL;
}

static void gfx_widget_achievement_popup_context_destroy(void)
{
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;

   gfx_widget_achievement_popup_free_all(state);
}

static void gfx_widget_achievement_popup_frame(void* data, void* userdata)
{
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;

   /* if there's nothing in the queue, just bail */
   if (state->queue_read_index < 0
      || !state->queue[state->queue_read_index].title)
      return;

#ifdef HAVE_THREADS
   slock_lock(state->queue_lock);
#endif

   {
      static float pure_white[16] = {
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
      };
      const video_frame_info_t* video_info = (const video_frame_info_t*)data;
      const unsigned video_width           = video_info->width;
      const unsigned video_height          = video_info->height;
      gfx_display_t* p_disp                = (gfx_display_t*)video_info->disp_userdata;
      gfx_display_ctx_driver_t* dispctx    = p_disp->dispctx;
      dispgfx_widget_t* p_dispwidget       = (dispgfx_widget_t*)userdata;

      unsigned text_unfold_offset = 0;
      bool is_folding = false;
      unsigned screen_padding_x = 0;
      unsigned screen_padding_y = 0;
      int screen_pos_x = 0;
      int screen_pos_y = 0;

      /* Slight additional offset for title/subtitle while unfolding */
      text_unfold_offset = ((1.0f - state->unfold) * state->width) * 0.5;

      /* Whether gfx scissoring should occur, partially hiding popup */
      is_folding = fabs(state->unfold - 1.0f) > 0.01;

      /* Calculate padding in screen space */
      if (state->padding_auto)
      {
         screen_padding_x = p_dispwidget->msg_queue_rect_start_x -
            p_dispwidget->msg_queue_icon_size_x;
         screen_padding_y = screen_padding_x;
      }
      else
      {
         screen_padding_x = state->target_h * video_width;
         screen_padding_y = state->target_v * video_height;
      }

      /* Initial horizontal position, then apply animated offset */
      switch (state->anchor_h)
      {
         case ANCHOR_LEFT:
            screen_pos_x = screen_padding_x;
            break;
         case ANCHOR_CENTER:
            screen_pos_x = (video_width - state->height) * 0.5;
            screen_pos_x -= (state->width / 2.0f) * state->slide_h;
            break;
         case ANCHOR_RIGHT:
            screen_pos_x = video_width - state->height - screen_padding_x;
            screen_pos_x -= state->width * state->slide_h;
            break;
      }

      /* Initial vertical position (off-screen), then apply animated offset */
      switch (state->anchor_v)
      {
         case ANCHOR_TOP:
            screen_pos_y = -(state->height);
            screen_pos_y += (screen_padding_y + state->height) * state->slide_v;
            break;
         case ANCHOR_BOTTOM:
            screen_pos_y = video_height;
            screen_pos_y -= (screen_padding_y + state->height) * state->slide_v;
            break;
      }

      gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);
      gfx_display_set_alpha(pure_white, 1.0f);

      /* badge wasn't ready, periodically see if it's become available */
      if (!state->queue[state->queue_read_index].badge &&
         state->queue[state->queue_read_index].badge_name)
      {
         const retro_time_t next_try = state->queue[state->queue_read_index].badge_retry;
         const retro_time_t now = cpu_features_get_time_usec();
         if (next_try == 0 || now > next_try)
         {
            /* try again in 250ms */
            state->queue[state->queue_read_index].badge_retry = now + 250000;
            state->queue[state->queue_read_index].badge =
               rcheevos_get_badge_texture(state->queue[state->queue_read_index].badge_name, false, false);
         }
      }

      /* Default Badge */
      if (!state->queue[state->queue_read_index].badge)
      {
         /* Backdrop */
         gfx_display_draw_quad(
            p_disp,
            video_info->userdata,
            video_width,
            video_height,
            screen_pos_x,
            screen_pos_y,
            state->height,
            state->height,
            video_width,
            video_height,
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
               screen_pos_x,
               screen_pos_y,
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
            screen_pos_x,
            screen_pos_y,
            0.0f, /* rad */
            1.0f, /* cos(rad)   = cos(0)  = 1.0f */
            0.0f, /* sine(rad)  = sine(0) = 0.0f */
            pure_white);
      }

      if (is_folding)
      {
         gfx_display_scissor_begin(
            p_disp,
            video_info->userdata,
            video_width,
            video_height,
            screen_pos_x + state->height,
            screen_pos_y,
            (unsigned)((float)(state->width) * state->unfold),
            state->height);
      }

      /* Backdrop */
      gfx_display_draw_quad(
         p_disp,
         video_info->userdata,
         video_width,
         video_height,
         screen_pos_x + state->height,
         screen_pos_y,
         state->width,
         state->height,
         video_width,
         video_height,
         p_dispwidget->backdrop_orig,
         NULL);

      /* Title */
      gfx_widgets_draw_text(
         &p_dispwidget->gfx_widget_fonts.regular,
         state->queue[state->queue_read_index].title,
         screen_pos_x + state->height - text_unfold_offset
         + p_dispwidget->simple_widget_padding,
         screen_pos_y
         + p_dispwidget->gfx_widget_fonts.regular.line_height
         + p_dispwidget->gfx_widget_fonts.regular.line_ascender,
         video_width,
         video_height,
         TEXT_COLOR_FAINT,
         TEXT_ALIGN_LEFT,
         true);

      /* Subtitle */

      /* TODO: is a ticker necessary ? */
      gfx_widgets_draw_text(
         &p_dispwidget->gfx_widget_fonts.regular,
         state->queue[state->queue_read_index].subtitle,
         screen_pos_x + state->height - text_unfold_offset
         + p_dispwidget->simple_widget_padding,
         screen_pos_y + state->height
         - p_dispwidget->gfx_widget_fonts.regular.line_height
         - p_dispwidget->gfx_widget_fonts.regular.line_descender,
         video_width,
         video_height,
         TEXT_COLOR_INFO,
         TEXT_ALIGN_LEFT,
         true);

      if (is_folding)
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

   if (state->queue[state->queue_read_index].badge_name)
   {
      free(state->queue[state->queue_read_index].badge_name);
      state->queue[state->queue_read_index].badge_name = NULL;
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
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;

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

static void gfx_widget_achievement_popup_dismiss(void* userdata)
{
   gfx_animation_ctx_entry_t entry;
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;
   const dispgfx_widget_t* p_dispwidget = (const dispgfx_widget_t*)
      state->dispwidget_ptr;

   /* Slide off-screen */
   entry.cb             = gfx_widget_achievement_popup_next;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->slide_v;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   gfx_animation_push(&entry);
}

static void gfx_widget_achievement_popup_fold(void* userdata)
{
   gfx_animation_ctx_entry_t anim_fold;
   gfx_animation_ctx_entry_t anim_slide;
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;
   const dispgfx_widget_t* p_dispwidget = (const dispgfx_widget_t*)
      state->dispwidget_ptr;

   /* Fold */
   anim_fold.cb           = gfx_widget_achievement_popup_dismiss;
   anim_fold.duration     = MSG_QUEUE_ANIMATION_DURATION;
   anim_fold.easing_enum  = EASING_OUT_QUAD;
   anim_fold.subject      = &state->unfold;
   anim_fold.tag          = p_dispwidget->gfx_widgets_generic_tag;
   anim_fold.target_value = 0.0f;
   anim_fold.userdata     = NULL;

   gfx_animation_push(&anim_fold);

   /* Slide horizontal (if required) */
   if (state->anchor_h != ANCHOR_LEFT)
   {
      anim_slide.cb           = NULL;
      anim_slide.duration     = MSG_QUEUE_ANIMATION_DURATION;
      anim_slide.easing_enum  = EASING_OUT_QUAD;
      anim_slide.subject      = &state->slide_h;
      anim_slide.tag          = p_dispwidget->gfx_widgets_generic_tag;
      anim_slide.target_value = 0.0f;
      anim_slide.userdata     = NULL;

      gfx_animation_push(&anim_slide);
   }
}

static void gfx_widget_achievement_popup_unfold(void* userdata)
{
   gfx_timer_ctx_entry_t timer;
   gfx_animation_ctx_entry_t anim_unfold;
   gfx_animation_ctx_entry_t anim_slide;
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;
   const dispgfx_widget_t* p_dispwidget = (const dispgfx_widget_t*)
      state->dispwidget_ptr;

   /* Unfold */
   anim_unfold.cb           = NULL;
   anim_unfold.duration     = MSG_QUEUE_ANIMATION_DURATION;
   anim_unfold.easing_enum  = EASING_OUT_QUAD;
   anim_unfold.subject      = &state->unfold;
   anim_unfold.tag          = p_dispwidget->gfx_widgets_generic_tag;
   anim_unfold.target_value = 1.0f;
   anim_unfold.userdata     = NULL;

   gfx_animation_push(&anim_unfold);

   /* Slide horizontal (if required) */
   if (state->anchor_h != ANCHOR_LEFT)
   {
      anim_slide.cb           = NULL;
      anim_slide.duration     = MSG_QUEUE_ANIMATION_DURATION;
      anim_slide.easing_enum  = EASING_OUT_QUAD;
      anim_slide.subject      = &state->slide_h;
      anim_slide.tag          = p_dispwidget->gfx_widgets_generic_tag;
      anim_slide.target_value = 1.0f;
      anim_slide.userdata     = NULL;

      gfx_animation_push(&anim_slide);
   }

   /* Wait before folding */
   timer.cb       = gfx_widget_achievement_popup_fold;
   timer.duration = MSG_QUEUE_ANIMATION_DURATION + CHEEVO_NOTIFICATION_DURATION;
   timer.userdata = NULL;

   gfx_animation_timer_start(&state->timer, &timer);
}

void gfx_widgets_update_cheevos_appearance(void)
{
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;
   const settings_t* settings = config_get_ptr();
   const float target_h = settings->floats.cheevos_appearance_padding_h;
   const float target_v = settings->floats.cheevos_appearance_padding_v;
   const bool autopadding = settings->bools.cheevos_appearance_padding_auto;
   const unsigned anchor = settings->uints.cheevos_appearance_anchor;

   state->target_h = target_h;
   state->target_v = target_v;
   state->padding_auto = autopadding;

   if (anchor == CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER ||
      anchor == CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER)
      state->anchor_h = ANCHOR_CENTER;
   else if (anchor == CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT ||
      anchor == CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT)
      state->anchor_h = ANCHOR_RIGHT;
   else
      state->anchor_h = ANCHOR_LEFT;

   if (anchor == CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT ||
      anchor == CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER ||
      anchor == CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT)
      state->anchor_v = ANCHOR_BOTTOM;
   else
      state->anchor_v = ANCHOR_TOP;
}

static void gfx_widget_achievement_popup_start(
   gfx_widget_achievement_popup_state_t* state)
{
   gfx_animation_ctx_entry_t anim_slide;
   const dispgfx_widget_t* p_dispwidget = (const dispgfx_widget_t*)
      state->dispwidget_ptr;

   state->height = p_dispwidget->gfx_widget_fonts.regular.line_height * 4.0f;
   state->width = MAX(
      font_driver_get_message_width(
         p_dispwidget->gfx_widget_fonts.regular.font,
         state->queue[state->queue_read_index].title, 0, 1.0f),
      font_driver_get_message_width(
         p_dispwidget->gfx_widget_fonts.regular.font,
         state->queue[state->queue_read_index].subtitle, 0, 1.0f)
   );
   state->width += p_dispwidget->simple_widget_padding * 2;
   state->unfold = 0.0f;
   state->slide_h = 0.0f;
   state->slide_v = 0.0f;

   /* Store settings to prevent lookup every _frame */
   gfx_widgets_update_cheevos_appearance();

   /* Slide vertical onto screen */
   anim_slide.cb = gfx_widget_achievement_popup_unfold;
   anim_slide.duration = MSG_QUEUE_ANIMATION_DURATION;
   anim_slide.easing_enum = EASING_OUT_QUAD;
   anim_slide.subject = &state->slide_v;
   anim_slide.tag = p_dispwidget->gfx_widgets_generic_tag;
   anim_slide.target_value = 1.0f;
   anim_slide.userdata = NULL;

   gfx_animation_push(&anim_slide);
}

void gfx_widgets_push_achievement(const char* title, const char* subtitle, const char* badge)
{
   gfx_widget_achievement_popup_state_t* state = &p_w_achievement_popup_st;
   int start_notification = 1;

   /* important - this must be done outside the lock because it has the potential to need to
    * lock the video thread, which may be waiting for the popup queue lock to render popups */
   uintptr_t badge_id = rcheevos_get_badge_texture(badge, false, true);

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
   state->queue[state->queue_write_index].badge_name = badge_id ? NULL : strdup(badge);
   state->queue[state->queue_write_index].badge_retry = 0;

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
