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

#include "../../../gfx/common/gl_common.h"
#include "../../../core_info.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"

void nk_wnd_shader_parameters(nk_menu_handle_t *nk)
{
   unsigned i;
   video_shader_ctx_t shader_info;
   struct nk_panel layout;
   struct nk_context *ctx = &nk->ctx;
   const int id           = NK_WND_SHADER_PARAMETERS;

   if (nk_begin(ctx, &layout, "Shader Parameters", nk_rect(240, 10, 300, 400),
         NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_MOVABLE|
         NK_WINDOW_SCALABLE|NK_WINDOW_BORDER))
   {
      nk_layout_row_dynamic(ctx, 30, 1);

      video_shader_driver_get_current_shader(&shader_info);

      if (shader_info.data)
      {
         for (i = 0; i < GFX_MAX_PARAMETERS; i++)
         {
            if (!string_is_empty(shader_info.data->parameters[i].desc))
            {
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
   nk_wnd_set_state(nk, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   nk_end(ctx);
}
