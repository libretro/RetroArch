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
#include <retro_atomic.h>

#include "../gfx_widgets.h"
#include "../gfx_animation.h"
#include "../gfx_display.h"
#include "../../configuration.h"

#define SCREENSHOT_DURATION_IN            66
#define SCREENSHOT_DURATION_OUT           SCREENSHOT_DURATION_IN*10

struct gfx_widget_screenshot_state
{
   uintptr_t texture;

   unsigned video_height;
   unsigned texture_dims;

   unsigned dims;
   unsigned thumbnail_dims;
   unsigned shotname_length;

   float scale_factor;
   float y;
   float alpha;
   float timer;         /* float alignment */

   char shotname[256];
   char filename[256];
   bool loaded;
   bool state_slot;

   /* The flash-speed and duration settings, latched here on the main
    * thread at the widget's two entry points. The fadeout and end
    * animation callbacks fire on the draw thread and must not read
    * the live settings there; they read these instead. Staleness
    * across a settings change costs one animation's timing. */
   retro_atomic_int_t flash_mode;
   retro_atomic_int_t show_duration;
};

typedef struct gfx_widget_screenshot_state gfx_widget_screenshot_state_t;

static gfx_widget_screenshot_state_t p_w_screenshot_st = {
   0,             /* texture */
   0,             /* video_height */
   0,             /* texture_dims */
   0,             /* dims */
   0,             /* thumbnail_dims */
   0,             /* shotname_length */
   0.0f,          /* scale_factor */
   0.0f,          /* y */
   0.0f,          /* alpha */
   0.0f,          /* timer */
   {0},           /* shotname */
   {0},           /* filename */
   false,         /* loaded */
   false          /* state_slot */
};

/* Animation callback: the draw thread. Settings come from the latch,
 * never live. */
static void gfx_widget_screenshot_fadeout(void *userdata)
{
   gfx_animation_ctx_entry_t entry;
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)userdata;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   entry.cb             = NULL;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->alpha;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   switch (retro_atomic_load_relaxed_int(&state->flash_mode))
   {
      case NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST:
         entry.duration = SCREENSHOT_DURATION_OUT/2;
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL:
      default:
         entry.duration = SCREENSHOT_DURATION_OUT;
         break;
   }

   gfx_animation_push_widget(&entry);
}

static void gfx_widget_screenshot_dispose(void *userdata)
{
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   state->loaded  = false;
   video_driver_texture_unload(&state->texture);
   state->texture = 0;
}

static void gfx_widgets_play_screenshot_flash(void *data)
{
   gfx_animation_ctx_entry_t entry;
   settings_t *settings                 = config_get_ptr();
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)data;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   entry.cb             = gfx_widget_screenshot_fadeout;
   entry.easing_enum    = EASING_IN_QUAD;
   entry.subject        = &state->alpha;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = 0.5f;
   entry.userdata       = p_dispwidget;

   switch (settings->uints.notification_show_screenshot_flash)
   {
      case NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST:
         entry.duration = SCREENSHOT_DURATION_IN/2;
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL:
      default:
         entry.duration = SCREENSHOT_DURATION_IN;
         break;
   }

   gfx_animation_push_widget(&entry);
}

static void gfx_widget_state_slot_show_state(
      void *data,
      const char *shotname, const char *filename)
{
   settings_t *settings                 = config_get_ptr();
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   retro_atomic_store_relaxed_int(&state->flash_mode,
         (int)settings->uints.notification_show_screenshot_flash);
   retro_atomic_store_relaxed_int(&state->show_duration,
         (int)settings->uints.notification_show_screenshot_duration);
   state->state_slot = true;

   if (!shotname || !filename)
   {
      gfx_widget_screenshot_dispose(NULL);
      return;
   }

   strlcpy(state->filename, filename, sizeof(state->filename));
   strlcpy(state->shotname, shotname, sizeof(state->shotname));
}

void gfx_widget_state_slot_show(
      void *data,
      const char *shotname, const char *filename)
{
   gfx_widgets_state_lock();
   gfx_widget_state_slot_show_state(data, shotname, filename);
   gfx_widgets_state_unlock();
}

static void gfx_widget_screenshot_taken_state(
      void *data,
      const char *shotname, const char *filename)
{
   settings_t *settings                 = config_get_ptr();
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)data;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   retro_atomic_store_relaxed_int(&state->flash_mode,
         (int)settings->uints.notification_show_screenshot_flash);
   retro_atomic_store_relaxed_int(&state->show_duration,
         (int)settings->uints.notification_show_screenshot_duration);
   state->state_slot = false;

   if (settings->uints.notification_show_screenshot_flash != NOTIFICATION_SHOW_SCREENSHOT_FLASH_OFF)
      gfx_widgets_play_screenshot_flash(p_dispwidget);

   if (settings->bools.notification_show_screenshot)
   {
      strlcpy(state->filename, filename, sizeof(state->filename));
      strlcpy(state->shotname, shotname, sizeof(state->shotname));
   }
}

void gfx_widget_screenshot_taken(
      void *data,
      const char *shotname, const char *filename)
{
   gfx_widgets_state_lock();
   gfx_widget_screenshot_taken_state(data, shotname, filename);
   gfx_widgets_state_unlock();
}

/* Animation callback: the draw thread. Settings come from the latch,
 * never live. */
static void gfx_widget_screenshot_end(void *userdata)
{
   gfx_animation_ctx_entry_t entry;
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)userdata;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;
   unsigned duration                    = (unsigned)
         retro_atomic_load_relaxed_int(&state->show_duration);

   entry.cb             = gfx_widget_screenshot_dispose;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->y;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = -((float)VIDEO_SCALE_H(state->dims));
   entry.userdata       = NULL;

   if (state->state_slot)
   {
      entry.target_value = (float)state->video_height;
      duration           = NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST;
   }

   switch (duration)
   {
      case NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST:
         entry.duration = MSG_QUEUE_ANIMATION_DURATION/1.25;
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST:
      case NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT:
         entry.duration = MSG_QUEUE_ANIMATION_DURATION/1.5;
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL:
      default:
         entry.duration = MSG_QUEUE_ANIMATION_DURATION;
         break;
   }

   gfx_animation_push_widget(&entry);
}

static void gfx_widget_screenshot_free(void)
{
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   state->alpha         = 0.0f;
   gfx_widget_screenshot_dispose(NULL);
}

static void gfx_widget_screenshot_context_destroy(void)
{
   gfx_widget_screenshot_dispose(NULL);
}

static void gfx_widget_screenshot_frame(void* data, void *user_data)
{
   float pure_white[16]          = {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
   };
   video_frame_info_t *video_info       = (video_frame_info_t*)data;
   void *userdata                       = video_info->userdata;
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)user_data;
   gfx_display_t            *p_disp     = (gfx_display_t*)video_info->disp_userdata;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;
   /* Not cached at init: the instance changes when the threaded
    * video worker takes the widgets over */
   gfx_animation_t          *p_anim     = anim_widgets_get_ptr();
   gfx_widget_font_data_t* font_regular = &p_dispwidget->gfx_widget_fonts.regular;
   int padding                          = (VIDEO_SCALE_H(state->dims)
         - (font_regular->line_height * 2.0f)) / 2.0f;

   /* Screenshot */
   if (state->loaded)
   {
      char shotname[256];
      gfx_animation_ctx_ticker_t ticker;

      gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_info->dims,
            0, state->y,
            state->dims,
            video_info->dims,
            p_dispwidget->backdrop_orig,
            NULL
            );

      gfx_display_set_alpha(pure_white, 0.5f);

      state->video_height = VIDEO_SCALE_H(video_info->dims);

      if (state->texture)
      {
         gfx_widgets_draw_icon(
               userdata,
               p_disp,
               video_info->dims,
               state->thumbnail_dims,
               state->texture,
               0,
               state->y,
               0.0f, /* rad */
               1.0f, /* cos(rad)   = cos(0)  = 1.0f */
               0.0f, /* sine(rad)  = sine(0) = 0.0f */
               pure_white
               );
      }
      else
      {
         float background_color[16]        = {
               0.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 0.0f, 0.0f, 1.0f,
         };

         /* Darken background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_info->dims,
               0,
               state->y,
               state->thumbnail_dims,
               video_info->dims,
               background_color,
               NULL);
      }

      gfx_widgets_draw_text(font_regular,
            (state->state_slot)
                  ? msg_hash_to_str(MSG_STATE_SLOT)
                  : msg_hash_to_str(MSG_SCREENSHOT_SAVED),
            VIDEO_SCALE_W(state->thumbnail_dims) + padding,
            padding + font_regular->line_ascender + state->y,
            video_info->dims,
            TEXT_COLOR_FAINT,
            TEXT_ALIGN_LEFT,
            true);

      ticker.idx        = p_anim->ticker_idx;
      ticker.len        = state->shotname_length;
      ticker.s          = shotname;
      ticker.s_len      = sizeof(shotname);
      ticker.selected   = true;
      ticker.str        = state->shotname;
      ticker.spacer     = NULL;

      gfx_animation_ticker_widget(&ticker);

      gfx_widgets_draw_text(font_regular,
            shotname,
            VIDEO_SCALE_W(state->thumbnail_dims) + padding,
            VIDEO_SCALE_H(state->dims) - padding
               - font_regular->line_descender + state->y,
            video_info->dims,
            TEXT_COLOR_INFO,
            TEXT_ALIGN_LEFT,
            true);
   }

   /* Flash effect */
   if (state->alpha > 0.0f)
   {
      gfx_display_set_alpha(pure_white, state->alpha);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_info->dims,
            0, 0,
            video_info->dims,
            video_info->dims,
            pure_white,
            NULL
            );
   }
}

static void gfx_widget_screenshot_iterate(
      void *user_data,
      unsigned dims, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded)
{
   settings_t *settings = config_get_ptr();
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)user_data;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;
   gfx_widget_font_data_t* font_regular = &p_dispwidget->gfx_widget_fonts.regular;
   unsigned padding                     = p_dispwidget->simple_widget_padding;
   unsigned duration                    = settings->uints.notification_show_screenshot_duration;

   /* Load screenshot and start its animation */
   if (state->filename[0] != '\0')
   {
      video_driver_state_t *video_st = video_state_get_ptr();
      gfx_timer_ctx_entry_t timer;

      video_driver_texture_unload(&state->texture);

      state->texture = 0;
      state->y       = 0.0f;

      gfx_display_reset_textures_list(state->filename,
            "", &state->texture,
            gfx_display_texture_filter_latched(),
            &state->texture_dims);

      state->dims   = VIDEO_SCALE_PACK(VIDEO_SCALE_W(dims),
            font_regular->line_height * 4);

      state->scale_factor = gfx_widgets_get_thumbnail_scale_factor(
            state->dims, state->texture_dims);

      /* State slot is double size and at the bottom */
      if (state->state_slot)
      {
         VIDEO_SCALE_PUT_H(state->dims, VIDEO_SCALE_H(state->dims) * 2);
         state->scale_factor *= 2;
         state->y             = VIDEO_SCALE_H(dims)
                              - VIDEO_SCALE_H(state->dims);
         duration             = NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST;
      }

      state->thumbnail_dims   = VIDEO_SCALE_PACK(
            VIDEO_SCALE_W(state->texture_dims) * state->scale_factor,
            VIDEO_SCALE_H(state->texture_dims) * state->scale_factor);

      /* Set image aspect ratio according to core geometry */
      if (video_st && video_st->av_info.geometry.aspect_ratio > 0)
      {
         float thumbnail_aspect = (float)VIDEO_SCALE_W(state->texture_dims)
            / (float)VIDEO_SCALE_H(state->texture_dims);
         float core_aspect      = video_st->av_info.geometry.aspect_ratio;

         VIDEO_SCALE_PUT_W(state->thumbnail_dims,
               VIDEO_SCALE_W(state->thumbnail_dims)
               / (thumbnail_aspect / core_aspect));
      }

      state->shotname_length  = (VIDEO_SCALE_W(dims)
            - VIDEO_SCALE_W(state->thumbnail_dims) - padding*2)
            / font_regular->glyph_width;

      timer.cb                = gfx_widget_screenshot_end;
      timer.userdata          = p_dispwidget;

      switch (duration)
      {
         case NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST:
            timer.duration = 2000;
            break;
         case NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST:
            timer.duration = 500;
            break;
         case NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT:
            timer.duration = 1;
            break;
         case NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL:
         default:
            timer.duration = 5000;
            break;
      }

      gfx_animation_timer_start_widget(&state->timer, &timer);

      state->loaded       = true;
      state->filename[0]  = '\0';
   }
}

static bool gfx_widget_screenshot_init(
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      bool video_is_threaded, bool fullscreen)
{
   return false;
}

static bool gfx_widget_screenshot_visible(void)
{
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;
   return state->loaded || state->alpha > 0.0f;
}

const gfx_widget_t gfx_widget_screenshot = {
   gfx_widget_screenshot_init,
   gfx_widget_screenshot_free,
   NULL, /* context_reset*/
   gfx_widget_screenshot_context_destroy,
   NULL, /* layout */
   gfx_widget_screenshot_iterate,
   gfx_widget_screenshot_frame,
   gfx_widget_screenshot_visible
};
