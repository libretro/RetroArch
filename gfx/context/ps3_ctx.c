/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "../../driver.h"

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../gl_common.h"
#include "../image.h"
#include "../../ps3/shared.h"

#include "ps3_ctx.h"

static struct texture_image menu_texture;

void gfx_ctx_set_swap_interval(unsigned interval, bool inited)
{
   if (inited)
   {
      if (interval)
         glEnable(GL_VSYNC_SCE);
      else
         glDisable(GL_VSYNC_SCE);
   }
}

void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   gl_t *gl = driver.video_data;
   *quit = false;
   *resize = false;

#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif

   if (gl->quitting)
      *quit = true;

   if (gl->should_resize)
      *resize = true;
}

bool gfx_ctx_window_has_focus(void)
{
   return true;
}

void gfx_ctx_swap_buffers(void)
{
   psglSwap();
}

bool gfx_ctx_menu_init(void)
{
   gl_t *gl = driver.video_data;

   if (!gl)
      return false;

#ifdef HAVE_CG_MENU
   glGenTextures(1, &gl->menu_texture_id);

   RARCH_LOG("Loading texture image for menu...\n");
   if(!texture_image_load(DEFAULT_MENU_BORDER_FILE, &menu_texture))
   {
      RARCH_ERR("Failed to load texture image for menu.\n");
      return false;
   }

   glBindTexture(GL_TEXTURE_2D, gl->menu_texture_id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB_SCE, menu_texture.width, menu_texture.height, 0,
		   GL_ARGB_SCE, GL_UNSIGNED_INT_8_8_8_8, menu_texture.pixels);

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   free(menu_texture.pixels);
#endif
	
   return true;
}
