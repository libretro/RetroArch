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

#include "../gfx_widgets.h"
#include "../gfx_animation.h"
#include "../gfx_display.h"
#include "../../retroarch.h"

#define LIBRETRO_MESSAGE_FADE_DURATION MSG_QUEUE_ANIMATION_DURATION

/* Widget state */

enum gfx_widget_libretro_message_status
{
   GFX_WIDGET_LIBRETRO_MESSAGE_IDLE = 0,
   GFX_WIDGET_LIBRETRO_MESSAGE_SLIDE_IN,
   GFX_WIDGET_LIBRETRO_MESSAGE_FADE_IN,
   GFX_WIDGET_LIBRETRO_MESSAGE_WAIT,
   GFX_WIDGET_LIBRETRO_MESSAGE_FADE_OUT
};

struct gfx_widget_libretro_message_state
{
   unsigned bg_width;
   unsigned bg_height;
   unsigned text_padding;
   unsigned text_color;
   unsigned frame_width;

   unsigned message_duration;

   gfx_timer_t timer;   /* float alignment */

   float bg_x;
   float bg_y_start;
   float bg_y_end;
   float text_x;
   float text_y_start;
   float text_y_end;
   float alpha;

   float frame_color[16];

   enum gfx_widget_libretro_message_status status;

   char message[512];
   bool message_updated;
};

typedef struct gfx_widget_libretro_message_state gfx_widget_libretro_message_state_t;

static gfx_widget_libretro_message_state_t p_w_libretro_message_st = {

   0,                                  /* bg_width */
   0,                                  /* bg_height */
   0,                                  /* text_padding */
   0xE0E0E0FF,                         /* text_color */
   0,                                  /* frame_width */

   0,                                  /* message_duration */

   0.0f,                               /* timer */

   0.0f,                               /* bg_x */
   0.0f,                               /* bg_y_start */
   0.0f,                               /* bg_y_end */
   0.0f,                               /* text_x */
   0.0f,                               /* text_y_start */
   0.0f,                               /* text_y_end */
   0.0f,                               /* alpha */

   COLOR_HEX_TO_FLOAT(0x909090, 1.0f), /* frame_color */

   GFX_WIDGET_LIBRETRO_MESSAGE_IDLE,   /* status */

   {'\0'},                             /* message */
   false                               /* message_updated */
};

/* Utilities */

static void gfx_widget_libretro_message_reset(bool cancel_pending)
{
   gfx_widget_libretro_message_state_t *state = &p_w_libretro_message_st;
   uintptr_t alpha_tag                        = (uintptr_t)&state->alpha;
   uintptr_t timer_tag                        = (uintptr_t)&state->timer;

   /* Kill any existing timers/animations */
   gfx_animation_kill_by_tag(&timer_tag);
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Reset status */
   state->status             = GFX_WIDGET_LIBRETRO_MESSAGE_IDLE;
   if (cancel_pending)
      state->message_updated = false;
}

/* Callbacks */

static void gfx_widget_libretro_message_fade_out_cb(void *userdata)
{
   gfx_widget_libretro_message_reset(false);
}

static void gfx_widget_libretro_message_wait_cb(void *userdata)
{
   gfx_widget_libretro_message_state_t *state = (gfx_widget_libretro_message_state_t*)userdata;
   uintptr_t alpha_tag                        = (uintptr_t)&state->alpha;
   gfx_animation_ctx_entry_t animation_entry;

   /* Trigger fade out */
   state->alpha                 = 1.0f;
   animation_entry.easing_enum  = EASING_OUT_QUAD;
   animation_entry.tag          = alpha_tag;
   animation_entry.duration     = LIBRETRO_MESSAGE_FADE_DURATION;
   animation_entry.target_value = 0.0f;
   animation_entry.subject      = &state->alpha;
   animation_entry.cb           = gfx_widget_libretro_message_fade_out_cb;
   animation_entry.userdata     = NULL;

   gfx_animation_push(&animation_entry);
   state->status = GFX_WIDGET_LIBRETRO_MESSAGE_FADE_OUT;
}

static void gfx_widget_libretro_message_slide_in_cb(void *userdata)
{
   gfx_widget_libretro_message_state_t *state = (gfx_widget_libretro_message_state_t*)userdata;
   gfx_timer_ctx_entry_t timer;

   /* Start wait timer */
   state->alpha   = 1.0f;
   timer.duration = state->message_duration;
   timer.cb       = gfx_widget_libretro_message_wait_cb;
   timer.userdata = state;

   gfx_animation_timer_start(&state->timer, &timer);
   state->status = GFX_WIDGET_LIBRETRO_MESSAGE_WAIT;
}

/* Widget interface */

void gfx_widget_set_libretro_message(
      const char *msg, unsigned duration)
{
   dispgfx_widget_t *p_dispwidget             = dispwidget_get_ptr();
   gfx_widget_libretro_message_state_t *state = &p_w_libretro_message_st;
   gfx_widget_font_data_t *font_msg_queue     = &p_dispwidget->gfx_widget_fonts.msg_queue;

   /* Ensure we have a valid message string */
   if (string_is_empty(msg))
      return;

   /* Cache message parameters */
   strlcpy(state->message, msg, sizeof(state->message));
   state->message_duration = duration;

   /* Get background width */
   state->bg_width = (state->text_padding * 2) +
         font_driver_get_message_width(
               font_msg_queue->font, state->message,
               (unsigned)strlen(state->message), 1.0f);

   /* If a 'slide in' animation is already in
    * progress, no further action is required;
    * just let animation continue with the updated
    * message text */
   if (state->status == GFX_WIDGET_LIBRETRO_MESSAGE_SLIDE_IN)
      return;

   /* Signal that message has been updated
    * > Note that we have to defer the triggering
    *   of any animation changes until the next
    *   call of gfx_widget_libretro_message_iterate().
    *   This is because cores will often send messages
    *   during initialisation - i.e. during processes
    *   that take a non-trivial amount of time. In these
    *   cases, updating the animation state here would
    *   result in the following:
    *   - Core starts initialisation/load content
    *   - Message is set, animation is triggered
    *   - Core finishes initialisation/load content,
    *     taking multiple 100's of ms
    *   - On next runloop iterate, animation status
    *     is checked - but because initialisation
    *     took so long, the animation duration has
    *     already elapsed
    *   - Animation 'finishes' immediately, and the
    *     user never sees it... */
   state->message_updated = true;
}

/* Widget layout() */

static void gfx_widget_libretro_message_layout(
      void *data,
      bool is_threaded, const char *dir_assets, char *font_path)
{
   dispgfx_widget_t *p_dispwidget             = (dispgfx_widget_t*)data;
   gfx_widget_libretro_message_state_t *state = &p_w_libretro_message_st;

   unsigned last_video_height                 = p_dispwidget->last_video_height;
   unsigned divider_width                     = p_dispwidget->divider_width_1px;
   gfx_widget_font_data_t *font_msg_queue     = &p_dispwidget->gfx_widget_fonts.msg_queue;

   /* Set values that are independent of message length */
   state->bg_height    = font_msg_queue->line_height * 2;
   state->text_padding = (unsigned)(((float)font_msg_queue->line_height * (2.0f / 3.0f)) + 0.5f);
   state->frame_width  = divider_width;

   state->bg_x         = 0.0f;
   state->bg_y_start   = (float)last_video_height + (float)state->frame_width;
   state->bg_y_end     = (float)last_video_height - (float)state->bg_height;
   state->text_x       = (float)state->text_padding;
   state->text_y_start = state->bg_y_start + ((float)state->bg_height * 0.5f) +
         (float)font_msg_queue->line_centre_offset;
   state->text_y_end   = state->bg_y_end + ((float)state->bg_height * 0.5f) +
         (float)font_msg_queue->line_centre_offset;

   /* Update values that are dependent upon message length */
   state->bg_width     = state->text_padding * 2;

   if (!string_is_empty(state->message))
      state->bg_width += font_driver_get_message_width(
            font_msg_queue->font, state->message,
            (unsigned)strlen(state->message), 1.0f);
}

/* Widget iterate() */

static void gfx_widget_libretro_message_iterate(void *user_data,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded)
{
   gfx_widget_libretro_message_state_t *state = &p_w_libretro_message_st;

   if (state->message_updated)
   {
      enum gfx_widget_libretro_message_status current_status = state->status;
      uintptr_t alpha_tag                                    = (uintptr_t)&state->alpha;
      gfx_animation_ctx_entry_t animation_entry;

      /* In all cases, reset any existing animation */
      gfx_widget_libretro_message_reset(false);

      /* If an animation was already in progress,
       * have to continue from the last active
       * animation phase */
      switch (current_status)
      {
         case GFX_WIDGET_LIBRETRO_MESSAGE_IDLE:
            /* Trigger 'slide in' animation */
            state->alpha                 = 0.0f;
            animation_entry.easing_enum  = EASING_OUT_QUAD;
            animation_entry.tag          = alpha_tag;
            animation_entry.duration     = LIBRETRO_MESSAGE_FADE_DURATION;
            animation_entry.target_value = 1.0f;
            animation_entry.subject      = &state->alpha;
            animation_entry.cb           = gfx_widget_libretro_message_slide_in_cb;
            animation_entry.userdata     = state;

            gfx_animation_push(&animation_entry);
            state->status = GFX_WIDGET_LIBRETRO_MESSAGE_SLIDE_IN;
            break;
         case GFX_WIDGET_LIBRETRO_MESSAGE_FADE_IN:
         case GFX_WIDGET_LIBRETRO_MESSAGE_FADE_OUT:
            {
               /* If we are fading in or out, start
                * a new fade in animation (transitioning
                * from the current alpha value to 1.0) */
               unsigned fade_duration = (unsigned)(((1.0f - state->alpha) *
                     (float)LIBRETRO_MESSAGE_FADE_DURATION) + 0.5f);
               if (fade_duration > LIBRETRO_MESSAGE_FADE_DURATION)
                  fade_duration       = LIBRETRO_MESSAGE_FADE_DURATION;

               /* > If current and final alpha values are the
                *   same, or fade duration is zero, skip
                *   straight to the wait phase */
               if ((state->alpha >= 1.0f) || (fade_duration < 1))
                  gfx_widget_libretro_message_slide_in_cb(state);
               else
               {
                  animation_entry.easing_enum  = EASING_OUT_QUAD;
                  animation_entry.tag          = alpha_tag;
                  animation_entry.duration     = fade_duration;
                  animation_entry.target_value = 1.0f;
                  animation_entry.subject      = &state->alpha;
                  /* Note that 'slide in' and 'fade in' share
                   * the same callback */
                  animation_entry.cb           = gfx_widget_libretro_message_slide_in_cb;
                  animation_entry.userdata     = state;

                  gfx_animation_push(&animation_entry);
                  state->status = GFX_WIDGET_LIBRETRO_MESSAGE_FADE_IN;
               }
            }
            break;
         case GFX_WIDGET_LIBRETRO_MESSAGE_WAIT:
            /* If we are currently waiting, just
             * 'reset' the wait timer */
            gfx_widget_libretro_message_slide_in_cb(state);
            break;
         default:
            /* The only remaining case is
             * GFX_WIDGET_LIBRETRO_MESSAGE_SLIDE_IN,
             * which can never happen (state->message_updated
             * will be false if it is, so this code will
             * not be invoked). If we reach this point, an
             * unknown error has occurred. We have already
             * reset any existing animation, so no further
             * action is required */
            break;
      }

      state->message_updated = false;
   }
}

/* Widget frame() */

static void gfx_widget_libretro_message_frame(void *data, void *user_data)
{
   gfx_widget_libretro_message_state_t *state = &p_w_libretro_message_st;

   if (state->status != GFX_WIDGET_LIBRETRO_MESSAGE_IDLE)
   {
      video_frame_info_t *video_info         = (video_frame_info_t*)data;
      gfx_display_t *p_disp                  = (gfx_display_t*)video_info->disp_userdata;
      dispgfx_widget_t *p_dispwidget         = (dispgfx_widget_t*)user_data;

      unsigned video_width                   = video_info->width;
      unsigned video_height                  = video_info->height;
      void *userdata                         = video_info->userdata;

      gfx_widget_font_data_t *font_msg_queue = &p_dispwidget->gfx_widget_fonts.msg_queue;
      size_t msg_queue_size                  = p_dispwidget->current_msgs_size;
      float *bg_color                        = p_dispwidget->backdrop_orig;
      float bg_alpha;
      float bg_y;

      unsigned text_color;
      float text_alpha;
      float text_y;

      /* Determine status-dependent opacity/position
       * values */
      switch (state->status)
      {
         case GFX_WIDGET_LIBRETRO_MESSAGE_SLIDE_IN:
            bg_alpha   = DEFAULT_BACKDROP * state->alpha;
            text_alpha = state->alpha;
            /* Use 'alpha' to determine the draw offset
             * > Saves having to trigger two animations */
            bg_y       = state->bg_y_start + (state->alpha *
                  (state->bg_y_end - state->bg_y_start));
            text_y     = state->text_y_start + (state->alpha *
                  (state->text_y_end - state->text_y_start));
            break;
         case GFX_WIDGET_LIBRETRO_MESSAGE_FADE_IN:
         case GFX_WIDGET_LIBRETRO_MESSAGE_FADE_OUT:
            bg_alpha   = DEFAULT_BACKDROP * state->alpha;
            text_alpha = state->alpha;
            bg_y       = state->bg_y_end;
            text_y     = state->text_y_end;
            break;
         case GFX_WIDGET_LIBRETRO_MESSAGE_WAIT:
            bg_alpha   = DEFAULT_BACKDROP;
            text_alpha = 1.0f;
            bg_y       = state->bg_y_end;
            text_y     = state->text_y_end;
            break;
         default:
            bg_alpha   = 0.0f;
            text_alpha = 0.0f;
            bg_y       = state->bg_y_start;
            text_y     = state->text_y_start;
            break;
      }

      /* Draw background */
      if (bg_alpha > 0.0f)
      {
         /* Set opacity
          * > Note: Background and frame have the
          *   same opacity */
         gfx_display_set_alpha(bg_color, bg_alpha);
         gfx_display_set_alpha(state->frame_color, bg_alpha);

         /* Background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               state->bg_x,
               bg_y,
               state->bg_width,
               state->bg_height,
               video_width,
               video_height,
               bg_color, 
               NULL);

         /* Frame */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               state->bg_x,
               bg_y - (float)state->frame_width,
               state->bg_width + state->frame_width,
               state->frame_width,
               video_width,
               video_height,
               state->frame_color,
               NULL);

         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               state->bg_x + (float)state->bg_width,
               bg_y,
               state->frame_width,
               state->bg_height,
               video_width,
               video_height,
               state->frame_color,
               NULL);
      }

      /* Draw text */
      if (text_alpha > 0.0f)
      {
         /* Set opacity */
         text_color = COLOR_TEXT_ALPHA(state->text_color,
               (unsigned)(text_alpha * 255.0f));

         /* Draw message */
         gfx_widgets_draw_text(
               font_msg_queue,
               state->message,
               state->text_x,
               text_y,
               video_width,
               video_height,
               text_color,
               TEXT_ALIGN_LEFT,
               true);

         /* If the message queue is active, must flush the
          * text here to avoid overlaps */
         if (msg_queue_size > 0)
            gfx_widgets_flush_text(video_width, video_height,
                  font_msg_queue);
      }
   }
}

/* Widget free() */

static void gfx_widget_libretro_message_free(void)
{
   gfx_widget_libretro_message_reset(true);
}

/* Widget definition */

const gfx_widget_t gfx_widget_libretro_message = {
   NULL, /* init */
   gfx_widget_libretro_message_free,
   NULL, /* context_reset*/
   NULL, /* context_destroy */
   gfx_widget_libretro_message_layout,
   gfx_widget_libretro_message_iterate,
   gfx_widget_libretro_message_frame
};
