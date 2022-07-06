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
#include "../../configuration.h"
#include "../../retroarch.h"

#define SCREENSHOT_DURATION_IN            66
#define SCREENSHOT_DURATION_OUT           SCREENSHOT_DURATION_IN*10

struct gfx_widget_screenshot_state
{
   uintptr_t texture;
   gfx_animation_t *p_anim;

   unsigned texture_width;
   unsigned texture_height;

   unsigned height;
   unsigned width;
   unsigned thumbnail_width;
   unsigned thumbnail_height;
   unsigned shotname_length;

   float scale_factor;
   float y;
   float alpha;
   float timer;         /* float alignment */

   char shotname[256];
   char filename[256];
   bool loaded;
};

typedef struct gfx_widget_screenshot_state gfx_widget_screenshot_state_t;

static gfx_widget_screenshot_state_t p_w_screenshot_st = {
   0,             /* texture */
   NULL,          /* p_anim */
   0,             /* texture_width */
   0,             /* texture_height */
   0,             /* height */
   0,             /* width */
   0,             /* thumbnail_width */
   0,             /* thumbnail_height */
   0,             /* shotname_length */
   0.0f,          /* scale_factor */
   0.0f,          /* y */
   0.0f,          /* alpha */
   0.0f,          /* timer */

   {0},           /* shotname */
   {0},           /* filename */
   false          /* loaded */
};

static void gfx_widget_screenshot_fadeout(void *userdata)
{
   gfx_animation_ctx_entry_t entry;
   settings_t *settings                 = config_get_ptr();
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)userdata;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   entry.cb             = NULL;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->alpha;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   switch (settings->uints.notification_show_screenshot_flash)
   {
      case NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST:
         entry.duration = SCREENSHOT_DURATION_OUT/2;
         break;
      case NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL:
      default:
         entry.duration = SCREENSHOT_DURATION_OUT;
         break;
   }

   gfx_animation_push(&entry);
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
   entry.target_value   = 1.0f;
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

   gfx_animation_push(&entry);
}

void gfx_widget_screenshot_taken(
      void *data,
      const char *shotname, const char *filename)
{
   settings_t *settings                 = config_get_ptr();
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)data;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   if (settings->uints.notification_show_screenshot_flash != NOTIFICATION_SHOW_SCREENSHOT_FLASH_OFF)
      gfx_widgets_play_screenshot_flash(p_dispwidget);

   if (settings->bools.notification_show_screenshot)
   {
      strlcpy(state->filename, filename, sizeof(state->filename));
      strlcpy(state->shotname, shotname, sizeof(state->shotname));
   }
}

static void gfx_widget_screenshot_dispose(void *userdata)
{
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   state->loaded  = false;
   video_driver_texture_unload(&state->texture);
   state->texture = 0;
}

static void gfx_widget_screenshot_end(void *userdata)
{
   gfx_animation_ctx_entry_t entry;
   settings_t *settings                 = config_get_ptr();
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)userdata;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   entry.cb             = gfx_widget_screenshot_dispose;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->y;
   entry.tag            = p_dispwidget->gfx_widgets_generic_tag;
   entry.target_value   = -((float)state->height);
   entry.userdata       = NULL;

   switch (settings->uints.notification_show_screenshot_duration)
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

   gfx_animation_push(&entry);
}

static void gfx_widget_screenshot_free(void)
{
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   state->alpha         = 0.0f;
   gfx_widget_screenshot_dispose(NULL);
}

static void gfx_widget_screenshot_frame(void* data, void *user_data)
{
   static float pure_white[16]          = {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
   };
   video_frame_info_t *video_info       = (video_frame_info_t*)data;
   void *userdata                       = video_info->userdata;
   unsigned video_width                 = video_info->width;
   unsigned video_height                = video_info->height;
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)user_data;
   gfx_display_t            *p_disp     = (gfx_display_t*)video_info->disp_userdata;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;
   gfx_animation_t          *p_anim     = state->p_anim;
   gfx_widget_font_data_t* font_regular = &p_dispwidget->gfx_widget_fonts.regular;
   int padding                          = (state->height - (font_regular->line_height * 2.0f)) / 2.0f;

   /* Screenshot */
   if (state->loaded)
   {
      char shotname[256];
      gfx_animation_ctx_ticker_t ticker;

      gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width, video_height,
            0, state->y,
            state->width, state->height,
            video_width, video_height,
            p_dispwidget->backdrop_orig,
            NULL
            );

      gfx_display_set_alpha(pure_white, 1.0f);
      gfx_widgets_draw_icon(
            userdata,
            p_disp,
            video_width,
            video_height,
            state->thumbnail_width,
            state->thumbnail_height,
            state->texture,
            0,
            state->y,
            0.0f, /* rad */
            1.0f, /* cos(rad)   = cos(0)  = 1.0f */
            0.0f, /* sine(rad)  = sine(0) = 0.0f */
            pure_white
            );

      gfx_widgets_draw_text(font_regular,
            msg_hash_to_str(MSG_SCREENSHOT_SAVED),
            state->thumbnail_width + padding,
            padding + font_regular->line_ascender + state->y,
            video_width, video_height,
            TEXT_COLOR_FAINT,
            TEXT_ALIGN_LEFT,
            true);

      ticker.idx        = p_anim->ticker_idx;
      ticker.len        = state->shotname_length;
      ticker.s          = shotname;
      ticker.selected   = true;
      ticker.str        = state->shotname;

      gfx_animation_ticker(&ticker);

      gfx_widgets_draw_text(font_regular,
            shotname,
            state->thumbnail_width + padding,
            state->height - padding - font_regular->line_descender + state->y,
            video_width, video_height,
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
            video_width,
            video_height,
            0, 0,
            video_width, video_height,
            video_width, video_height,
            pure_white,
            NULL
            );
   }
}

static void gfx_widget_screenshot_iterate(
      void *user_data,
      unsigned width,
      unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      bool is_threaded)
{
   settings_t *settings = config_get_ptr();
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)user_data;
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;
   unsigned padding                     = p_dispwidget->simple_widget_padding;
   gfx_widget_font_data_t* font_regular = &p_dispwidget->gfx_widget_fonts.regular;

   /* Load screenshot and start its animation */
   if (state->filename[0] != '\0')
   {
      gfx_timer_ctx_entry_t timer;

      video_driver_texture_unload(&state->texture);

      state->texture = 0;

      gfx_display_reset_textures_list(state->filename,
            "", &state->texture, TEXTURE_FILTER_MIPMAP_LINEAR,
            &state->texture_width, &state->texture_height);

      state->height = font_regular->line_height * 4;
      state->width  = width;

      state->scale_factor = gfx_widgets_get_thumbnail_scale_factor(
         width, state->height,
         state->texture_width, state->texture_height
      );

      state->thumbnail_width  = state->texture_width * state->scale_factor;
      state->thumbnail_height = state->texture_height * state->scale_factor;

      state->shotname_length  = (width - state->thumbnail_width - padding*2) / font_regular->glyph_width;

      state->y = 0.0f;

      timer.cb       = gfx_widget_screenshot_end;

      switch (settings->uints.notification_show_screenshot_duration)
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
            timer.duration = 6000;
            break;
      }

      timer.userdata = p_dispwidget;

      gfx_animation_timer_start(&state->timer, &timer);

      state->loaded       = true;
      state->filename[0]  = '\0';
   }
}

static bool gfx_widget_screenshot_init(
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      bool video_is_threaded, bool fullscreen)
{
   gfx_widget_screenshot_state_t *state = &p_w_screenshot_st;

   state->p_anim = p_anim;

   return false;
}

const gfx_widget_t gfx_widget_screenshot = {
   gfx_widget_screenshot_init,
   gfx_widget_screenshot_free,
   NULL, /* context_reset*/
   NULL, /* context_destroy */
   NULL, /* layout */
   gfx_widget_screenshot_iterate,
   gfx_widget_screenshot_frame
};
