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

#include "ffemu.h"
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static const ffemu_backend_t *ffemu_backends[] = {
#ifdef HAVE_FFMPEG
   &ffemu_ffmpeg,
#endif
   NULL,
};

const ffemu_backend_t *ffemu_find_backend(const char *ident)
{
   unsigned i;
   for (i = 0; ffemu_backends[i]; i++)
   {
      if (!strcmp(ffemu_backends[i]->ident, ident))
         return ffemu_backends[i];
   }

   return NULL;
}

bool ffemu_init_first(const ffemu_backend_t **backend, void **data, const struct ffemu_params *params)
{
   unsigned i;
   for (i = 0; ffemu_backends[i]; i++)
   {
      void *handle = ffemu_backends[i]->init(params);
      if (handle)
      {
         *backend = ffemu_backends[i];
         *data = handle;
         return true;
      }
   }

   return false;
}

