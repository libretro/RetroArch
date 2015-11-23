/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "../general.h"
#include "../verbosity.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static const font_renderer_driver_t *font_backends[] = {
#ifdef HAVE_FREETYPE
   &freetype_font_renderer,
#endif
#if defined(__APPLE__) && defined(HAVE_CORETEXT)
   &coretext_font_renderer,
#endif
#ifdef HAVE_STB_FONT
   &stb_font_renderer,
#endif
   &bitmap_font_renderer,
   NULL
};

int font_renderer_get_message_width(const char *msg, float scale)
{
    driver_t *driver                        = driver_get_ptr();  
    const struct font_renderer *font_driver = driver ? driver->font_osd_driver : NULL;
    
    if (!font_driver || !font_driver->get_message_width)
       return 0;
       
    return font_driver->get_message_width(driver->font_osd_data, msg, strlen(msg), scale);
}

bool font_renderer_create_default(
      const font_renderer_driver_t **drv, void **handle,
      const char *font_path, unsigned font_size)
{
   unsigned i;

   for (i = 0; font_backends[i]; i++)
   {
      const char *path = font_path;

      if (!path)
         path = font_backends[i]->get_default_font();
      if (!path)
         continue;

      *handle = font_backends[i]->init(path, font_size);
      if (*handle)
      {
         RARCH_LOG("Using font rendering backend: %s.\n",
               font_backends[i]->ident);
         *drv = font_backends[i];
         return true;
      }
      else
         RARCH_ERR("Failed to create rendering backend: %s.\n",
               font_backends[i]->ident);
   }

   *drv = NULL;
   *handle = NULL;
   return false;
}
