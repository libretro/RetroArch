/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include "gfx_context.h"
#include "../general.h"
#include <string.h>
#include "image_context.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static const image_ctx_driver_t *image_ctx_drivers[] = {
#if defined(__CELLOS_LV2__)
   &image_ctx_ps3,
#endif
#if defined(_XBOX1)
   &image_ctx_xdk1,
#endif
   &image_ctx_rpng,
#if defined(HAVE_SDL_IMAGE)
   &image_ctx_sdl,
#endif
   NULL
};

static int find_image_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; image_ctx_drivers[i]; i++)
      if (strcasecmp(driver, image_ctx_drivers[i]->ident) == 0)
         return i;
   return -1;
}

void find_image_driver(void)
{
   int i;
   if (driver.image)
      return;

   i = find_image_driver_index(g_settings.image.driver);
   if (i >= 0)
      driver.image = image_ctx_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any image driver named \"%s\"\n", g_settings.image.driver);
      RARCH_LOG_OUTPUT("Available image drivers are:\n");
      for (d = 0; image_ctx_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", image_ctx_drivers[d]->ident);

      rarch_fail(1, "find_image_driver()");
   }
}

void find_prev_image_driver(void)
{
   int i = find_image_driver_index(g_settings.image.driver);
   if (i > 0)
   {
      strlcpy(g_settings.image.driver, image_ctx_drivers[i - 1]->ident, sizeof(g_settings.image.driver));
      driver.image = (image_ctx_driver_t*)image_ctx_drivers[i - 1];
   }
   else
      RARCH_WARN("Couldn't find any previous image driver (current one: \"%s\").\n", g_settings.image.driver);
}

void find_next_image_driver(void)
{
   int i = find_image_driver_index(g_settings.image.driver);
   if (i >= 0 && image_ctx_drivers[i + 1])
   {
      strlcpy(g_settings.image.driver, image_ctx_drivers[i + 1]->ident, sizeof(g_settings.image.driver));
      driver.image = (image_ctx_driver_t*)image_ctx_drivers[i + 1];
   }
   else
      RARCH_WARN("Couldn't find any next image driver (current one: \"%s\").\n", g_settings.image.driver);
}
