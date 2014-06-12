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

#define ZEUS_DEFAULT_BINDS \
DECL_BTN(a, 4) \
DECL_BTN(b, 23) \
DECL_BTN(x, 100) \
DECL_BTN(y, 99) \
DECL_BTN(start, 108) \
DECL_BTN(select, 109) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
"input_menu_toggle_btn = 82\n"

// TODO
// - Analog sticks
// - L2 / R2 buttons
#define WIKIPAD_DEFAULT_BINDS \
DECL_BTN(a, 96) \
DECL_BTN(b, 97) \
DECL_BTN(x, 99) \
DECL_BTN(y, 100) \
DECL_BTN(start, 108) \
DECL_BTN(select, 109) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_BTN(l3, 106) \
DECL_BTN(r3, 107)

#define ARCHOS_GAMEPAD_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 96) \
DECL_BTN(x, 100) \
DECL_BTN(y, 99) \
DECL_BTN(start, 108) \
DECL_BTN(select, 109) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_BTN(l2, 104) \
DECL_BTN(r2, 105)

#define SAMSUNG_EIGP20_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 96) \
DECL_BTN(x, 99) \
DECL_BTN(y, 100) \
DECL_BTN(start, 108) \
DECL_BTN(select, 109) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103)

#define OUYA_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 96) \
DECL_BTN(x, 100) \
DECL_BTN(y, 99) \
DECL_BTN(start, 107) \
DECL_BTN(select, 106) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 104) \
DECL_BTN(l2, 104) \
DECL_BTN(r2, 105) \
DECL_AXIS(l_x_plus, +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus, +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus, +3) \
DECL_AXIS(r_y_minus, -3) \
"input_menu_toggle_btn = 82\n"

#define SIDEWINDER_DUAL_STRIKE_DEFAULT_BINDS \
DECL_BTN(a, 190) \
DECL_BTN(b, 191) \
DECL_BTN(x, 188) \
DECL_BTN(y, 189) \
DECL_BTN(start, 192) \
DECL_BTN(select, 193) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 194) \
DECL_BTN(r, 195) \
DECL_AXIS(r_x_plus,  +0) \
DECL_AXIS(r_x_minus, -0) \
DECL_AXIS(r_y_plus, -1) \
DECL_AXIS(r_y_minus, +1) \
"input_menu_toggle_btn = 196\n"

#define SIDEWINDER_CLASSIC_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 96) \
DECL_BTN(x, 99) \
DECL_BTN(y, 100) \
DECL_BTN(start, 104) \
DECL_BTN(select, 105) \
DECL_AXIS(up, -1) \
DECL_AXIS(down, +1) \
DECL_AXIS(left, -0) \
DECL_AXIS(right, +0) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_BTN(l2, 101) \
DECL_BTN(r2, 98)

#define PS2_WISEGROUP_DEFAULT_BINDS \
DECL_BTN(a, 189) \
DECL_BTN(b, 190) \
DECL_BTN(x, 188) \
DECL_BTN(y, 191) \
DECL_BTN(start, 196) \
DECL_BTN(select, 197) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 194) \
DECL_BTN(r, 195) \
DECL_BTN(l2, 192) \
DECL_BTN(r2, 193) \
DECL_BTN(l3, 198) \
DECL_BTN(r3, 199) \
DECL_AXIS(l_x_plus, +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus, +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus, +3) \
DECL_AXIS(r_y_minus, -3)

#define GAMEMID_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 96) \
DECL_BTN(x, 100) \
DECL_BTN(y, 99) \
DECL_BTN(start, 108) \
DECL_BTN(select, 109) \
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

#define PS3_DEFAULT_BINDS \
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

#define TOMMO_NEOGEOX_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 96) \
DECL_BTN(x, 98) \
DECL_BTN(y, 99) \
DECL_BTN(start, 105) \
DECL_BTN(select, 104) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22)

//TODO
// - Analog sticks (?)
#define JXD_S5110B_DEFAULT_BINDS \
DECL_BTN(a, 96) \
DECL_BTN(b, 97) \
DECL_BTN(x, 99) \
DECL_BTN(y, 100) \
DECL_BTN(start, 62) \
DECL_BTN(select, 66) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103)

//TODO
// - Analog sticks (?)
#define JXD_S5110B_SKELROM_DEFAULT_BINDS \
DECL_BTN(a, 96) \
DECL_BTN(b, 97) \
DECL_BTN(x, 99) \
DECL_BTN(y, 100) \
DECL_BTN(start, 108) \
DECL_BTN(select, 109) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103)

//TODO
// - Analog sticks (?)
#define JXD_S7300B_DEFAULT_BINDS \
DECL_BTN(a, 96) \
DECL_BTN(b, 97) \
DECL_BTN(x, 99) \
DECL_BTN(y, 100) \
DECL_BTN(start, 66) \
DECL_BTN(select, 62) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103)

//TODO
// - Analog sticks
#define JXD_S7800B_DEFAULT_BINDS \
DECL_BTN(a, 96) \
DECL_BTN(b, 97) \
DECL_BTN(x, 99) \
DECL_BTN(y, 100) \
DECL_BTN(start, 108) \
DECL_BTN(select, 109) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_BTN(l2, 104) \
DECL_BTN(r2, 105)

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

   "input_device = \"Xperia Play\" \n"
   "input_driver = \"android\"                    \n"
   ZEUS_DEFAULT_BINDS,

   "input_device = \"JXD S5110B\" \n"
   "input_driver = \"android\"                    \n"
   JXD_S5110B_DEFAULT_BINDS,

   "input_device = \"JXD S7300B\" \n"
   "input_driver = \"android\"                    \n"
   JXD_S7300B_DEFAULT_BINDS,

   "input_device = \"JXD S5110B (Skelrom)\" \n"
   "input_driver = \"android\"                    \n"
   JXD_S5110B_SKELROM_DEFAULT_BINDS,

   "input_device = \"JXD S7800B\" \n"
   "input_driver = \"android\"                    \n"
   JXD_S7800B_DEFAULT_BINDS,

   "input_device = \"Archos Gamepad\" \n"
   "input_driver = \"android\"                    \n"
   ARCHOS_GAMEPAD_DEFAULT_BINDS,

   "input_device = \"Wikipad\" \n"
   "input_driver = \"android\"                    \n"
   WIKIPAD_DEFAULT_BINDS,

   "input_device = \"TOMMO Neo-Geo X\" \n"
   "input_driver = \"android\"                    \n"
   TOMMO_NEOGEOX_DEFAULT_BINDS,

   "input_device = \"Samsung Gamepad EI-GP20\" \n"
   "input_driver = \"android\"                    \n"
   SAMSUNG_EIGP20_DEFAULT_BINDS,

   "input_device = \"GameMID\" \n"
   "input_driver = \"android\"                    \n"
   GAMEMID_DEFAULT_BINDS,

   "input_device = \"OUYA\" \n"
   "input_driver = \"android\"                    \n"
   OUYA_DEFAULT_BINDS,

   "input_device = \"RumblePad 2\" \n"
   "input_driver = \"android\"                    \n"
   RUMBLEPAD2_DEFAULT_BINDS,

   "input_device = \"SideWinder Dual Strike\" \n"
   "input_driver = \"android\"                    \n"
   SIDEWINDER_DUAL_STRIKE_DEFAULT_BINDS,

   "input_device = \"SideWinder Classic\" \n"
   "input_driver = \"android\"                    \n"
   SIDEWINDER_CLASSIC_DEFAULT_BINDS,

   "input_device = \"XBox 360\" \n"
   "input_driver = \"android\"                    \n"
   XBOX360_DEFAULT_BINDS,

   "input_device = \"PlayStation2 WiseGroup\" \n"
   "input_driver = \"android\"                    \n"
   PS2_WISEGROUP_DEFAULT_BINDS,

   "input_device = \"PlayStation3\" \n"
   "input_driver = \"android\"                    \n"
   PS3_DEFAULT_BINDS,

   "input_device = \"HuiJia\" \n"
   "input_driver = \"android\"                    \n"
   HUIJIA_DEFAULT_BINDS,

   NULL
};
