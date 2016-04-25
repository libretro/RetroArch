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

#include "nk_common.h"
#include "../menu_display.h"
#include "../menu_input.h"

enum
{
   NK_TEXTURE_POINTER = 0,
   NK_TEXTURE_LAST
};

enum
{
   ZRMENU_WND_MAIN = 0,
   ZRMENU_WND_SHADER_PARAMETERS,
   ZRMENU_WND_TEST,
};

enum nk_menu_theme
{
   THEME_DARK = 0,
   THEME_LIGHT
};

struct icons {
    struct nk_image folder;
    struct nk_image monitor;
    struct nk_image gamepad;
    struct nk_image settings;
    struct nk_image speaker;
    struct nk_image invader;
    struct nk_image page_on;
    struct nk_image page_off;
};

struct window {
   bool open;
   struct nk_vec2 position;
   struct nk_vec2 size;
};

typedef struct nk_menu_handle
{
   /* zahnrad mandatory */
   void *memory;
   struct nk_context ctx;
   struct nk_memory_status status;
   enum menu_action action;

   /* window control variables */
   struct nk_vec2 size;
   bool size_changed;
   struct window window[5];

   /* menu driver variables */
   char box_message[PATH_MAX_LENGTH];

   /* image & theme related variables */
   char assets_directory[PATH_MAX_LENGTH];
   struct icons icons;
   enum nk_menu_theme theme;


   struct
   {
      menu_texture_item bg;
      menu_texture_item list[NK_TEXTURE_LAST];
   } textures;

   gfx_font_raster_block_t list_block;
} nk_menu_handle_t;

void nk_menu_wnd_shader_parameters(nk_menu_handle_t *zr);
void nk_menu_wnd_test(nk_menu_handle_t *zr);
void nk_menu_wnd_main(nk_menu_handle_t *zr);

