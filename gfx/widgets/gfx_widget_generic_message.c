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

struct gfx_widget_generic_message_state
{
   gfx_timer_t timer;
   float alpha;
   char message[256];
};

typedef struct gfx_widget_generic_message_state gfx_widget_generic_message_state_t;

static gfx_widget_generic_message_state_t p_w_generic_message_st = {
   0.0f,
   0.0f,
   {'\0'}
};

static gfx_widget_generic_message_state_t* gfx_widget_generic_message_get_ptr()
{
   return &p_w_generic_message_st;
}

static void gfx_widget_generic_message_fadeout(void *userdata)
{
   gfx_widget_generic_message_state_t* state = gfx_widget_generic_message_get_ptr();
   gfx_animation_ctx_entry_t entry;
   gfx_animation_ctx_tag tag = (uintptr_t) &state->timer;

   /* Start fade out animation */
   entry.cb             = NULL;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->alpha;
   entry.tag            = tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   gfx_animation_push(&entry);
}

void gfx_widget_set_message(char *msg)
{
   gfx_widget_generic_message_state_t* state = gfx_widget_generic_message_get_ptr();
   gfx_timer_ctx_entry_t timer;
   gfx_animation_ctx_tag tag = (uintptr_t) &state->timer;

   if (!gfx_widgets_active())
      return;

   strlcpy(state->message, msg, sizeof(state->message));

   state->alpha = DEFAULT_BACKDROP;

   /* Kill and restart the timer / animation */
   gfx_timer_kill(&state->timer);
   gfx_animation_kill_by_tag(&tag);

   timer.cb       = gfx_widget_generic_message_fadeout;
   timer.duration = GENERIC_MESSAGE_DURATION;
   timer.userdata = NULL;

   gfx_timer_start(&state->timer, &timer);
}

static void gfx_widget_generic_message_frame(void* data)
{
   gfx_widget_generic_message_state_t* state = gfx_widget_generic_message_get_ptr();

   if (state->alpha > 0.0f)
   {
      video_frame_info_t* video_info      = (video_frame_info_t*)data;
      void* userdata                      = video_info->userdata;
      unsigned video_width                = video_info->width;
      unsigned video_height               = video_info->height;

      unsigned height = gfx_widgets_get_generic_message_height();

      unsigned text_color = COLOR_TEXT_ALPHA(0xffffffff, (unsigned)(state->alpha*255.0f));
      gfx_display_set_alpha(gfx_widgets_get_backdrop_orig(), state->alpha);

      gfx_display_draw_quad(userdata,
            video_width, video_height,
            0, video_height - height,
            video_width, height,
            video_width, video_height,
            gfx_widgets_get_backdrop_orig());

      gfx_display_draw_text(gfx_widgets_get_font_regular(), state->message,
         video_width/2,
         video_height - height/2 + gfx_widgets_get_font_size()/4,
         video_width, video_height,
         text_color, TEXT_ALIGN_CENTER,
         1, false, 0, false);
   }
}

static void gfx_widget_generic_message_free(void)
{
   gfx_widget_generic_message_state_t* state = gfx_widget_generic_message_get_ptr();
   state->alpha = 0.0f;
}

const gfx_widget_t gfx_widget_generic_message = {
   NULL, /* init */
   gfx_widget_generic_message_free,
   NULL, /* context_reset*/
   NULL, /* context_destroy */
   NULL, /* layout */
   NULL, /* iterate */
   gfx_widget_generic_message_frame
};
