/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
 *  Copyright (C) 2016      - Andrés Suárez
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

#include "nk_menu.h"

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#include "../menu_driver.h"
#include "../menu_hash.h"

#include "../../gfx/common/gl_common.h"
#include "../../core_info.h"
#include "../../configuration.h"
#include "../../retroarch.h"


#define LEN(a) (sizeof(a)/sizeof(a)[0])


/* gamepad demo variables */
enum widget_id 
{
   WINDOW_MODE = 0,
   MODEL_DETAIL,
   TEXTURES,
   SHADOWS,
   LIGHTNING,
   EFFECTS,
   CONSOLE,
   BRIGHTNESS,
   VOLUME,
   WIDGET_MAX
};
enum display_settings {
   WINDOWED = 0, FULLSCREEN};
enum detail_settings {LOW, MEDIUM, HIGH, EXTRA_HIGH};
enum state_settings {OFF, ON};
const char *display[] = {"Windowed", "Fullscreen"};
const char *state[] = {"Off", "On"};
const char *detail[] = {"Low", "Medium", "High", "Extra High"};

static int window_mode = FULLSCREEN;
static int model_detail = HIGH;
static int texture_detail = EXTRA_HIGH;
static int shadow_detail = HIGH;
static int lighning_detail = LOW;
static int effects_detail = MEDIUM;
static int show_console = ON;
static int brightness = 90;
static int volume = 30;
static int active = WINDOW_MODE;
/* end of gamepad demo variables */


/*tatic int ui_selector(struct nk_context *ctx, const char *title, int *selected, const char *items[],
    int max, int active)
{
   struct nk_vec2 item_padding;
   struct nk_rect bounds, label, content, tri, sel;
   struct nk_panel *layout;
   struct nk_command_buffer *out;
   struct nk_color col;
   struct nk_vec2 result[3];
   nk_size text_len, text_width;

   if (!ctx || !ctx->current)
      return 0;

   layout = nk_window_get_panel(ctx);

   out = nk_window_get_canvas(ctx);
   if (!nk_widget(&bounds, ctx))
      return 0;

   item_padding = nk_get_property(ctx, NK_PROPERTY_ITEM_PADDING);
   bounds.x += item_padding.x;
   bounds.y += item_padding.y;
   bounds.w -= 2 * item_padding.x;
   bounds.h -= 2 * item_padding.y;

   label.h = bounds.h;
   label.w = bounds.w / 2.0f;
   label.x = bounds.x + item_padding.x;
   label.y = bounds.y + label.h/2.0f - (float)ctx->style.font.height/2.0f;

   content.x = bounds.x + bounds.w/2.0f;
   content.y = bounds.y;
   content.w = bounds.w / 2.0f;
   content.h = bounds.h;

   if (active) nk_draw_rect(out, bounds, 0, nk_rgba(220, 220, 220, 135));
   text_len = strlen(title);
   col = (active) ? nk_rgba(0, 0, 0, 255): nk_rgba(220,220,220,220);
   nk_draw_text(out, label, title, text_len, &ctx->style.font, nk_rgba(0,0,0,0), col);

   if (nk_input_is_key_pressed(&ctx->input, NK_KEY_RIGHT) && active)
      *selected = MIN(*selected+1, max-1);
   else if (nk_input_is_key_pressed(&ctx->input, NK_KEY_LEFT) && active)
      *selected = MAX(0, *selected-1);

   tri.h = ctx->style.font.height - 2 * item_padding.y;
   tri.w = tri.h/2.0f;
   tri.x = content.x + item_padding.x;
   tri.y = content.y + content.h/2 - tri.h/2.0f;

   sel.x = tri.x + item_padding.x;
   sel.y = tri.y;
   sel.h = content.h;

   if (*selected > 0) {
      nk_triangle_from_direction(result, tri, 0, 0, NK_LEFT);
      nk_draw_triangle(out, result[0].x, result[0].y, result[1].x, result[1].y,
         result[2].x, result[2].y, (active) ? nk_rgba(0, 0, 0, 255):
         nk_rgba(100, 100, 100, 150));
   }

   tri.x = content.x + (content.w - item_padding.x) - tri.w;
   sel.w = tri.x - sel.x;

   if (*selected < max-1) {
      nk_triangle_from_direction(result, tri, 0, 0, NK_RIGHT);
      nk_draw_triangle(out, result[0].x, result[0].y, result[1].x, result[1].y,
         result[2].x, result[2].y, (active) ? nk_rgba(0, 0, 0, 255):
      nk_rgba(100, 100, 100, 150));
   }

   text_width = ctx->style.font.width(ctx->style.font.userdata,
      ctx->style.font.height, items[*selected], strlen(items[*selected]));

   label.w = MAX(1, (float)text_width);
   label.x = (sel.x + (sel.w - label.w) / 2);
   label.x = MAX(sel.x, label.x);
   label.w = MIN(sel.x + sel.w, label.x + label.w);
   if (label.w >= label.x) label.w -= label.x;
   nk_draw_text(out, label, items[*selected], strlen(items[*selected]),
      &ctx->style.font, nk_rgba(0,0,0,0), col);
   return 0;
}
*/
/*
static void ui_slider(struct nk_context *ctx, const char *title, int *value, int max, int active)
{
   struct nk_vec2 item_padding;
   struct nk_rect bounds, label, content, bar, cursor, tri;
   struct nk_panel *layout;
   struct nk_command_buffer *out;
   struct nk_color col;
   nk_size text_len, text_width;
   float prog_scale = (float)*value / (float)max;
   struct nk_vec2 result[3];

   if (!ctx || !ctx->current)
      return;

    layout = nk_window_get_panel(ctx);

    out = nk_window_get_canvas(ctx);
    if (!nk_widget(&bounds, ctx))
        return;

   item_padding = nk_get_property(ctx, NK_PROPERTY_ITEM_PADDING);
   bounds.x += item_padding.x;
   bounds.y += item_padding.y;
   bounds.w -= 2 * item_padding.x;
   bounds.h -= 2 * item_padding.y;

   label.h = bounds.h;
   label.w = bounds.w / 2.0f;
   label.x = bounds.x + item_padding.x;
   label.y = bounds.y + label.h/2.0f - (float)ctx->style.font.height/2.0f;

   content.x = bounds.x + bounds.w/2.0f;
   content.y = bounds.y;
   content.w = bounds.w / 2.0f;
   content.h = bounds.h;

   if (active) nk_draw_rect(out, bounds, 0, nk_rgba(220, 220, 220, 135));
   text_len = strlen(title);
   col = (active) ? nk_rgba(0, 0, 0, 255): nk_rgba(220,220,220,220);
   nk_draw_text(out, label, title, text_len, &ctx->style.font, nk_rgba(0,0,0,0), col);

   if (nk_input_is_key_pressed(&ctx->input, NK_KEY_LEFT) && active)
      *value = MAX(0, *value - 10);
   if (nk_input_is_key_pressed(&ctx->input, NK_KEY_RIGHT) && active)
      *value = MIN(*value + 10, max);

   tri.h = ctx->style.font.height - 2 * item_padding.y;
   tri.w = tri.h/2.0f;
   tri.x = content.x + item_padding.x;
   tri.y = content.y + content.h/2 - tri.h/2.0f;

   bar.x = tri.x + 4 * item_padding.x + tri.w;
   bar.h = tri.h / 4.0f;
   bar.y = tri.y + tri.h/2.0f - bar.h/2.0f;

   if (*value > 0) {
      nk_triangle_from_direction(result, tri, 0, 0, NK_LEFT);
      nk_draw_triangle(out, result[0].x, result[0].y, result[1].x, result[1].y,
         result[2].x, result[2].y, (active) ? nk_rgba(0, 0, 0, 255):
         nk_rgba(100, 100, 100, 150));
    }

   tri.x = content.x + (content.w - item_padding.x) - tri.w;
   bar.w = (tri.x - bar.x) - 4 * item_padding.x;

   if (*value < max) 
   {
      nk_triangle_from_direction(result, tri, 0, 0, NK_RIGHT);
      nk_draw_triangle(out, result[0].x, result[0].y, result[1].x, result[1].y,
         result[2].x, result[2].y, (active) ? nk_rgba(0, 0, 0, 255):
      nk_rgba(100, 100, 100, 150));
    }

   bar.w = (bar.w - tri.h/2.0f);
   if (active) 
   {
      nk_draw_rect(out, bar, 0, nk_rgba(0, 0, 0, 135));
      bar.w = bar.w * prog_scale;
      bar.y = tri.y; bar.x = bar.x + bar.w; bar.w = tri.h; bar.h = tri.h;
      nk_draw_circle(out, bar, nk_rgba(220, 220, 220, 255));
   } 
   else 
   {
      nk_draw_rect(out, bar, 0, nk_rgba(220, 220, 220, 135));
      bar.w = bar.w * prog_scale;
      bar.y = tri.y; bar.x = bar.x + bar.w; bar.w = tri.h; bar.h = tri.h;
      nk_draw_circle(out, bar, nk_rgba(190, 190, 190, 255));
    }
}
*/
/*bool nk_checkbox_bool(struct nk_context* cx, const char* text, bool *active)
{
   int    x = *active;
   bool ret = nk_check_text(cx, text, &x);
   *active  = x;

   return ret;
}

float nk_checkbox_float(struct nk_context* cx, const char* text, float *active)
{
   int x     = *active;
   float ret = nk_check_text(cx, text, &x);
   *active   = x;

   return ret;
}*/

/*static void nk_labelf(struct nk_context *ctx,
      enum nk_text_align align, const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    buffer[1023] = 0;
    nk_label(ctx, buffer, align);
    va_end(args);
}*/

void nk_menu_set_state(nk_menu_handle_t *zr, const int id,
   struct nk_vec2 pos, struct nk_vec2 size)
{
   zr->window[id].position = pos;
   zr->window[id].size = size;
}

void nk_menu_get_state(nk_menu_handle_t *zr, const int id,
   struct nk_vec2 *pos, struct nk_vec2 *size)
{
   *pos = zr->window[id].position;
   *size = zr->window[id].size;
}

void nk_menu_wnd_shader_parameters(nk_menu_handle_t *zr)
{
   unsigned i;
   video_shader_ctx_t shader_info;
   struct nk_panel layout;
   struct nk_context *ctx = &zr->ctx;
   const int id           = ZRMENU_WND_SHADER_PARAMETERS;
   settings_t *settings   = config_get_ptr();

   if (nk_begin(ctx, &layout, "Shader Parameters", nk_rect(240, 10, 300, 400),
         NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_MOVABLE|
         NK_WINDOW_SCALABLE|NK_WINDOW_BORDER))
   {
      struct nk_panel combo;
      static const char *themes[] = {"Dark", "Light"};
      enum   nk_menu_theme old     = zr->theme;

      nk_layout_row_dynamic(ctx, 30, 1);

      video_shader_driver_get_current_shader(&shader_info);

      if (shader_info.data)
      {
         for (i = 0; i < GFX_MAX_PARAMETERS; i++)
         {
            if (!string_is_empty(shader_info.data->parameters[i].desc))
            {
/*               if(shader_info.data->parameters[i].minimum == 0 &&
                     shader_info.data->parameters[i].maximum == 1 &&
                     shader_info.data->parameters[i].step == 1)
                  nk_checkbox_float(ctx, shader_info.data->parameters[i].desc,
                        &(shader_info.data->parameters[i].current));
               else*/
                  nk_property_float(ctx, shader_info.data->parameters[i].desc,
                        shader_info.data->parameters[i].minimum,
                        &(shader_info.data->parameters[i].current),
                        shader_info.data->parameters[i].maximum,
                        shader_info.data->parameters[i].step, 1);
            }
         }
      }
   }
   /* save position and size to restore after context reset */
   nk_menu_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   nk_end(ctx);
}

void nk_menu_wnd_test(nk_menu_handle_t *zr)
{
   struct nk_panel layout;
   struct nk_context *ctx = &zr->ctx;
   const int id           = ZRMENU_WND_TEST;
   settings_t *settings   = config_get_ptr();

   if (nk_begin(ctx, &layout, "Test", nk_rect(140, 90, 500, 600),
         NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_MOVABLE|
         NK_WINDOW_SCALABLE|NK_WINDOW_BORDER))
   {
      unsigned size;
      struct nk_panel combo;
      menu_entry_t entry;
      static const char *themes[] = {"Dark", "Light"};
      enum   nk_menu_theme old     = zr->theme;

      nk_layout_row_dynamic(ctx, 30, 2);

      nk_layout_row_dynamic(ctx, 30, 4);
      //nk_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      //nk_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      //nk_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      //nk_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      nk_layout_row_dynamic(ctx, 30, 2);
      nk_label(ctx, "Volume:", NK_TEXT_LEFT);
      nk_slider_float(ctx, -80, &settings->audio.volume, 12, 0.5);
      nk_layout_row_dynamic(ctx, 30, 1);
      nk_property_int(ctx, "Max Users:", 1, (int*)&(settings->input.max_users),
         MAX_USERS, 1, 1);


      nk_label(ctx, "History:", NK_TEXT_LEFT);

      size = menu_entries_get_size();
   }
   /* save position and size to restore after context reset */
   nk_menu_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   nk_end(ctx);
}

void nk_menu_wnd_main(nk_menu_handle_t *zr)
{
   struct nk_panel layout;
   struct nk_context *ctx = &zr->ctx;
   const int id           = ZRMENU_WND_MAIN;
   settings_t *settings   = config_get_ptr();

   if (nk_begin(ctx, &layout, "Main", nk_rect(-1, -1, 120, zr->size.x + 1),
         NK_WINDOW_NO_SCROLLBAR))
   {
      struct nk_panel menu;
      struct nk_panel node, context_menu;

      /* context menu */

      /* main menu */
      nk_menubar_begin(ctx);
      nk_layout_row_begin(ctx, NK_STATIC, 25, 1);
      nk_layout_row_push(ctx, 100);

      nk_layout_row_push(ctx, 60);
      nk_menubar_end(ctx);
   }


   /* save position and size to restore after context reset */
   nk_menu_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   if (zr->size_changed)
      nk_window_set_size(ctx, nk_vec2(nk_window_get_size(ctx).x, zr->size.y));

   nk_end(ctx);
}
