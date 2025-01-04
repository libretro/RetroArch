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

#include <features/features_cpu.h>

#include "../gfx_display.h"
#include "../gfx_widgets.h"

#include "../../cheevos/cheevos.h"

#define CHEEVO_LBOARD_ARRAY_SIZE 4
#define CHEEVO_CHALLENGE_ARRAY_SIZE 8

#define CHEEVO_LBOARD_DISPLAY_PADDING 3

#define CHEEVO_PROGRESS_TRACKER_DURATION 2000

struct leaderboard_display_info
{
   unsigned id;
   unsigned width;
   char display[24]; /* should never exceed 12 bytes, but aligns the structure at 32 bytes */
};

struct challenge_display_info
{
   unsigned id;
   uintptr_t image;
};

struct progress_tracker_info
{
   uintptr_t image;
   unsigned width;
   char display[32];
   retro_time_t show_until;
};

#define CHEEVO_LBOARD_FIRST_FIXED_CHAR 0x2D /* -./0123456789: */
#define CHEEVO_LBOARD_LAST_FIXED_CHAR 0x3A

/* TODO: rename; this file handles all achievement tracker information, not just leaderboards */
struct gfx_widget_leaderboard_display_state
{
#ifdef HAVE_THREADS
   slock_t* array_lock;
#endif
   const dispgfx_widget_t *dispwidget_ptr;
   struct leaderboard_display_info tracker_info[CHEEVO_LBOARD_ARRAY_SIZE];
   struct challenge_display_info challenge_info[CHEEVO_CHALLENGE_ARRAY_SIZE];
   struct progress_tracker_info progress_tracker;
   unsigned tracker_count;
   unsigned challenge_count;
   uint16_t char_width[CHEEVO_LBOARD_LAST_FIXED_CHAR - CHEEVO_LBOARD_FIRST_FIXED_CHAR + 1];
   uint16_t fixed_char_width;
   uint16_t loading;
   bool disconnected;
};

typedef struct gfx_widget_leaderboard_display_state gfx_widget_leaderboard_display_state_t;

static gfx_widget_leaderboard_display_state_t p_w_leaderboard_display_st;

static bool gfx_widget_leaderboard_display_init(
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      bool video_is_threaded, bool fullscreen)
{
   gfx_widget_leaderboard_display_state_t *state =
      &p_w_leaderboard_display_st;
   memset(state, 0, sizeof(*state));
   state->dispwidget_ptr   = (const dispgfx_widget_t*)
      dispwidget_get_ptr();

   return true;
}

static void gfx_widget_leaderboard_display_free(void)
{
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;

   state->tracker_count   = 0;
   state->challenge_count = 0;
#ifdef HAVE_THREADS
   slock_free(state->array_lock);
   state->array_lock      = NULL;
#endif
   state->dispwidget_ptr  = NULL;
}

static void gfx_widget_leaderboard_display_context_destroy(void)
{
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;
   state->tracker_count   = 0;
   state->challenge_count = 0;
}

static void gfx_widget_leaderboard_display_frame(void* data, void* userdata)
{
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;

   /* if there's nothing to display, just bail */
   if (state->tracker_count == 0 &&
       state->challenge_count == 0 &&
       state->progress_tracker.show_until == 0 &&
       !state->loading &&
       !state->disconnected)
      return;

#ifdef HAVE_THREADS
   slock_lock(state->array_lock);
#endif

   {
      static float pure_white[16] = {
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
      };
      unsigned i, x;
      dispgfx_widget_t         *p_dispwidget = (dispgfx_widget_t*)userdata;
      const video_frame_info_t *video_info   = (const video_frame_info_t*)data;
      gfx_display_t *p_disp                  = (gfx_display_t*)video_info->disp_userdata;
      const unsigned video_width             = video_info->width;
      const unsigned video_height            = video_info->height;
      const unsigned spacing                 = MIN(video_width, video_height) / 64;
      const unsigned widget_height           = p_dispwidget->gfx_widget_fonts.regular.line_height + (CHEEVO_LBOARD_DISPLAY_PADDING - 1) * 2;
      unsigned y                             = video_height;
      char buffer[2] = "0";
      const char* ptr;
      float char_x, char_y;

      gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);
      gfx_display_set_alpha(pure_white, 1.0f);

      for (i = 0; i < state->tracker_count; ++i)
      {
         const unsigned widget_width = state->tracker_info[i].width;
         x                           = video_width - widget_width - spacing;
         y                          -= (widget_height + spacing);

         /* Backdrop */
         gfx_display_draw_quad(
               p_disp,
               video_info->userdata,
               video_width, video_height,
               (int)x, (int)y, widget_width, widget_height,
               video_width, video_height,
               p_dispwidget->backdrop_orig,
               NULL);

         /* Text */
         char_x = (float)(x + CHEEVO_LBOARD_DISPLAY_PADDING);
         char_y = (float)(y + widget_height - (CHEEVO_LBOARD_DISPLAY_PADDING - 1)
               - p_dispwidget->gfx_widget_fonts.regular.line_descender);

         ptr = state->tracker_info[i].display;
         while (*ptr)
         {
            float next_char_x = char_x + state->fixed_char_width;
            const char c = *ptr++;
            if (c >= CHEEVO_LBOARD_FIRST_FIXED_CHAR && c <= CHEEVO_LBOARD_LAST_FIXED_CHAR)
            {
               unsigned char_width = state->char_width[c - CHEEVO_LBOARD_FIRST_FIXED_CHAR];
               if (c >= '0' && c <= '9')
               {
                  float padding = (float)(state->fixed_char_width - char_width) / 2.0;
                  char_x += padding;
               }
               else
                  next_char_x = char_x + char_width;
            }

            buffer[0] = c;
            gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
                  buffer, char_x, char_y,
                  video_width, video_height,
                  TEXT_COLOR_INFO, TEXT_ALIGN_LEFT, true);

            char_x = next_char_x;
         }
      }

      if (state->challenge_count)
      {
         const unsigned widget_size = spacing * 4;

         x = video_width;
         y -= (widget_size + spacing);

         for (i = 0; i < state->challenge_count; ++i)
         {
            x -= (widget_size + spacing);

            if (!state->challenge_info[i].image)
            {
               /* default icon */
               if (p_dispwidget->gfx_widgets_icons_textures[
                     MENU_WIDGETS_ICON_ACHIEVEMENT])
               {
                  gfx_display_ctx_driver_t* dispctx = p_disp->dispctx;
                  if (dispctx && dispctx->blend_begin)
                     dispctx->blend_begin(video_info->userdata);

                  gfx_widgets_draw_icon(
                        video_info->userdata,
                        p_disp,
                        video_width,
                        video_height,
                        widget_size,
                        widget_size,
                        p_dispwidget->gfx_widgets_icons_textures[
                              MENU_WIDGETS_ICON_ACHIEVEMENT],
                        x,
                        y,
                        0.0f, /* rad */
                        1.0f, /* cos(rad)   = cos(0)  = 1.0f */
                        0.0f, /* sine(rad)  = sine(0) = 0.0f */
                        pure_white);

                  if (dispctx && dispctx->blend_end)
                     dispctx->blend_end(video_info->userdata);
               }
            }
            else
            {
               /* achievement badge */
               gfx_widgets_draw_icon(
                     video_info->userdata,
                     p_disp,
                     video_width,
                     video_height,
                     widget_size,
                     widget_size,
                     state->challenge_info[i].image,
                     x,
                     y,
                     0.0f, /* rad */
                     1.0f, /* cos(rad)   = cos(0)  = 1.0f */
                     0.0f, /* sine(rad)  = sine(0) = 0.0f */
                     pure_white);
            }
         }
      }

      if (state->progress_tracker.show_until)
      {
         retro_time_t now = cpu_features_get_time_usec();
         if (now >= state->progress_tracker.show_until)
         {
            gfx_widget_set_achievement_progress(NULL, NULL);
         }
         else
         {
            const unsigned image_size = spacing * 4;
            const unsigned tracker_height = image_size + spacing;
            const unsigned tracker_width = state->progress_tracker.width + image_size + spacing * 2;
            x = video_width - tracker_width - spacing;
            y -= (tracker_height + spacing);

            /* Backdrop */
            gfx_display_draw_quad(
                  p_disp,
                  video_info->userdata,
                  video_width, video_height,
                  (int)x, (int)y, tracker_width, tracker_height,
                  video_width, video_height,
                  p_dispwidget->backdrop_orig,
                  NULL);

            x += spacing / 2;
            y += spacing / 2;

            if (!state->progress_tracker.image)
            {
               /* default icon */
               if (p_dispwidget->gfx_widgets_icons_textures[
                     MENU_WIDGETS_ICON_ACHIEVEMENT])
               {
                  gfx_display_ctx_driver_t* dispctx = p_disp->dispctx;
                  if (dispctx && dispctx->blend_begin)
                     dispctx->blend_begin(video_info->userdata);

                  gfx_widgets_draw_icon(
                        video_info->userdata,
                        p_disp,
                        video_width,
                        video_height,
                        image_size,
                        image_size,
                        p_dispwidget->gfx_widgets_icons_textures[
                              MENU_WIDGETS_ICON_ACHIEVEMENT],
                        x,
                        y,
                        0.0f, /* rad */
                        1.0f, /* cos(rad)   = cos(0)  = 1.0f */
                        0.0f, /* sine(rad)  = sine(0) = 0.0f */
                        pure_white);

                  if (dispctx && dispctx->blend_end)
                     dispctx->blend_end(video_info->userdata);
               }
            }
            else
            {
               /* achievement badge */
               gfx_widgets_draw_icon(
                     video_info->userdata,
                     p_disp,
                     video_width,
                     video_height,
                     image_size,
                     image_size,
                     state->progress_tracker.image,
                     x,
                     y,
                     0.0f, /* rad */
                     1.0f, /* cos(rad)   = cos(0)  = 1.0f */
                     0.0f, /* sine(rad)  = sine(0) = 0.0f */
                     pure_white);
            }

            x += image_size + spacing;
            y = (float)y + image_size / 2 + p_dispwidget->gfx_widget_fonts.regular.line_height / 2 - p_dispwidget->gfx_widget_fonts.regular.line_descender;
            gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
                  state->progress_tracker.display, x, y,
                  video_width, video_height,
                  TEXT_COLOR_INFO, TEXT_ALIGN_LEFT, true);
         }
      }

      if (state->disconnected || state->loading)
      {
         char loading_buffer[8] = "RA ...";
         const char *disconnected_text = state->disconnected ? "! RA !" : loading_buffer;
         const unsigned disconnect_widget_width = font_driver_get_message_width(
            state->dispwidget_ptr->gfx_widget_fonts.msg_queue.font,
            disconnected_text, 0, 1) + CHEEVO_LBOARD_DISPLAY_PADDING * 2;
         const unsigned disconnect_widget_height =
            p_dispwidget->gfx_widget_fonts.msg_queue.line_height + (CHEEVO_LBOARD_DISPLAY_PADDING - 1) * 2;
         x  = video_width - disconnect_widget_width - spacing;
         y -= disconnect_widget_height + spacing;

         if (state->loading)
         {
            const uint16_t loading_shift = 5;
            loading_buffer[((state->loading - 1) >> loading_shift) + 3] = '\0';
            state->loading &= (1 << (loading_shift + 2)) - 1;
            ++state->loading;
         }

         /* Backdrop */
         gfx_display_draw_quad(
            p_disp,
            video_info->userdata,
            video_width, video_height,
            (int)x, (int)y, disconnect_widget_width, disconnect_widget_height,
            video_width, video_height,
            p_dispwidget->backdrop_orig,
            NULL);

         /* Text */
         char_x = (float)(x + CHEEVO_LBOARD_DISPLAY_PADDING);
         char_y = (float)(y + disconnect_widget_height - (CHEEVO_LBOARD_DISPLAY_PADDING - 1)
            - p_dispwidget->gfx_widget_fonts.msg_queue.line_descender);

         gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
            disconnected_text, char_x, char_y,
            video_width, video_height,
            TEXT_COLOR_INFO, TEXT_ALIGN_LEFT, true);
      }
   }

#ifdef HAVE_THREADS
   slock_unlock(state->array_lock);
#endif
}

void gfx_widgets_clear_leaderboard_displays(void)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;

#ifdef HAVE_THREADS
   slock_lock(state->array_lock);
#endif

   state->tracker_count = 0;

#ifdef HAVE_THREADS
   slock_unlock(state->array_lock);
#endif
}

void gfx_widgets_set_leaderboard_display(unsigned id, const char* value)
{
   unsigned i;
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;

#ifdef HAVE_THREADS
   slock_lock(state->array_lock);
#endif

   for (i = 0; i < state->tracker_count; ++i)
   {
      if (state->tracker_info[i].id == id)
         break;
   }

   if (i < CHEEVO_LBOARD_ARRAY_SIZE)
   {
      if (value == NULL)
      {
         /* hide display */
         if (i < state->tracker_count)
         {
            --state->tracker_count;
            if (i < state->tracker_count)
            {
               memcpy(&state->tracker_info[i], &state->tracker_info[i + 1],
                     (state->tracker_count - i) * sizeof(state->tracker_info[i]));
            }
         }
      }
      else
      {
         /* calculate fixed width spacing */
         if (state->fixed_char_width == 0)
         {
            char buffer[2] = "0";
            int j = 0;
            for (j = 0; j < (int)ARRAY_SIZE(state->char_width); ++j)
            {
               buffer[0] = (char)(j + CHEEVO_LBOARD_FIRST_FIXED_CHAR);
               state->char_width[j] = (uint16_t)font_driver_get_message_width(
                     state->dispwidget_ptr->gfx_widget_fonts.regular.font,
                     buffer, 0, 1);
               if (state->char_width[j] > state->fixed_char_width)
                  state->fixed_char_width = state->char_width[j];
            }
         }

         /* show or update display */
         if (i == state->tracker_count)
            state->tracker_info[state->tracker_count++].id = id;

         strncpy(state->tracker_info[i].display, value, sizeof(state->tracker_info[i].display));

         {
            unsigned width = CHEEVO_LBOARD_DISPLAY_PADDING * 2;
            const char* ptr = state->tracker_info[i].display;
            while (*ptr)
            {
               const char c = *ptr++;
               if (c >= '0' && c <= '9')
                  width += state->fixed_char_width;
               else if (c >= CHEEVO_LBOARD_FIRST_FIXED_CHAR && c <= CHEEVO_LBOARD_LAST_FIXED_CHAR)
                  width += state->char_width[c - CHEEVO_LBOARD_FIRST_FIXED_CHAR];
               else
                  width += state->fixed_char_width;
            }

            state->tracker_info[i].width = width;
         }
      }
   }

#ifdef HAVE_THREADS
   slock_unlock(state->array_lock);
#endif
}

void gfx_widgets_clear_challenge_displays(void)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;

#ifdef HAVE_THREADS
   slock_lock(state->array_lock);
#endif

   state->challenge_count = 0;

#ifdef HAVE_THREADS
   slock_unlock(state->array_lock);
#endif
}

void gfx_widgets_set_challenge_display(unsigned id, const char* badge)
{
   unsigned i;
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;

   /* important - this must be done outside the lock because it has the potential to need to
    * lock the video thread, which may be waiting for the popup queue lock to render popups */
   uintptr_t badge_id     = badge ? rcheevos_get_badge_texture(badge, false, true) : 0;
   uintptr_t old_badge_id = 0;

#ifdef HAVE_THREADS
   slock_lock(state->array_lock);
#endif

   for (i = 0; i < state->challenge_count; ++i)
   {
      if (state->challenge_info[i].id == id)
         break;
   }

   if (i < CHEEVO_CHALLENGE_ARRAY_SIZE)
   {
      if (badge == NULL)
      {
         /* hide indicator */
         if (i < state->challenge_count)
         {
            old_badge_id = state->challenge_info[i].image;

            --state->challenge_count;
            if (i < state->challenge_count)
            {
               memcpy(&state->challenge_info[i], &state->challenge_info[i + 1],
                  (state->challenge_count - i) * sizeof(state->challenge_info[i]));
            }

            state->challenge_info[state->challenge_count].image = 0;
         }
      }
      else
      {
         /* show indicator */
         if (i == state->challenge_count)
         {
            /* new indicator, assign id */
            state->challenge_info[state->challenge_count++].id = id;
         }
         else if (state->challenge_info[i].image)
         {
            /* existing indicator, free old image */
            old_badge_id = state->challenge_info[i].image;
         }

         state->challenge_info[i].image = badge_id;
      }
   }

#ifdef HAVE_THREADS
   slock_unlock(state->array_lock);
#endif

   if (old_badge_id)
      video_driver_texture_unload(&old_badge_id);
}

void gfx_widget_set_achievement_progress(const char* badge, const char* progress)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;
   uintptr_t old_badge_id = state->progress_tracker.image;

   if (badge == NULL)
   {
      /* hide indicator */
      state->progress_tracker.image = 0;
      state->progress_tracker.show_until = 0;
   }
   else
   {
      /* show indicator */
      state->progress_tracker.show_until = cpu_features_get_time_usec() + CHEEVO_PROGRESS_TRACKER_DURATION * 1000;
      state->progress_tracker.image = rcheevos_get_badge_texture(badge, true, true);

      snprintf(state->progress_tracker.display, sizeof(state->progress_tracker.display), "%s", progress);
      state->progress_tracker.width = (uint16_t)font_driver_get_message_width(
            state->dispwidget_ptr->gfx_widget_fonts.regular.font,
            progress, 0, 1);
   }

   if (old_badge_id)
      video_driver_texture_unload(&old_badge_id);
}

void gfx_widget_set_cheevos_disconnect(bool value)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;
   state->disconnected = value;
}

void gfx_widget_set_cheevos_set_loading(bool value)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;
   state->loading = value ? 1 : 0;
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
