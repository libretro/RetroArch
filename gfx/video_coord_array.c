/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <stdlib.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>

#include "video_coord_array.h"

static INLINE bool realloc_checked(void **ptr, size_t size)
{
   void *nptr = NULL;

   if (*ptr)
      nptr = realloc(*ptr, size);
   else
      nptr = malloc(size);

   if (nptr)
      *ptr = nptr;

   return *ptr == nptr;
}

bool gfx_coord_array_add(gfx_coord_array_t *ca,
      const gfx_coords_t *coords, unsigned count)
{
   size_t base_size, offset;
   bool success   = false;

   count          = MIN(count, coords->vertices);

   if (ca->coords.vertices + count >= ca->allocated)
   {
      unsigned alloc_size = next_pow2(ca->coords.vertices + count);
      size_t base_size    = sizeof(float) * alloc_size;

      bool vert_ok        = realloc_checked((void**)&ca->coords.vertex,
            2 * base_size);
      bool color_ok       = realloc_checked((void**)&ca->coords.color,
            4 * base_size);
      bool tex_ok         = realloc_checked((void**)&ca->coords.tex_coord,
            2 * base_size);
      bool lut_ok         = realloc_checked((void**)&ca->coords.lut_tex_coord,
            2 * base_size);

      if (vert_ok && color_ok && tex_ok && lut_ok)
      {
         ca->allocated = alloc_size;
         success       = true;
      }
   }
   else
      success = true;

   if (!success) /* Allocation failed. */
      return false;

   base_size = count * sizeof(float);
   offset    = ca->coords.vertices;

   /* XXX: I wish we used interlaced arrays so 
    * we could call memcpy only once. */
   memcpy(ca->coords.vertex        + offset * 2,
         coords->vertex, base_size * 2);

   memcpy(ca->coords.color         + offset * 4,
         coords->color, base_size * 4);

   memcpy(ca->coords.tex_coord     + offset * 2,
         coords->tex_coord, base_size * 2);

   memcpy(ca->coords.lut_tex_coord + offset * 2,
         coords->lut_tex_coord, base_size * 2);

   ca->coords.vertices += count;

   return true;
}

void gfx_coord_array_free(gfx_coord_array_t *ca)
{
   if (!ca->allocated)
      return;

   if (ca->coords.vertex)
      free(ca->coords.vertex);
   ca->coords.vertex        = NULL;

   if (ca->coords.color)
      free(ca->coords.color);
   ca->coords.color         = NULL;

   if (ca->coords.tex_coord)
      free(ca->coords.tex_coord);
   ca->coords.tex_coord     = NULL;

   if (ca->coords.lut_tex_coord)
      free(ca->coords.lut_tex_coord);
   ca->coords.lut_tex_coord = NULL;

   ca->coords.vertices      = 0;
   ca->allocated            = 0;
}
