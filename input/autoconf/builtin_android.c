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
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
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

#define SIXAXIS_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 96) \
DECL_BTN(x, 100) \
DECL_BTN(y, 99) \
DECL_BTN(start, 108) \
DECL_BTN(select, 4) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_BTN(l2, 104) \
DECL_BTN(r2, 105) \
DECL_BTN(l3, 106) \
DECL_BTN(r3, 107) \
DECL_AXIS(l_x_plus, +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus, +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus, +3) \
DECL_AXIS(r_y_minus, -3)

#define XBOX360_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 96) \
DECL_BTN(x, 100) \
DECL_BTN(y, 99) \
DECL_BTN(start, 108) \
DECL_BTN(select, 4) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_AXIS(l2, +6) \
DECL_AXIS(r2, +7) \
DECL_BTN(l3, 106) \
DECL_BTN(r3, 107) \
DECL_AXIS(l_x_plus, +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus, +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus, +3) \
DECL_AXIS(r_y_minus, -3)

#define HUIJIA_DEFAULT_BINDS \
DECL_BTN(a, 189) \
DECL_BTN(b, 190) \
DECL_BTN(x, 188) \
DECL_BTN(y, 191) \
DECL_BTN(start, 197) \
DECL_BTN(select, 196) \
DECL_AXIS(up, -1) \
DECL_AXIS(down, +1) \
DECL_AXIS(left, -0) \
DECL_AXIS(right, +0) \
DECL_BTN(l, 194) \
DECL_BTN(r, 195)


#define RUMBLEPAD2_DEFAULT_BINDS \
DECL_BTN(a, 190) \
DECL_BTN(b, 189) \
DECL_BTN(x, 191) \
DECL_BTN(y, 188) \
DECL_BTN(start, 197) \
DECL_BTN(select, 196) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 192) \
DECL_BTN(r, 193) \
DECL_BTN(l2, 194) \
DECL_BTN(r2, 195) \
DECL_BTN(l3, 198) \
DECL_BTN(r3, 199) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

// Some hardcoded autoconfig information. Will be used for pads with no autoconfig cfg files.
const char* const input_builtin_autoconfs[] =
{
   "input_device = \"NVIDIA Shield\" \n"
   "input_driver = \"android\"                    \n"
   SHIELD_DEFAULT_BINDS,

   "input_device = \"RumblePad 2\" \n"
   "input_driver = \"android\"                    \n"
   RUMBLEPAD2_DEFAULT_BINDS,

   "input_device = \"XBox 360\" \n"
   "input_driver = \"android\"                    \n"
   XBOX360_DEFAULT_BINDS,

   "input_device = \"PlayStation3\" \n"
   "input_driver = \"android\"                    \n"
   SIXAXIS_DEFAULT_BINDS,

   "input_device = \"HuiJia\" \n"
   "input_driver = \"android\"                    \n"
   HUIJIA_DEFAULT_BINDS,

   NULL
};
