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

#ifdef HAVE_THREADS
#define SLOCK_LOCK(x) slock_lock(x)
#define SLOCK_UNLOCK(x) slock_unlock(x)
#else
#define SLOCK_LOCK(x)
#define SLOCK_UNLOCK(x)
#endif

#define CHEEVO_LBOARD_ARRAY_SIZE 4

#define CHEEVO_LBOARD_DISPLAY_PADDING 3

struct leaderboard_display_info
{
   unsigned id;
   unsigned width;
   char display[24]; /* should never exceed 12 bytes, but aligns the structure at 32 bytes */
};

struct gfx_widget_leaderboard_display_state
{
#ifdef HAVE_THREADS
   slock_t* array_lock;
#endif
   struct leaderboard_display_info info[CHEEVO_LBOARD_ARRAY_SIZE];
   int count;
};

typedef struct gfx_widget_leaderboard_display_state gfx_widget_leaderboard_display_state_t;

static gfx_widget_leaderboard_display_state_t p_w_leaderboard_display_st;

static bool gfx_widget_leaderboard_display_init(bool video_is_threaded, bool fullscreen)
{
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;
   memset(state, 0, sizeof(*state));

   return true;
}

static void gfx_widget_leaderboard_display_free(void)
{
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;

   state->count      = 0;
#ifdef HAVE_THREADS
   slock_free(state->array_lock);
   state->array_lock = NULL;
#endif
}

static void gfx_widget_leaderboard_display_context_destroy(void)
{
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;
   state->count      = 0;
}

static void gfx_widget_leaderboard_display_frame(void* data, void* userdata)
{
   gfx_display_t *p_disp                         = disp_get_ptr();
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;

   /* if there's nothing to display, just bail */
   if (state->count == 0)
      return;

   SLOCK_LOCK(state->array_lock);
   {
      static float pure_white[16] = {
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
      };
      dispgfx_widget_t* p_dispwidget = (dispgfx_widget_t*)userdata;
      const video_frame_info_t* video_info = (const video_frame_info_t*)data;
      const unsigned video_width = video_info->width;
      const unsigned video_height = video_info->height;
      const unsigned spacing = MIN(video_width, video_height) / 64;
      const unsigned widget_height = p_dispwidget->gfx_widget_fonts.regular.line_height + (CHEEVO_LBOARD_DISPLAY_PADDING - 1) * 2;
      unsigned y = video_height;
      unsigned x;
      int i;

      gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);
      gfx_display_set_alpha(pure_white, 1.0f);

      for (i = 0; i < state->count; ++i)
      {
         const unsigned widget_width = state->info[i].width;
         x = video_width - widget_width - spacing;
         y -= (widget_height + spacing);

         /* Backdrop */
         gfx_display_draw_quad(
               p_disp,
               video_info->userdata,
               video_width, video_height,
               (int)x, (int)y, widget_width, widget_height,
               video_width, video_height,
               p_dispwidget->backdrop_orig);

         /* Text */
         gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
            state->info[i].display,
            (float)(x + CHEEVO_LBOARD_DISPLAY_PADDING),
            (float)(y + widget_height - (CHEEVO_LBOARD_DISPLAY_PADDING - 1) - p_dispwidget->gfx_widget_fonts.regular.line_descender),
            video_width, video_height,
            TEXT_COLOR_INFO,
            TEXT_ALIGN_LEFT,
            true);
      }
   }

   SLOCK_UNLOCK(state->array_lock);
}

void gfx_widgets_set_leaderboard_display(unsigned id, const char* value)
{
   int i;
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;

   SLOCK_LOCK(state->array_lock);

   for (i = 0; i < state->count; ++i)
   {
      if (state->info[i].id == id)
         break;
   }

   if (i < CHEEVO_LBOARD_ARRAY_SIZE)
   {
      if (value == NULL)
      {
         /* hide display */
         if (i < state->count)
         {
            --state->count;
            if (i < state->count)
            {
               memcpy(&state->info[i], &state->info[i + 1],
                     (state->count - i) * sizeof(state->info[i]));
            }
         }
      }
      else
      {
         /* show or update display */
         const dispgfx_widget_t* p_dispwidget = (const dispgfx_widget_t*)dispwidget_get_ptr();

         if (i == state->count)
            state->info[state->count++].id = id;

         strncpy(state->info[i].display, value, sizeof(state->info[i].display));
         state->info[i].width = font_driver_get_message_width(
               p_dispwidget->gfx_widget_fonts.regular.font,
               state->info[i].display, 0, 1);
         state->info[i].width += CHEEVO_LBOARD_DISPLAY_PADDING * 2;
      }
   }

   SLOCK_UNLOCK(state->array_lock);
}

const gfx_widget_t gfx_widget_leaderboard_display = {
   &gfx_widget_leaderboard_display_init,
   &gfx_widget_leaderboard_display_free,
   NULL, /* context_reset*/
   &gfx_widget_leaderboard_display_context_destroy,
   NULL, /* layout */
   NULL, /* iterate */
   &gfx_widget_leaderboard_display_frame
};
