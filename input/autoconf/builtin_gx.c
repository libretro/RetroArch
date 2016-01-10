/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../input_autodetect.h"
#include "builtin.h"

#define GXINPUT_GAMECUBE_DEFAULT_BINDS \
DECL_BTN(a, 0) \
DECL_BTN(b, 1) \
DECL_BTN(x, 2) \
DECL_BTN(y, 3) \
DECL_BTN(start, 4) \
DECL_BTN(select, 6) \
DECL_BTN(up, 9) \
DECL_BTN(down, 10) \
DECL_BTN(left, 11) \
DECL_BTN(right, 12) \
DECL_BTN(l, 7) \
DECL_BTN(r, 8) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

#define GXINPUT_WIIMOTE_DEFAULT_BINDS \
DECL_BTN(a, 31) \
DECL_BTN(b, 30) \
DECL_BTN(x, 29) \
DECL_BTN(y, 28) \
DECL_BTN(start, 32) \
DECL_BTN(select, 33) \
DECL_BTN(up, 35) \
DECL_BTN(down, 36) \
DECL_BTN(left, 37) \
DECL_BTN(right, 38)

#define GXINPUT_NUNCHUK_DEFAULT_BINDS \
DECL_BTN(a, 28) \
DECL_BTN(b, 29) \
DECL_BTN(x, 30) \
DECL_BTN(y, 31) \
DECL_BTN(start, 32) \
DECL_BTN(select, 33) \
DECL_BTN(up, 35) \
DECL_BTN(down, 36) \
DECL_BTN(left, 37) \
DECL_BTN(right, 38) \
DECL_BTN(l, 39) \
DECL_BTN(r, 40) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1)

#define GXINPUT_CLASSIC_DEFAULT_BINDS \
DECL_BTN(a, 13) \
DECL_BTN(b, 14) \
DECL_BTN(x, 15) \
DECL_BTN(y, 16) \
DECL_BTN(start, 17) \
DECL_BTN(select, 18) \
DECL_BTN(up, 24) \
DECL_BTN(down, 25) \
DECL_BTN(left, 26) \
DECL_BTN(right, 27) \
DECL_BTN(l, 20) \
DECL_BTN(r, 21) \
DECL_BTN(l2, 22) \
DECL_BTN(r2, 23) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

const char* const input_builtin_autoconfs[] =
{
   DECL_AUTOCONF_DEVICE("GameCube Controller", "gx", GXINPUT_GAMECUBE_DEFAULT_BINDS),
#ifdef HW_RVL
   DECL_AUTOCONF_DEVICE("Wiimote Controller", "gx", GXINPUT_WIIMOTE_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Nunchuk Controller", "gx", GXINPUT_NUNCHUK_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Classic Controller", "gx", GXINPUT_CLASSIC_DEFAULT_BINDS),
#endif
   NULL
};

