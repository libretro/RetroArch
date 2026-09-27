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

#include <compat/strl.h>
#include <features/features_cpu.h>
#include <string/stdstring.h>

#include "../gfx_display.h"
#include "../gfx_widgets.h"
#include <queues/mpsc_stack.h>

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
   char badge_name[8];
   uint8_t hold;      /* see CHEEVO_BADGE_HOLD_FRAMES */
};

/* The tracker is shared by every achievement with measured progress,
 * and a game that advances two of them at once has it change hands
 * every few frames. The badges it has shown are therefore kept, a few
 * of them, so that going back to one costs nothing. */
#define CHEEVO_PROGRESS_TRACKER_BADGES 4

struct progress_tracker_badge
{
   uintptr_t image;   /* the tracker's to unload; 0: entry unused */
   unsigned last_used;
   char badge_name[8];
};

struct progress_tracker_info
{
   uintptr_t image;   /* one of badges[].image, or 0: draw the placeholder */
   unsigned width;
   char display[32];
   char badge_name[8];
   retro_time_t show_until;
   /* An update whose badge is not here yet. What is on screen - the
    * previous progress, or nothing - stays as it is until the badge
    * arrives, so neither the placeholder nor a gap is ever drawn for
    * a badge that is a few frames away. */
   unsigned next_width;
   char next_display[32];
   char next_badge_name[8];
   uint8_t hold;      /* see CHEEVO_BADGE_HOLD_FRAMES; non-zero: an update waits */
   unsigned badge_tick;
   struct progress_tracker_badge badges[CHEEVO_PROGRESS_TRACKER_BADGES];
};

/* A badge on disk reaches the widget a few frames after it is asked
 * for: decoded by a task, uploaded by the video thread, never waited
 * on. Drawing the placeholder icon for those frames reads as a
 * flicker, so a challenge indicator that has just asked for its badge
 * is not drawn until the badge arrives, and a progress update whose
 * badge is new is not applied until it does - for at most this many
 * frames, and not at all when the badge is not coming soon (it is
 * being downloaded, or failed to load): then the placeholder is drawn
 * at once, as before. */
#define CHEEVO_BADGE_HOLD_FRAMES 30

#define CHEEVO_LBOARD_FIRST_FIXED_CHAR 0x2C /* ,-./0123456789: */
#define CHEEVO_LBOARD_LAST_FIXED_CHAR 0x3A

/* TODO: rename; this file handles all achievement tracker information, not just leaderboards */
/* Every mutation of this widget's state arrives from the cheevos
 * thread as a whole command node on the lock-free MPSC stack below,
 * and the draw thread's iterate drains and applies them in arrival
 * order. From then on the arrays, the progress tracker and the two
 * flags are draw-thread-only plain state: the frame renders and the
 * visibility test reads with no lock, and the badge texture fetches,
 * unloads and font-width queries all run on the draw thread, where
 * those calls belong. */
enum lbd_cmd_kind
{
   LBD_CMD_SET_TRACKER = 0,
   LBD_CMD_CLEAR_TRACKERS,
   LBD_CMD_SET_CHALLENGE,
   LBD_CMD_CLEAR_CHALLENGES,
   LBD_CMD_SET_PROGRESS,
   LBD_CMD_SET_DISCONNECT,
   LBD_CMD_SET_LOADING
};

struct lbd_cmd
{
   mpsc_stack_node_t link;  /* first: the stack's, from push to drain */
   unsigned id;
   uint8_t  kind;           /* enum lbd_cmd_kind */
   bool     has_value;      /* SET_*: payload present; the two flag
                             * kinds: the flag's value */
   char     value[32];      /* tracker display / challenge badge /
                             * progress text */
   char     badge[16];      /* SET_PROGRESS: the badge name */
};

static mpsc_stack_t lbd_pending;

struct gfx_widget_leaderboard_display_state
{
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
   mpsc_stack_init(&lbd_pending);

   return true;
}

static void gfx_widget_leaderboard_display_drop_tracker_badges(
      gfx_widget_leaderboard_display_state_t *state)
{
   unsigned i;
   for (i = 0; i < CHEEVO_PROGRESS_TRACKER_BADGES; i++)
   {
      if (state->progress_tracker.badges[i].image)
         video_driver_texture_unload(&state->progress_tracker.badges[i].image);
      state->progress_tracker.badges[i].badge_name[0] = '\0';
   }
   state->progress_tracker.image      = 0;
   state->progress_tracker.hold       = 0;
   state->progress_tracker.show_until = 0;
}

static void gfx_widget_leaderboard_display_free(void)
{
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;

   mpsc_stack_node_t *link = mpsc_stack_drain(&lbd_pending);
   while (link)
   {
      struct lbd_cmd *cmd = (struct lbd_cmd *)link;
      link = link->next;
      free(cmd);
   }
   gfx_widget_leaderboard_display_drop_tracker_badges(state);
   state->tracker_count   = 0;
   state->challenge_count = 0;
   state->dispwidget_ptr  = NULL;
}

static void gfx_widget_leaderboard_display_context_destroy(void)
{
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;
   gfx_widget_leaderboard_display_drop_tracker_badges(state);
   state->tracker_count   = 0;
   state->challenge_count = 0;
}

/* Ask for a badge the indicator does not have yet, and count its
 * hold down: to nothing when there is no point in waiting any more. */
static void gfx_widget_leaderboard_display_poll_badge(uintptr_t *image,
      uint8_t *hold, const char *badge_name, bool locked,
      bool download_if_missing)
{
   bool pending = false;
   *image = rcheevos_get_badge_texture_ex(badge_name, locked,
         download_if_missing, &pending);
   if (*image || !pending)
      *hold = 0;
   else if (*hold)
      (*hold)--;
}

static uintptr_t gfx_widget_progress_tracker_find_badge(
      struct progress_tracker_info *tracker, const char *badge_name)
{
   unsigned i;
   for (i = 0; i < CHEEVO_PROGRESS_TRACKER_BADGES; i++)
   {
      struct progress_tracker_badge *entry = &tracker->badges[i];
      if (entry->image && string_is_equal(entry->badge_name, badge_name))
      {
         entry->last_used = ++tracker->badge_tick;
         return entry->image;
      }
   }
   return 0;
}

/* @image is the tracker's from here on. The least recently shown
 * badge makes room, never the one on screen. */
static void gfx_widget_progress_tracker_keep_badge(
      struct progress_tracker_info *tracker, const char *badge_name,
      uintptr_t image)
{
   unsigned i;
   struct progress_tracker_badge *victim = NULL;
   for (i = 0; i < CHEEVO_PROGRESS_TRACKER_BADGES; i++)
   {
      struct progress_tracker_badge *entry = &tracker->badges[i];
      if (!entry->image)
      {
         victim = entry;
         break;
      }
      if (entry->image == tracker->image)
         continue;
      if (!victim || entry->last_used < victim->last_used)
         victim = entry;
   }
   if (victim->image)
      video_driver_texture_unload(&victim->image);
   victim->image     = image;
   victim->last_used = ++tracker->badge_tick;
   strlcpy(victim->badge_name, badge_name, sizeof(victim->badge_name));
}

/* The waiting update goes on screen, with @image or the placeholder. */
static void gfx_widget_progress_tracker_commit(
      struct progress_tracker_info *tracker, uintptr_t image,
      retro_time_t now)
{
   tracker->image      = image;
   tracker->width      = tracker->next_width;
   tracker->hold       = 0;
   tracker->show_until = now + CHEEVO_PROGRESS_TRACKER_DURATION * 1000;
   strlcpy(tracker->display, tracker->next_display, sizeof(tracker->display));
   strlcpy(tracker->badge_name, tracker->next_badge_name, sizeof(tracker->badge_name));
}

/* Ask for the badge of whichever the tracker is short of: the waiting
 * update's, else the one on screen with the placeholder. */
static void gfx_widget_progress_tracker_poll(
      struct progress_tracker_info *tracker, retro_time_t now,
      bool download_if_missing)
{
   uintptr_t image = 0;

   if (tracker->hold)
   {
      gfx_widget_leaderboard_display_poll_badge(&image, &tracker->hold,
            tracker->next_badge_name, true, download_if_missing);
      if (image)
         gfx_widget_progress_tracker_keep_badge(tracker,
               tracker->next_badge_name, image);
      if (!tracker->hold)
         gfx_widget_progress_tracker_commit(tracker, image, now);
   }
   else if (tracker->show_until && !tracker->image)
   {
      uint8_t hold = 0;
      gfx_widget_leaderboard_display_poll_badge(&image, &hold,
            tracker->badge_name, true, false);
      if (image)
      {
         gfx_widget_progress_tracker_keep_badge(tracker,
               tracker->badge_name, image);
         tracker->image = image;
      }
   }
}

static void gfx_widget_leaderboard_display_frame(void* data, void* userdata)
{
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;

   /* if there's nothing to display, just bail */
   if (state->tracker_count == 0 &&
       state->challenge_count == 0 &&
       state->progress_tracker.show_until == 0 &&
       state->progress_tracker.hold == 0 &&
       !state->loading &&
       !state->disconnected)
      return;


   {
      float pure_white[16] = {
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
         1.00, 1.00, 1.00, 1.00,
      };
      unsigned i, x;
      dispgfx_widget_t         *p_dispwidget = (dispgfx_widget_t*)userdata;
      const video_frame_info_t *video_info   = (const video_frame_info_t*)data;
      gfx_display_t *p_disp                  = (gfx_display_t*)video_info->disp_userdata;
      const unsigned video_width             = VIDEO_SCALE_W(video_info->dims);
      const unsigned video_height            = VIDEO_SCALE_H(video_info->dims);
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
               VIDEO_SCALE_PACK(video_width, video_height),
               (int)x, (int)y, VIDEO_SCALE_PACK(widget_width, widget_height),
               VIDEO_SCALE_PACK(video_width, video_height),
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
                  VIDEO_SCALE_PACK(video_width, video_height),
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
               gfx_widget_leaderboard_display_poll_badge(
                     &state->challenge_info[i].image,
                     &state->challenge_info[i].hold,
                     state->challenge_info[i].badge_name, false, false);
               /* its place is kept, so its neighbours do not move */
               if (state->challenge_info[i].hold)
                  continue;
            }

            if (!state->challenge_info[i].image)
            {
               /* default icon */
               if (p_dispwidget->gfx_widgets_icons_textures[
                     MENU_WIDGETS_ICON_ACHIEVEMENT])
               {
                  gfx_display_ctx_driver_t* dispctx = p_disp->dispctx;
                  gfx_display_blend_begin(dispctx, video_info->userdata);

                  gfx_widgets_draw_icon(
                        video_info->userdata,
                        p_disp,
                        VIDEO_SCALE_PACK(video_width, video_height),
                        VIDEO_SCALE_PACK(widget_size, widget_size),
                        p_dispwidget->gfx_widgets_icons_textures[
                              MENU_WIDGETS_ICON_ACHIEVEMENT],
                        x,
                        y,
                        0.0f, /* rad */
                        1.0f, /* cos(rad)   = cos(0)  = 1.0f */
                        0.0f, /* sine(rad)  = sine(0) = 0.0f */
                        pure_white);

                  gfx_display_blend_end(dispctx, video_info->userdata);
               }
            }
            else
            {
               /* achievement badge */
               gfx_display_ctx_driver_t* dispctx = p_disp->dispctx;
               gfx_display_blend_begin(dispctx, video_info->userdata);

               gfx_widgets_draw_icon(
                     video_info->userdata,
                     p_disp,
                     VIDEO_SCALE_PACK(video_width, video_height),
                     VIDEO_SCALE_PACK(widget_size, widget_size),
                     state->challenge_info[i].image,
                     x,
                     y,
                     0.0f, /* rad */
                     1.0f, /* cos(rad)   = cos(0)  = 1.0f */
                     0.0f, /* sine(rad)  = sine(0) = 0.0f */
                     pure_white);

               gfx_display_blend_end(dispctx, video_info->userdata);
            }
         }
      }

      if (     state->progress_tracker.show_until
            || state->progress_tracker.hold)
      {
         retro_time_t now = cpu_features_get_time_usec();

         gfx_widget_progress_tracker_poll(&state->progress_tracker, now, false);

         if (!state->progress_tracker.show_until)
         {
            /* first update, its badge a few frames away: nothing yet */
         }
         else if (  now >= state->progress_tracker.show_until
               /* not while an update waits: it restarts the clock */
               && !state->progress_tracker.hold)
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
                  VIDEO_SCALE_PACK(video_width, video_height),
                  (int)x, (int)y, VIDEO_SCALE_PACK(tracker_width,
                        tracker_height),
                  VIDEO_SCALE_PACK(video_width, video_height),
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
                  gfx_display_blend_begin(dispctx, video_info->userdata);

                  gfx_widgets_draw_icon(
                        video_info->userdata,
                        p_disp,
                        VIDEO_SCALE_PACK(video_width, video_height),
                        VIDEO_SCALE_PACK(image_size, image_size),
                        p_dispwidget->gfx_widgets_icons_textures[
                              MENU_WIDGETS_ICON_ACHIEVEMENT],
                        x,
                        y,
                        0.0f, /* rad */
                        1.0f, /* cos(rad)   = cos(0)  = 1.0f */
                        0.0f, /* sine(rad)  = sine(0) = 0.0f */
                        pure_white);

                  gfx_display_blend_end(dispctx, video_info->userdata);
               }
            }
            else
            {
               /* achievement badge */
               gfx_display_ctx_driver_t* dispctx = p_disp->dispctx;
               gfx_display_blend_begin(dispctx, video_info->userdata);

               gfx_widgets_draw_icon(
                     video_info->userdata,
                     p_disp,
                     VIDEO_SCALE_PACK(video_width, video_height),
                     VIDEO_SCALE_PACK(image_size, image_size),
                     state->progress_tracker.image,
                     x,
                     y,
                     0.0f, /* rad */
                     1.0f, /* cos(rad)   = cos(0)  = 1.0f */
                     0.0f, /* sine(rad)  = sine(0) = 0.0f */
                     pure_white);

               gfx_display_blend_end(dispctx, video_info->userdata);
            }

            x += image_size + spacing;
            y = (float)y + image_size / 2 + p_dispwidget->gfx_widget_fonts.regular.line_height / 2 - p_dispwidget->gfx_widget_fonts.regular.line_descender;
            gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.regular,
                  state->progress_tracker.display, x, y,
                  VIDEO_SCALE_PACK(video_width, video_height),
                  TEXT_COLOR_INFO, TEXT_ALIGN_LEFT, true);
         }
      }

      if (state->disconnected || state->loading)
      {
         char loading_buffer[8] = "RA ...";
         const char *disconnected_text = state->disconnected ? "! RA !" : loading_buffer;
         const unsigned disconnect_widget_width = font_driver_get_message_width(
            state->dispwidget_ptr->gfx_widget_fonts.msg_queue.font,
            disconnected_text, strlen(disconnected_text), 1) + CHEEVO_LBOARD_DISPLAY_PADDING * 2;
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
            VIDEO_SCALE_PACK(video_width, video_height),
            (int)x, (int)y, VIDEO_SCALE_PACK(disconnect_widget_width,
                  disconnect_widget_height),
            VIDEO_SCALE_PACK(video_width, video_height),
            p_dispwidget->backdrop_orig,
            NULL);

         /* Text */
         char_x = (float)(x + CHEEVO_LBOARD_DISPLAY_PADDING);
         char_y = (float)(y + disconnect_widget_height - (CHEEVO_LBOARD_DISPLAY_PADDING - 1)
            - p_dispwidget->gfx_widget_fonts.msg_queue.line_descender);

         gfx_widgets_draw_text(&p_dispwidget->gfx_widget_fonts.msg_queue,
            disconnected_text, char_x, char_y,
            VIDEO_SCALE_PACK(video_width, video_height),
            TEXT_COLOR_INFO, TEXT_ALIGN_LEFT, true);
      }
   }

}

static void gfx_widgets_clear_leaderboard_displays_state(void)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;


   state->tracker_count = 0;

}

static void lbd_push(uint8_t kind, unsigned id,
      const char *value, const char *badge, bool flag)
{
   struct lbd_cmd *cmd = (struct lbd_cmd *)malloc(sizeof(*cmd));
   if (!cmd)
      return;
   cmd->kind      = kind;
   cmd->id        = id;
   cmd->has_value = value ? true : flag;
   cmd->value[0]  = '\0';
   cmd->badge[0]  = '\0';
   if (value)
      strlcpy(cmd->value, value, sizeof(cmd->value));
   if (badge)
      strlcpy(cmd->badge, badge, sizeof(cmd->badge));
   mpsc_stack_push(&lbd_pending, &cmd->link);
}

/* The seven producers below run on the cheevos thread and touch only
 * the command stack; no widget-state lock is involved. */
void gfx_widgets_clear_leaderboard_displays(void)
{
   lbd_push(LBD_CMD_CLEAR_TRACKERS, 0, NULL, NULL, false);
}

static void gfx_widgets_set_leaderboard_display_state(unsigned id, const char* value)
{
   unsigned i;
   gfx_widget_leaderboard_display_state_t *state = &p_w_leaderboard_display_st;


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
               /* Multi-element downward shift - regions overlap,
                * so memcpy is undefined here */
               memmove(&state->tracker_info[i], &state->tracker_info[i + 1],
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
                     buffer, 1, 1);
               if (state->char_width[j] > state->fixed_char_width)
                  state->fixed_char_width = state->char_width[j];
            }
         }

         /* show or update display */
         if (i == state->tracker_count)
            state->tracker_info[state->tracker_count++].id = id;

         strlcpy(state->tracker_info[i].display, value,
               sizeof(state->tracker_info[i].display));

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

}

void gfx_widgets_set_leaderboard_display(unsigned id, const char* value)
{
   lbd_push(LBD_CMD_SET_TRACKER, id, value, NULL, false);
}

static void gfx_widgets_clear_challenge_displays_state(void)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;


   state->challenge_count = 0;

}

void gfx_widgets_clear_challenge_displays(void)
{
   lbd_push(LBD_CMD_CLEAR_CHALLENGES, 0, NULL, NULL, false);
}

static void gfx_widgets_set_challenge_display_state(unsigned id, const char* badge)
{
   unsigned i;
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;

   /* Draw-thread applier: the badge texture fetch runs here, on the
    * thread the video driver expects it from. */
   uintptr_t old_badge_id = 0;


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
               /* Multi-element downward shift - regions overlap,
                * so memcpy is undefined here */
               memmove(&state->challenge_info[i], &state->challenge_info[i + 1],
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
            state->challenge_info[i].image = 0;
            state->challenge_info[state->challenge_count++].id = id;
         }
         else if (state->challenge_info[i].image)
         {
            if (!string_is_equal(state->challenge_info[i].badge_name, badge))
            {
               /* existing indicator, different image. discard and replace */
               old_badge_id = state->challenge_info[i].image;
               state->challenge_info[i].image = 0;
            }
         }

         if (!state->challenge_info[i].image)
         {
            strlcpy(state->challenge_info[i].badge_name, badge, sizeof(state->challenge_info[i].badge_name));
            state->challenge_info[i].hold = CHEEVO_BADGE_HOLD_FRAMES;
            gfx_widget_leaderboard_display_poll_badge(
                  &state->challenge_info[i].image,
                  &state->challenge_info[i].hold,
                  state->challenge_info[i].badge_name, false, true);
         }
      }
   }


   if (old_badge_id)
      video_driver_texture_unload(&old_badge_id);
}

void gfx_widgets_set_challenge_display(unsigned id, const char* badge)
{
   lbd_push(LBD_CMD_SET_CHALLENGE, id, badge, NULL, false);
}

/* Applier, draw thread: the badge fetch, the font width and the
 * texture unloads all belong to this thread. */
static void gfx_widget_set_achievement_progress_state(const char* badge, const char* progress)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;
   struct progress_tracker_info *tracker         = &state->progress_tracker;

   if (badge == NULL)
   {
      /* hide indicator; its badges are kept for the next time */
      tracker->image      = 0;
      tracker->hold       = 0;
      tracker->show_until = 0;
   }
   else
   {
      /* show indicator */
      const retro_time_t now = cpu_features_get_time_usec();
      uintptr_t image        = gfx_widget_progress_tracker_find_badge(tracker, badge);

      snprintf(tracker->next_display, sizeof(tracker->next_display), "%s", progress);
      strlcpy(tracker->next_badge_name, badge, sizeof(tracker->next_badge_name));
      tracker->next_width = (uint16_t)font_driver_get_message_width(
            state->dispwidget_ptr->gfx_widget_fonts.regular.font,
            progress, strlen(progress), 1);

      if (image)
         gfx_widget_progress_tracker_commit(tracker, image, now);
      else
      {
         /* what is on screen stays until the badge is here */
         tracker->hold = CHEEVO_BADGE_HOLD_FRAMES;
         gfx_widget_progress_tracker_poll(tracker, now, true);
      }
   }
}

void gfx_widget_set_achievement_progress(const char* badge, const char* progress)
{
   lbd_push(LBD_CMD_SET_PROGRESS, 0, progress, badge, false);
}

static void gfx_widget_set_cheevos_disconnect_state(bool value)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;
   state->disconnected = value;
}

void gfx_widget_set_cheevos_disconnect(bool value)
{
   lbd_push(LBD_CMD_SET_DISCONNECT, 0, NULL, NULL, value);
}

static void gfx_widget_set_cheevos_set_loading_state(bool value)
{
   gfx_widget_leaderboard_display_state_t* state = &p_w_leaderboard_display_st;
   state->loading = value ? 1 : 0;
}

void gfx_widget_set_cheevos_set_loading(bool value)
{
   lbd_push(LBD_CMD_SET_LOADING, 0, NULL, NULL, value);
}


static void gfx_widget_leaderboard_display_iterate(void *user_data,
      unsigned dims, bool fullscreen,
      const char *dir_assets, char *font_path, bool is_threaded)
{
   mpsc_stack_node_t *link =
         mpsc_stack_reverse(mpsc_stack_drain(&lbd_pending));

   while (link)
   {
      struct lbd_cmd *cmd = (struct lbd_cmd *)link;
      link = link->next;

      switch (cmd->kind)
      {
         case LBD_CMD_SET_TRACKER:
            gfx_widgets_set_leaderboard_display_state(cmd->id,
                  cmd->has_value ? cmd->value : NULL);
            break;
         case LBD_CMD_CLEAR_TRACKERS:
            gfx_widgets_clear_leaderboard_displays_state();
            break;
         case LBD_CMD_SET_CHALLENGE:
            gfx_widgets_set_challenge_display_state(cmd->id,
                  cmd->has_value ? cmd->value : NULL);
            break;
         case LBD_CMD_CLEAR_CHALLENGES:
            gfx_widgets_clear_challenge_displays_state();
            break;
         case LBD_CMD_SET_PROGRESS:
            gfx_widget_set_achievement_progress_state(
                  cmd->has_value ? cmd->badge : NULL,
                  cmd->has_value ? cmd->value : NULL);
            break;
         case LBD_CMD_SET_DISCONNECT:
            gfx_widget_set_cheevos_disconnect_state(cmd->has_value);
            break;
         case LBD_CMD_SET_LOADING:
            gfx_widget_set_cheevos_set_loading_state(cmd->has_value);
            break;
      }
      free(cmd);
   }
}

static bool gfx_widget_leaderboard_display_visible(void)
{
   gfx_widget_leaderboard_display_state_t *state =
      &p_w_leaderboard_display_st;
   return state->tracker_count != 0
      || state->challenge_count != 0
      || state->progress_tracker.show_until != 0
      || state->progress_tracker.hold != 0
      || state->loading
      || state->disconnected;
}

const gfx_widget_t gfx_widget_leaderboard_display = {
   &gfx_widget_leaderboard_display_init,
   &gfx_widget_leaderboard_display_free,
   NULL, /* context_reset*/
   &gfx_widget_leaderboard_display_context_destroy,
   NULL, /* layout */
   &gfx_widget_leaderboard_display_iterate,
   &gfx_widget_leaderboard_display_frame,
   &gfx_widget_leaderboard_display_visible
};
