/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - pinumbernumber
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

#define XINPUT_DEFAULT_BINDS \
DECL_BTN(a, 1) \
DECL_BTN(b, 0) \
DECL_BTN(x, 3) \
DECL_BTN(y, 2) \
DECL_BTN(start, 6) \
DECL_BTN(select, 7) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 4) \
DECL_BTN(r, 5) \
DECL_BTN(l3, 8) \
DECL_BTN(r3, 9) \
DECL_BTN(menu_toggle, 10) \
DECL_AXIS(l2, +4) \
DECL_AXIS(r2, +5) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  -1) \
DECL_AXIS(l_y_minus, +1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

// Some hardcoded autoconfig information. Will be used for pads with no autoconfig cfg files.
const char* const input_builtin_autoconfs[] =
{
   "input_device = \"XInput Controller (Player 1)\" \n"
   "input_driver = \"winxinput\"                    \n"
   XINPUT_DEFAULT_BINDS,

   "input_device = \"XInput Controller (Player 2)\" \n"
   "input_driver = \"winxinput\"                    \n"
   XINPUT_DEFAULT_BINDS,

   "input_device = \"XInput Controller (Player 3)\" \n"
   "input_driver = \"winxinput\"                    \n"
   XINPUT_DEFAULT_BINDS,

   "input_device = \"XInput Controller (Player 4)\" \n"
   "input_driver = \"winxinput\"                    \n"
   XINPUT_DEFAULT_BINDS,

   NULL
};

