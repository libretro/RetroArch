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
DECL_AXIS(r_y_plus,  +3) \
DECL_AXIS(r_y_minus, -3) \
DECL_MENU(108)

// TODO
// Verify if stick works
// Verify whether or not hats can go or
// analog stick
#define TTT_HT_PLAYER1_DEFAULT_BINDS \
DECL_BTN(a, 193) \
DECL_BTN(b, 192) \
DECL_BTN(x, 189) \
DECL_BTN(y, 188) \
DECL_BTN(start, 196) \
DECL_BTN(select, 195) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 190) \
DECL_BTN(r, 194) \
DECL_BTN(l2, 194) \
DECL_BTN(r2, 191) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1)

// TODO
// Verify if stick works
#define TTT_HT_PLAYER2_DEFAULT_BINDS \
DECL_BTN(a, 200) \
DECL_BTN(b, 199) \
DECL_BTN(x, 201) \
DECL_BTN(y, 198) \
DECL_BTN(start, 99) \
DECL_BTN(select, 98) \
DECL_BTN(up, 101) \
DECL_BTN(down, 100) \
DECL_BTN(left, 98) \
DECL_BTN(right, 99) \
DECL_BTN(l, 202) \
DECL_BTN(r, 203) \
DECL_BTN(l2, 96) \
DECL_BTN(r2, 97) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1)

// TODO
// Verify buttons
#define IDROID_X360_DEFAULT_BINDS \
DECL_BTN(a, 69) \
DECL_BTN(b, 124) \
DECL_BTN(x, 76) \
DECL_BTN(y, 123) \
DECL_BTN(start, 126) \
DECL_BTN(select, 70) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 71) \
DECL_BTN(r, 112) \
DECL_AXIS(l2, +6) \
DECL_AXIS(r2, +7) \
DECL_BTN(l_x_plus,  150) \
DECL_BTN(l_x_minus, 148) \
DECL_BTN(l_y_plus,  152) \
DECL_BTN(l_y_minus, 146) \
DECL_BTN(r_x_plus,  113) \
DECL_BTN(r_x_minus, 72) \
DECL_BTN(r_y_plus,  92) \
DECL_BTN(r_y_minus, 93) \
DECL_MENU(82)

// TODO
// Add menu button (?)
// Verify if sticks work
#define NYKO_PLAYPAD_PRO_DEFAULT_BINDS \
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
DECL_BTN(l3, 106) \
DECL_BTN(r3, 107) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

// TODO
// Add menu button
// Verify if sticks work
#define MAYFLASH_WII_CLASSIC_DEFAULT_BINDS \
DECL_BTN(a, 188) \
DECL_BTN(b, 189) \
DECL_BTN(x, 190) \
DECL_BTN(y, 191) \
DECL_BTN(start, 197) \
DECL_BTN(select, 196) \
DECL_BTN(up, 199) \
DECL_BTN(down, 201) \
DECL_BTN(left, 200) \
DECL_BTN(right, 202) \
DECL_BTN(l, 192) \
DECL_BTN(r, 193) \
DECL_BTN(l2, 194) \
DECL_BTN(r2, 195) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

// TODO
// Add menu button
// Verify if sticks work
#define SZMY_POWER_DUAL_BOX_WII_CLASSIC_DEFAULT_BINDS \
DECL_BTN(a, 189) \
DECL_BTN(b, 190) \
DECL_BTN(x, 188) \
DECL_BTN(y, 191) \
DECL_BTN(start, 197) \
DECL_BTN(select, 196) \
DECL_BTN(up, 200) \
DECL_BTN(down, 202) \
DECL_BTN(left, 203) \
DECL_BTN(right, 201) \
DECL_BTN(l, 192) \
DECL_BTN(r, 193) \
DECL_BTN(l2, 194) \
DECL_BTN(r2, 195) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

#define LOGITECH_PRECISION_DEFAULT_BINDS \
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
DECL_BTN(r2, 195)

// TODO
// Verify if stick works
#define SEGA_VIRTUA_STICK_DEFAULT_BINDS \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(y, 96) \
DECL_BTN(x, 99) \
DECL_BTN(b, 97) \
DECL_BTN(a, 98) \
DECL_BTN(r, 101) \
DECL_BTN(r2, 103) \
DECL_BTN(l, 100) \
DECL_BTN(l2, 102) \
DECL_BTN(start, 105) \
DECL_BTN(select, 110) \
DECL_MENU(104)

// TODO
// - D-pad - verify if it works
#define SUPER_SMARTJOY2_DEFAULT_BINDS \
DECL_BTN(a, 189) \
DECL_BTN(b, 190) \
DECL_BTN(x, 188) \
DECL_BTN(y, 191) \
DECL_BTN(select, 192) \
DECL_BTN(start, 193) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 194) \
DECL_BTN(r, 195)

#define FC30_GAMEPAD_DEFAULT_BINDS \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(a, 96) \
DECL_BTN(b, 97) \
DECL_BTN(x, 99) \
DECL_BTN(y, 100) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_BTN(select, 98) \
DECL_BTN(start, 108)

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
DECL_MENU(82)

// TODO
// - Analog sticks - verify if they work
#define MUCH_IREADYGO_I5_DEFAULT_BINDS \
DECL_BTN(a, 97) \
DECL_BTN(b, 23) \
DECL_BTN(x, 100) \
DECL_BTN(y, 99) \
DECL_BTN(up, 19) \
DECL_BTN(down, 20) \
DECL_BTN(left, 21) \
DECL_BTN(right, 22) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

// TODO
// - Analog sticks - verify if they work
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
DECL_BTN(r3, 107) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

// TODO
// - Analog sticks - verify if they work
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
DECL_BTN(r2, 105) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

// TODO
// - Analog sticks - verify if they work
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
DECL_BTN(r, 103) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

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
DECL_MENU(82)

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
DECL_MENU(196)

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

#define PS2_JCPS102_DEFAULT_BINDS \
DECL_BTN(a, 189) \
DECL_BTN(b, 190) \
DECL_BTN(x, 188) \
DECL_BTN(y, 191) \
DECL_BTN(start, 196) \
DECL_BTN(select, 197) \
DECL_BTN(up, 200) \
DECL_BTN(down, 202) \
DECL_BTN(left, 203) \
DECL_BTN(right, 201) \
DECL_BTN(l, 194) \
DECL_BTN(r, 195) \
DECL_BTN(l2, 195) \
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

#define PS2_GENERIC_DEFAULT_BINDS \
DECL_BTN(a, 189) \
DECL_BTN(b, 190) \
DECL_BTN(x, 188) \
DECL_BTN(y, 191) \
DECL_BTN(start, 196) \
DECL_BTN(select, 197) \
DECL_BTN(up, 200) \
DECL_BTN(down, 202) \
DECL_BTN(left, 203) \
DECL_BTN(right, 201) \
DECL_BTN(l, 194) \
DECL_BTN(r, 192) \
DECL_BTN(l2, 195) \
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

#define XBOX360_WIRELESS_DEFAULT_BINDS \
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
DECL_AXIS(r_y_minus, -3) \
DECL_MENU(82)

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

// TODO
// - Analog sticks - verify if they work
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
DECL_BTN(r, 103) \
DECL_AXIS(l_x_plus, +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus, +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus, +3) \
DECL_AXIS(r_y_minus, -3)

//TODO
// - Analog sticks - verify if they work
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
DECL_BTN(r2, 105) \
DECL_AXIS(l_x_plus, +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus, +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus, +3) \
DECL_AXIS(r_y_minus, -3)

// TODO
// Verify if Dpad works
// Verify if sticks work
#define LOGICOOL_RUMBLEPAD2_DEFAULT_BINDS \
DECL_BTN(a, 98) \
DECL_BTN(b, 97) \
DECL_BTN(x, 99) \
DECL_BTN(y, 96) \
DECL_BTN(start, 105) \
DECL_BTN(select, 104) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 100) \
DECL_BTN(r, 101) \
DECL_BTN(l2, 102) \
DECL_BTN(r2, 103) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

// TODO
// Verify if Dpad works
// Verify if sticks work
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

// TODO
// Verify if Dpad works
// Verify if sticks work
#define LOGITECH_DUAL_ACTION_DEFAULT_BINDS \
DECL_BTN(a, 100) \
DECL_BTN(b, 99) \
DECL_BTN(x, 97) \
DECL_BTN(y, 96) \
DECL_BTN(start, 108) \
DECL_BTN(select, 109) \
DECL_BTN(up, h0up) \
DECL_BTN(down, h0down) \
DECL_BTN(left, h0left) \
DECL_BTN(right, h0right) \
DECL_BTN(l, 102) \
DECL_BTN(r, 103) \
DECL_BTN(l2, 104) \
DECL_BTN(r2, 105) \
DECL_BTN(l3, 106) \
DECL_BTN(r3, 107) \
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
   DECL_AUTOCONF_DEVICE("NVIDIA Shield", "android", SHIELD_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Xperia Play", "android", ZEUS_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("JXD S5110B", "android", JXD_S5110B_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Mayflash Wii Classic", "android", MAYFLASH_WII_CLASSIC_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("SZMy Power Dual Box Wii", "android", SZMY_POWER_DUAL_BOX_WII_CLASSIC_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("TTT THT Arcade (Player 1)", "android", TTT_HT_PLAYER1_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("TTT THT Arcade (Player 2)", "android", TTT_HT_PLAYER2_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("JXD S7300B", "android", JXD_S7300B_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("JXD S5110B (Skelrom)", "android", JXD_S5110B_SKELROM_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("JXD S7800B", "android", JXD_S7800B_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Archos Gamepad", "android", ARCHOS_GAMEPAD_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Wikipad", "android", WIKIPAD_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("TOMMO Neo-Geo X", "android", TOMMO_NEOGEOX_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Logitech Precision", "android", LOGITECH_PRECISION_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Logitech Dual Action", "android", LOGITECH_DUAL_ACTION_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("iDroid x360", "android", IDROID_X360_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Samsung Gamepad EI-GP20", "android", SAMSUNG_EIGP20_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("GameMID", "android", GAMEMID_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("OUYA", "android", OUYA_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Logicool RumblePad 2", "android", LOGICOOL_RUMBLEPAD2_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("RumblePad 2", "android", RUMBLEPAD2_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("SideWinder Dual Strike", "android", SIDEWINDER_DUAL_STRIKE_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("SideWinder Classic", "android", SIDEWINDER_CLASSIC_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("MUCH iReadyGo i5", "android", MUCH_IREADYGO_I5_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("XBox 360", "android", XBOX360_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("XBox 360 Wireless", "android", XBOX360_WIRELESS_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Sega Virtua Stick", "android", SEGA_VIRTUA_STICK_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("PlayStation2 JCPS102", "android", PS2_JCPS102_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("PlayStation2 Generic", "android", PS2_GENERIC_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("PlayStation2 WiseGroup", "android", PS2_WISEGROUP_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("PlayStation3", "android", PS3_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("HuiJia", "android", HUIJIA_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Super Smartjoy 2", "android", SUPER_SMARTJOY2_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Nyko Playpad Pro", "android", NYKO_PLAYPAD_PRO_DEFAULT_BINDS),
   NULL
};
