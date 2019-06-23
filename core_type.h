/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef __CORE_TYPE_H
#define __CORE_TYPE_H

enum rarch_core_type
{
   CORE_TYPE_PLAIN = 0,
   CORE_TYPE_DUMMY,
   CORE_TYPE_FFMPEG,
   CORE_TYPE_MPV,
   CORE_TYPE_IMAGEVIEWER,
   CORE_TYPE_NETRETROPAD,
   CORE_TYPE_VIDEO_PROCESSOR,
   CORE_TYPE_GONG
};

#endif
