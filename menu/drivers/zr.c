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

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include <retro_assert.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <formats/image.h>
#include <gfx/math/matrix_4x4.h>
#include <string/stdstring.h>
#include <string/string_list.h>
#include "../../gfx/video_context_driver.h"
#include "../../gfx/video_shader_driver.h"

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_navigation.h"
#include "../menu_hash.h"
#include "../menu_display.h"

#include "../../core_info.h"
#include "../../configuration.h"
#include "../../frontend/frontend_driver.h"
#include "../../system.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

#include "playlist.h"

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../../gfx/common/gl_common.h"
#endif

#include "../../deps/zahnrad/zahnrad.h"

#define MAX_VERTEX_MEMORY     (512 * 1024)
#define MAX_ELEMENT_MEMORY    (128 * 1024)

#define ZR_SYSTEM_TAB_END     ZR_SYSTEM_TAB_SETTINGS

enum
{
   ZR_TEXTURE_POINTER = 0,
   ZR_TEXTURE_BACK,
   ZR_TEXTURE_SWITCH_ON,
   ZR_TEXTURE_SWITCH_OFF,
   ZR_TEXTURE_TAB_MAIN_ACTIVE,
   ZR_TEXTURE_TAB_PLAYLISTS_ACTIVE,
   ZR_TEXTURE_TAB_SETTINGS_ACTIVE,
   ZR_TEXTURE_TAB_MAIN_PASSIVE,
   ZR_TEXTURE_TAB_PLAYLISTS_PASSIVE,
   ZR_TEXTURE_TAB_SETTINGS_PASSIVE,
   ZR_TEXTURE_LAST
};

enum
{
   ZR_SYSTEM_TAB_MAIN = 0,
   ZR_SYSTEM_TAB_PLAYLISTS,
   ZR_SYSTEM_TAB_SETTINGS
};

enum zr_theme
{
   THEME_DARK = 0,
   THEME_LIGHT
};

struct zrmenu
{
   void *memory;
   struct zr_context ctx;
   enum zr_theme theme;
   struct zr_memory_status status;
};

struct zr_device
{
   struct zr_buffer cmds;
   struct zr_draw_null_texture null;
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLuint vbo, vao, ebo;

   GLuint prog;
   GLuint vert_shdr;
   GLuint frag_shdr;

   GLint attrib_pos;
   GLint attrib_uv;
   GLint attrib_col;

   GLint uniform_proj;
   GLuint font_tex;
#endif
};

static struct zr_device device;
static struct zr_font font;
static char zr_font_path[PATH_MAX_LENGTH];

static struct zr_user_font usrfnt;
static struct zr_allocator zr_alloc;
static struct zrmenu gui;

static bool wnd_test              = false;
static bool wnd_control           = false;
static bool wnd_shader_parameters = false;

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

static void zrmenu_set_style(struct zr_context *ctx, enum zr_theme theme)
{
   unsigned i;

   for (i = 0; i < ZR_ROUNDING_MAX; ++i)
      (&gui.ctx)->style.rounding[i] = 0;

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

static void zrmenu_wnd_shader_parameters(struct zr_context *ctx,
      int width, int height, struct zrmenu *gui)
{
   unsigned i;
   video_shader_ctx_t shader_info;
   struct zr_panel layout;
   settings_t *settings = config_get_ptr();

   if (zr_begin(ctx, &layout, "Shader Parameters", zr_rect(240, 10, 300, 400),
         ZR_WINDOW_CLOSABLE|ZR_WINDOW_MINIMIZABLE|ZR_WINDOW_MOVABLE|
         ZR_WINDOW_SCALABLE|ZR_WINDOW_BORDER))
   {
      struct zr_panel combo;
      static const char *themes[] = {"Dark", "Light"};
      enum   zr_theme old         = gui->theme;

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
   zr_end(ctx);
}

bool zrmenu_wnd_control(struct zr_context *ctx,
      int width, int height, struct zrmenu *gui)
{
   static int wnd_x = 900;
   static int wnd_y = 60;
   struct zr_panel layout;

   bool ret = (zr_begin(ctx, &layout, "Control", zr_rect(wnd_x, wnd_y, 350, 520),
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
         zr_labelf(ctx, ZR_TEXT_LEFT, "%lu", gui->status.size);
         zr_label(ctx,"Used:", ZR_TEXT_LEFT);
         zr_labelf(ctx, ZR_TEXT_LEFT, "%lu", gui->status.allocated);
         zr_label(ctx,"Required:", ZR_TEXT_LEFT);
         zr_labelf(ctx, ZR_TEXT_LEFT, "%lu", gui->status.needed);
         zr_label(ctx,"Calls:", ZR_TEXT_LEFT);
         zr_labelf(ctx, ZR_TEXT_LEFT, "%lu", gui->status.calls);
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
      if (zr_layout_push(ctx, ZR_LAYOUT_TAB, "Color", ZR_MINIMIZED))
      {
         struct zr_panel tab, combo;
         enum zr_theme old           = gui->theme;
         static const char *themes[] = {"Dark", "Light"};

         zr_layout_row_dynamic(ctx,  25, 2);
         zr_label(ctx, "THEME:", ZR_TEXT_LEFT);
         if (zr_combo_begin_text(ctx, &combo, themes[gui->theme], 300))
         {
            zr_layout_row_dynamic(ctx, 25, 1);
            gui->theme = zr_combo_item(ctx, themes[THEME_DARK], ZR_TEXT_CENTERED) ? THEME_DARK : gui->theme;
            gui->theme = zr_combo_item(ctx, themes[THEME_LIGHT], ZR_TEXT_CENTERED) ? THEME_LIGHT : gui->theme;
            if (old != gui->theme) zrmenu_set_style(ctx, gui->theme);
               zr_combo_end(ctx);
         }

         zr_layout_row_dynamic(ctx, 300, 1);
         if (zr_group_begin(ctx, &tab, "Colors", 0))
         {
            for (i = 0; i < ZR_COLOR_COUNT; ++i)
            {
               zr_layout_row_dynamic(ctx, 25, 2);
               zr_label(ctx, zr_get_color_name((enum zr_style_colors)i), ZR_TEXT_LEFT);
               if (zr_combo_begin_color(ctx, &combo, ctx->style.colors[i], 200))
               {
                  zr_layout_row_dynamic(ctx, 25, 1);
                  ctx->style.colors[i].r =
                     (zr_byte)zr_propertyi(ctx, "#R:", 0, ctx->style.colors[i].r, 255, 1,1);
                  ctx->style.colors[i].g =
                     (zr_byte)zr_propertyi(ctx, "#G:", 0, ctx->style.colors[i].g, 255, 1,1);
                  ctx->style.colors[i].b =
                     (zr_byte)zr_propertyi(ctx, "#B:", 0, ctx->style.colors[i].b, 255, 1,1);
                  ctx->style.colors[i].a =
                     (zr_byte)zr_propertyi(ctx, "#A:", 0, ctx->style.colors[i].a, 255, 1,1);
                  zr_combo_end(ctx);
               }
            }
            zr_group_end(ctx);
         }
         zr_layout_pop(ctx);
      }
   }
   zr_end(ctx);


   return ret;
}

static void zrmenu_wnd_test(struct zr_context *ctx, int width, int height, struct zrmenu *gui)
{
   settings_t *settings = config_get_ptr();

   struct zr_panel layout;
   if (zr_begin(ctx, &layout, "Test", zr_rect(140, 90, 500, 600),
         ZR_WINDOW_CLOSABLE|ZR_WINDOW_MINIMIZABLE|ZR_WINDOW_MOVABLE|
         ZR_WINDOW_SCALABLE|ZR_WINDOW_BORDER))
   {
      struct zr_panel combo;
      static const char *themes[] = {"Dark", "Light"};
      enum   zr_theme old         = gui->theme;

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
      zr_property_int(ctx, "Max Users:", 1, (int*)&(settings->input.max_users), MAX_USERS, 1, 1);

      if (zr_combo_begin_text(ctx, &combo, themes[gui->theme], 200))
      {
         zr_layout_row_dynamic(ctx, 25, 1);
         gui->theme = zr_combo_item(ctx, themes[THEME_DARK], ZR_TEXT_CENTERED) ? THEME_DARK : gui->theme;
         gui->theme = zr_combo_item(ctx, themes[THEME_LIGHT], ZR_TEXT_CENTERED) ? THEME_LIGHT : gui->theme;
         if (old != gui->theme) zrmenu_set_style(ctx, gui->theme);
         zr_combo_end(ctx);
      }

      zr_label(ctx, "History:", ZR_TEXT_LEFT);
      unsigned size = menu_entries_get_size();
      menu_entry_t entry;
      if (zr_combo_begin_text(ctx, &combo, "", 180))
      {

         for (int i=0; i < size; i++)
         {
            menu_entry_get(&entry, 0, i, NULL, true);
            zr_layout_row_dynamic(ctx, 25, 1);
            zr_combo_item(ctx, entry.path, ZR_TEXT_LEFT);
         }
         zr_combo_end(ctx);
      }
   }
   zr_end(ctx);
}

static void zrmenu_wnd_main(struct zr_context *ctx, int width, int height, struct zrmenu *gui)
{
   settings_t *settings = config_get_ptr();
   struct zr_panel layout;

   if (zr_begin(ctx, &layout, "Main", zr_rect(-1, -1, 120, height + 1),
         ZR_WINDOW_NO_SCROLLBAR))
   {
      /* context menu */
      struct zr_panel node, context_menu;

      if (zr_contextual_begin(ctx, &context_menu, 0, zr_vec2(100, 220), zr_window_get_bounds(ctx))) {
          zr_layout_row_dynamic(ctx, 25, 1);
          if (zr_contextual_item(ctx, "Test 1", ZR_TEXT_CENTERED))
             printf("test \n");
          if (zr_contextual_item(ctx, "Test 2",ZR_TEXT_CENTERED))
             printf("test \n");
          zr_contextual_end(ctx);
      }

      /* main menu */
      struct zr_panel menu;

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
            wnd_control = !wnd_control;
         }

         if (zr_menu_item(ctx, ZR_TEXT_LEFT, "Shader Parameters"))
         {
            zr_window_close(ctx, "Shader Parameters");
            wnd_shader_parameters = !wnd_shader_parameters;
         }

         if (zr_menu_item(ctx, ZR_TEXT_LEFT, "Test"))
         {
            zr_window_close(ctx, "Test");
            wnd_test = !wnd_test;
         }

          zr_menu_end(ctx);
      }
      zr_layout_row_push(ctx, 60);
      zr_menubar_end(ctx);

   }
   zr_end(ctx);
}

static void zrmenu_frame(struct zrmenu *gui, int width, int height)
{
   struct zr_context *ctx = &gui->ctx;

   zrmenu_wnd_main(ctx, width, height, gui);

   if (wnd_test)
      zrmenu_wnd_test(ctx, width, height, gui);
   if (wnd_control)
      zrmenu_wnd_control(ctx, width, height, gui);
   if (wnd_shader_parameters)
      zrmenu_wnd_shader_parameters(ctx, width, height, gui);

   wnd_control = !zr_window_is_closed(ctx, "Control");
   wnd_test = !zr_window_is_closed(ctx, "Test");
   wnd_shader_parameters = !zr_window_is_closed(ctx, "Shader Parameters");

   zr_buffer_info(&gui->status, &gui->ctx.memory);
}

static char* zrmenu_file_load(const char* path, size_t* siz)
{
   char *buf;
   FILE *fd = fopen(path, "rb");

   fseek(fd, 0, SEEK_END);
   *siz = (size_t)ftell(fd);
   fseek(fd, 0, SEEK_SET);
   buf = (char*)calloc(*siz, 1);
   fread(buf, *siz, 1, fd);
   fclose(fd);
   return buf;
}


static void zr_device_init(struct zr_device *dev)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLint status;
   static const GLchar *vertex_shader =
      "#version 300 es\n"
      "uniform mat4 ProjMtx;\n"
      "in vec2 Position;\n"
      "in vec2 TexCoord;\n"
      "in vec4 Color;\n"
      "out vec2 Frag_UV;\n"
      "out vec4 Frag_Color;\n"
      "void main() {\n"
      "   Frag_UV = TexCoord;\n"
      "   Frag_Color = Color;\n"
      "   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
      "}\n";
   static const GLchar *fragment_shader =
      "#version 300 es\n"
      "precision mediump float;\n"
      "uniform sampler2D Texture;\n"
      "in vec2 Frag_UV;\n"
      "in vec4 Frag_Color;\n"
      "out vec4 Out_Color;\n"
      "void main(){\n"
      "   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
      "}\n";

   dev->prog = glCreateProgram();
   dev->vert_shdr = glCreateShader(GL_VERTEX_SHADER);
   dev->frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(dev->vert_shdr, 1, &vertex_shader, 0);
   glShaderSource(dev->frag_shdr, 1, &fragment_shader, 0);
   glCompileShader(dev->vert_shdr);
   glCompileShader(dev->frag_shdr);
   glGetShaderiv(dev->vert_shdr, GL_COMPILE_STATUS, &status);
   assert(status == GL_TRUE);
   glGetShaderiv(dev->frag_shdr, GL_COMPILE_STATUS, &status);
   assert(status == GL_TRUE);
   glAttachShader(dev->prog, dev->vert_shdr);
   glAttachShader(dev->prog, dev->frag_shdr);
   glLinkProgram(dev->prog);
   glGetProgramiv(dev->prog, GL_LINK_STATUS, &status);
   assert(status == GL_TRUE);

   dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
   dev->attrib_pos   = glGetAttribLocation(dev->prog, "Position");
   dev->attrib_uv    = glGetAttribLocation(dev->prog, "TexCoord");
   dev->attrib_col   = glGetAttribLocation(dev->prog, "Color");

   {
      /* buffer setup */
      GLsizei vs = sizeof(struct zr_draw_vertex);
      size_t vp = offsetof(struct zr_draw_vertex, position);
      size_t vt = offsetof(struct zr_draw_vertex, uv);
      size_t vc = offsetof(struct zr_draw_vertex, col);

      glGenBuffers(1, &dev->vbo);
      glGenBuffers(1, &dev->ebo);
      glGenVertexArrays(1, &dev->vao);

      glBindVertexArray(dev->vao);
      glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

      glEnableVertexAttribArray((GLuint)dev->attrib_pos);
      glEnableVertexAttribArray((GLuint)dev->attrib_uv);
      glEnableVertexAttribArray((GLuint)dev->attrib_col);

      glVertexAttribPointer((GLuint)dev->attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void*)vp);
      glVertexAttribPointer((GLuint)dev->attrib_uv, 2, GL_FLOAT, GL_FALSE, vs, (void*)vt);
      glVertexAttribPointer((GLuint)dev->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void*)vc);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   glBindVertexArray(0);
#endif
}

static struct zr_user_font font_bake_and_upload(
      struct zr_device *dev,
      struct zr_font *font,
      const char *path,
      unsigned int font_height,
      const zr_rune *range)
{
   int glyph_count;
   int img_width, img_height;
   struct zr_font_glyph *glyphes;
   struct zr_baked_font baked_font;
   struct zr_user_font user_font;
   struct zr_recti custom;

   memset(&baked_font, 0, sizeof(baked_font));
   memset(&user_font, 0, sizeof(user_font));
   memset(&custom, 0, sizeof(custom));

   {
      struct texture_image ti;
      /* bake and upload font texture */
      struct zr_font_config config;
      void *img, *tmp;
      size_t ttf_size;
      size_t tmp_size, img_size;
      const char *custom_data = "....";
      char *ttf_blob = zrmenu_file_load(path, &ttf_size);
       /* setup font configuration */
      memset(&config, 0, sizeof(config));

      config.ttf_blob     = ttf_blob;
      config.ttf_size     = ttf_size;
      config.font         = &baked_font;
      config.coord_type   = ZR_COORD_UV;
      config.range        = range;
      config.pixel_snap   = zr_false;
      config.size         = (float)font_height;
      config.spacing      = zr_vec2(0,0);
      config.oversample_h = 1;
      config.oversample_v = 1;

      /* query needed amount of memory for the font baking process */
      zr_font_bake_memory(&tmp_size, &glyph_count, &config, 1);
      glyphes = (struct zr_font_glyph*)
         calloc(sizeof(struct zr_font_glyph), (size_t)glyph_count);
      tmp = calloc(1, tmp_size);

      /* pack all glyphes and return needed image width, height and memory size*/
      custom.w = 2; custom.h = 2;
      zr_font_bake_pack(&img_size,
            &img_width,&img_height,&custom,tmp,tmp_size,&config, 1);

      /* bake all glyphes and custom white pixel into image */
      img = calloc(1, img_size);
      zr_font_bake(img, img_width,
            img_height, tmp, tmp_size, glyphes, glyph_count, &config, 1);
      zr_font_bake_custom_data(img,
            img_width, img_height, custom, custom_data, 2, 2, '.', 'X');

      {
         /* convert alpha8 image into rgba8 image */
         void *img_rgba = calloc(4, (size_t)(img_height * img_width));
         zr_font_bake_convert(img_rgba, img_width, img_height, img);
         free(img);
         img = img_rgba;
      }

      /* upload baked font image */
      ti.pixels = (uint32_t*)img;
      ti.width  = (GLsizei)img_width;
      ti.height = (GLsizei)img_height;

      video_driver_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_NEAREST, (uintptr_t*)&dev->font_tex);

      free(ttf_blob);
      free(tmp);
      free(img);
   }

   /* default white pixel in a texture which is needed to draw primitives */
   dev->null.texture.id = (int)dev->font_tex;
   dev->null.uv = zr_vec2((custom.x + 0.5f)/(float)img_width,
      (custom.y + 0.5f)/(float)img_height);

   /* setup font with glyphes. IMPORTANT: the font only references the glyphes
      this was done to have the possibility to have multible fonts with one
      total glyph array. Not quite sure if it is a good thing since the
      glyphes have to be freed as well. */
   zr_font_init(font,
         (float)font_height, '?', glyphes,
         &baked_font, dev->null.texture);
   user_font = zr_font_ref(font);
   return user_font;
}

static void zr_device_shutdown(struct zr_device *dev)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   glDetachShader(dev->prog, dev->vert_shdr);
   glDetachShader(dev->prog, dev->frag_shdr);
   glDeleteShader(dev->vert_shdr);
   glDeleteShader(dev->frag_shdr);
   glDeleteProgram(dev->prog);
   glDeleteTextures(1, &dev->font_tex);
   glDeleteBuffers(1, &dev->vbo);
   glDeleteBuffers(1, &dev->ebo);
#endif
}

static void zr_device_draw(struct zr_device *dev,
      struct zr_context *ctx, int width, int height,
      enum zr_anti_aliasing AA)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   GLint last_prog, last_tex;
   GLint last_ebo, last_vbo, last_vao;
   GLfloat ortho[4][4] = {
      {2.0f, 0.0f, 0.0f, 0.0f},
      {0.0f,-2.0f, 0.0f, 0.0f},
      {0.0f, 0.0f,-1.0f, 0.0f},
      {-1.0f,1.0f, 0.0f, 1.0f},
   };
   ortho[0][0] /= (GLfloat)width;
   ortho[1][1] /= (GLfloat)height;

   /* save previous opengl state */
   glGetIntegerv(GL_CURRENT_PROGRAM, &last_prog);
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
   glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_vao);
   glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_ebo);
   glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vbo);

   /* setup global state */
   glEnable(GL_BLEND);
   glBlendEquation(GL_FUNC_ADD);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glActiveTexture(GL_TEXTURE0);

   /* setup program */
   glUseProgram(dev->prog);
   glUniformMatrix4fv(dev->uniform_proj, 1, GL_FALSE, &ortho[0][0]);

   {
      /* convert from command queue into draw list and draw to screen */
      const struct zr_draw_command *cmd;
      void *vertices, *elements;
      const zr_draw_index *offset = NULL;

      /* allocate vertex and element buffer */
      glBindVertexArray(dev->vao);
      glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

      glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_MEMORY, NULL, GL_STREAM_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_ELEMENT_MEMORY, NULL, GL_STREAM_DRAW);

      /* load draw vertices & elements directly into vertex + element buffer */
      vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
      elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
      {
         struct zr_buffer vbuf, ebuf;

         /* fill converting configuration */
         struct zr_convert_config config;
         memset(&config, 0, sizeof(config));
         config.global_alpha = 1.0f;
         config.shape_AA = AA;
         config.line_AA = AA;
         config.circle_segment_count = 22;
         config.line_thickness = 1.0f;
         config.null = dev->null;

         /* setup buffers to load vertices and elements */
         zr_buffer_init_fixed(&vbuf, vertices, MAX_VERTEX_MEMORY);
         zr_buffer_init_fixed(&ebuf, elements, MAX_ELEMENT_MEMORY);
         zr_convert(ctx, &dev->cmds, &vbuf, &ebuf, &config);
      }
      glUnmapBuffer(GL_ARRAY_BUFFER);
      glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

      /* iterate over and execute each draw command */
      zr_draw_foreach(cmd, ctx, &dev->cmds)
      {
         if (!cmd->elem_count)
            continue;
         glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
         glScissor((GLint)cmd->clip_rect.x,
            height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h),
            (GLint)cmd->clip_rect.w, (GLint)cmd->clip_rect.h);
         glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count,
               GL_UNSIGNED_SHORT, offset);
         offset += cmd->elem_count;
       }
       zr_clear(ctx);
   }

   /* restore old state */
   glUseProgram((GLuint)last_prog);
   glBindTexture(GL_TEXTURE_2D, (GLuint)last_tex);
   glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_vbo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_ebo);
   glBindVertexArray((GLuint)last_vao);
   glDisable(GL_BLEND);
#endif
}

static void* zrmenu_mem_alloc(zr_handle unused, size_t size)
{
   (void)unused;
   return calloc(1, size);
}

static void zrmenu_mem_free(zr_handle unused, void *ptr)
{
   (void)unused;
   free(ptr);
}

static void zrmenu_input_mouse_movement(struct zr_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   zr_input_motion(ctx, mouse_x, mouse_y);
}

static void zrmenu_input_mouse_button(struct zr_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   zr_input_button(ctx, ZR_BUTTON_LEFT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON));
   zr_input_button(ctx, ZR_BUTTON_RIGHT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON));
}

static void zrmenu_input_keyboard(struct zr_context *ctx)
{
   /* placeholder, it just presses 1 on right click
      needs to be hooked up correctly
   */
   if(menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON))
      zr_input_char(ctx, '1');
}

static void zrmenu_init(int width, int height)
{
   settings_t *settings = config_get_ptr();

   fill_pathname_join(zr_font_path, settings->assets_directory,
         "zahnrad", sizeof(zr_font_path));
   fill_pathname_join(zr_font_path, zr_font_path,
         "DroidSans.ttf", sizeof(zr_font_path));

   zr_alloc.userdata.ptr = NULL;
   zr_alloc.alloc = zrmenu_mem_alloc;
   zr_alloc.free = zrmenu_mem_free;
   zr_buffer_init(&device.cmds, &zr_alloc, 1024);
   usrfnt = font_bake_and_upload(&device, &font, zr_font_path, 16,
      zr_font_default_glyph_ranges());
   zr_init(&gui.ctx, &zr_alloc, &usrfnt);
   zr_device_init(&device);
   zrmenu_set_style(&gui.ctx, THEME_DARK);
}

static void zrmenu_deinit()
{
   free(font.glyphs);
   zr_free(&gui.ctx);
   zr_buffer_free(&device.cmds);
   zr_device_shutdown(&device);
}

/* normal glui code starts here */

struct wimp_texture_item
{
   uintptr_t id;
};

typedef struct wimp_handle
{
   char box_message[PATH_MAX_LENGTH];

   struct
   {
      struct
      {
         float alpha;
      } arrow;

      struct wimp_texture_item bg;
      struct wimp_texture_item list[ZR_TEXTURE_LAST];
   } textures;

   gfx_font_raster_block_t list_block;
   float scroll_y;
} wimp_handle_t;

static void wimp_context_reset_textures(wimp_handle_t *wimp,
      const char *iconpath)
{
   unsigned i;

   for (i = 0; i < ZR_TEXTURE_LAST; i++)
   {
      struct texture_image ti     = {0};
      char path[PATH_MAX_LENGTH]  = {0};

      switch(i)
      {
         case ZR_TEXTURE_POINTER:
            fill_pathname_join(path, iconpath,
                  "pointer.png", sizeof(path));
            break;
      }

      if (string_is_empty(path) || !path_file_exists(path))
         continue;

      video_texture_image_load(&ti, path);
      video_driver_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_LINEAR, &wimp->textures.list[i].id);

      video_texture_image_free(&ti);
   }
}

static void wimp_get_message(void *data, const char *message)
{
   wimp_handle_t *wimp   = (wimp_handle_t*)data;

   if (!wimp || !message || !*message)
      return;

   strlcpy(wimp->box_message, message, sizeof(wimp->box_message));
}

static void zrmenu_draw_cursor(wimp_handle_t *wimp,
      float *color,
      float x, float y, unsigned width, unsigned height)
{
   menu_display_ctx_draw_t draw;
   struct gfx_coords coords;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_BEGIN, NULL);

   draw.x           = x - 32;
   draw.y           = (int)height - y - 32;
   draw.width       = 64;
   draw.height      = 64;
   draw.coords      = &coords;
   draw.matrix_data = NULL;
   draw.texture     = wimp->textures.list[ZR_TEXTURE_POINTER].id;
   draw.prim_type   = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

   menu_display_ctl(MENU_DISPLAY_CTL_DRAW, &draw);

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_END, NULL);
}

static void wimp_frame(void *data)
{
   float white_bg[16]=  {
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
   };

   unsigned width, height, ticker_limit, i;
   wimp_handle_t *wimp               = (wimp_handle_t*)data;
   settings_t *settings            = config_get_ptr();

   if (!wimp)
      return;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   zr_input_begin(&gui.ctx);
   zrmenu_input_mouse_movement(&gui.ctx);
   zrmenu_input_mouse_button(&gui.ctx);
   zrmenu_input_keyboard(&gui.ctx);

   zr_input_end(&gui.ctx);
   zrmenu_frame(&gui, width, height);
   zr_device_draw(&device, &gui.ctx, width, height, ZR_ANTI_ALIASING_ON);

   if (settings->menu.mouse.enable && (settings->video.fullscreen
            || !video_driver_ctl(RARCH_DISPLAY_CTL_HAS_WINDOWED, NULL)))
   {
      int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
      int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      zrmenu_draw_cursor(wimp, &white_bg[0], mouse_x, mouse_y, width, height);
   }

   menu_display_ctl(MENU_DISPLAY_CTL_RESTORE_CLEAR_COLOR, NULL);
   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}

static void wimp_layout(wimp_handle_t *wimp)
{
   void *fb_buf;
   float scale_factor;
   int new_font_size;
   unsigned width, height, new_header_height;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_GET_DPI, &scale_factor);


   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEADER_HEIGHT,
         &new_header_height);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_FONT_SIZE,
         &new_font_size);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_BUF, &fb_buf);

   if (fb_buf) /* calculate a more realistic ticker_limit */
   {
      unsigned m_width =
         font_driver_get_message_width(fb_buf, "a", 1, 1);


   }
}

static void *wimp_init(void **userdata)
{
   settings_t *settings = config_get_ptr();
   wimp_handle_t   *wimp = NULL;
   menu_handle_t *menu = (menu_handle_t*)
      calloc(1, sizeof(*menu));
   unsigned width, height = 0;

   video_driver_get_size(&width, &height);

   if (!menu)
      goto error;

   if (!menu_display_ctl(MENU_DISPLAY_CTL_INIT_FIRST_DRIVER, NULL))
      goto error;

   wimp = (wimp_handle_t*)calloc(1, sizeof(wimp_handle_t));

   if (!wimp)
      goto error;

   *userdata = wimp;

   zrmenu_init(width, height);

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void wimp_free(void *data)
{
   wimp_handle_t *wimp   = (wimp_handle_t*)data;

   if (!wimp)
      return;

   zrmenu_deinit();

   gfx_coord_array_free(&wimp->list_block.carr);

   font_driver_bind_block(NULL, NULL);
}

static void wimp_context_bg_destroy(wimp_handle_t *wimp)
{
   if (!wimp)
      return;

}

static void zrmenu_context_destroy(void *data)
{
   unsigned i;
   wimp_handle_t *wimp   = (wimp_handle_t*)data;

   if (!wimp)
      return;

   for (i = 0; i < ZR_TEXTURE_LAST; i++)
      video_driver_texture_unload((uintptr_t*)&wimp->textures.list[i].id);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_MAIN_DEINIT, NULL);

   wimp_context_bg_destroy(wimp);
}

static void zrmenu_context_reset(void *data)
{
   char iconpath[PATH_MAX_LENGTH] = {0};
   wimp_handle_t *wimp              = (wimp_handle_t*)data;
   settings_t *settings           = config_get_ptr();
   unsigned width, height = 0;

   video_driver_get_size(&width, &height);

   if (!wimp || !settings)
      return;

   fill_pathname_join(iconpath, settings->assets_directory,
         "zahnrad", sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   wimp_layout(wimp);
   zrmenu_init(width, height);
   wimp_context_bg_destroy(wimp);
   wimp_context_reset_textures(wimp, iconpath);

   rarch_task_push_image_load(settings->menu.wallpaper, "cb_menu_wallpaper",
         menu_display_handle_wallpaper_upload, NULL);
}

static int wimp_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   switch (type)
   {
      case 0:
      default:
         break;
   }

   return -1;
}

static bool zrmenu_init_list(void *data)
{
   menu_displaylist_info_t info = {0};
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   strlcpy(info.label,
         menu_hash_to_str(MENU_VALUE_HISTORY_TAB), sizeof(info.label));

   menu_entries_push(menu_stack,
         info.path, info.label, info.type, info.flags, 0);

   event_cmd_ctl(EVENT_CMD_HISTORY_INIT, NULL);

   info.list  = selection_buf;

   if (menu_displaylist_ctl(DISPLAYLIST_HISTORY, &info))
   {
      info.need_push = true;
      return menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info);
   }

   return false;
}

menu_ctx_driver_t menu_ctx_zr = {
   NULL,
   wimp_get_message,
   generic_menu_iterate,
   NULL,
   wimp_frame,
   wimp_init,
   wimp_free,
   zrmenu_context_reset,
   zrmenu_context_destroy,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   zrmenu_init_list,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "zahnrad",
   wimp_environ,
   NULL,
};
