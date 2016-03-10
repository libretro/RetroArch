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

#include "zr_menu.h"

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <string/string_list.h>

#include "../menu_driver.h"
#include "../menu_hash.h"

#include "../../gfx/common/gl_common.h"
#include "../../core_info.h"
#include "../../configuration.h"

bool zr_checkbox_bool(struct zr_context* cx, const char* text, bool *active)
{
   int    x = *active;
   bool ret = zr_checkbox(cx, text, &x);
   *active  = x;

   return ret;
}

float zr_checkbox_float(struct zr_context* cx, const char* text, float *active)
{
   int x     = *active;
   float ret = zr_checkbox(cx, text, &x);
   *active   = x;

   return ret;
}

static void zr_labelf(struct zr_context *ctx,
      enum zr_text_align align, const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    buffer[1023] = 0;
    zr_label(ctx, buffer, align);
    va_end(args);
}

void zrmenu_set_style(struct zr_context *ctx, enum zrmenu_theme theme)
{
   unsigned i;

   for (i = 0; i < ZR_ROUNDING_MAX; ++i)
      (ctx)->style.rounding[i] = 0;

   switch (theme)
   {
      case THEME_LIGHT:
         ctx->style.rounding[ZR_ROUNDING_SCROLLBAR] = 0;
         ctx->style.properties[ZR_PROPERTY_SCROLLBAR_SIZE] = zr_vec2(10,10);
         ctx->style.colors[ZR_COLOR_TEXT] = zr_rgba(20, 20, 20, 255);
         ctx->style.colors[ZR_COLOR_TEXT_HOVERING] = zr_rgba(195, 195, 195, 255);
         ctx->style.colors[ZR_COLOR_TEXT_ACTIVE] = zr_rgba(200, 200, 200, 255);
         ctx->style.colors[ZR_COLOR_WINDOW] = zr_rgba(202, 212, 214, 215);
         ctx->style.colors[ZR_COLOR_HEADER] = zr_rgba(137, 182, 224, 220);
         ctx->style.colors[ZR_COLOR_BORDER] = zr_rgba(140, 159, 173, 0);
         ctx->style.colors[ZR_COLOR_BUTTON] = zr_rgba(137, 182, 224, 255);
         ctx->style.colors[ZR_COLOR_BUTTON_HOVER] = zr_rgba(142, 187, 229, 255);
         ctx->style.colors[ZR_COLOR_BUTTON_ACTIVE] = zr_rgba(147, 192, 234, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE] = zr_rgba(177, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE_HOVER] = zr_rgba(245, 245, 245, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE_CURSOR] = zr_rgba(142, 187, 229, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE] = zr_rgba(147, 192, 234, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE_HOVER] = zr_rgba(150, 150, 150, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE_TEXT] = zr_rgba(70, 70, 70, 255);
         ctx->style.colors[ZR_COLOR_SLIDER] = zr_rgba(177, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR] = zr_rgba(137, 182, 224, 245);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR_HOVER] = zr_rgba(142, 188, 229, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR_ACTIVE] = zr_rgba(147, 193, 234, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS] = zr_rgba(177, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR] = zr_rgba(137, 182, 224, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR_HOVER] = zr_rgba(142, 188, 229, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR_ACTIVE] = zr_rgba(147, 193, 234, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY] = zr_rgba(210, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY_HOVER] = zr_rgba(235, 235, 235, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY_ACTIVE] = zr_rgba(230, 230, 230, 255);
         ctx->style.colors[ZR_COLOR_INPUT] = zr_rgba(210, 210, 210, 225);
         ctx->style.colors[ZR_COLOR_INPUT_CURSOR] = zr_rgba(20, 20, 20, 255);
         ctx->style.colors[ZR_COLOR_INPUT_TEXT] = zr_rgba(20, 20, 20, 255);
         ctx->style.colors[ZR_COLOR_COMBO] = zr_rgba(210, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_HISTO] = zr_rgba(210, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_HISTO_BARS] = zr_rgba(137, 182, 224, 255);
         ctx->style.colors[ZR_COLOR_HISTO_HIGHLIGHT] = zr_rgba( 255, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_PLOT] = zr_rgba(210, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_PLOT_LINES] = zr_rgba(137, 182, 224, 255);
         ctx->style.colors[ZR_COLOR_PLOT_HIGHLIGHT] = zr_rgba(255, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR] = zr_rgba(190, 200, 200, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR] = zr_rgba(64, 84, 95, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR_HOVER] = zr_rgba(70, 90, 100, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR_ACTIVE] = zr_rgba(75, 95, 105, 255);
         ctx->style.colors[ZR_COLOR_TABLE_LINES] = zr_rgba(100, 100, 100, 255);
         ctx->style.colors[ZR_COLOR_TAB_HEADER] = zr_rgba(156, 193, 220, 255);
         ctx->style.colors[ZR_COLOR_SCALER] = zr_rgba(100, 100, 100, 255);
         break;
      case THEME_DARK:
         ctx->style.rounding[ZR_ROUNDING_SCROLLBAR] = 0;
         ctx->style.properties[ZR_PROPERTY_SCROLLBAR_SIZE] = zr_vec2(10,10);
         ctx->style.colors[ZR_COLOR_TEXT] = zr_rgba(210, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_TEXT_HOVERING] = zr_rgba(195, 195, 195, 255);
         ctx->style.colors[ZR_COLOR_TEXT_ACTIVE] = zr_rgba(200, 200, 200, 255);
         ctx->style.colors[ZR_COLOR_WINDOW] = zr_rgba(45, 53, 56, 215);
         ctx->style.colors[ZR_COLOR_HEADER] = zr_rgba(46, 46, 46, 255);
         ctx->style.colors[ZR_COLOR_BORDER] = zr_rgba(46, 46, 46, 0);
         ctx->style.colors[ZR_COLOR_BUTTON] = zr_rgba(48, 83, 111, 255);
         ctx->style.colors[ZR_COLOR_BUTTON_HOVER] = zr_rgba(53, 88, 116, 255);
         ctx->style.colors[ZR_COLOR_BUTTON_ACTIVE] = zr_rgba(58, 93, 121, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE] = zr_rgba(50, 58, 61, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE_HOVER] = zr_rgba(55, 63, 66, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE_CURSOR] = zr_rgba(48, 83, 111, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE] = zr_rgba(48, 83, 111, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE_HOVER] = zr_rgba(48, 83, 111, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE_TEXT] = zr_rgba(210, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_SLIDER] = zr_rgba(50, 58, 61, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR] = zr_rgba(48, 83, 111, 245);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR_HOVER] = zr_rgba(53, 88, 116, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR_ACTIVE] = zr_rgba(58, 93, 121, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS] = zr_rgba(50, 58, 61, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR] = zr_rgba(48, 83, 111, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR_HOVER] = zr_rgba(53, 88, 116, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR_ACTIVE] = zr_rgba(58, 93, 121, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY] = zr_rgba(50, 58, 61, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY_HOVER] = zr_rgba(55, 63, 66, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY_ACTIVE] = zr_rgba(60, 68, 71, 255);
         ctx->style.colors[ZR_COLOR_INPUT] = zr_rgba(50, 58, 61, 225);
         ctx->style.colors[ZR_COLOR_INPUT_CURSOR] = zr_rgba(210, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_INPUT_TEXT] = zr_rgba(210, 210, 210, 255);
         ctx->style.colors[ZR_COLOR_COMBO] = zr_rgba(50, 58, 61, 255);
         ctx->style.colors[ZR_COLOR_HISTO] = zr_rgba(50, 58, 61, 255);
         ctx->style.colors[ZR_COLOR_HISTO_BARS] = zr_rgba(48, 83, 111, 255);
         ctx->style.colors[ZR_COLOR_HISTO_HIGHLIGHT] = zr_rgba(255, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_PLOT] = zr_rgba(50, 58, 61, 255);
         ctx->style.colors[ZR_COLOR_PLOT_LINES] = zr_rgba(48, 83, 111, 255);
         ctx->style.colors[ZR_COLOR_PLOT_HIGHLIGHT] = zr_rgba(255, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR] = zr_rgba(50, 58, 61, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR] = zr_rgba(48, 83, 111, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR_HOVER] = zr_rgba(53, 88, 116, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR_ACTIVE] = zr_rgba(58, 93, 121, 255);
         ctx->style.colors[ZR_COLOR_TABLE_LINES] = zr_rgba(100, 100, 100, 255);
         ctx->style.colors[ZR_COLOR_TAB_HEADER] = zr_rgba(48, 83, 111, 255);
         ctx->style.colors[ZR_COLOR_SCALER] = zr_rgba(100, 100, 100, 255);
         break;
      default:
         zr_load_default_style(ctx, ZR_DEFAULT_ALL);
   }
}

void zrmenu_set_state(zrmenu_handle_t *zr, const int id,
   struct zr_vec2 pos, struct zr_vec2 size)
{
   zr->window[id].position = pos;
   zr->window[id].size = size;
}

void zrmenu_get_state(zrmenu_handle_t *zr, const int id,
   struct zr_vec2 *pos, struct zr_vec2 *size)
{
   *pos = zr->window[id].position;
   *size = zr->window[id].size;
}

void zrmenu_wnd_shader_parameters(zrmenu_handle_t *zr)
{
   struct zr_context *ctx = &zr->ctx;
   const int id = ZRMENU_WND_SHADER_PARAMETERS;
   settings_t *settings = config_get_ptr();

   video_shader_ctx_t shader_info;
   unsigned i;

   struct zr_panel layout;

   if (zr_begin(ctx, &layout, "Shader Parameters", zr_rect(240, 10, 300, 400),
         ZR_WINDOW_CLOSABLE|ZR_WINDOW_MINIMIZABLE|ZR_WINDOW_MOVABLE|
         ZR_WINDOW_SCALABLE|ZR_WINDOW_BORDER))
   {
      struct zr_panel combo;
      static const char *themes[] = {"Dark", "Light"};
      enum   zrmenu_theme old         = zr->theme;

      zr_layout_row_dynamic(ctx, 30, 1);

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
                  zr_checkbox_float(ctx, shader_info.data->parameters[i].desc,
                        &(shader_info.data->parameters[i].current));
               else
                  zr_property_float(ctx, shader_info.data->parameters[i].desc,
                        shader_info.data->parameters[i].minimum,
                        &(shader_info.data->parameters[i].current),
                        shader_info.data->parameters[i].maximum,
                        shader_info.data->parameters[i].step, 1);
            }
         }
      }
   }
   /* save position and size to restore after context reset */
   zrmenu_set_state(zr, id, zr_window_get_position(ctx), zr_window_get_size(ctx));
   zr_end(ctx);
}

void zrmenu_wnd_control(zrmenu_handle_t *zr)
{
   struct zr_context *ctx = &zr->ctx;
   const int id = ZRMENU_WND_CONTROL;
   static int wnd_x = 900;
   static int wnd_y = 60;
   struct zr_panel layout;

   bool ret = (zr_begin(ctx, &layout, "Control",
      zr_rect(wnd_x, wnd_y, 350, 520),
      ZR_WINDOW_CLOSABLE|ZR_WINDOW_MINIMIZABLE|ZR_WINDOW_MOVABLE|
      ZR_WINDOW_SCALABLE|ZR_WINDOW_BORDER));

   if (ret)
   {
      unsigned i;

      /* Style */
      if (zr_layout_push(ctx, ZR_LAYOUT_TAB, "Metrics", ZR_MINIMIZED))
      {
         zr_layout_row_dynamic(ctx, 20, 2);
         zr_label(ctx,"Total:", ZR_TEXT_LEFT);
         zr_labelf(ctx, ZR_TEXT_LEFT, "%lu", zr->status.size);
         zr_label(ctx,"Used:", ZR_TEXT_LEFT);
         zr_labelf(ctx, ZR_TEXT_LEFT, "%lu", zr->status.allocated);
         zr_label(ctx,"Required:", ZR_TEXT_LEFT);
         zr_labelf(ctx, ZR_TEXT_LEFT, "%lu", zr->status.needed);
         zr_label(ctx,"Calls:", ZR_TEXT_LEFT);
         zr_labelf(ctx, ZR_TEXT_LEFT, "%lu", zr->status.calls);
         zr_layout_pop(ctx);
      }
      if (zr_layout_push(ctx, ZR_LAYOUT_TAB, "Properties", ZR_MINIMIZED))
      {
         zr_layout_row_dynamic(ctx, 22, 3);
         for (i = 0; i <= ZR_PROPERTY_SCROLLBAR_SIZE; ++i)
         {
            zr_label(ctx, zr_get_property_name((enum zr_style_properties)i), ZR_TEXT_LEFT);
            zr_property_float(ctx, "#X:", 0, &ctx->style.properties[i].x, 20, 1, 1);
            zr_property_float(ctx, "#Y:", 0, &ctx->style.properties[i].y, 20, 1, 1);
         }
         zr_layout_pop(ctx);
      }
      if (zr_layout_push(ctx, ZR_LAYOUT_TAB, "Rounding", ZR_MINIMIZED))
      {
         zr_layout_row_dynamic(ctx, 22, 2);
         for (i = 0; i < ZR_ROUNDING_MAX; ++i)
         {
            zr_label(ctx, zr_get_rounding_name((enum zr_style_rounding)i), ZR_TEXT_LEFT);
            zr_property_float(ctx, "#R:", 0, &ctx->style.rounding[i], 20, 1, 1);
         }
         zr_layout_pop(ctx);
      }
   }
   /* save position and size to restore after context reset */
   zrmenu_set_state(zr, id, zr_window_get_position(ctx), zr_window_get_size(ctx));
   zr_end(ctx);
}

void zrmenu_wnd_test(zrmenu_handle_t *zr)
{
   struct zr_context *ctx = &zr->ctx;
   const int id = ZRMENU_WND_TEST;
   settings_t *settings = config_get_ptr();

   struct zr_panel layout;
   if (zr_begin(ctx, &layout, "Test", zr_rect(140, 90, 500, 600),
         ZR_WINDOW_CLOSABLE|ZR_WINDOW_MINIMIZABLE|ZR_WINDOW_MOVABLE|
         ZR_WINDOW_SCALABLE|ZR_WINDOW_BORDER))
   {
      unsigned size;
      struct zr_panel combo;
      menu_entry_t entry;
      static const char *themes[] = {"Dark", "Light"};
      enum   zrmenu_theme old         = zr->theme;

      zr_layout_row_dynamic(ctx, 30, 2);

      if (zr_button_text(ctx, "Quit", ZR_BUTTON_DEFAULT))
      {
            /* event handling */
            printf("Pressed Event\n");
            rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
      }

      if (zr_button_text(ctx, "Quit", ZR_BUTTON_DEFAULT))
      {
            /* event handling */
            printf("Pressed Event\n");
            rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
      }
      zr_layout_row_dynamic(ctx, 30, 4);
      zr_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      zr_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      zr_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      zr_checkbox_bool(ctx, "Show FPS", &(settings->fps_show));
      zr_layout_row_dynamic(ctx, 30, 2);
      zr_label(ctx, "Volume:", ZR_TEXT_LEFT);
      zr_slider_float(ctx, -80, &settings->audio.volume, 12, 0.5);
      zr_layout_row_dynamic(ctx, 30, 1);
      zr_property_int(ctx, "Max Users:", 1, (int*)&(settings->input.max_users),
         MAX_USERS, 1, 1);

      if (zr_combo_begin_text(ctx, &combo, themes[zr->theme], 200))
      {
         zr_layout_row_dynamic(ctx, 25, 1);
         zr->theme = zr_combo_item(ctx, themes[THEME_DARK], ZR_TEXT_CENTERED)
            ? THEME_DARK : zr->theme;
         zr->theme = zr_combo_item(ctx, themes[THEME_LIGHT], ZR_TEXT_CENTERED)
            ? THEME_LIGHT : zr->theme;
         if (old != zr->theme) zrmenu_set_style(ctx, zr->theme);
         zr_combo_end(ctx);
      }

      zr_label(ctx, "History:", ZR_TEXT_LEFT);

      size = menu_entries_get_size();
      if (zr_combo_begin_text(ctx, &combo, "", 180))
      {
         unsigned i;

         for (i = 0; i < size; i++)
         {
            menu_entry_get(&entry, 0, i, NULL, true);
            zr_layout_row_dynamic(ctx, 25, 1);
            zr_combo_item(ctx, entry.path, ZR_TEXT_LEFT);
         }
         zr_combo_end(ctx);
      }
   }
   /* save position and size to restore after context reset */
   zrmenu_set_state(zr, id, zr_window_get_position(ctx), zr_window_get_size(ctx));
   zr_end(ctx);
}

void zrmenu_wnd_main(zrmenu_handle_t *zr)
{
   struct zr_context *ctx = &zr->ctx;
   const int id = ZRMENU_WND_MAIN;

   settings_t *settings = config_get_ptr();
   struct zr_panel layout;

   if (zr_begin(ctx, &layout, "Main", zr_rect(-1, -1, 120, zr->size.x + 1),
         ZR_WINDOW_NO_SCROLLBAR))
   {
      struct zr_panel menu;
      struct zr_panel node, context_menu;

      /* context menu */
      if (zr_contextual_begin(ctx, &context_menu, 0, zr_vec2(100, 220),
         zr_window_get_bounds(ctx)))
      {
          zr_layout_row_dynamic(ctx, 25, 1);
          if (zr_contextual_item(ctx, "Test 1", ZR_TEXT_CENTERED))
             printf("test \n");
          if (zr_contextual_item(ctx, "Test 2",ZR_TEXT_CENTERED))
             printf("test \n");
          zr_contextual_end(ctx);
      }

      /* main menu */
      zr_menubar_begin(ctx);
      zr_layout_row_begin(ctx, ZR_STATIC, 25, 1);
      zr_layout_row_push(ctx, 100);

      if (zr_menu_text_begin(ctx, &menu, "Menu", ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, 120))
      {
          zr_layout_row_dynamic(ctx, 25, 1);

          if (zr_menu_item(ctx, ZR_TEXT_LEFT, "Test"))
            printf("test \n");
          if (zr_menu_item(ctx, ZR_TEXT_LEFT, "About"))
             printf("test \n");
         if (zr_menu_item(ctx, ZR_TEXT_LEFT, "Exit"))
            rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);

         zr_menu_end(ctx);
      }
      if (zr_menu_text_begin(ctx, &menu, "Window", ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, 120))
      {
          zr_layout_row_dynamic(ctx, 25, 1);

         if (zr_menu_item(ctx, ZR_TEXT_LEFT, "Control"))
         {
            zr_window_close(ctx, "Control");
            zr->window[ZRMENU_WND_CONTROL].open =
               !zr->window[ZRMENU_WND_CONTROL].open;
         }

         if (zr_menu_item(ctx, ZR_TEXT_LEFT, "Shader Parameters"))
         {
            zr_window_close(ctx, "Shader Parameters");
            zr->window[ZRMENU_WND_SHADER_PARAMETERS].open =
               !zr->window[ZRMENU_WND_SHADER_PARAMETERS].open;
         }

         if (zr_menu_item(ctx, ZR_TEXT_LEFT, "Test Window"))
         {
            zr_window_close(ctx, "Test");
            zr->window[ZRMENU_WND_TEST].open =
               !zr->window[ZRMENU_WND_TEST].open;
         }

         if (zr_menu_item(ctx, ZR_TEXT_LEFT, "Wizard"))
         {
            zr_window_close(ctx, "Test");
            zr->window[ZRMENU_WND_WIZARD].open =
               !zr->window[ZRMENU_WND_WIZARD].open;
         }

         zr_menu_end(ctx);
      }
      zr_layout_row_push(ctx, 60);
      zr_menubar_end(ctx);

   }


   /* save position and size to restore after context reset */
   zrmenu_set_state(zr, id, zr_window_get_position(ctx), zr_window_get_size(ctx));
   if (zr->size_changed)
      zr_window_set_size(ctx, zr_vec2(zr_window_get_size(ctx).x, zr->size.y));

   zr_end(ctx);
}

void zrmenu_wnd_wizard(zrmenu_handle_t *zr)
{
   struct zr_context *ctx = &zr->ctx;
   const int id = ZRMENU_WND_WIZARD;

   static int width = 640;
   static int height = 480;
   static int panel = 0;

   settings_t *settings = config_get_ptr();
   struct zr_panel layout;

   if (zr_begin(ctx, &layout, "Setup Wizard", zr_rect(zr->size.x/2 -width/2,
      zr->size.y/2 - height/2, width, height),
         ZR_WINDOW_CLOSABLE|ZR_WINDOW_MINIMIZABLE|ZR_WINDOW_MOVABLE|
         ZR_WINDOW_BORDER|ZR_WINDOW_NO_SCROLLBAR))
   {
      /* uncomment this to hide the panel backgrounds
      zr_push_color(ctx, ZR_COLOR_WINDOW, zr_rgba(0,0,0,0)); */
      struct zr_panel sub;
      switch (panel)
      {
         case 0:
            zr_layout_row_begin(ctx, ZR_DYNAMIC, height * 0.80f, 2);
            zr_layout_row_push(ctx, 0.15f);
            if (zr_group_begin(ctx, &sub, "", ZR_WINDOW_NO_SCROLLBAR|ZR_WINDOW_BORDER|ZR_WINDOW_NO_SCROLLBAR))
            {

               zr_layout_space_begin(ctx, ZR_STATIC, width * 0.15f, 1);
               zr_layout_space_push(ctx, zr_rect(0, 0, width * 0.15f, width * 0.15f));
               zr_image(ctx, zr->icons.invader);
               zr_layout_space_end(ctx);
               zr_group_end(ctx);
            }
            zr_layout_row_push(ctx, 0.85f);

            if (zr_group_begin(ctx, &sub, "", ZR_WINDOW_NO_SCROLLBAR|ZR_WINDOW_BORDER|ZR_WINDOW_NO_SCROLLBAR))
            {

               zr_layout_row_dynamic(ctx, 14, 15);
               zr_image(ctx, zr->icons.page_on);
               zr_image(ctx, zr->icons.page_off);
               zr_image(ctx, zr->icons.page_off);
               zr_image(ctx, zr->icons.page_off);
               zr_image(ctx, zr->icons.page_off);

               zr_layout_row_dynamic(ctx, 20, 1);
               zr_label(ctx, "Label aligned left", ZR_TEXT_LEFT);
               zr_label(ctx, "Label aligned centered", ZR_TEXT_CENTERED);
               zr_label(ctx, "Label aligned right", ZR_TEXT_RIGHT);
               zr_label_colored(ctx, "Blue text", ZR_TEXT_LEFT, zr_rgb(0,0,255));
               zr_label_colored(ctx, "Yellow text", ZR_TEXT_LEFT, zr_rgb(255,255,0));
               zr_text(ctx, "Text without /0", 15, ZR_TEXT_RIGHT);
               zr_layout_row_dynamic(ctx, 100, 1);
               zr_label_wrap(ctx, "This is a very long line to hopefully get this text to be wrapped into multiple lines to show line wrapping, someone should write some text welcoming users to retroarch or something like that in this window,  I'm not really good at this");
               zr_group_end(ctx);
            }
            zr_layout_row_end(ctx);
            zr_layout_row_dynamic(ctx, 30, 4);
            zr_label(ctx,"", ZR_TEXT_RIGHT);
            zr_label(ctx,"", ZR_TEXT_RIGHT);
            zr_label(ctx,"", ZR_TEXT_RIGHT);
            if (zr_button_text(ctx, "Next", ZR_BUTTON_DEFAULT))
               panel++;
            break;
         case 1:
            zr_layout_row_begin(ctx, ZR_DYNAMIC, height * 0.80f, 2);
            zr_layout_row_push(ctx, 0.15f);
            if (zr_group_begin(ctx, &sub, "", ZR_WINDOW_NO_SCROLLBAR|ZR_WINDOW_BORDER))
            {
               zr_layout_space_begin(ctx, ZR_STATIC, width * 0.15f, 1);
               zr_layout_space_push(ctx, zr_rect(0, 0, width * 0.15f, width * 0.15f));
               zr_layout_space_end(ctx);
               zr_group_end(ctx);
            }
            zr_layout_row_push(ctx, 0.85f);

            if (zr_group_begin(ctx, &sub, "", ZR_WINDOW_NO_SCROLLBAR|ZR_WINDOW_BORDER|ZR_WINDOW_NO_SCROLLBAR))
            {

               zr_layout_row_dynamic(ctx, 14, 15);
               zr_image(ctx, zr->icons.page_on);
               zr_image(ctx, zr->icons.page_on);
               zr_image(ctx, zr->icons.page_off);
               zr_image(ctx, zr->icons.page_off);
               zr_image(ctx, zr->icons.page_off);

               zr_layout_row_dynamic(ctx, 40, 1);
               zr_button_text_image(ctx, zr->icons.folder, settings->libretro, ZR_TEXT_CENTERED, ZR_BUTTON_DEFAULT);
               zr_button_text_image(ctx, zr->icons.folder, settings->libretro, ZR_TEXT_CENTERED, ZR_BUTTON_DEFAULT);
               zr_button_text_image(ctx, zr->icons.folder, settings->libretro, ZR_TEXT_CENTERED, ZR_BUTTON_DEFAULT);
               zr_button_text_image(ctx, zr->icons.folder, settings->libretro, ZR_TEXT_CENTERED, ZR_BUTTON_DEFAULT);
               zr_button_text_image(ctx, zr->icons.folder, settings->libretro, ZR_TEXT_CENTERED, ZR_BUTTON_DEFAULT);
               zr_button_text_image(ctx, zr->icons.folder, settings->libretro, ZR_TEXT_CENTERED, ZR_BUTTON_DEFAULT);
               zr_button_text_image(ctx, zr->icons.folder, settings->libretro, ZR_TEXT_CENTERED, ZR_BUTTON_DEFAULT);
               zr_button_text_image(ctx, zr->icons.folder, settings->libretro, ZR_TEXT_CENTERED, ZR_BUTTON_DEFAULT);
               zr_group_end(ctx);
            }
            zr_layout_row_end(ctx);
            zr_layout_row_dynamic(ctx, 30, 4);
            zr_label(ctx,"", ZR_TEXT_RIGHT);
            zr_label(ctx,"", ZR_TEXT_RIGHT);
            if (zr_button_text(ctx, "Previous", ZR_BUTTON_DEFAULT))
               panel--;
            if (zr_button_text(ctx, "Next", ZR_BUTTON_DEFAULT))
               panel++;
            break;
         default:
            zr_layout_row_begin(ctx, ZR_DYNAMIC, height * 0.80f, 2);
            zr_layout_row_push(ctx, 0.15f);
            if (zr_group_begin(ctx, &sub, "", ZR_WINDOW_NO_SCROLLBAR|ZR_WINDOW_BORDER))
            {
               zr_layout_space_begin(ctx, ZR_STATIC, width * 0.15f, 1);
               zr_layout_space_push(ctx, zr_rect(0, 0, width * 0.15f, width * 0.15f));
               zr_layout_space_end(ctx);
               zr_group_end(ctx);
            }
            zr_layout_row_push(ctx, 0.85f);

            if (zr_group_begin(ctx, &sub, "", ZR_WINDOW_NO_SCROLLBAR|ZR_WINDOW_BORDER|ZR_WINDOW_NO_SCROLLBAR))
            {
               zr_layout_row_dynamic(ctx, 14, 15);
               zr_image(ctx, zr->icons.page_on);
               zr_image(ctx, zr->icons.page_on);
               zr_image(ctx, zr->icons.page_on);
               zr_image(ctx, zr->icons.page_off);
               zr_image(ctx, zr->icons.page_off);

               zr_layout_row_dynamic(ctx, 40, 1);
               zr_group_end(ctx);
            }
            zr_layout_row_end(ctx);
            zr_layout_row_dynamic(ctx, 30, 4);
            zr_label(ctx,"", ZR_TEXT_RIGHT);
            zr_label(ctx,"", ZR_TEXT_RIGHT);
            if (zr_button_text(ctx, "Previous", ZR_BUTTON_DEFAULT))
               panel--;
            if (zr_button_text(ctx, "Next", ZR_BUTTON_DEFAULT))
               panel++;
            break;
      }

      zr_reset_colors(ctx);
   }
   /* save position and size to restore after context reset */
   zrmenu_set_state(zr, id, zr_window_get_position(ctx), zr_window_get_size(ctx));
   zr_end(ctx);
}
