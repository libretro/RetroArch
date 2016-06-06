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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#include "../../menu_driver.h"
#include "../../menu_hash.h"

void nk_wnd_settings(nk_menu_handle_t *nk)
{
   unsigned i;
   video_shader_ctx_t shader_info;
   struct nk_panel layout;
   struct nk_context *ctx = &nk->ctx;
   const int id           = NK_WND_SETTINGS;

   if (nk_begin(ctx, &layout, "Settings", nk_rect(240, 10, 300, 400),
         NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_MOVABLE|
         NK_WINDOW_SCALABLE|NK_WINDOW_BORDER))
   {
      nk_layout_row_dynamic(ctx, 30, 1);
   }

   /* save position and size to restore after context reset */
   nk_wnd_set_state(nk, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   nk_end(ctx);
}
