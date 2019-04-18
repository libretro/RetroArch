/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef XINERAMA_COMMON_H__
#define XINERAMA_COMMON_H__

#include <boolean.h>

#include "x11_common.h"

void xinerama_save_last_used_monitor(Window win);

bool xinerama_get_coord(Display *dpy, int screen,
      int *x, int *y, unsigned *w, unsigned *h);

unsigned xinerama_get_monitor(Display *dpy,
      int x, int y, int w, int h);

#endif
