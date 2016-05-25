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

#include "../../menu_driver.h"
#include "../../menu_hash.h"

#include "../../../gfx/common/gl_common.h"
#include "../../../core_info.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"


#define LEN(a) (sizeof(a)/sizeof(a)[0])


void nk_menu_wnd_set_state(nk_menu_handle_t *zr, const int id,
   struct nk_vec2 pos, struct nk_vec2 size)
{
   zr->window[id].position = pos;
   zr->window[id].size = size;
}

void nk_menu_wnd_get_state(nk_menu_handle_t *zr, const int id,
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
   nk_menu_wnd_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
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
   nk_menu_wnd_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
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
   nk_menu_wnd_set_state(zr, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   if (zr->size_changed)
      nk_window_set_size(ctx, nk_vec2(nk_window_get_size(ctx).x, zr->size.y));

   nk_end(ctx);
}
