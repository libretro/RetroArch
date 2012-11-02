/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef _ANDROID_GENERAL_H
#define _ANDROID_GENERAL_H

#include "android_glue.h"
#include "../../../boolean.h"

struct droid
{
   struct android_app* app;
   uint64_t input_state;
};

enum {
   ANDROID_STATE_QUIT =             1 << 0,
   ANDROID_STATE_KILL =             1 << 1,
   ANDROID_STATE_VOLUME_UP =        1 << 2,
   ANDROID_STATE_VOLUME_DOWN =      1 << 3,
   ANDROID_WINDOW_READY =           1 << 4,
};

extern struct droid g_android;

#endif
