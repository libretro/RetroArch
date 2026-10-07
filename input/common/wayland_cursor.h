/*  RetroArch - A frontend for libretro.
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

#ifndef __WAYLAND_CURSOR_H
#define __WAYLAND_CURSOR_H

#include <stdlib.h>

#include <boolean.h>
#include <retro_inline.h>

/* The size the toolkits fall back to, and the largest a theme ships. */
#define WL_CURSOR_DEFAULT_SIZE 24
#define WL_CURSOR_MAX_SIZE     256

/* The cursor size in surface-local pixels: XCURSOR_SIZE, as the
 * toolkits read it, else their default. */
static INLINE unsigned wl_cursor_size(const char *env)
{
   char *end;
   long size;
   if (!env || !*env)
      return WL_CURSOR_DEFAULT_SIZE;
   size = strtol(env, &end, 10);
   if (*end || size < 1 || size > WL_CURSOR_MAX_SIZE)
      return WL_CURSOR_DEFAULT_SIZE;
   return (unsigned)size;
}

/* The whole scale a cursor buffer is drawn at: the surface's buffer
 * scale or, with fractional scaling, the next whole scale up from
 * @fractional_num / 120 - the compositor shrinks the buffer to size
 * rather than stretching a smaller one.  A cursor surface that cannot
 * take a buffer scale is drawn at 1. */
static INLINE unsigned wl_cursor_scale(bool fractional,
      unsigned buffer_scale, unsigned fractional_num,
      bool can_scale)
{
   unsigned scale = fractional
         ? (fractional_num + 119) / 120
         : buffer_scale;
   if (!can_scale || scale < 1)
      return 1;
   return scale;
}

#endif
