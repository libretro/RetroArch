/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* A frame turned by quarter turns on the CPU, for the drivers whose
 * display hardware cannot turn it: 16 and 32 bits per pixel. Included
 * by each such driver. */

#ifndef __VIDEO_TURN_H
#define __VIDEO_TURN_H

#include <stddef.h>
#include <stdint.h>

#include <retro_inline.h>

/* Where output row @oy of @w x @h pixels turned @rot quarter turns
 * counter-clockwise starts in the source, and the step to the next
 * pixel of that row, in bytes */
static INLINE const uint8_t *video_turned_row(const uint8_t *src,
      size_t pitch, unsigned w, unsigned h, unsigned bpp, unsigned rot,
      unsigned oy, ptrdiff_t *step)
{
   switch (rot & 3)
   {
      case 1:
         *step = (ptrdiff_t)pitch;
         return src + (size_t)(w - 1 - oy) * bpp;
      case 2:
         *step = -(ptrdiff_t)bpp;
         return src + (size_t)(h - 1 - oy) * pitch + (size_t)(w - 1) * bpp;
      case 3:
         *step = -(ptrdiff_t)pitch;
         return src + (size_t)(h - 1) * pitch + (size_t)oy * bpp;
      default:
         break;
   }
   *step = (ptrdiff_t)bpp;
   return src + (size_t)oy * pitch;
}

/* @w x @h pixels of @src turned @rot quarter turns counter-clockwise
 * into @dst, written in order */
static INLINE void video_turn_copy(uint8_t *dst, size_t stride,
      const uint8_t *src, size_t pitch, unsigned w, unsigned h,
      unsigned bpp, unsigned rot)
{
   unsigned ox, oy;
   unsigned ow = (rot & 1) ? h : w;
   unsigned oh = (rot & 1) ? w : h;

   for (oy = 0; oy < oh; oy++, dst += stride)
   {
      ptrdiff_t step;
      const uint8_t *in = video_turned_row(src, pitch, w, h, bpp, rot, oy,
            &step);
      if (bpp == 4)
      {
         uint32_t *out = (uint32_t*)dst;
         for (ox = 0; ox < ow; ox++, in += step)
            out[ox] = *(const uint32_t*)in;
      }
      else
      {
         uint16_t *out = (uint16_t*)dst;
         for (ox = 0; ox < ow; ox++, in += step)
            out[ox] = *(const uint16_t*)in;
      }
   }
}

#endif
