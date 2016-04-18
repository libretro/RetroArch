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


static int ui_selector(struct nk_context *ctx, const char *title, int *selected, const char *items[],
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

bool nk_checkbox_bool(struct nk_context* cx, const char* text, bool *active)
{
   int    x = *active;
   bool ret = nk_checkbox(cx, text, &x);
   *active  = x;

   return ret;
}

float nk_checkbox_float(struct nk_context* cx, const char* text, float *active)
{
   int x     = *active;
   float ret = nk_checkbox(cx, text, &x);
   *active   = x;

   return ret;
}

static void nk_labelf(struct nk_context *ctx,
      enum nk_text_align align, const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    buffer[1023] = 0;
    nk_label(ctx, buffer, align);
    va_end(args);
}

void zrmenu_set_style(struct nk_context *ctx, enum zrmenu_theme theme)
{
   unsigned i;

   for (i = 0; i < NK_ROUNDING_MAX; ++i)
      (ctx)->style.rounding[i] = 0;

   switch (theme)
   {
      case THEME_LIGHT:
         ctx->style.rounding[NK_ROUNDING_SCROLLBAR] = 0;
         ctx->style.properties[NK_PROPERTY_SCROLLBAR_SIZE] = nk_vec2(10,10);
         ctx->style.colors[NK_COLOR_TEXT] = nk_rgba(20, 20, 20, 255);
         ctx->style.colors[NK_COLOR_TEXT_HOVERING] = nk_rgba(195, 195, 195, 255);
         ctx->style.colors[NK_COLOR_TEXT_ACTIVE] = nk_rgba(200, 200, 200, 255);
         ctx->style.colors[NK_COLOR_WINDOW] = nk_rgba(202, 212, 214, 215);
         ctx->style.colors[NK_COLOR_HEADER] = nk_rgba(137, 182, 224, 220);
         ctx->style.colors[NK_COLOR_BORDER] = nk_rgba(140, 159, 173, 0);
         ctx->style.colors[NK_COLOR_BUTTON] = nk_rgba(137, 182, 224, 255);
         ctx->style.colors[NK_COLOR_BUTTON_HOVER] = nk_rgba(142, 187, 229, 255);
         ctx->style.colors[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(147, 192, 234, 255);
         ctx->style.colors[NK_COLOR_TOGGLE] = nk_rgba(177, 210, 210, 255);
         ctx->style.colors[NK_COLOR_TOGGLE_HOVER] = nk_rgba(245, 245, 245, 255);
         ctx->style.colors[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(142, 187, 229, 255);
         ctx->style.colors[NK_COLOR_SELECTABLE] = nk_rgba(147, 192, 234, 255);
         ctx->style.colors[NK_COLOR_SELECTABLE_HOVER] = nk_rgba(150, 150, 150, 255);
         ctx->style.colors[NK_COLOR_SELECTABLE_TEXT] = nk_rgba(70, 70, 70, 255);
         ctx->style.colors[NK_COLOR_SLIDER] = nk_rgba(177, 210, 210, 255);
         ctx->style.colors[NK_COLOR_SLIDER_CURSOR] = nk_rgba(137, 182, 224, 245);
         ctx->style.colors[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(142, 188, 229, 255);
         ctx->style.colors[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(147, 193, 234, 255);
         ctx->style.colors[NK_COLOR_PROGRESS] = nk_rgba(177, 210, 210, 255);
         ctx->style.colors[NK_COLOR_PROGRESS_CURSOR] = nk_rgba(137, 182, 224, 255);
         ctx->style.colors[NK_COLOR_PROGRESS_CURSOR_HOVER] = nk_rgba(142, 188, 229, 255);
         ctx->style.colors[NK_COLOR_PROGRESS_CURSOR_ACTIVE] = nk_rgba(147, 193, 234, 255);
         ctx->style.colors[NK_COLOR_PROPERTY] = nk_rgba(210, 210, 210, 255);
         ctx->style.colors[NK_COLOR_PROPERTY_HOVER] = nk_rgba(235, 235, 235, 255);
         ctx->style.colors[NK_COLOR_PROPERTY_ACTIVE] = nk_rgba(230, 230, 230, 255);
         ctx->style.colors[NK_COLOR_INPUT] = nk_rgba(210, 210, 210, 225);
         ctx->style.colors[NK_COLOR_INPUT_CURSOR] = nk_rgba(20, 20, 20, 255);
         ctx->style.colors[NK_COLOR_INPUT_TEXT] = nk_rgba(20, 20, 20, 255);
         ctx->style.colors[NK_COLOR_COMBO] = nk_rgba(210, 210, 210, 255);
         ctx->style.colors[NK_COLOR_HISTO] = nk_rgba(210, 210, 210, 255);
         ctx->style.colors[NK_COLOR_HISTO_BARS] = nk_rgba(137, 182, 224, 255);
         ctx->style.colors[NK_COLOR_HISTO_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
         ctx->style.colors[NK_COLOR_PLOT] = nk_rgba(210, 210, 210, 255);
         ctx->style.colors[NK_COLOR_PLOT_LINES] = nk_rgba(137, 182, 224, 255);
         ctx->style.colors[NK_COLOR_PLOT_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
         ctx->style.colors[NK_COLOR_SCROLLBAR] = nk_rgba(190, 200, 200, 255);
         ctx->style.colors[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 84, 95, 255);
         ctx->style.colors[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(70, 90, 100, 255);
         ctx->style.colors[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
         ctx->style.colors[NK_COLOR_TABLE_LINES] = nk_rgba(100, 100, 100, 255);
         ctx->style.colors[NK_COLOR_TAB_HEADER] = nk_rgba(156, 193, 220, 255);
         ctx->style.colors[NK_COLOR_SCALER] = nk_rgba(100, 100, 100, 255);
         break;
      case THEME_DARK:
         ctx->style.rounding[NK_ROUNDING_SCROLLBAR] = 0;
         ctx->style.properties[NK_PROPERTY_SCROLLBAR_SIZE] = nk_vec2(10,10);
         ctx->style.colors[NK_COLOR_TEXT] = nk_rgba(210, 210, 210, 255);
         ctx->style.colors[NK_COLOR_TEXT_HOVERING] = nk_rgba(195, 195, 195, 255);
         ctx->style.colors[NK_COLOR_TEXT_ACTIVE] = nk_rgba(200, 200, 200, 255);
         ctx->style.colors[NK_COLOR_WINDOW] = nk_rgba(45, 53, 56, 215);
         ctx->style.colors[NK_COLOR_HEADER] = nk_rgba(46, 46, 46, 255);
         ctx->style.colors[NK_COLOR_BORDER] = nk_rgba(46, 46, 46, 0);
         ctx->style.colors[NK_COLOR_BUTTON] = nk_rgba(48, 83, 111, 255);
         ctx->style.colors[NK_COLOR_BUTTON_HOVER] = nk_rgba(53, 88, 116, 255);
         ctx->style.colors[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(58, 93, 121, 255);
         ctx->style.colors[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 255);
         ctx->style.colors[NK_COLOR_TOGGLE_HOVER] = nk_rgba(55, 63, 66, 255);
         ctx->style.colors[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 255);
         ctx->style.colors[NK_COLOR_SELECTABLE] = nk_rgba(48, 83, 111, 255);
         ctx->style.colors[NK_COLOR_SELECTABLE_HOVER] = nk_rgba(48, 83, 111, 255);
         ctx->style.colors[NK_COLOR_SELECTABLE_TEXT] = nk_rgba(210, 210, 210, 255);
         ctx->style.colors[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 255);
         ctx->style.colors[NK_COLOR_SLIDER_CURSOR] = nk_rgba(48, 83, 111, 245);
         ctx->style.colors[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
         ctx->style.colors[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
         ctx->style.colors[NK_COLOR_PROGRESS] = nk_rgba(50, 58, 61, 255);
         ctx->style.colors[NK_COLOR_PROGRESS_CURSOR] = nk_rgba(48, 83, 111, 255);
         ctx->style.colors[NK_COLOR_PROGRESS_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
         ctx->style.colors[NK_COLOR_PROGRESS_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
         ctx->style.colors[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 255);
         ctx->style.colors[NK_COLOR_PROPERTY_HOVER] = nk_rgba(55, 63, 66, 255);
         ctx->style.colors[NK_COLOR_PROPERTY_ACTIVE] = nk_rgba(60, 68, 71, 255);
         ctx->style.colors[NK_COLOR_INPUT] = nk_rgba(50, 58, 61, 225);
         ctx->style.colors[NK_COLOR_INPUT_CURSOR] = nk_rgba(210, 210, 210, 255);
         ctx->style.colors[NK_COLOR_INPUT_TEXT] = nk_rgba(210, 210, 210, 255);
         ctx->style.colors[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 255);
         ctx->style.colors[NK_COLOR_HISTO] = nk_rgba(50, 58, 61, 255);
         ctx->style.colors[NK_COLOR_HISTO_BARS] = nk_rgba(48, 83, 111, 255);
         ctx->style.colors[NK_COLOR_HISTO_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
         ctx->style.colors[NK_COLOR_PLOT] = nk_rgba(50, 58, 61, 255);
         ctx->style.colors[NK_COLOR_PLOT_LINES] = nk_rgba(48, 83, 111, 255);
         ctx->style.colors[NK_COLOR_PLOT_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
         ctx->style.colors[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 255);
         ctx->style.colors[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 255);
         ctx->style.colors[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
         ctx->style.colors[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
         ctx->style.colors[NK_COLOR_TABLE_LINES] = nk_rgba(100, 100, 100, 255);
         ctx->style.colors[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 255);
         ctx->style.colors[NK_COLOR_SCALER] = nk_rgba(100, 100, 100, 255);
         break;
      default:
         nk_load_default_style(ctx, NK_DEFAULT_ALL);
   }
}

void zrmenu_set_state(zrmenu_handle_t *zr, const int id,
   struct nk_vec2 pos, struct nk_vec2 size)
{
   zr->window[id].position = pos;
   zr->window[id].size = size;
}

void zrmenu_get_state(zrmenu_handle_t *zr, const int id,
   struct nk_vec2 *pos, struct nk_vec2 *size)
{
   *pos = zr->window[id].position;
   *size = zr->window[id].size;
}

void zrmenu_wnd_shader_parameters(zrmenu_handle_t *zr)
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
      enum   zrmenu_theme old     = zr->theme;

      nk_layout_row_dynamic(ctx, 30, 1);

      video_shader_driver_ctl(SHADER_CTL_GET_CURRENT_SHADER, &shader_info);

      if (shader_info.data)
      {
         for (i = 0; i < GFX_MAX_PARAMETERS; i++)
         {
            if (!string_is_empty(shader_info.data->parameters[i].desc))
            {
               if(shader_info.data->parameters[i].minimum == 0 &&
                     shader_info.data->parameters[i].maximum == 1 &&
                     shader_info.data->parameters[i].step == 1)
                  nk_checkbox_float(ctx, shader_info.data->parameters[i].desc,
                        &(shader_info.data->parameters[i].current));
               else
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
   zrmenu_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   nk_end(ctx);
}

void zrmenu_wnd_control(zrmenu_handle_t *zr)
{
   struct nk_panel layout;
   struct nk_context *ctx = &zr->ctx;
   const int id           = ZRMENU_WND_CONTROL;
   static int wnd_x       = 900;
   static int wnd_y       = 60;
   bool ret               = (nk_begin(ctx, &layout, "Control",
      nk_rect(wnd_x, wnd_y, 350, 520),
      NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_MOVABLE|
      NK_WINDOW_SCALABLE|NK_WINDOW_BORDER));

   if (ret)
   {
      unsigned i;

      /* Style */
      if (nk_layout_push(ctx, NK_LAYOUT_TAB, "Metrics", NK_MINIMIZED))
      {
         nk_layout_row_dynamic(ctx, 20, 2);
         nk_label(ctx,"Total:", NK_TEXT_LEFT);
         nk_labelf(ctx, NK_TEXT_LEFT, "%lu", zr->status.size);
         nk_label(ctx,"Used:", NK_TEXT_LEFT);
         nk_labelf(ctx, NK_TEXT_LEFT, "%lu", zr->status.allocated);
         nk_label(ctx,"Required:", NK_TEXT_LEFT);
         nk_labelf(ctx, NK_TEXT_LEFT, "%lu", zr->status.needed);
         nk_label(ctx,"Calls:", NK_TEXT_LEFT);
         nk_labelf(ctx, NK_TEXT_LEFT, "%lu", zr->status.calls);
         nk_layout_pop(ctx);
      }
      if (nk_layout_push(ctx, NK_LAYOUT_TAB, "Properties", NK_MINIMIZED))
      {
         nk_layout_row_dynamic(ctx, 22, 3);
         for (i = 0; i <= NK_PROPERTY_SCROLLBAR_SIZE; ++i)
         {
            nk_label(ctx, nk_get_property_name((enum nk_style_properties)i), NK_TEXT_LEFT);
            nk_property_float(ctx, "#X:", 0, &ctx->style.properties[i].x, 20, 1, 1);
            nk_property_float(ctx, "#Y:", 0, &ctx->style.properties[i].y, 20, 1, 1);
         }
         nk_layout_pop(ctx);
      }
      if (nk_layout_push(ctx, NK_LAYOUT_TAB, "Rounding", NK_MINIMIZED))
      {
         nk_layout_row_dynamic(ctx, 22, 2);
         for (i = 0; i < NK_ROUNDING_MAX; ++i)
         {
            nk_label(ctx, nk_get_rounding_name((enum nk_style_rounding)i), NK_TEXT_LEFT);
            nk_property_float(ctx, "#R:", 0, &ctx->style.rounding[i], 20, 1, 1);
         }
         nk_layout_pop(ctx);
      }
   }
   /* save position and size to restore after context reset */
   zrmenu_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   nk_end(ctx);
}

void zrmenu_wnd_test(zrmenu_handle_t *zr)
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
      enum   zrmenu_theme old     = zr->theme;

      nk_layout_row_dynamic(ctx, 30, 2);

      if (nk_button_text(ctx, "Quit", NK_BUTTON_DEFAULT))
      {
            /* event handling */
            printf("Pressed Event\n");
            rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
      }

      if (nk_button_text(ctx, "Quit", NK_BUTTON_DEFAULT))
      {
            /* event handling */
            printf("Pressed Event\n");
            rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
      }
      nk_layout_row_dynamic(ctx, 30, 4);
      nk_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      nk_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      nk_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      nk_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      nk_layout_row_dynamic(ctx, 30, 2);
      nk_label(ctx, "Volume:", NK_TEXT_LEFT);
      nk_slider_float(ctx, -80, &settings->audio.volume, 12, 0.5);
      nk_layout_row_dynamic(ctx, 30, 1);
      nk_property_int(ctx, "Max Users:", 1, (int*)&(settings->input.max_users),
         MAX_USERS, 1, 1);

      if (nk_combo_begin_text(ctx, &combo, themes[zr->theme], 200))
      {
         nk_layout_row_dynamic(ctx, 25, 1);
         zr->theme = nk_combo_item(ctx, themes[THEME_DARK], NK_TEXT_CENTERED)
            ? THEME_DARK : zr->theme;
         zr->theme = nk_combo_item(ctx, themes[THEME_LIGHT], NK_TEXT_CENTERED)
            ? THEME_LIGHT : zr->theme;
         if (old != zr->theme) zrmenu_set_style(ctx, zr->theme);
         nk_combo_end(ctx);
      }

      nk_label(ctx, "History:", NK_TEXT_LEFT);

      size = menu_entries_get_size();
      if (nk_combo_begin_text(ctx, &combo, "", 180))
      {
         unsigned i;

         for (i = 0; i < size; i++)
         {
            menu_entry_get(&entry, 0, i, NULL, true);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_combo_item(ctx, entry.path, NK_TEXT_LEFT);
         }
         nk_combo_end(ctx);
      }
   }
   /* save position and size to restore after context reset */
   zrmenu_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   nk_end(ctx);
}

void zrmenu_wnd_main(zrmenu_handle_t *zr)
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
      if (nk_contextual_begin(ctx, &context_menu, 0, nk_vec2(100, 220),
         nk_window_get_bounds(ctx)))
      {
          nk_layout_row_dynamic(ctx, 25, 1);
          if (nk_contextual_item(ctx, "Test 1", NK_TEXT_CENTERED))
             printf("test \n");
          if (nk_contextual_item(ctx, "Test 2",NK_TEXT_CENTERED))
             printf("test \n");
          nk_contextual_end(ctx);
      }

      /* main menu */
      nk_menubar_begin(ctx);
      nk_layout_row_begin(ctx, NK_STATIC, 25, 1);
      nk_layout_row_push(ctx, 100);

      if (nk_menu_text_begin(ctx, &menu, "Menu", NK_TEXT_LEFT|NK_TEXT_MIDDLE, 120))
      {
          nk_layout_row_dynamic(ctx, 25, 1);

          if (nk_menu_item(ctx, NK_TEXT_LEFT, "Test"))
            printf("test \n");
          if (nk_menu_item(ctx, NK_TEXT_LEFT, "About"))
             printf("test \n");
         if (nk_menu_item(ctx, NK_TEXT_LEFT, "Exit"))
            rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);

         nk_menu_end(ctx);
      }
      if (nk_menu_text_begin(ctx, &menu, "Window", NK_TEXT_LEFT|NK_TEXT_MIDDLE, 120))
      {
          nk_layout_row_dynamic(ctx, 25, 1);

         if (nk_menu_item(ctx, NK_TEXT_LEFT, "Control"))
         {
            nk_window_close(ctx, "Control");
            zr->window[ZRMENU_WND_CONTROL].open =
               !zr->window[ZRMENU_WND_CONTROL].open;
         }

         if (nk_menu_item(ctx, NK_TEXT_LEFT, "Shader Parameters"))
         {
            nk_window_close(ctx, "Shader Parameters");
            zr->window[ZRMENU_WND_SHADER_PARAMETERS].open =
               !zr->window[ZRMENU_WND_SHADER_PARAMETERS].open;
         }

         if (nk_menu_item(ctx, NK_TEXT_LEFT, "Test Window"))
         {
            nk_window_close(ctx, "Test");
            zr->window[ZRMENU_WND_TEST].open =
               !zr->window[ZRMENU_WND_TEST].open;
         }

         if (nk_menu_item(ctx, NK_TEXT_LEFT, "Wizard"))
         {
            nk_window_close(ctx, "Test");
            zr->window[ZRMENU_WND_WIZARD].open =
               !zr->window[ZRMENU_WND_WIZARD].open;
         }

         nk_menu_end(ctx);
      }
      nk_layout_row_push(ctx, 60);
      nk_menubar_end(ctx);
   }


   /* save position and size to restore after context reset */
   zrmenu_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   if (zr->size_changed)
      nk_window_set_size(ctx, nk_vec2(nk_window_get_size(ctx).x, zr->size.y));

   nk_end(ctx);
}

void zrmenu_wnd_wizard(zrmenu_handle_t *zr)
{
   struct nk_panel layout;
   struct nk_context *ctx = &zr->ctx;
   const int id           = ZRMENU_WND_WIZARD;
   static int width       = 800;
   static int height      = 600;
   static int panel       = 0;
   settings_t *settings   = config_get_ptr();

   if (nk_begin(ctx, &layout, "Setup Wizard", nk_rect(zr->size.x/2 -width/2,
      zr->size.y/2 - height/2, width, height),
         NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_MOVABLE|
         NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
   {
      /* uncomment this to hide the panel backgrounds
      nk_push_color(ctx, NK_COLOR_WINDOW, nk_rgba(0,0,0,0)); */
      struct nk_panel sub;
      switch (panel)
      {
         case 0:
            nk_layout_row_begin(ctx, NK_DYNAMIC, height * 0.80f, 2);
            nk_layout_row_push(ctx, 0.15f);
            if (nk_group_begin(ctx, &sub, "", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
            {

               nk_layout_space_begin(ctx, NK_STATIC, width * 0.15f, 1);
               nk_layout_space_push(ctx, nk_rect(0, 0, width * 0.15f, width * 0.15f));
               nk_image(ctx, zr->icons.invader);
               nk_layout_space_end(ctx);
               nk_group_end(ctx);
            }
            nk_layout_row_push(ctx, 0.85f);

            if (nk_group_begin(ctx, &sub, "", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
            {

               nk_layout_row_dynamic(ctx, 14, 15);
               nk_image(ctx, zr->icons.page_on);
               nk_image(ctx, zr->icons.page_off);
               nk_image(ctx, zr->icons.page_off);
               nk_image(ctx, zr->icons.page_off);
               nk_image(ctx, zr->icons.page_off);

               nk_layout_row_dynamic(ctx, 20, 1);
               nk_label(ctx, "Label aligned left", NK_TEXT_LEFT);
               nk_label(ctx, "Label aligned centered", NK_TEXT_CENTERED);
               nk_label(ctx, "Label aligned right", NK_TEXT_RIGHT);
               nk_label_colored(ctx, "Blue text", NK_TEXT_LEFT, nk_rgb(0,0,255));
               nk_label_colored(ctx, "Yellow text", NK_TEXT_LEFT, nk_rgb(255,255,0));
               nk_text(ctx, "Text without /0", 15, NK_TEXT_RIGHT);
               nk_layout_row_dynamic(ctx, 100, 1);
               nk_label_wrap(ctx, "This is a very long line to hopefully get this text to be wrapped into multiple lines to show line wrapping, someone should write some text welcoming users to retroarch or something like that in this window,  I'm not really good at this");
               nk_group_end(ctx);
            }
            nk_layout_row_end(ctx);
            nk_layout_row_dynamic(ctx, 30, 4);
            nk_label(ctx,"", NK_TEXT_RIGHT);
            nk_label(ctx,"", NK_TEXT_RIGHT);
            nk_label(ctx,"", NK_TEXT_RIGHT);
            if (nk_button_text(ctx, "Next", NK_BUTTON_DEFAULT))
               panel++;
            break;
         case 1:
            nk_layout_row_begin(ctx, NK_DYNAMIC, height * 0.80f, 2);
            nk_layout_row_push(ctx, 0.15f);
            if (nk_group_begin(ctx, &sub, "", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER))
            {
               nk_layout_space_begin(ctx, NK_STATIC, width * 0.15f, 1);
               nk_layout_space_push(ctx, nk_rect(0, 0, width * 0.15f, width * 0.15f));
               nk_layout_space_end(ctx);
               nk_group_end(ctx);
            }
            nk_layout_row_push(ctx, 0.85f);

            if (nk_group_begin(ctx, &sub, "", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
            {

               nk_layout_row_dynamic(ctx, 14, 15);
               nk_image(ctx, zr->icons.page_on);
               nk_image(ctx, zr->icons.page_on);
               nk_image(ctx, zr->icons.page_off);
               nk_image(ctx, zr->icons.page_off);
               nk_image(ctx, zr->icons.page_off);

               nk_layout_row_dynamic(ctx, 40, 1);
               nk_button_text_image(ctx, zr->icons.folder, settings->libretro, NK_TEXT_CENTERED, NK_BUTTON_DEFAULT);
               nk_button_text_image(ctx, zr->icons.folder, settings->libretro, NK_TEXT_CENTERED, NK_BUTTON_DEFAULT);
               nk_button_text_image(ctx, zr->icons.folder, settings->libretro, NK_TEXT_CENTERED, NK_BUTTON_DEFAULT);
               nk_button_text_image(ctx, zr->icons.folder, settings->libretro, NK_TEXT_CENTERED, NK_BUTTON_DEFAULT);
               nk_button_text_image(ctx, zr->icons.folder, settings->libretro, NK_TEXT_CENTERED, NK_BUTTON_DEFAULT);
               nk_button_text_image(ctx, zr->icons.folder, settings->libretro, NK_TEXT_CENTERED, NK_BUTTON_DEFAULT);
               nk_button_text_image(ctx, zr->icons.folder, settings->libretro, NK_TEXT_CENTERED, NK_BUTTON_DEFAULT);
               nk_button_text_image(ctx, zr->icons.folder, settings->libretro, NK_TEXT_CENTERED, NK_BUTTON_DEFAULT);
               nk_group_end(ctx);
            }
            nk_layout_row_end(ctx);
            nk_layout_row_dynamic(ctx, 30, 4);
            nk_label(ctx,"", NK_TEXT_RIGHT);
            nk_label(ctx,"", NK_TEXT_RIGHT);
            if (nk_button_text(ctx, "Previous", NK_BUTTON_DEFAULT))
               panel--;
            if (nk_button_text(ctx, "Next", NK_BUTTON_DEFAULT))
               panel++;
            break;
         case 2:
            nk_layout_row_begin(ctx, NK_DYNAMIC, height * 0.80f, 2);
            nk_layout_row_push(ctx, 0.15f);
            if (nk_group_begin(ctx, &sub, "", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER))
            {
               nk_layout_space_begin(ctx, NK_STATIC, width * 0.15f, 1);
               nk_layout_space_push(ctx, nk_rect(0, 0, width * 0.15f, width * 0.15f));
               nk_layout_space_end(ctx);
               nk_group_end(ctx);
            }
            nk_layout_row_push(ctx, 0.85f);

            if (nk_group_begin(ctx, &sub, "", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
            {

               nk_layout_row_dynamic(ctx, 14, 15);
               nk_image(ctx, zr->icons.page_on);
               nk_image(ctx, zr->icons.page_on);
               nk_image(ctx, zr->icons.page_on);
               nk_image(ctx, zr->icons.page_off);
               nk_image(ctx, zr->icons.page_off);

               nk_layout_row_dynamic(ctx, 50, 1);
               ui_selector(ctx, "Windowed Mode", &window_mode, display, LEN(display), WINDOW_MODE == active);
               ui_selector(ctx, "Model Detail", &model_detail, detail, LEN(detail), MODEL_DETAIL == active);
               ui_selector(ctx, "Textures", &texture_detail, detail, LEN(detail), TEXTURES == active);
               ui_selector(ctx, "Shadows", &shadow_detail, detail, LEN(detail), SHADOWS == active);
               ui_selector(ctx, "Lighting", &lighning_detail, detail, LEN(detail), LIGHTNING == active);
               ui_selector(ctx, "Effects", &effects_detail, detail, LEN(detail), EFFECTS == active);
               ui_selector(ctx, "Console", &show_console, state, LEN(state), CONSOLE == active);
               ui_slider(ctx, "Brightness", &brightness, 100, BRIGHTNESS == active);
               ui_slider(ctx, "Volume", &volume, 100, VOLUME == active);

               if (nk_input_is_key_pressed(&ctx->input, NK_KEY_UP))
                   active = MAX(0, active-1);
               if (nk_input_is_key_pressed(&ctx->input, NK_KEY_DOWN))
                   active = MIN(active+1, WIDGET_MAX - 2);
               nk_group_end(ctx);
            }
            nk_layout_row_end(ctx);
            nk_layout_row_dynamic(ctx, 30, 4);
            nk_label(ctx,"", NK_TEXT_RIGHT);
            nk_label(ctx,"", NK_TEXT_RIGHT);
            if (nk_button_text(ctx, "Previous", NK_BUTTON_DEFAULT))
               panel--;
            if (nk_button_text(ctx, "Next", NK_BUTTON_DEFAULT))
               panel++;
            break;
         default:
            nk_layout_row_begin(ctx, NK_DYNAMIC, height * 0.80f, 2);
            nk_layout_row_push(ctx, 0.15f);
            if (nk_group_begin(ctx, &sub, "", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER))
            {
               nk_layout_space_begin(ctx, NK_STATIC, width * 0.15f, 1);
               nk_layout_space_push(ctx, nk_rect(0, 0, width * 0.15f, width * 0.15f));
               nk_layout_space_end(ctx);
               nk_group_end(ctx);
            }
            nk_layout_row_push(ctx, 0.85f);

            if (nk_group_begin(ctx, &sub, "", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
            {
               nk_layout_row_dynamic(ctx, 14, 15);
               nk_image(ctx, zr->icons.page_on);
               nk_image(ctx, zr->icons.page_on);
               nk_image(ctx, zr->icons.page_on);
               nk_image(ctx, zr->icons.page_off);
               nk_image(ctx, zr->icons.page_off);

               nk_layout_row_dynamic(ctx, 40, 1);
               nk_group_end(ctx);
            }
            nk_layout_row_end(ctx);
            nk_layout_row_dynamic(ctx, 30, 4);
            nk_label(ctx,"", NK_TEXT_RIGHT);
            nk_label(ctx,"", NK_TEXT_RIGHT);
            if (nk_button_text(ctx, "Previous", NK_BUTTON_DEFAULT))
               panel--;
            if (nk_button_text(ctx, "Next", NK_BUTTON_DEFAULT))
               panel++;
            break;
      }

      nk_reset_colors(ctx);
   }
   /* save position and size to restore after context reset */
   zrmenu_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   nk_end(ctx);
}
