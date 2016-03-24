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

#include "zr_common.h"
#include "../menu_display.h"
#include "../menu_input.h"

enum
{
   ZR_TEXTURE_POINTER = 0,
   ZR_TEXTURE_LAST
};

enum
{
   ZRMENU_WND_MAIN = 0,
   ZRMENU_WND_CONTROL,
   ZRMENU_WND_SHADER_PARAMETERS,
   ZRMENU_WND_TEST,
   ZRMENU_WND_WIZARD
};

enum zrmenu_theme
{
   THEME_DARK = 0,
   THEME_LIGHT
};

struct icons {
    struct zr_image folder;
    struct zr_image monitor;
    struct zr_image gamepad;
    struct zr_image settings;
    struct zr_image speaker;
    struct zr_image invader;
    struct zr_image page_on;
    struct zr_image page_off;
};

struct window {
   bool open;
   struct zr_vec2 position;
   struct zr_vec2 size;
};

typedef struct zrmenu_handle
{
   /* zahnrad mandatory */
   void *memory;
   struct zr_context ctx;
   struct zr_memory_status status;
   enum menu_action action;

   /* window control variables */
   struct zr_vec2 size;
   bool size_changed;
   struct window window[5];

   /* menu driver variables */
   char box_message[PATH_MAX_LENGTH];

   /* image & theme related variables */
   char assets_directory[PATH_MAX_LENGTH];
   struct icons icons;
   enum zrmenu_theme theme;


   struct
   {
      menu_texture_item bg;
      menu_texture_item list[ZR_TEXTURE_LAST];
   } textures;

   gfx_font_raster_block_t list_block;
} zrmenu_handle_t;

void zrmenu_set_style(struct zr_context *ctx, enum zrmenu_theme theme);

void zrmenu_wnd_wizard(zrmenu_handle_t *zr);
void zrmenu_wnd_shader_parameters(zrmenu_handle_t *zr);
void zrmenu_wnd_control(zrmenu_handle_t *zr);
void zrmenu_wnd_test(zrmenu_handle_t *zr);
void zrmenu_wnd_main(zrmenu_handle_t *zr);

static void ui_slider(struct zr_context *ctx, const char *title, int *value, int max, int active);
static int ui_selector(struct zr_context *ctx, const char *title, int *selected, const char *items[], int max, int active);