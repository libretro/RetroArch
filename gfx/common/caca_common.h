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

#ifndef __CACA_COMMON_H
#define __CACA_COMMON_H

struct caca_canvas;
struct caca_dither;
struct caca_display;

typedef struct caca_canvas caca_canvas_t;
typedef struct caca_dither caca_dither_t;
typedef struct caca_display caca_display_t;

typedef struct caca
{
   caca_canvas_t **caca_cv;
   caca_dither_t **caca_dither;
   caca_display_t **caca_display;
} caca_t;

#endif
