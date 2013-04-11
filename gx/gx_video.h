/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012 - Michael Lelli
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

#ifndef _GX_VIDEO_H__
#define _GX_VIDEO_H__

typedef struct gx_video
{
   bool should_resize;
   bool keep_aspect;
   bool double_strike;
   bool rgb32;
   uint32_t *menu_data; // FIXME: Should be const uint16_t*.
   bool rgui_texture_enable;
   rarch_viewport_t vp;
   unsigned scale;
} gx_video_t;

void gx_set_video_mode(unsigned fbWidth, unsigned lines);
const char *gx_get_video_mode(void);

#endif

