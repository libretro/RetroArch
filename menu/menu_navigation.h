/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef _MENU_NAVIGATION_H
#define _MENU_NAVIGATION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum menu_navigation_ctl_state
{
   MENU_NAVIGATION_CTL_CLEAR = 0,
   MENU_NAVIGATION_CTL_DEINIT,
   MENU_NAVIGATION_CTL_INCREMENT,
   MENU_NAVIGATION_CTL_DECREMENT,
   MENU_NAVIGATION_CTL_SET,
   MENU_NAVIGATION_CTL_SET_LAST,
   MENU_NAVIGATION_CTL_DESCEND_ALPHABET,
   MENU_NAVIGATION_CTL_ASCEND_ALPHABET,
   MENU_NAVIGATION_CTL_SET_SELECTION,
   MENU_NAVIGATION_CTL_GET_SELECTION,
   MENU_NAVIGATION_CTL_CLEAR_SCROLL_INDICES,
   MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX,
   MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL,
   MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL
};

bool menu_navigation_ctl(enum menu_navigation_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif
