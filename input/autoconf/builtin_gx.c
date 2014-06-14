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

#define GXINPUT_GAMECUBE_DEFAULT_BINDS \
DECL_BTN(a, 0) \
DECL_BTN(b, 1) \
DECL_BTN(x, 2) \
DECL_BTN(y, 3) \
DECL_BTN(start, 4) \
DECL_BTN(select, 5) \
DECL_BTN(up, 8) \
DECL_BTN(down, 9) \
DECL_BTN(left, 10) \
DECL_BTN(right, 11) \
DECL_BTN(l, 6) \
DECL_BTN(r, 7) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

#define GXINPUT_WIIMOTE_DEFAULT_BINDS \
DECL_BTN(a, 46) \
DECL_BTN(b, 45) \
DECL_BTN(x, 45) \
DECL_BTN(y, 43) \
DECL_BTN(start, 47) \
DECL_BTN(select, 48) \
DECL_BTN(up, 50) \
DECL_BTN(down, 51) \
DECL_BTN(left, 52) \
DECL_BTN(right, 53)

#define GXINPUT_NUNCHUK_DEFAULT_BINDS \
DECL_BTN(a, 43) \
DECL_BTN(b, 44) \
DECL_BTN(x, 45) \
DECL_BTN(y, 46) \
DECL_BTN(start, 47) \
DECL_BTN(select, 48) \
DECL_BTN(up, 50) \
DECL_BTN(down, 51) \
DECL_BTN(left, 52) \
DECL_BTN(right, 53) \
DECL_BTN(l, 54) \
DECL_BTN(r, 55) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1)

#define GXINPUT_CLASSIC_DEFAULT_BINDS \
DECL_BTN(a, 20) \
DECL_BTN(b, 21) \
DECL_BTN(x, 22) \
DECL_BTN(y, 23) \
DECL_BTN(start, 24) \
DECL_BTN(select, 25) \
DECL_BTN(up, 31) \
DECL_BTN(down, 32) \
DECL_BTN(left, 33) \
DECL_BTN(right, 34) \
DECL_BTN(l, 27) \
DECL_BTN(r, 28) \
DECL_BTN(l2, 29) \
DECL_BTN(r2, 30) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

// TODO: missing Libsicksaxis binds

// Some hardcoded autoconfig information. Will be used for pads with no autoconfig cfg files.
const char* const input_builtin_autoconfs[] =
{
   DECL_AUTOCONF_DEVICE("GameCube Controller", "gx", GXINPUT_GAMECUBE_DEFAULT_BINDS),
#ifdef HW_RVL
   DECL_AUTOCONF_DEVICE("Wiimote Controller", "gx", GXINPUT_WIIMOTE_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("NunChuk Controller", "gx", GXINPUT_NUNCHUK_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Classic Controller", "gx", GXINPUT_CLASSIC_DEFAULT_BINDS),
#endif
   NULL
};

