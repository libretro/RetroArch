/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "frontend_context.h"
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static const frontend_ctx_driver_t *frontend_ctx_drivers[] = {
#if defined(__CELLOS_LV2__)
   &frontend_ctx_ps3,
#endif
#if defined(_XBOX)
   &frontend_ctx_xdk,
#endif
#if defined(GEKKO)
   &frontend_ctx_gx,
#endif
#if defined(__QNX__)
   &frontend_ctx_qnx,
#endif
#if defined(IOS) || defined(OSX)
   &frontend_ctx_apple,
#endif
   NULL // zero length array is not valid
};

const frontend_ctx_driver_t *frontend_ctx_find_driver(const char *ident)
{
   for (unsigned i = 0; frontend_ctx_drivers[i]; i++)
   {
      if (strcmp(frontend_ctx_drivers[i]->ident, ident) == 0)
         return frontend_ctx_drivers[i];
   }

   return NULL;
}

const frontend_ctx_driver_t *frontend_ctx_init_first(void)
{
   for (unsigned i = 0; frontend_ctx_drivers[i]; i++)
      return frontend_ctx_drivers[i];

   return NULL;
}
