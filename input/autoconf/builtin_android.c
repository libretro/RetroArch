/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "builtin.h"

#define SHIELD_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 96) \
DECL_BTN(x, 100) \
DECL_BTN(y, 99) \
DECL_BTN(start, 107) \
DECL_BTN(select, 106) \
DECL_AXIS(up, -5) \
DECL_AXIS(down, +5) \
DECL_AXIS(left, -4) \
DECL_AXIS(right, +4) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_AXIS(l2, +6) \
DECL_AXIS(r2, +7) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3) \
"input_menu_toggle_btn = 108\n"

// Some hardcoded autoconfig information. Will be used for pads with no autoconfig cfg files.
const char* const input_builtin_autoconfs[] =
{
   "input_device = \"NVIDIA Shield\" \n"
   "input_driver = \"android\"                    \n"
   SHIELD_DEFAULT_BINDS,

   NULL
};

