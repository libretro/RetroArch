/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "fonts.h"
#include "../../general.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

static const font_renderer_driver_t *font_backends[] = {
#ifdef HAVE_FREETYPE
   &ft_font_renderer,
#endif
   &bitmap_font_renderer,
};

bool font_renderer_create_default(const font_renderer_driver_t **driver, void **handle)
{
   unsigned i;
   for (i = 0; i < ARRAY_SIZE(font_backends); i++)
   {
      const char *font_path = *g_settings.video.font_path ? g_settings.video.font_path : NULL;
      if (!font_path)
         font_path = font_backends[i]->get_default_font();

      if (!font_path)
         continue;

      *handle = font_backends[i]->init(font_path, g_settings.video.font_size);
      if (*handle)
      {
         RARCH_LOG("Using font rendering backend: %s.\n", font_backends[i]->ident);
         *driver = font_backends[i];
         return true;
      }
      else
         RARCH_ERR("Failed to create rendering backend: %s.\n", font_backends[i]->ident);
   }

   *driver = NULL;
   *handle = NULL;
   return false;
}

