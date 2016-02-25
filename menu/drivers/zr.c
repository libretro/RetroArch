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

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../../gfx/common/gl_common.h"
#endif

#include "../../deps/zahnrad/zahnrad.h"

#define MAX_VERTEX_MEMORY     512 * 1024
#define MAX_ELEMENT_MEMORY    128 * 1024

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)              ((a) < (b) ? (b) : (a))
#endif

#ifndef CLAMP
#define CLAMP(i,v,x)          (MAX(MIN(v,x), i))
#endif

#define LEN(a)                (sizeof(a)/sizeof(a)[0])
#define UNUSED(a)             ((void)(a))

#define MAX_BUFFER            64
#define MAX_MEMORY            (32 * 1024)
#define MAX_COMMAND_MEMORY    (16 * 1024)
#define WINDOW_WIDTH          1200
#define WINDOW_HEIGHT         800

enum theme
{
   THEME_BLACK = 0,
   THEME_WHITE,
   THEME_RED,
   THEME_BLUE,
   THEME_DARK
};

struct wimp 
{
   void *memory;
   struct zr_context ctx;
   enum theme theme;
   struct zr_memory_status status;
};

struct device
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

   GLint uniform_tex;
   GLint uniform_proj;
   GLuint font_tex;
#endif
};

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

static int z =0 ;

bool zr_checkbox_bool(struct zr_context* cx, const char* text, bool *active)
{
   int x = *active;
   bool ret = zr_checkbox(cx, text, &x);
   *active = x;
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

/* zahnrad code */

static void set_style(struct zr_context *ctx, enum theme theme)
{
   switch (theme)
   {
      case THEME_WHITE:
         ctx->style.colors[ZR_COLOR_TEXT] = zr_rgba(70, 70, 70, 255);
         ctx->style.colors[ZR_COLOR_TEXT_HOVERING] = zr_rgba(10, 10, 10, 255);
         ctx->style.colors[ZR_COLOR_TEXT_ACTIVE] = zr_rgba(20, 20, 20, 255);
         ctx->style.colors[ZR_COLOR_WINDOW] = zr_rgba(175, 175, 175, 255);
         ctx->style.colors[ZR_COLOR_HEADER] = zr_rgba(175, 175, 175, 255);
         ctx->style.colors[ZR_COLOR_BORDER] = zr_rgba(0, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_BUTTON] = zr_rgba(185, 185, 185, 255);
         ctx->style.colors[ZR_COLOR_BUTTON_HOVER] = zr_rgba(170, 170, 170, 255);
         ctx->style.colors[ZR_COLOR_BUTTON_ACTIVE] = zr_rgba(160, 160, 160, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE] = zr_rgba(150, 150, 150, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE_HOVER] = zr_rgba(120, 120, 120, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE_CURSOR] = zr_rgba(175, 175, 175, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE] = zr_rgba(190, 190, 190, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE_HOVER] = zr_rgba(150, 150, 150, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE_TEXT] = zr_rgba(70, 70, 70, 255);
         ctx->style.colors[ZR_COLOR_SLIDER] = zr_rgba(190, 190, 190, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR] = zr_rgba(80, 80, 80, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR_HOVER] = zr_rgba(70, 70, 70, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR_ACTIVE] = zr_rgba(60, 60, 60, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS] = zr_rgba(190, 190, 190, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR] = zr_rgba(80, 80, 80, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR_HOVER] = zr_rgba(70, 70, 70, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR_ACTIVE] = zr_rgba(60, 60, 60, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY] = zr_rgba(175, 175, 175, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY_HOVER] = zr_rgba(160, 160, 160, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY_ACTIVE] = zr_rgba(165, 165, 165, 255);
         ctx->style.colors[ZR_COLOR_INPUT] = zr_rgba(150, 150, 150, 255);
         ctx->style.colors[ZR_COLOR_INPUT_CURSOR] = zr_rgba(0, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_INPUT_TEXT] = zr_rgba(0, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_COMBO] = zr_rgba(175, 175, 175, 255);
         ctx->style.colors[ZR_COLOR_HISTO] = zr_rgba(160, 160, 160, 255);
         ctx->style.colors[ZR_COLOR_HISTO_BARS] = zr_rgba(45, 45, 45, 255);
         ctx->style.colors[ZR_COLOR_HISTO_HIGHLIGHT] = zr_rgba( 255, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_PLOT] = zr_rgba(160, 160, 160, 255);
         ctx->style.colors[ZR_COLOR_PLOT_LINES] = zr_rgba(45, 45, 45, 255);
         ctx->style.colors[ZR_COLOR_PLOT_HIGHLIGHT] = zr_rgba(255, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR] = zr_rgba(180, 180, 180, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR] = zr_rgba(140, 140, 140, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR_HOVER] = zr_rgba(150, 150, 150, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR_ACTIVE] = zr_rgba(160, 160, 160, 255);
         ctx->style.colors[ZR_COLOR_TABLE_LINES] = zr_rgba(100, 100, 100, 255);
         ctx->style.colors[ZR_COLOR_TAB_HEADER] = zr_rgba(180, 180, 180, 255);
         ctx->style.colors[ZR_COLOR_SCALER] = zr_rgba(100, 100, 100, 255);
         break;
      case THEME_RED:
         ctx->style.rounding[ZR_ROUNDING_SCROLLBAR] = 0;
         ctx->style.properties[ZR_PROPERTY_SCROLLBAR_SIZE] = zr_vec2(10,10);
         ctx->style.colors[ZR_COLOR_TEXT] = zr_rgba(190, 190, 190, 255);
         ctx->style.colors[ZR_COLOR_TEXT_HOVERING] = zr_rgba(195, 195, 195, 255);
         ctx->style.colors[ZR_COLOR_TEXT_ACTIVE] = zr_rgba(200, 200, 200, 255);
         ctx->style.colors[ZR_COLOR_WINDOW] = zr_rgba(30, 33, 40, 215);
         ctx->style.colors[ZR_COLOR_HEADER] = zr_rgba(181, 45, 69, 220);
         ctx->style.colors[ZR_COLOR_BORDER] = zr_rgba(51, 55, 67, 255);
         ctx->style.colors[ZR_COLOR_BUTTON] = zr_rgba(181, 45, 69, 255);
         ctx->style.colors[ZR_COLOR_BUTTON_HOVER] = zr_rgba(190, 50, 70, 255);
         ctx->style.colors[ZR_COLOR_BUTTON_ACTIVE] = zr_rgba(195, 55, 75, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE] = zr_rgba(51, 55, 67, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE_HOVER] = zr_rgba(45, 60, 60, 255);
         ctx->style.colors[ZR_COLOR_TOGGLE_CURSOR] = zr_rgba(181, 45, 69, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE] = zr_rgba(181, 45, 69, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE_HOVER] = zr_rgba(181, 45, 69, 255);
         ctx->style.colors[ZR_COLOR_SELECTABLE_TEXT] = zr_rgba(190, 190, 190, 255);
         ctx->style.colors[ZR_COLOR_SLIDER] = zr_rgba(51, 55, 67, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR] = zr_rgba(181, 45, 69, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR_HOVER] = zr_rgba(186, 50, 74, 255);
         ctx->style.colors[ZR_COLOR_SLIDER_CURSOR_ACTIVE] = zr_rgba(191, 55, 79, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS] = zr_rgba(51, 55, 67, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR] = zr_rgba(181, 45, 69, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR_HOVER] = zr_rgba(186, 50, 74, 255);
         ctx->style.colors[ZR_COLOR_PROGRESS_CURSOR_ACTIVE] = zr_rgba(191, 55, 79, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY] = zr_rgba(51, 55, 67, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY_HOVER] = zr_rgba(55, 60, 72, 255);
         ctx->style.colors[ZR_COLOR_PROPERTY_ACTIVE] = zr_rgba(60, 65, 77, 255);
         ctx->style.colors[ZR_COLOR_INPUT] = zr_rgba(51, 55, 67, 225);
         ctx->style.colors[ZR_COLOR_INPUT_CURSOR] = zr_rgba(190, 190, 190, 255);
         ctx->style.colors[ZR_COLOR_INPUT_TEXT] = zr_rgba(190, 190, 190, 255);
         ctx->style.colors[ZR_COLOR_COMBO] = zr_rgba(51, 55, 67, 255);
         ctx->style.colors[ZR_COLOR_HISTO] = zr_rgba(51, 55, 67, 255);
         ctx->style.colors[ZR_COLOR_HISTO_BARS] = zr_rgba(170, 40, 60, 255);
         ctx->style.colors[ZR_COLOR_HISTO_HIGHLIGHT] = zr_rgba( 255, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_PLOT] = zr_rgba(51, 55, 67, 255);
         ctx->style.colors[ZR_COLOR_PLOT_LINES] = zr_rgba(170, 40, 60, 255);
         ctx->style.colors[ZR_COLOR_PLOT_HIGHLIGHT] = zr_rgba(255, 0, 0, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR] = zr_rgba(30, 33, 40, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR] = zr_rgba(64, 84, 95, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR_HOVER] = zr_rgba(70, 90, 100, 255);
         ctx->style.colors[ZR_COLOR_SCROLLBAR_CURSOR_ACTIVE] = zr_rgba(75, 95, 105, 255);
         ctx->style.colors[ZR_COLOR_TABLE_LINES] = zr_rgba(100, 100, 100, 255);
         ctx->style.colors[ZR_COLOR_TAB_HEADER] = zr_rgba(181, 45, 69, 220);
         ctx->style.colors[ZR_COLOR_SCALER] = zr_rgba(100, 100, 100, 255);
         break;
      case THEME_BLUE:
         ctx->style.rounding[ZR_ROUNDING_SCROLLBAR] = 0;
         ctx->style.properties[ZR_PROPERTY_SCROLLBAR_SIZE] = zr_vec2(10,10);
         ctx->style.colors[ZR_COLOR_TEXT] = zr_rgba(20, 20, 20, 255);
         ctx->style.colors[ZR_COLOR_TEXT_HOVERING] = zr_rgba(195, 195, 195, 255);
         ctx->style.colors[ZR_COLOR_TEXT_ACTIVE] = zr_rgba(200, 200, 200, 255);
         ctx->style.colors[ZR_COLOR_WINDOW] = zr_rgba(202, 212, 214, 215);
         ctx->style.colors[ZR_COLOR_HEADER] = zr_rgba(137, 182, 224, 220);
         ctx->style.colors[ZR_COLOR_BORDER] = zr_rgba(140, 159, 173, 255);
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
         ctx->style.colors[ZR_COLOR_HEADER] = zr_rgba(46, 46, 46, 220);
         ctx->style.colors[ZR_COLOR_BORDER] = zr_rgba(46, 46, 46, 255);
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


static int wimp_control(struct zr_context *ctx,
      int width, int height, struct wimp *gui)
{
   int i;
   struct zr_panel layout;
   if (zr_begin(ctx, &layout, "Control", zr_rect(900, 10, 350, 520),
      ZR_WINDOW_CLOSABLE|ZR_WINDOW_MINIMIZABLE|ZR_WINDOW_MOVABLE|
      ZR_WINDOW_SCALABLE|ZR_WINDOW_BORDER))
   {
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
         enum theme old = gui->theme;
         static const char *themes[] = {"Black", "White", "Red", "Blue", "Dark", "Grey"};

         zr_layout_row_dynamic(ctx,  25, 2);
         zr_label(ctx, "THEME:", ZR_TEXT_LEFT);
         if (zr_combo_begin_text(ctx, &combo, themes[gui->theme], 300)) 
         {
            zr_layout_row_dynamic(ctx, 25, 1);
            gui->theme = zr_combo_item(ctx, themes[THEME_BLACK], ZR_TEXT_CENTERED) ? THEME_BLACK : gui->theme;
            gui->theme = zr_combo_item(ctx, themes[THEME_WHITE], ZR_TEXT_CENTERED) ? THEME_WHITE : gui->theme;
            gui->theme = zr_combo_item(ctx, themes[THEME_RED], ZR_TEXT_CENTERED) ? THEME_RED : gui->theme;
            gui->theme = zr_combo_item(ctx, themes[THEME_BLUE], ZR_TEXT_CENTERED) ? THEME_BLUE : gui->theme;
            gui->theme = zr_combo_item(ctx, themes[THEME_DARK], ZR_TEXT_CENTERED) ? THEME_DARK : gui->theme;
            if (old != gui->theme) set_style(ctx, gui->theme);
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
   return !zr_window_is_closed(ctx, "Control");
}

static void wimp_main(struct zr_context *ctx, int width, int height, struct wimp *gui)
{
   settings_t *settings = config_get_ptr();

   struct zr_panel layout;
   if (zr_begin(ctx, &layout, "Demo Window", zr_rect(10, 10, width/2, 400),
         ZR_WINDOW_CLOSABLE|ZR_WINDOW_MINIMIZABLE|ZR_WINDOW_MOVABLE|
         ZR_WINDOW_SCALABLE|ZR_WINDOW_BORDER))
   {
      zr_layout_row_dynamic(ctx, 30, 2);
      if (zr_button_text(ctx, "Quit", ZR_BUTTON_DEFAULT)) {
            /* event handling */
            printf("Pressed Event\n");
            rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
      }
      if (zr_button_text(ctx, "Quit", ZR_BUTTON_DEFAULT)) {
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

      struct zr_panel combo;
      static const char *themes[] = {"Black", "White", "Red", "Blue", "Dark", "Grey"};
      enum theme old = gui->theme;
      if (zr_combo_begin_text(ctx, &combo, themes[gui->theme], 300)) {
          zr_layout_row_dynamic(ctx, 25, 1);
          gui->theme = zr_combo_item(ctx, themes[THEME_BLACK], ZR_TEXT_CENTERED) ? THEME_BLACK : gui->theme;
          gui->theme = zr_combo_item(ctx, themes[THEME_WHITE], ZR_TEXT_CENTERED) ? THEME_WHITE : gui->theme;
          gui->theme = zr_combo_item(ctx, themes[THEME_RED], ZR_TEXT_CENTERED) ? THEME_RED : gui->theme;
          gui->theme = zr_combo_item(ctx, themes[THEME_BLUE], ZR_TEXT_CENTERED) ? THEME_BLUE : gui->theme;
          gui->theme = zr_combo_item(ctx, themes[THEME_DARK], ZR_TEXT_CENTERED) ? THEME_DARK : gui->theme;
          if (old != gui->theme) set_style(ctx, gui->theme);
          zr_combo_end(ctx);
      }

      
   }
   zr_end(ctx);
}

static int wimp_start(struct wimp *gui, int width, int height)
{
   int ret = 1;
   static int init = 0;
   struct zr_context *ctx = &gui->ctx;

   if (!init) {
     init = 1;
   }

   /* set rounding to zero on all elements */
   for (int i = 0; i < ZR_ROUNDING_MAX; ++i)
   {
      ctx->style.rounding[i] = 0;
   }
   wimp_main(ctx, width, height, gui);
   wimp_control(ctx, width, height, gui);
   zr_buffer_info(&gui->status, &gui->ctx.memory);
   return ret;
}

static struct wimp gui;

static char* file_load(const char* path, size_t* siz)
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


static void device_init(struct device *dev)
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

   dev->uniform_tex = glGetUniformLocation(dev->prog, "Texture");
   dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
   dev->attrib_pos = glGetAttribLocation(dev->prog, "Position");
   dev->attrib_uv = glGetAttribLocation(dev->prog, "TexCoord");
   dev->attrib_col = glGetAttribLocation(dev->prog, "Color");

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

static struct zr_user_font font_bake_and_upload(struct device *dev, struct zr_font *font,
    const char *path, unsigned int font_height, const zr_rune *range)
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
      /* bake and upload font texture */
      void *img, *tmp;
      size_t ttf_size;
      size_t tmp_size, img_size;
      const char *custom_data = "....";
      struct zr_font_config config;
      char *ttf_blob = file_load(path, &ttf_size);
       /* setup font configuration */
      memset(&config, 0, sizeof(config));
      config.ttf_blob = ttf_blob;
      config.ttf_size = ttf_size;
      config.font = &baked_font;
      config.coord_type = ZR_COORD_UV;
      config.range = range;
      config.pixel_snap = zr_false;
      config.size = (float)font_height;
      config.spacing = zr_vec2(0,0);
      config.oversample_h = 1;
      config.oversample_v = 1;

      /* query needed amount of memory for the font baking process */
      zr_font_bake_memory(&tmp_size, &glyph_count, &config, 1);
      glyphes = (struct zr_font_glyph*)calloc(sizeof(struct zr_font_glyph), (size_t)glyph_count);
      tmp = calloc(1, tmp_size);

      /* pack all glyphes and return needed image width, height and memory size*/
      custom.w = 2; custom.h = 2;
      zr_font_bake_pack(&img_size, &img_width,&img_height,&custom,tmp,tmp_size,&config, 1);

      /* bake all glyphes and custom white pixel into image */
      img = calloc(1, img_size);
      zr_font_bake(img, img_width, img_height, tmp, tmp_size, glyphes, glyph_count, &config, 1);
      zr_font_bake_custom_data(img, img_width, img_height, custom, custom_data, 2, 2, '.', 'X');
      {
         /* convert alpha8 image into rgba8 image */
         void *img_rgba = calloc(4, (size_t)(img_height * img_width));
         zr_font_bake_convert(img_rgba, img_width, img_height, img);
         free(img);
         img = img_rgba;
      }

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
      /* upload baked font image */
      glGenTextures(1, &dev->font_tex);
      glBindTexture(GL_TEXTURE_2D, dev->font_tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)img_width, (GLsizei)img_height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, img);
#endif

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
   zr_font_init(font, (float)font_height, '?', glyphes, &baked_font, dev->null.texture);
   user_font = zr_font_ref(font);
   return user_font;
}

static void device_shutdown(struct device *dev)
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

static void device_draw(struct device *dev,
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
   glDisable(GL_CULL_FACE);
   glDisable(GL_DEPTH_TEST);
   glEnable(GL_SCISSOR_TEST);
   glActiveTexture(GL_TEXTURE0);

   /* setup program */
   glUseProgram(dev->prog);
   glUniform1i(dev->uniform_tex, 0);
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
         glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
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
   glDisable(GL_SCISSOR_TEST);
#endif
}

static void* mem_alloc(zr_handle unused, size_t size)
{
   UNUSED(unused);
   return calloc(1, size);
}

static void mem_free(zr_handle unused, void *ptr)
{
   UNUSED(unused);
   free(ptr);
}

struct device device;
struct zr_font font;
char font_path[PATH_MAX_LENGTH];
int width = 0, height = 0;

struct zr_user_font usrfnt;
struct zr_allocator alloc;

static void wimp_input_motion(struct zr_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);
   
   zr_input_motion(ctx, mouse_x, mouse_y);
}

static void wimp_input_button(struct zr_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   zr_input_button(ctx, ZR_BUTTON_LEFT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON));
   zr_input_button(ctx, ZR_BUTTON_RIGHT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON));
}

/* zahnrad code */


#define ZR_SYSTEM_TAB_END ZR_SYSTEM_TAB_SETTINGS

struct wimp_texture_item
{
   uintptr_t id;
};

typedef struct wimp_handle
{
   unsigned tabs_height;
   unsigned line_height;
   unsigned shadow_height;
   unsigned scrollbar_width;
   unsigned icon_size;
   unsigned margin;
   unsigned glyph_width;
   char box_message[PATH_MAX_LENGTH];

   struct 
   {
      struct
      {
         float alpha;
      } arrow;

      struct wimp_texture_item bg;
      struct wimp_texture_item list[ZR_TEXTURE_LAST];
      uintptr_t white;
   } textures;

   struct
   {
      struct
      {
         unsigned idx;
         unsigned idx_old;
      } active;

      float x_pos;
      size_t selection_ptr_old;
      size_t selection_ptr;
   } categories;

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
         case ZR_TEXTURE_BACK:
            fill_pathname_join(path, iconpath,
                  "back.png", sizeof(path));
            break;
         case ZR_TEXTURE_SWITCH_ON:
            fill_pathname_join(path, iconpath,
                  "on.png", sizeof(path));
            break;
         case ZR_TEXTURE_SWITCH_OFF:
            fill_pathname_join(path, iconpath,
                  "off.png", sizeof(path));
            break;
         case ZR_TEXTURE_TAB_MAIN_ACTIVE:
            fill_pathname_join(path, iconpath,
                  "main_tab_active.png", sizeof(path));
            break;
         case ZR_TEXTURE_TAB_PLAYLISTS_ACTIVE:
            fill_pathname_join(path, iconpath,
                  "playlists_tab_active.png", sizeof(path));
            break;
         case ZR_TEXTURE_TAB_SETTINGS_ACTIVE:
            fill_pathname_join(path, iconpath,
                  "settings_tab_active.png", sizeof(path));
            break;
         case ZR_TEXTURE_TAB_MAIN_PASSIVE:
            fill_pathname_join(path, iconpath,
                  "main_tab_passive.png", sizeof(path));
            break;
         case ZR_TEXTURE_TAB_PLAYLISTS_PASSIVE:
            fill_pathname_join(path, iconpath,
                  "playlists_tab_passive.png", sizeof(path));
            break;
         case ZR_TEXTURE_TAB_SETTINGS_PASSIVE:
            fill_pathname_join(path, iconpath,
                  "settings_tab_passive.png", sizeof(path));
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

static void wimp_draw_icon(wimp_handle_t *wimp,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color)
{
   menu_display_ctx_rotate_draw_t rotate_draw;
   menu_display_ctx_draw_t draw;
   struct gfx_coords coords;
   math_matrix_4x4 mymat;

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_BEGIN, NULL);

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_ctl(MENU_DISPLAY_CTL_ROTATE_Z, &rotate_draw);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   draw.x           = x;
   draw.y           = height - y - wimp->icon_size;
   draw.width       = wimp->icon_size;
   draw.height      = wimp->icon_size;
   draw.coords      = &coords;
   draw.matrix_data = &mymat;
   draw.texture     = texture;
   draw.prim_type   = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

   menu_display_ctl(MENU_DISPLAY_CTL_DRAW, &draw);

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_END, NULL);
}


static void wimp_draw_tab(wimp_handle_t *wimp,
      unsigned i,
      unsigned width, unsigned height,
      float *pure_white)
{
   unsigned tab_icon;
   switch (i)
   {
      case ZR_SYSTEM_TAB_MAIN:
         tab_icon = (i == wimp->categories.selection_ptr)
            ? ZR_TEXTURE_TAB_MAIN_ACTIVE
            : ZR_TEXTURE_TAB_MAIN_PASSIVE;
         break;
      case ZR_SYSTEM_TAB_PLAYLISTS:
         tab_icon = (i == wimp->categories.selection_ptr)
            ? ZR_TEXTURE_TAB_PLAYLISTS_ACTIVE
            : ZR_TEXTURE_TAB_PLAYLISTS_PASSIVE;
         break;
      case ZR_SYSTEM_TAB_SETTINGS:
         tab_icon = (i == wimp->categories.selection_ptr)
            ? ZR_TEXTURE_TAB_SETTINGS_ACTIVE
            : ZR_TEXTURE_TAB_SETTINGS_PASSIVE;
         break;
   }

   wimp_draw_icon(wimp, wimp->textures.list[tab_icon].id,
         width / (ZR_SYSTEM_TAB_END+1) * (i+0.5) - wimp->icon_size/2,
         height - wimp->tabs_height,
         width, height, 0, 1, &pure_white[0]);
}

static void wimp_blit_line(float x, float y, unsigned width, unsigned height,
      const char *message, uint32_t color, enum text_alignment text_align)
{
   int font_size;
   struct font_params params;
   void *fb_buf              = NULL;

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_SIZE, &font_size);

   params.x           = x / width;
   params.y           = 1.0f - (y + font_size / 3) / height;
   params.scale       = 1.0f;
   params.drop_mod    = 0.0f;
   params.drop_x      = 0.0f;
   params.drop_y      = 0.0f;
   params.color       = color;
   params.full_screen = true;
   params.text_align  = text_align;

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_BUF, &fb_buf);

   video_driver_set_osd_msg(message, &params, fb_buf);
}

static void wimp_render_quad(wimp_handle_t *wimp,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *coord_color)
{
   menu_display_ctx_draw_t draw;
   struct gfx_coords coords;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = coord_color;

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_BEGIN, NULL);

   draw.x           = x;
   draw.y           = (int)height - y - (int)h;
   draw.width       = w;
   draw.height      = h;
   draw.coords      = &coords;
   draw.matrix_data = NULL;
   draw.texture     = wimp->textures.white;
   draw.prim_type   = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

   menu_display_ctl(MENU_DISPLAY_CTL_DRAW, &draw);

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_END, NULL);
}

static void wimp_draw_tab_begin(wimp_handle_t *wimp,
      unsigned width, unsigned height,
      float *white_bg, float *grey_bg)
{
   float scale_factor;
   menu_display_ctl(MENU_DISPLAY_CTL_GET_DPI, &scale_factor);

   wimp->tabs_height = scale_factor / 3;

   /* tabs background */
   wimp_render_quad(wimp, 0, height - wimp->tabs_height, width,
         wimp->tabs_height,
         width, height,
         white_bg);

   /* tabs separator */
   wimp_render_quad(wimp, 0, height - wimp->tabs_height, width,
         1,
         width, height,
         grey_bg);
}

static void wimp_draw_tab_end(wimp_handle_t *wimp,
      unsigned width, unsigned height,
      unsigned header_height,
      float *blue_bg)
{
   /* active tab marker */
   unsigned tab_width = width / (ZR_SYSTEM_TAB_END+1);

   wimp_render_quad(wimp, wimp->categories.selection_ptr * tab_width,
         height - (header_height/16),
         tab_width,
         header_height/16,
         width, height,
         &blue_bg[0]);
}

static void wimp_draw_scrollbar(wimp_handle_t *wimp, 
      unsigned width, unsigned height, float *coord_color)
{
   unsigned header_height;
   float content_height, total_height,
         scrollbar_height, scrollbar_margin, y;

   if (!wimp)
      return;

   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   content_height   = menu_entries_get_end() * wimp->line_height;
   total_height     = height - header_height - wimp->tabs_height;
   scrollbar_margin = wimp->scrollbar_width;
   scrollbar_height = total_height / (content_height / total_height);
   y                = total_height * wimp->scroll_y / content_height;

   /* apply a margin on the top and bottom of the scrollbar for aestetic */
   scrollbar_height -= scrollbar_margin * 2;
   y += scrollbar_margin;

   if (content_height >= total_height)
   {
      /* if the scrollbar is extremely short, display it as a square */
      if (scrollbar_height <= wimp->scrollbar_width)
         scrollbar_height = wimp->scrollbar_width;

      wimp_render_quad(wimp,
            width - wimp->scrollbar_width - scrollbar_margin,
            header_height + y,
            wimp->scrollbar_width,
            scrollbar_height,
            width, height,
            coord_color);
   }
}

static void wimp_get_message(void *data, const char *message)
{
   wimp_handle_t *wimp   = (wimp_handle_t*)data;

   if (!wimp || !message || !*message)
      return;

   strlcpy(wimp->box_message, message, sizeof(wimp->box_message));
}

static void wimp_render_messagebox(const char *message)
{
   unsigned i, width, height;
   uint32_t normal_color;
   int x, y, font_size;
   settings_t *settings     = config_get_ptr();
   struct string_list *list = (struct string_list*)
      string_split(message, "\n");

   if (!list)
      return;
   if (list->elems == 0)
      goto end;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_SIZE, &font_size);

   x = width  / 2;
   y = height / 2 - list->size * font_size / 2;

   normal_color = FONT_COLOR_ARGB_TO_RGBA(settings->menu.entry_normal_color);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      if (msg)
         wimp_blit_line(x, y + i * font_size,
               width, height,
               msg, normal_color, TEXT_ALIGN_CENTER);
   }

end:
   string_list_free(list);
}

static void wimp_render(void *data)
{
   size_t i             = 0;
   float delta_time;
   menu_animation_ctx_delta_t delta;
   unsigned bottom, width, height, header_height;
   wimp_handle_t *wimp    = (wimp_handle_t*)data;
   settings_t *settings = config_get_ptr();

   if (!wimp)
      return;

   video_driver_get_size(&width, &height);

   menu_animation_ctl(MENU_ANIMATION_CTL_DELTA_TIME, &delta_time);

   delta.current = delta_time;

   if (menu_animation_ctl(MENU_ANIMATION_CTL_IDEAL_DELTA_TIME_GET, &delta))
      menu_animation_ctl(MENU_ANIMATION_CTL_UPDATE, &delta.ideal);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_WIDTH,  &width);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEIGHT, &height);
   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   if (settings->menu.pointer.enable)
   {
      int16_t pointer_y = menu_input_pointer_state(MENU_POINTER_Y_AXIS);
      float    old_accel_val, new_accel_val;
      unsigned new_pointer_val = 
         (pointer_y - wimp->line_height + wimp->scroll_y - 16)
         / wimp->line_height;

      menu_input_ctl(MENU_INPUT_CTL_POINTER_ACCEL_READ, &old_accel_val);
      menu_input_ctl(MENU_INPUT_CTL_POINTER_PTR, &new_pointer_val);

      wimp->scroll_y            -= old_accel_val / 60.0;

      new_accel_val = old_accel_val * 0.96;

      menu_input_ctl(MENU_INPUT_CTL_POINTER_ACCEL_WRITE, &new_accel_val);
   }

   if (wimp->scroll_y < 0)
      wimp->scroll_y = 0;

   bottom = menu_entries_get_end() * wimp->line_height
      - height + header_height + wimp->tabs_height;
   if (wimp->scroll_y > bottom)
      wimp->scroll_y = bottom;

   if (menu_entries_get_end() * wimp->line_height
         < height - header_height - wimp->tabs_height)
      wimp->scroll_y = 0;

   if (menu_entries_get_end() < height / wimp->line_height) { }
   else
      i = wimp->scroll_y / wimp->line_height;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
}

static void wimp_render_label_value(wimp_handle_t *wimp,
      int y, unsigned width, unsigned height,
      uint64_t index, uint32_t color, bool selected, const char *label,
      const char *value, float *pure_white)
{
   menu_animation_ctx_ticker_t ticker;
   char label_str[PATH_MAX_LENGTH];
   char value_str[PATH_MAX_LENGTH];
   int value_len            = strlen(value);
   int ticker_limit         = 0;
   size_t usable_width      = 0;
   uintptr_t texture_switch = 0;
   bool do_draw_text        = false;
   uint32_t hash_value      = 0;

   usable_width = width - (wimp->margin * 2);

   if (value_len * wimp->glyph_width > usable_width / 2)
      value_len = (usable_width/2) / wimp->glyph_width;

   ticker_limit = (usable_width / wimp->glyph_width) - (value_len + 2);

   ticker.s        = label_str;
   ticker.len      = ticker_limit;
   ticker.idx      = index;
   ticker.str      = label;
   ticker.selected = selected;

   menu_animation_ctl(MENU_ANIMATION_CTL_TICKER, &ticker);

   ticker.s        = value_str;
   ticker.len      = value_len;
   ticker.str      = value;

   menu_animation_ctl(MENU_ANIMATION_CTL_TICKER, &ticker);

   wimp_blit_line(wimp->margin, y + wimp->line_height / 2,
         width, height, label_str, color, TEXT_ALIGN_LEFT);

   hash_value = menu_hash_calculate(value);

   if (string_is_equal(value, "disabled") || string_is_equal(value, "off"))
   {
      if (wimp->textures.list[ZR_TEXTURE_SWITCH_OFF].id)
         texture_switch = wimp->textures.list[ZR_TEXTURE_SWITCH_OFF].id;
      else
         do_draw_text = true;
   }
   else if (string_is_equal(value, "enabled") || string_is_equal(value, "on"))
   {
      if (wimp->textures.list[ZR_TEXTURE_SWITCH_ON].id)
         texture_switch = wimp->textures.list[ZR_TEXTURE_SWITCH_ON].id;
      else
         do_draw_text = true;
   }
   else
   {
      switch (hash_value)
      {
         case MENU_VALUE_COMP:
            break;
         case MENU_VALUE_MORE:
            break;
         case MENU_VALUE_CORE:
            break;
         case MENU_VALUE_RDB:
            break;
         case MENU_VALUE_CURSOR:
            break;
         case MENU_VALUE_FILE:
            break;
         case MENU_VALUE_DIR:
            break;
         case MENU_VALUE_MUSIC:
            break;
         case MENU_VALUE_IMAGE:
            break;
         case MENU_VALUE_MOVIE:
            break;
         case MENU_VALUE_ON:
            if (wimp->textures.list[ZR_TEXTURE_SWITCH_ON].id)
               texture_switch = wimp->textures.list[ZR_TEXTURE_SWITCH_ON].id;
            else
               do_draw_text = true;
            break;
         case MENU_VALUE_OFF:
            if (wimp->textures.list[ZR_TEXTURE_SWITCH_OFF].id)
               texture_switch = wimp->textures.list[ZR_TEXTURE_SWITCH_OFF].id;
            else
               do_draw_text = true;
            break;
         default:
            do_draw_text = true;
            break;
      }
   }

   if (do_draw_text)
      wimp_blit_line(width - wimp->margin,
            y + wimp->line_height / 2,
            width, height, value_str, color, TEXT_ALIGN_RIGHT);

   if (texture_switch)
      wimp_draw_icon(wimp, texture_switch,
            width - wimp->margin - wimp->icon_size, y,
            width, height, 0, 1, &pure_white[0]);
}

static void wimp_render_menu_list(wimp_handle_t *wimp,
      unsigned width, unsigned height,
      uint32_t normal_color,
      uint32_t hover_color,
      float *pure_white)
{
   unsigned header_height;
   uint64_t *frame_count;
   size_t i                = 0;
   size_t          end     = menu_entries_get_end();
   video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);

   if (!menu_display_ctl(MENU_DISPLAY_CTL_UPDATE_PENDING, NULL))
      return;

   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   wimp->list_block.carr.coords.vertices = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   for (; i < end; i++)
   {
      int y;
      size_t selection;
      bool entry_selected;
      menu_entry_t entry;

      if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
         continue;

      y = header_height - wimp->scroll_y + (wimp->line_height * i);

      if ((y - (int)wimp->line_height) > (int)height
            || ((y + (int)wimp->line_height) < 0))
         continue;

      menu_entry_get(&entry, 0, i, NULL, true);

      entry_selected = selection == i;

      wimp_render_label_value(wimp, y, width, height, *frame_count / 20,
         entry_selected ? hover_color : normal_color, entry_selected,
         entry.path, entry.value, pure_white);
   }
}

#if 0
static void wimp_draw_cursor(wimp_handle_t *wimp,
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
#endif

static size_t wimp_list_get_size(void *data, enum menu_list_type type)
{
   size_t list_size = 0;
   (void)data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         list_size  = menu_entries_get_stack_size(0);
         break;
      case MENU_LIST_TABS:
         list_size = ZR_SYSTEM_TAB_END;
         break;
      default:
         break;
   }

   return list_size;
}

static void bgcolor_setalpha(float *bg, float alpha)
{
   bg[3]  = alpha;
   bg[7]  = alpha;
   bg[11] = alpha;
   bg[15] = alpha;
}

static int wimp_get_core_title(char *s, size_t len)
{
   struct retro_system_info    *system = NULL;
   rarch_system_info_t      *info = NULL;
   settings_t *settings           = config_get_ptr();
   const char *core_name          = NULL;
   const char *core_version       = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET,
         &system);
   
   core_name    = system->library_name;
   core_version = system->library_version;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   if (!settings->menu.core_enable)
      return -1; 

   if (string_is_empty(core_name))
      core_name = info->info.library_name;
   if (string_is_empty(core_name))
      core_name = menu_hash_to_str(MENU_VALUE_NO_CORE);

   if (!core_version)
      core_version = info->info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(s, len, "%s %s", core_name, core_version);

   return 0;
}

static void wimp_frame(void *data)
{   
   unsigned header_height;
   bool display_kb;
   float black_bg[16] = {
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
   };
   float blue_bg[16] = {
      0.13, 0.59, 0.95, 1,
      0.13, 0.59, 0.95, 1,
      0.13, 0.59, 0.95, 1,
      0.13, 0.59, 0.95, 1,
   };
   float lightblue_bg[16] = {
      0.89, 0.95, 0.99, 1.00,
      0.89, 0.95, 0.99, 1.00,
      0.89, 0.95, 0.99, 1.00,
      0.89, 0.95, 0.99, 1.00,
   };
   float pure_white[16]=  {
      1, 1, 1, 1,
      1, 1, 1, 1,
      1, 1, 1, 1,
      1, 1, 1, 1,
   };
   float white_bg[16]=  {
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
   };
   float white_transp_bg[16]=  {
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
   };
   float grey_bg[16]=  {
      0.78, 0.78, 0.78, 1,
      0.78, 0.78, 0.78, 1,
      0.78, 0.78, 0.78, 1,
      0.78, 0.78, 0.78, 1,
   };
   float shadow_bg[16]=  {
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0.2,
      0, 0, 0, 0.2,
   };
   menu_animation_ctx_ticker_t ticker;
   unsigned width, height, ticker_limit, i;
   char msg[256];
   char title[256];
   char title_buf[256];
   char title_msg[256];
   size_t selection;
   size_t title_margin;
   uint64_t *frame_count;
   menu_display_ctx_draw_t draw;
   wimp_handle_t *wimp               = (wimp_handle_t*)data;
   settings_t *settings            = config_get_ptr();
   const uint32_t normal_color     = 0x212121ff;
   const uint32_t hover_color      = 0x212121ff;
   const uint32_t title_color      = 0xffffffff;
   const uint32_t activetab_color  = 0x0096f2ff;
   const uint32_t passivetab_color = 0x9e9e9eff;
   bool background_rendered        = false;
   bool libretro_running           = menu_display_ctl(
         MENU_DISPLAY_CTL_LIBRETRO_RUNNING, NULL);

   video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);

   (void)passivetab_color;
   (void)activetab_color;

   if (!wimp)
      return;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);
   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   if (libretro_running)
   {
      memset(&draw, 0, sizeof(menu_display_ctx_draw_t));

      draw.width              = width;
      draw.height             = height;
      draw.texture            = wimp->textures.white;
      draw.handle_alpha       = 0.75f;
      draw.force_transparency = false;
      draw.color              = &white_transp_bg[0];
      draw.color2             = &white_bg[0];
      draw.vertex             = NULL;
      draw.tex_coord          = NULL;
      draw.vertex_count       = 4;
      draw.prim_type          = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

      menu_display_ctl(MENU_DISPLAY_CTL_DRAW_BG, &draw);
   }
   else
   {
      menu_display_ctx_clearcolor_t clearcolor;

      clearcolor.r = 1.0f;
      clearcolor.g = 1.0f;
      clearcolor.b = 1.0f;
      clearcolor.a = 0.75f;

      menu_display_ctl(MENU_DISPLAY_CTL_CLEAR_COLOR, &clearcolor);

      if (wimp->textures.bg.id)
      {
         background_rendered = true;

         /* Set new opacity for transposed white background */
         bgcolor_setalpha(white_transp_bg, 0.30);

         memset(&draw, 0, sizeof(menu_display_ctx_draw_t));

         draw.width              = width;
         draw.height             = height;
         draw.texture            = wimp->textures.bg.id;
         draw.handle_alpha       = 0.75f;
         draw.force_transparency = true;
         draw.color              = &white_transp_bg[0];
         draw.color2             = &white_bg[0];
         draw.vertex             = NULL;
         draw.tex_coord          = NULL;
         draw.vertex_count       = 4;
         draw.prim_type          = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

         menu_display_ctl(MENU_DISPLAY_CTL_DRAW_BG, &draw);

         /* Restore opacity of transposed white background */
         bgcolor_setalpha(white_transp_bg, 0.90);
      }
   }

   menu_entries_get_title(title, sizeof(title));

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   if (background_rendered || libretro_running)
      bgcolor_setalpha(lightblue_bg, 0.75);
   else
      bgcolor_setalpha(lightblue_bg, 1.0);

   /* highlighted entry */
   wimp_render_quad(wimp, 0,
         header_height -   wimp->scroll_y + wimp->line_height *
         selection, width, wimp->line_height,
         width, height,
         &lightblue_bg[0]);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_BIND_BLOCK, &wimp->list_block);

   wimp_render_menu_list(wimp, width, height,
         normal_color, hover_color, &pure_white[0]);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_FLUSH_BLOCK, NULL);
   menu_animation_ctl(MENU_ANIMATION_CTL_SET_ACTIVE, NULL);

   /* header */
   wimp_render_quad(wimp, 0, 0, width, header_height,
         width, height, &blue_bg[0]);

   wimp->tabs_height = 0;

   /* display tabs if depth equal one, if not hide them */
   if (wimp_list_get_size(wimp, MENU_LIST_PLAIN) == 1)
   {
      wimp_draw_tab_begin(wimp, width, height, &white_bg[0], &grey_bg[0]);

      for (i = 0; i <= ZR_SYSTEM_TAB_END; i++)
         wimp_draw_tab(wimp, i, width, height, &pure_white[0]);

      wimp_draw_tab_end(wimp, width, height, header_height, &blue_bg[0]);
   }

   wimp_render_quad(wimp, 0, header_height, width,
         wimp->shadow_height,
         width, height,
         &shadow_bg[0]);

   title_margin = wimp->margin;

   if (menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL))
   {
      title_margin = wimp->icon_size;
      wimp_draw_icon(wimp, wimp->textures.list[ZR_TEXTURE_BACK].id,
         0, 0, width, height, 0, 1, &pure_white[0]);
   }

   ticker_limit = (width - wimp->margin*2) / wimp->glyph_width;

   ticker.s        = title_buf;
   ticker.len      = ticker_limit;
   ticker.idx      = *frame_count / 100;
   ticker.str      = title;
   ticker.selected = true;

   menu_animation_ctl(MENU_ANIMATION_CTL_TICKER, &ticker);

   /* Title */
   if (wimp_get_core_title(title_msg, sizeof(title_msg)) == 0)
   {
      char title_buf_msg_tmp[256];
      char title_buf_msg[256];
      size_t usable_width = width - (wimp->margin * 2);
      int ticker_limit, value_len;
      
      snprintf(title_buf_msg, sizeof(title_buf), "%s (%s)",
            title_buf, title_msg);
      value_len = strlen(title_buf);
      ticker_limit = (usable_width / wimp->glyph_width) - (value_len + 2);

      ticker.s        = title_buf_msg_tmp;
      ticker.len      = ticker_limit;
      ticker.idx      = *frame_count / 20;
      ticker.str      = title_buf_msg;
      ticker.selected = true;

      menu_animation_ctl(MENU_ANIMATION_CTL_TICKER, &ticker);

      strlcpy(title_buf, title_buf_msg_tmp, sizeof(title_buf));
   }

   wimp_blit_line(title_margin, header_height / 2, width, height,
         title_buf, title_color, TEXT_ALIGN_LEFT);

   wimp_draw_scrollbar(wimp, width, height, &grey_bg[0]);

   menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_DISPLAY, &display_kb);

   if (display_kb)
   {
      const char *str = NULL, *label = NULL;
      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_BUFF_PTR, &str);
      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_LABEL,    &label);

      if (!str)
         str = "";
      wimp_render_quad(wimp, 0, 0, width, height, width, height, &black_bg[0]);
      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      wimp_render_messagebox(msg);
   }

   if (!string_is_empty(wimp->box_message))
   {
      wimp_render_quad(wimp, 0, 0, width, height, width, height, &black_bg[0]);
      wimp_render_messagebox(wimp->box_message);
      wimp->box_message[0] = '\0';
   }

   /* zahnrad code */
   zr_input_begin(&gui.ctx);
   wimp_input_motion(&gui.ctx);
   wimp_input_button(&gui.ctx);
   zr_input_end(&gui.ctx);
   
   wimp_start(&gui, width, height);
   glViewport(0, 0, width, height);
   device_draw(&device, &gui.ctx, width, height, ZR_ANTI_ALIASING_ON);   

   /* zahnrad code */

   menu_display_ctl(MENU_DISPLAY_CTL_RESTORE_CLEAR_COLOR, NULL);
   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}

static void wimp_allocate_white_texture(wimp_handle_t *wimp)
{
   struct texture_image ti;
   static const uint8_t white_data[] = { 0xff, 0xff, 0xff, 0xff };

   ti.width  = 1;
   ti.height = 1;
   ti.pixels = (uint32_t*)&white_data;

   video_driver_texture_load(&ti,
         TEXTURE_FILTER_NEAREST, &wimp->textures.white);
}

static void wimp_font(void)
{
   int font_size;
   char mediapath[PATH_MAX_LENGTH], fontpath[PATH_MAX_LENGTH];
   menu_display_ctx_font_t font_info;
   settings_t *settings = config_get_ptr();

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_SIZE, &font_size);

   fill_pathname_join(mediapath, settings->assets_directory,
         "wimp", sizeof(mediapath));
   fill_pathname_join(fontpath, mediapath,
         "Roboto-Regular.ttf", sizeof(fontpath));

   font_info.path = fontpath;
   font_info.size = font_size;

   if (!menu_display_ctl(MENU_DISPLAY_CTL_FONT_MAIN_INIT, &font_info))
      RARCH_WARN("Failed to load font.");
}

static void wimp_layout(wimp_handle_t *wimp)
{
   void *fb_buf;
   float scale_factor;
   int new_font_size;
   unsigned width, height, new_header_height;

   video_driver_get_size(&width, &height);

   /* Mobiles platforms may have very small display metrics 
    * coupled to a high resolution, so we should be DPI aware 
    * to ensure the entries hitboxes are big enough.
    *
    * On desktops, we just care about readability, with every widget
    * size proportional to the display width. */
   menu_display_ctl(MENU_DISPLAY_CTL_GET_DPI, &scale_factor);

   new_header_height    = scale_factor / 3;
   new_font_size        = scale_factor / 9;

   wimp->shadow_height   = scale_factor / 36;
   wimp->scrollbar_width = scale_factor / 36;
   wimp->tabs_height     = scale_factor / 3;
   wimp->line_height     = scale_factor / 3;
   wimp->margin          = scale_factor / 9;
   wimp->icon_size       = scale_factor / 3;

   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEADER_HEIGHT,
         &new_header_height);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_FONT_SIZE,
         &new_font_size);

   /* we assume the average glyph aspect ratio is close to 3:4 */
   wimp->glyph_width = new_font_size * 3/4;

   wimp_font();

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_BUF, &fb_buf);

   if (fb_buf) /* calculate a more realistic ticker_limit */
   {
      unsigned m_width = 
         font_driver_get_message_width(fb_buf, "a", 1, 1);

      if (m_width)
         wimp->glyph_width = m_width;
   }
}

static void *wimp_init(void **userdata)
{
   settings_t *settings = config_get_ptr();
   wimp_handle_t   *wimp = NULL;
   menu_handle_t *menu = (menu_handle_t*)
      calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

   if (!menu_display_ctl(MENU_DISPLAY_CTL_INIT_FIRST_DRIVER, NULL))
      goto error;

   wimp = (wimp_handle_t*)calloc(1, sizeof(wimp_handle_t));

   if (!wimp)
      goto error;

   *userdata = wimp;

   wimp_layout(wimp);
   wimp_allocate_white_texture(wimp);

   /* zahnrad code
      just init the font_path variable for the
      drawing function */
   fill_pathname_join(font_path, settings->assets_directory,
         "wimp", sizeof(font_path));
   fill_pathname_join(font_path, font_path,
         "Roboto-Regular.ttf", sizeof(font_path));
         
   glViewport(0, 0, width, height);
   alloc.userdata.ptr = NULL;
   alloc.alloc = mem_alloc;
   alloc.free = mem_free;
   zr_buffer_init(&device.cmds, &alloc, 1024);
   usrfnt = font_bake_and_upload(&device, &font, font_path, 14,
      zr_font_default_glyph_ranges());
   zr_init(&gui.ctx, &alloc, &usrfnt);
   device_init(&device);
   /* zahnrad code */

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

   /* zahnrad code */
   free(font.glyphs);
   zr_free(&gui.ctx);
   zr_buffer_free(&device.cmds);
   device_shutdown(&device);
   /* zahnrad code */

   gfx_coord_array_free(&wimp->list_block.carr);

   font_driver_bind_block(NULL, NULL);
}

static void wimp_context_bg_destroy(wimp_handle_t *wimp)
{
   if (!wimp)
      return;

   video_driver_texture_unload((uintptr_t*)&wimp->textures.bg.id);
   video_driver_texture_unload((uintptr_t*)&wimp->textures.white);
}

static void wimp_context_destroy(void *data)
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

static bool wimp_load_image(void *userdata, void *data,
      enum menu_image_type type)
{
   wimp_handle_t *wimp = (wimp_handle_t*)userdata;

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         wimp_context_bg_destroy(wimp);
         video_driver_texture_load(data,
               TEXTURE_FILTER_MIPMAP_LINEAR, &wimp->textures.bg.id);
         wimp_allocate_white_texture(wimp);
         break;
      case MENU_IMAGE_BOXART:
         break;
   }

   return true;
}

static float wimp_get_scroll(wimp_handle_t *wimp)
{
   size_t selection;
   unsigned width, height, half = 0;

   if (!wimp)
      return 0;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return 0;

   video_driver_get_size(&width, &height);

   if (wimp->line_height)
      half = (height / wimp->line_height) / 2;

   if (selection < half)
      return 0;

   return ((selection + 2 - half) * wimp->line_height);
}

static void wimp_navigation_set(void *data, bool scroll)
{
   menu_animation_ctx_entry_t entry;
   wimp_handle_t *wimp    = (wimp_handle_t*)data;
   float     scroll_pos   = wimp ? wimp_get_scroll(wimp) : 0.0f;

   if (!wimp || !scroll)
      return;

   entry.duration         = 10;
   entry.target_value     = scroll_pos;
   entry.subject          = &wimp->scroll_y;
   entry.easing_enum      = EASING_IN_OUT_QUAD;
   entry.tag              = -1;
   entry.cb               = NULL;

   menu_animation_ctl(MENU_ANIMATION_CTL_PUSH, &entry);
}

static void  wimp_list_set_selection(void *data, file_list_t *list)
{
   wimp_navigation_set(data, true);
}

static void wimp_navigation_clear(void *data, bool pending_push)
{
   size_t i             = 0;
   wimp_handle_t *wimp    = (wimp_handle_t*)data;
   if (!wimp)
      return;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   wimp->scroll_y = 0;
}

static void wimp_navigation_set_last(void *data)
{
   wimp_navigation_set(data, true);
}

static void wimp_navigation_alphabet(void *data, size_t *unused)
{
   wimp_navigation_set(data, true);
}

static void wimp_populate_entries(
      void *data, const char *path,
      const char *label, unsigned i)
{
   wimp_handle_t *wimp    = (wimp_handle_t*)data;
   if (!wimp)
      return;

   wimp->scroll_y = wimp_get_scroll(wimp);
}

static void wimp_context_reset(void *data)
{
   char iconpath[PATH_MAX_LENGTH] = {0};
   wimp_handle_t *wimp              = (wimp_handle_t*)data;
   settings_t *settings           = config_get_ptr();

   if (!wimp || !settings)
      return;

   fill_pathname_join(iconpath, settings->assets_directory,
         "glui", sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   wimp_layout(wimp);
   wimp_context_bg_destroy(wimp);
   wimp_allocate_white_texture(wimp);
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

static void wimp_preswitch_tabs(wimp_handle_t *wimp, unsigned action)
{
   size_t idx              = 0;
   size_t stack_size       = 0;
   file_list_t *menu_stack = NULL;

   if (!wimp)
      return;

   menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);

   menu_stack = menu_entries_get_menu_stack_ptr(0);
   stack_size = menu_stack->size;

   if (menu_stack->list[stack_size - 1].label)
      free(menu_stack->list[stack_size - 1].label);
   menu_stack->list[stack_size - 1].label = NULL;

   switch (wimp->categories.selection_ptr)
   {
      case ZR_SYSTEM_TAB_MAIN:
         menu_stack->list[stack_size - 1].label = 
            strdup(menu_hash_to_str(MENU_VALUE_MAIN_MENU));
         menu_stack->list[stack_size - 1].type = 
            MENU_SETTINGS;
         break;
      case ZR_SYSTEM_TAB_PLAYLISTS:
         menu_stack->list[stack_size - 1].label = 
            strdup(menu_hash_to_str(MENU_VALUE_PLAYLISTS_TAB));
         menu_stack->list[stack_size - 1].type = 
            MENU_PLAYLISTS_TAB;
         break;
      case ZR_SYSTEM_TAB_SETTINGS:
         menu_stack->list[stack_size - 1].label = 
            strdup(menu_hash_to_str(MENU_VALUE_SETTINGS_TAB));
         menu_stack->list[stack_size - 1].type = 
            MENU_SETTINGS;
         break;
   }
}

static void wimp_list_cache(void *data, enum menu_list_type type, unsigned action)
{
   size_t list_size;
   wimp_handle_t *wimp   = (wimp_handle_t*)data;

   if (!wimp)
      return;

   list_size = ZR_SYSTEM_TAB_END;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         break;
      case MENU_LIST_HORIZONTAL:
         wimp->categories.selection_ptr_old = wimp->categories.selection_ptr;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (wimp->categories.selection_ptr == 0)
               {
                  wimp->categories.selection_ptr = list_size;
                  wimp->categories.active.idx = list_size - 1;
               }
               else
                  wimp->categories.selection_ptr--;
               break;
            default:
               if (wimp->categories.selection_ptr == list_size)
               {
                  wimp->categories.selection_ptr = 0;
                  wimp->categories.active.idx = 1;
               }
               else
                  wimp->categories.selection_ptr++;
               break;
         }

         wimp_preswitch_tabs(wimp, action);
         break;
      default:
         break;
   }
}

static int wimp_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   int ret                = -1;
   core_info_list_t *list = NULL;
   menu_handle_t *menu    = (menu_handle_t*)data;

   (void)userdata;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_LOAD_CONTENT),
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT),
               MENU_SETTING_ACTION, 0, 0);

         core_info_ctl(CORE_INFO_CTL_LIST_GET, &list);
         if (core_info_list_num_info_files(list))
         {
            menu_entries_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_DETECT_CORE_LIST),
                  menu_hash_to_str(MENU_LABEL_DETECT_CORE_LIST),
                  MENU_SETTING_ACTION, 0, 0);

            menu_entries_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  menu_hash_to_str(MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  MENU_SETTING_ACTION, 0, 0);
         }

         info->need_push    = true;
         info->need_refresh = true;
         ret = 0;
         break;
      case DISPLAYLIST_MAIN_MENU:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
         {
            menu_displaylist_parse_settings(menu, info,
                  menu_hash_to_str(MENU_LABEL_CONTENT_SETTINGS), PARSE_ACTION, false);
         }

         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_START_CORE), PARSE_ACTION, false);

#ifndef HAVE_DYNAMIC
         if (frontend_driver_has_fork())
#endif
         {
            menu_displaylist_parse_settings(menu, info,
                  menu_hash_to_str(MENU_LABEL_CORE_LIST), PARSE_ACTION, false);
         }
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT_LIST), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT_HISTORY), PARSE_ACTION, false);
#if defined(HAVE_NETWORKING)
#if defined(HAVE_LIBRETRODB)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_ADD_CONTENT_LIST), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_ONLINE_UPDATER), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INFORMATION_LIST), PARSE_ACTION, false);
#ifndef HAVE_DYNAMIC
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_RESTART_RETROARCH), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_CONFIGURATIONS), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SAVE_CURRENT_CONFIG), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SAVE_NEW_CONFIG), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_HELP_LIST), PARSE_ACTION, false);
#if !defined(IOS)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_QUIT_RETROARCH), PARSE_ACTION, false);
#endif
#if defined(HAVE_LAKKA)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SHUTDOWN), PARSE_ACTION, false);
#endif
         info->need_push    = true;
         ret = 0;
         break;
   }
   return ret;
}

static size_t wimp_list_get_selection(void *data)
{
   wimp_handle_t *wimp   = (wimp_handle_t*)data;

   if (!wimp)
      return 0;

   return wimp->categories.selection_ptr;
}

static int wimp_pointer_tap(void *userdata,
      unsigned x, unsigned y, 
      unsigned ptr, menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   size_t selection, idx;
   unsigned header_height, width, height, i;
   bool scroll                = false;
   wimp_handle_t *wimp          = (wimp_handle_t*)userdata;
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!wimp)
      return 0;

   video_driver_get_size(&width, &height);

   menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   if (y < header_height)
   {
      menu_entries_pop_stack(&selection, 0);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);
   }
   else if (y > height - wimp->tabs_height)
   {
      for (i = 0; i <= ZR_SYSTEM_TAB_END; i++)
      {
         unsigned tab_width = width / (ZR_SYSTEM_TAB_END + 1);
         unsigned start     = tab_width * i;

         if ((x >= start) && (x < (start + tab_width)))
         {
            wimp->categories.selection_ptr = i;

            wimp_preswitch_tabs(wimp, action);

            if (cbs && cbs->action_content_list_switch)
               return cbs->action_content_list_switch(selection_buf, menu_stack,
                     "", "", 0);
         }
      }
   }
   else if (ptr <= (menu_entries_get_size() - 1))
   {
      if (ptr == selection && cbs && cbs->action_select)
         return menu_entry_action(entry, selection, MENU_ACTION_SELECT);

      idx  = ptr;

      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_zr = {
   NULL,
   wimp_get_message,
   generic_menu_iterate,
   wimp_render,
   wimp_frame,
   wimp_init,
   wimp_free,
   wimp_context_reset,
   wimp_context_destroy,
   wimp_populate_entries,
   NULL,
   wimp_navigation_clear,
   NULL,
   NULL,
   wimp_navigation_set,
   wimp_navigation_set_last,
   wimp_navigation_alphabet,
   wimp_navigation_alphabet,
   generic_menu_init_list,
   NULL,
   NULL,
   NULL,
   wimp_list_cache,
   wimp_list_push,
   wimp_list_get_selection,
   wimp_list_get_size,
   NULL,
   wimp_list_set_selection,
   NULL,
   wimp_load_image,
   "zahrand",
   wimp_environ,
   wimp_pointer_tap,
};
