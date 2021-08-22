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

/* Constants */
#define VOLUME_DURATION 3000

enum gfx_widget_volume_icon
{
   ICON_MED = 0,
   ICON_MAX,
   ICON_MIN,
   ICON_MUTE,

   ICON_LAST
};

static const char* const ICONS_NAMES[ICON_LAST] = {
   "menu_volume_med.png",
   "menu_volume_max.png",
   "menu_volume_min.png",
   "menu_volume_mute.png",
};

/* Widget state */
struct gfx_widget_volume_state
{
   uintptr_t tag;
   uintptr_t textures[ICON_LAST];

   unsigned widget_width;
   unsigned widget_height;

   float bar_background[16];
   float bar_normal[16];
   float bar_loud[16];
   float bar_loudest[16];
   float alpha;
   float text_alpha;
   float db;
   float percent;
   gfx_timer_t timer;   /* float alignment */

   bool mute;
};

typedef struct gfx_widget_volume_state gfx_widget_volume_state_t;

static gfx_widget_volume_state_t p_w_volume_st = {
   (uintptr_t) &p_w_volume_st,
   {0},
   0,
   0,
   COLOR_HEX_TO_FLOAT(0x1A1A1A, 1.0f),
   COLOR_HEX_TO_FLOAT(0x198AC6, 1.0f),
   COLOR_HEX_TO_FLOAT(0xF5DD19, 1.0f),
   COLOR_HEX_TO_FLOAT(0xC23B22, 1.0f),
   0.0f,
   0.0f,
   0.0f,
   1.0f,
   0.0f,
   false
};

static void gfx_widget_volume_frame(void* data, void *user_data)
{
   static float pure_white[16]             = {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
   };
   gfx_widget_volume_state_t *state        = &p_w_volume_st;

   if (state->alpha > 0.0f)
   {
      char msg[255];
      char percentage_msg[255];
      video_frame_info_t *video_info       = (video_frame_info_t*)data;
      dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)user_data;
      gfx_widget_font_data_t *font_regular = &p_dispwidget->gfx_widget_fonts.regular;

      void *userdata                       = video_info->userdata;
      unsigned video_width                 = video_info->width;
      unsigned video_height                = video_info->height;

      unsigned padding                     = p_dispwidget->simple_widget_padding;

      float* backdrop_orig                 = p_dispwidget->backdrop_orig;

      uintptr_t volume_icon                = 0;
      unsigned icon_size                   = state->textures[ICON_MED] ? state->widget_height : padding;
      unsigned text_color                  = COLOR_TEXT_ALPHA(0xffffffff, (unsigned)(state->text_alpha*255.0f));
      unsigned text_color_db               = COLOR_TEXT_ALPHA(TEXT_COLOR_FAINT, (unsigned)(state->text_alpha*255.0f));

      unsigned bar_x                       = icon_size;
      unsigned bar_height                  = font_regular->line_height / 2;
      unsigned bar_width                   = state->widget_width - bar_x - padding;
      unsigned bar_y                       = state->widget_height / 2 + bar_height;

      float *bar_background                = NULL;
      float *bar_foreground                = NULL;
      float bar_percentage                 = 0.0f;
      gfx_display_t            *p_disp     = (gfx_display_t*)video_info->disp_userdata;
      gfx_display_ctx_driver_t *dispctx    = p_disp->dispctx;

      /* Note: Volume + percentage text has no component
       * that extends below the baseline, so we shift
       * the text down by the font descender to achieve
       * better spacing */
      unsigned volume_text_y               = (bar_y / 2.0f) 
         + font_regular->line_centre_offset 
         + font_regular->line_descender;

      msg[0]                               = '\0';
      percentage_msg[0]                    = '\0';

      if (state->mute)
         volume_icon                       = state->textures[ICON_MUTE];
      else if (state->percent <= 1.0f)
      {
         if (state->percent <= 0.5f)
            volume_icon                    = state->textures[ICON_MIN];
         else
            volume_icon                    = state->textures[ICON_MED];

         bar_background                    = state->bar_background;
         bar_foreground                    = state->bar_normal;
         bar_percentage                    = state->percent;
      }
      else if (state->percent > 1.0f && state->percent <= 2.0f)
      {
         volume_icon                       = state->textures[ICON_MAX];

         bar_background                    = state->bar_normal;
         bar_foreground                    = state->bar_loud;
         bar_percentage                    = state->percent - 1.0f;
      }
      else
      {
         volume_icon                       = state->textures[ICON_MAX];

         bar_background                    = state->bar_loud;
         bar_foreground                    = state->bar_loudest;
         bar_percentage                    = state->percent - 2.0f;
      }

      if (bar_percentage > 1.0f)
         bar_percentage                    = 1.0f;

      /* Backdrop */
      gfx_display_set_alpha(backdrop_orig, state->alpha);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0, 0,
            state->widget_width,
            state->widget_height,
            video_width,
            video_height,
            backdrop_orig,
            NULL
            );

      /* Icon */
      if (volume_icon)
      {
         gfx_display_set_alpha(pure_white, state->text_alpha);

         if (dispctx && dispctx->blend_begin)
            dispctx->blend_begin(userdata);
         gfx_widgets_draw_icon(
               userdata,
               p_disp,
               video_width,
               video_height,
               icon_size, icon_size,
               volume_icon,
               0, 0,
               0, 1, pure_white
               );
         if (dispctx && dispctx->blend_end)
            dispctx->blend_end(userdata);
      }

      if (state->mute)
      {
         if (!state->textures[ICON_MUTE])
         {
            const char *text  = msg_hash_to_str(MSG_AUDIO_MUTED);
            gfx_widgets_draw_text(font_regular,
                  text,
                  state->widget_width / 2,
                  state->widget_height / 2.0f 
                  + font_regular->line_centre_offset,
                  video_width, video_height,
                  text_color, TEXT_ALIGN_CENTER,
                  true);
         }
      }
      else
      {
         /* Bar */
         gfx_display_set_alpha(bar_background, state->text_alpha);
         gfx_display_set_alpha(bar_foreground, state->text_alpha);

         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               bar_x + bar_percentage * bar_width, bar_y,
               bar_width - bar_percentage * bar_width, bar_height,
               video_width, video_height,
               bar_background,
               NULL
               );

         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               bar_x, bar_y,
               bar_percentage * bar_width, bar_height,
               video_width, video_height,
               bar_foreground,
               NULL
               );

         /* Text */
         snprintf(msg, sizeof(msg), (state->db >= 0 ? "+%.1f dB" : "%.1f dB"),
            state->db);

         snprintf(percentage_msg, sizeof(percentage_msg), "%d%%",
            (int)(state->percent * 100.0f));

         gfx_widgets_draw_text(font_regular,
               msg,
               state->widget_width - padding, volume_text_y,
               video_width, video_height,
               text_color_db,
               TEXT_ALIGN_RIGHT,
               false);

         gfx_widgets_draw_text(font_regular,
            percentage_msg,
            icon_size, volume_text_y,
            video_width, video_height,
            text_color,
            TEXT_ALIGN_LEFT,
            false);
      }
   }
}

static void gfx_widget_volume_timer_end(void *userdata)
{
   gfx_animation_ctx_entry_t entry;
   gfx_widget_volume_state_t *state = &p_w_volume_st;

   entry.cb             = NULL;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->alpha;
   entry.tag            = state->tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   gfx_animation_push(&entry);

   entry.subject        = &state->text_alpha;

   gfx_animation_push(&entry);
}

void gfx_widget_volume_update_and_show(float new_volume, bool mute)
{
   gfx_timer_ctx_entry_t entry;
   gfx_widget_volume_state_t *state = &p_w_volume_st;

   gfx_animation_kill_by_tag(&state->tag);

   state->db         = new_volume;
   state->percent    = pow(10, new_volume/20);
   state->alpha      = DEFAULT_BACKDROP;
   state->text_alpha = 1.0f;
   state->mute       = mute;

   entry.cb          = gfx_widget_volume_timer_end;
   entry.duration    = VOLUME_DURATION;
   entry.userdata    = NULL;

   gfx_animation_timer_start(&state->timer, &entry);
}

static void gfx_widget_volume_layout(
      void *data,
      bool is_threaded, const char *dir_assets, char *font_path)
{
   dispgfx_widget_t *p_dispwidget       = (dispgfx_widget_t*)data;
   gfx_widget_volume_state_t *state     = &p_w_volume_st;
   unsigned last_video_width            = p_dispwidget->last_video_width;
   gfx_widget_font_data_t *font_regular = &p_dispwidget->gfx_widget_fonts.regular;

   state->widget_height                 = font_regular->line_height * 4;
   state->widget_width                  = state->widget_height * 4;

   /* Volume widget cannot exceed screen width
    * > If it does, scale it down */
   if (state->widget_width > last_video_width)
   {
      state->widget_width  = last_video_width;
      state->widget_height = state->widget_width / 4;
   }
}

static void gfx_widget_volume_context_reset(bool is_threaded,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path,
      char* menu_png_path,
      char* widgets_png_path)
{
   size_t i;
   gfx_widget_volume_state_t *state     = &p_w_volume_st;

   for (i = 0; i < ICON_LAST; i++)
      gfx_display_reset_textures_list(ICONS_NAMES[i], menu_png_path, &state->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL);
}

static void gfx_widget_volume_context_destroy(void)
{
   size_t i;
   gfx_widget_volume_state_t *state     = &p_w_volume_st;

   for (i = 0; i < ICON_LAST; i++)
      video_driver_texture_unload(&state->textures[i]);
}

static void gfx_widget_volume_free(void)
{
   gfx_widget_volume_state_t *state     = &p_w_volume_st;

   /* Kill all running animations */
   gfx_animation_kill_by_tag(&state->tag);

   state->alpha = 0.0f;
}

const gfx_widget_t gfx_widget_volume = {
   NULL, /* init */
   gfx_widget_volume_free,
   gfx_widget_volume_context_reset,
   gfx_widget_volume_context_destroy,
   gfx_widget_volume_layout,
   NULL, /* iterate */
   gfx_widget_volume_frame
};
