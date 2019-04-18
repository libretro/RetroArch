/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018      - natinusala
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

#ifndef _OZONE_SIDEBAR_H
#define _OZONE_SIDEBAR_H

#include "ozone_texture.h"

enum
{
   OZONE_SYSTEM_TAB_MAIN = 0,
   OZONE_SYSTEM_TAB_SETTINGS,
   OZONE_SYSTEM_TAB_HISTORY,
   OZONE_SYSTEM_TAB_FAVORITES,
   OZONE_SYSTEM_TAB_MUSIC,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   OZONE_SYSTEM_TAB_VIDEO,
#endif
#ifdef HAVE_IMAGEVIEWER
   OZONE_SYSTEM_TAB_IMAGES,
#endif
#ifdef HAVE_NETWORKING
   OZONE_SYSTEM_TAB_NETPLAY,
#endif
   OZONE_SYSTEM_TAB_ADD,

   /* End of this enum - use the last one to determine num of possible tabs */
   OZONE_SYSTEM_TAB_LAST
};

extern enum msg_hash_enums ozone_system_tabs_value[OZONE_SYSTEM_TAB_LAST];

extern enum menu_settings_type ozone_system_tabs_type[OZONE_SYSTEM_TAB_LAST];

extern enum msg_hash_enums ozone_system_tabs_idx[OZONE_SYSTEM_TAB_LAST];

extern unsigned ozone_system_tabs_icons[OZONE_SYSTEM_TAB_LAST];

#endif
