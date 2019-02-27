/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
 *  copyright (c) 2016-2019 - Brad Parker
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

#ifndef __GDI_COMMON_H
#define __GDI_COMMON_H

#include <retro_environment.h>

typedef struct gdi
{
#ifndef __WINRT__
   WNDCLASSEX wndclass;
#endif
   HDC winDC;
   HDC memDC;
   HDC texDC;
   HBITMAP bmp;
   HBITMAP bmp_old;
   unsigned video_width;
   unsigned video_height;
   unsigned screen_width;
   unsigned screen_height;
   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;
} gdi_t;

typedef struct gdi_texture
{
   int width;
   int height;
   int active_width;
   int active_height;

   enum texture_filter_type type;
   void* data;
   HBITMAP bmp;
   HBITMAP bmp_old;
} gdi_texture_t;

#endif
