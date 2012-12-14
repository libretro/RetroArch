/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "gl_font.h"
#include "../general.h"

static const gl_font_renderer_t *backends[] = {
   &gl_raster_font,
};

const gl_font_renderer_t *gl_font_init_first(void *data, const char *font_path, unsigned font_size)
{
   for (unsigned i = 0; i < ARRAY_SIZE(backends); i++)
   {
      if (backends[i]->init(data, font_path, font_size))
         return backends[i];
   }

   return NULL;
}

