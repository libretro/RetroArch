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

/* Widget state */

struct gfx_widget_progress_message_state
{
   unsigned widget_width;
   unsigned widget_height;
   unsigned text_width;
   unsigned bar_bg_width;
   unsigned bar_bg_height;

   unsigned bar_max_width;
   unsigned bar_height;

   unsigned priority;

   float timer;      /* float alignment */
   float alpha;
   float widget_x;
   float widget_y;
   float text_x;
   float text_y;
   float bar_bg_x;
   float bar_bg_y;
   float bar_x;
   float bar_y;
   float bar_bg_color[16];
   float bar_color[16];
   float bar_disabled_color[16];

   int8_t progress;
   char message[256];
   bool active;
};

typedef struct gfx_widget_progress_message_state gfx_widget_progress_message_state_t;

static gfx_widget_progress_message_state_t p_w_progress_message_st = {

   0,                                  /* widget_width */
   0,                                  /* widget_height */
   0,                                  /* text_width */
   0,                                  /* bar_bg_width */
   0,                                  /* bar_bg_height */

   0,                                  /* bar_max_width */
   0,                                  /* bar_height */

   0,                                  /* priority */

   0.0f,                               /* timer */
   0.0f,                               /* alpha */
   0.0f,                               /* widget_x */
   0.0f,                               /* widget_y */

   0.0f,                               /* text_x */
   0.0f,                               /* text_y */

   0.0f,                               /* bar_bg_x */
   0.0f,                               /* bar_bg_y */

   0.0f,                               /* bar_x */
   0.0f,                               /* bar_y */

   COLOR_HEX_TO_FLOAT(0x3A3A3A, 1.0f), /* bar_bg_color */
   COLOR_HEX_TO_FLOAT(0x198AC6, 1.0f), /* bar_color */
   COLOR_HEX_TO_FLOAT(0x000000, 1.0f), /* bar_disabled_color */

   -1,                                 /* progress */
   {'\0'},                             /* message */
   false,                              /* active */
};

/* Callbacks */

static void gfx_widget_progress_message_fadeout_cb(void *userdata)
{
   gfx_widget_progress_message_state_t *state = (gfx_widget_progress_message_state_t*)userdata;

   /* Deactivate widget */
   state->active = false;
}

static void gfx_widget_progress_message_fadeout(void *userdata)
{
   gfx_animation_ctx_entry_t animation_entry;
   gfx_widget_progress_message_state_t *state = (gfx_widget_progress_message_state_t*)userdata;
   uintptr_t alpha_tag                        = (uintptr_t)&state->alpha;

   /* Trigger fade out animation */
   animation_entry.easing_enum  = EASING_OUT_QUAD;
   animation_entry.tag          = alpha_tag;
   animation_entry.duration     = MSG_QUEUE_ANIMATION_DURATION;
   animation_entry.target_value = 0.0f;
   animation_entry.subject      = &state->alpha;
   animation_entry.cb           = gfx_widget_progress_message_fadeout_cb;
   animation_entry.userdata     = state;

   gfx_animation_push(&animation_entry);
}

/* Widget interface */

void gfx_widget_set_progress_message(
      const char *message, unsigned duration,
      unsigned priority, int8_t progress)
{
   gfx_timer_ctx_entry_t timer;
   dispgfx_widget_t *p_dispwidget             = dispwidget_get_ptr();
   gfx_widget_progress_message_state_t *state = &p_w_progress_message_st;
   gfx_widget_font_data_t *font_regular       = &p_dispwidget->gfx_widget_fonts.regular;
   uintptr_t alpha_tag                        = (uintptr_t)&state->alpha;
   uintptr_t timer_tag                        = (uintptr_t)&state->timer;

   /* Ensure we have a valid message string */
   if (string_is_empty(message))
      return;

   /* If widget is currently active, ignore new
    * message if it has a lower priority */
   if (state->active && (state->priority > priority))
      return;

   /* Cache message parameters */
   strlcpy(state->message, message, sizeof(state->message));
   state->priority   = priority;
   state->progress   = progress;

   /* Cache text width */
   state->text_width = font_driver_get_message_width(
         font_regular->font,
         state->message,
         (unsigned)strlen(state->message),
         1.0f);

   /* Kill any existing timer/animation */
   gfx_animation_kill_by_tag(&timer_tag);
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Start new message timer */
   timer.duration = duration;
   timer.cb       = gfx_widget_progress_message_fadeout;
   timer.userdata = state;

   gfx_animation_timer_start(&state->timer, &timer);

   /* Set initial widget opacity */
   state->alpha  = 1.0f;

   /* Set 'active' flag */
   state->active = true;
}

/* Widget layout() */

static void gfx_widget_progress_message_layout(
      void *data,
      bool is_threaded, const char *dir_assets, char *font_path)
{
   float bar_padding;
   dispgfx_widget_t *p_dispwidget             = (dispgfx_widget_t*)data;
   gfx_widget_progress_message_state_t *state = &p_w_progress_message_st;
   unsigned last_video_width                  = p_dispwidget->last_video_width;
   unsigned last_video_height                 = p_dispwidget->last_video_height;
   unsigned widget_padding                    = p_dispwidget->simple_widget_padding;
   gfx_widget_font_data_t *font_regular       = &p_dispwidget->gfx_widget_fonts.regular;

   /* Base widget layout */
   state->widget_width                        = last_video_width;
   state->widget_height                       = (unsigned)(((float)font_regular->line_height * 3.3f) + 0.5f);
   state->widget_x                            = 0.0f;
   state->widget_y                            = (float)(last_video_height - state->widget_height);

   /* Text layout */
   state->text_x                              = (float)last_video_width / 2.0f;
   state->text_y                              = (float)(last_video_height -
         font_regular->line_height + font_regular->line_centre_offset);

   /* Progress bar layout */
   state->bar_bg_width                        = last_video_width - (2 * widget_padding);
   state->bar_bg_height                       = (unsigned)(((float)font_regular->line_height * 0.7f) + 0.5f);
   state->bar_bg_x                            = (float)widget_padding;
   state->bar_bg_y                            = (float)last_video_height -
         (float)state->widget_height +
         (((float)state->widget_height - (font_regular->line_height * 1.5f)) * 0.5f) -
         ((float)state->bar_bg_height * 0.5f);

   state->bar_height                          = (unsigned)(((float)font_regular->line_height * 0.5f) + 0.5f);
   bar_padding                                = (float)(state->bar_bg_height - state->bar_height) * 0.5f;
   state->bar_max_width                       = state->bar_bg_width - (unsigned)((bar_padding * 2.0f) + 0.5f);
   state->bar_x                               = state->bar_bg_x + bar_padding;
   state->bar_y                               = state->bar_bg_y + bar_padding;
}

/* Widget frame() */

static void gfx_widget_progress_message_frame(void *data, void *user_data)
{
   gfx_widget_progress_message_state_t *state = &p_w_progress_message_st;

   if (state->active)
   {
      video_frame_info_t *video_info       = (video_frame_info_t*)data;
      dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)user_data;

      unsigned video_width                 = video_info->width;
      unsigned video_height                = video_info->height;
      void *userdata                       = video_info->userdata;
      gfx_display_t *p_disp                = (gfx_display_t*)video_info->disp_userdata;

      float *backdrop_color                = p_dispwidget->backdrop_orig;
      unsigned text_color                  = COLOR_TEXT_ALPHA(0xFFFFFFFF, (unsigned)(state->alpha * 255.0f));

      gfx_widget_font_data_t *font_regular = &p_dispwidget->gfx_widget_fonts.regular;
      size_t msg_queue_size                = p_dispwidget->current_msgs_size;
      unsigned bar_width                   = state->bar_max_width;
      float *bar_color                     = state->bar_disabled_color;

      /* Draw backdrop */
      gfx_display_set_alpha(backdrop_color, state->alpha * DEFAULT_BACKDROP);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            state->widget_x,
            state->widget_y,
            state->widget_width,
            state->widget_height,
            video_width,
            video_height,
            backdrop_color,
            NULL);

      /* Draw progress bar background */
      gfx_display_set_alpha(state->bar_bg_color, state->alpha);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            state->bar_bg_x,
            state->bar_bg_y,
            state->bar_bg_width,
            state->bar_bg_height,
            video_width,
            video_height,
            state->bar_bg_color,
            NULL);

      /* Draw progress bar */
      if (state->progress >= 0)
      {
         bar_width = (unsigned)((((float)state->progress / 100.0f) * (float)state->bar_max_width) + 0.5f);
         if (bar_width > state->bar_max_width)
            bar_width = state->bar_max_width;

         bar_color = state->bar_color;
      }

      gfx_display_set_alpha(bar_color, state->alpha);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            state->bar_x,
            state->bar_y,
            bar_width,
            state->bar_height,
            video_width,
            video_height,
            bar_color,
            NULL);

      /* Draw message text */
      gfx_widgets_draw_text(
            font_regular,
            state->message,
            state->text_x,
            state->text_y,
            video_width,
            video_height,
            text_color,
            TEXT_ALIGN_CENTER,
            true);

      /* If the message queue is active, must flush the
       * text here to avoid overlaps */
      if (msg_queue_size > 0)
         gfx_widgets_flush_text(video_width, video_height, font_regular);
   }
}

/* Widget free() */

static void gfx_widget_progress_message_free(void)
{
   gfx_widget_progress_message_state_t *state = &p_w_progress_message_st;
   uintptr_t alpha_tag                        = (uintptr_t)&state->alpha;
   uintptr_t timer_tag                        = (uintptr_t)&state->timer;

   /* Kill any existing timer / animation */
   gfx_animation_kill_by_tag(&timer_tag);
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Deactivate widget */
   state->alpha  = 0.0f;
   state->active = false;
}

/* Widget definition */

const gfx_widget_t gfx_widget_progress_message = {
   NULL, /* init */
   gfx_widget_progress_message_free,
   NULL, /* context_reset*/
   NULL, /* context_destroy */
   gfx_widget_progress_message_layout,
   NULL, /* iterate */
   gfx_widget_progress_message_frame
};
