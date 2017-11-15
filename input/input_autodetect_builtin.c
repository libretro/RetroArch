/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <retro_environment.h>

#include "../tasks/tasks_internal.h"
#include "input_driver.h"

#ifdef __QNX__
#include <screen/screen.h>
#endif

#define DECL_BTN(btn, bind) "input_" #btn "_btn = " #bind "\n"
#define DECL_BTN_EX(btn, bind, name) "input_" #btn "_btn = " #bind "\ninput_" #btn "_btn_label = \"" name "\"\n"
#define DECL_AXIS(axis, bind) "input_" #axis "_axis = " #bind "\n"
#define DECL_AXIS_EX(axis, bind, name) "input_" #axis "_axis = " #bind "\ninput_" #axis "_axis_label = \"" name "\"\n"
#define DECL_MENU(btn) "input_menu_toggle_btn = " #btn "\n"
#define DECL_AUTOCONF_DEVICE(device, driver, binds) "input_device = \"" #device "\" \ninput_driver = \"" #driver "\"                    \n" binds

/* TODO/FIXME - Missing L2/R2 */

#define SDL2_DEFAULT_BINDS \
DECL_BTN(a, 1) \
DECL_BTN(b, 0) \
DECL_BTN(x, 3) \
DECL_BTN(y, 2) \
DECL_BTN(start, 6) \
DECL_BTN(select, 4) \
DECL_BTN(up, 11) \
DECL_BTN(down, 12) \
DECL_BTN(left, 13) \
DECL_BTN(right, 14) \
DECL_BTN(l, 9) \
DECL_BTN(r, 10) \
DECL_BTN(l3, 7) \
DECL_BTN(r3, 8) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

#if defined(ANDROID)
#define ANDROID_DEFAULT_BINDS \
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
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)
#endif

#ifdef __QNX__
#define QNX_DEFAULT_BINDS \
DECL_BTN(a, 3) \
DECL_BTN(b, 2 ) \
DECL_BTN(x, 4 ) \
DECL_BTN(y, 1 ) \
DECL_BTN(start, 10) \
DECL_BTN(select, 9 ) \
DECL_MENU(13) \
DECL_BTN(up, 16 ) \
DECL_BTN(down, 17) \
DECL_BTN(left, 18 ) \
DECL_BTN(right, 19 ) \
DECL_BTN(l, 5 ) \
DECL_BTN(r, 6 ) \
DECL_BTN(l2, 7 ) \
DECL_BTN(r2, 8 ) \
DECL_BTN(l3, 11 ) \
DECL_BTN(r3, 12 ) \
DECL_BTN(enable_hotkey, 0) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

#define QNX_DUALSHOCK_BINDS \
DECL_BTN(a, 3) \
DECL_BTN(b, 2 ) \
DECL_BTN(x, 4 ) \
DECL_BTN(y, 1 ) \
DECL_BTN(start, 10) \
DECL_BTN(select, 9 ) \
DECL_MENU(13) \
DECL_BTN(up, 16 ) \
DECL_BTN(down, 17) \
DECL_BTN(left, 18 ) \
DECL_BTN(right, 19 ) \
DECL_BTN(l, 5 ) \
DECL_BTN(r, 6 ) \
DECL_BTN(l2, 7 ) \
DECL_BTN(r2, 8 ) \
DECL_BTN(l3, 11 ) \
DECL_BTN(r3, 12 ) \
DECL_BTN(enable_hotkey, 0) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)
#endif

#define PSPINPUT_DEFAULT_BINDS \
DECL_BTN(a, 8) \
DECL_BTN(b, 0) \
DECL_BTN(x, 9) \
DECL_BTN(y, 1) \
DECL_BTN(start, 3) \
DECL_BTN(select, 2) \
DECL_BTN(up, 4) \
DECL_BTN(down, 5) \
DECL_BTN(left, 6) \
DECL_BTN(right, 7) \
DECL_BTN(l, 10) \
DECL_BTN(r, 11) \
DECL_BTN(l2, 12) \
DECL_BTN(r2, 13) \
DECL_BTN(l3, 14) \
DECL_BTN(r3, 15) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

#define CTRINPUT_DEFAULT_BINDS \
DECL_BTN(a, 8) \
DECL_BTN(b, 0) \
DECL_BTN(x, 9) \
DECL_BTN(y, 1) \
DECL_BTN(start, 3) \
DECL_BTN(select, 2) \
DECL_BTN(up, 4) \
DECL_BTN(down, 5) \
DECL_BTN(left, 6) \
DECL_BTN(right, 7) \
DECL_BTN(l, 10) \
DECL_BTN(r, 11) \
DECL_BTN(l2, 12) \
DECL_BTN(r2, 13) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

#define DOSINPUT_DEFAULT_BINDS \
DECL_BTN(a, 8) \
DECL_BTN(b, 0) \
DECL_BTN(x, 9) \
DECL_BTN(y, 1) \
DECL_BTN(start, 3) \
DECL_BTN(select, 2) \
DECL_BTN(up, 4) \
DECL_BTN(down, 5) \
DECL_BTN(left, 6) \
DECL_BTN(right, 7) \
DECL_BTN(l, 10) \
DECL_BTN(r, 11) \
DECL_BTN(l2, 12) \
DECL_BTN(r2, 13) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

#ifdef WIIU

#define WIIUINPUT_GAMEPAD_DEFAULT_BINDS \
DECL_BTN_EX(menu_toggle,  1, "Home") \
DECL_BTN_EX(select,       2, "-") \
DECL_BTN_EX(start,        3, "+") \
DECL_BTN_EX(r,            4, "R") \
DECL_BTN_EX(l,            5, "L") \
DECL_BTN_EX(r2,           6, "ZR") \
DECL_BTN_EX(l2,           7, "ZL") \
DECL_BTN_EX(down,         8, "D-Pad Down") \
DECL_BTN_EX(up,           9, "D-Pad Up") \
DECL_BTN_EX(right,       10, "D-Pad Right") \
DECL_BTN_EX(left,        11, "D-Pad Left") \
DECL_BTN_EX(y,           12, "Y") \
DECL_BTN_EX(x,           13, "X") \
DECL_BTN_EX(b,           14, "B") \
DECL_BTN_EX(a,           15, "A") \
DECL_BTN_EX(r3,          17, "Right Thumb") \
DECL_BTN_EX(l3,          18, "Left Thumb") \
DECL_AXIS_EX(l_x_plus,   +0, "L-Stick right") \
DECL_AXIS_EX(l_x_minus,  -0, "L-Stick left") \
DECL_AXIS_EX(l_y_minus,  +1, "L-Stick up") \
DECL_AXIS_EX(l_y_plus,   -1, "L-Stick down") \
DECL_AXIS_EX(r_x_plus,   +2, "R-Stick right") \
DECL_AXIS_EX(r_x_minus,  -2, "R-Stick left") \
DECL_AXIS_EX(r_y_minus,  +3, "R-Stick up") \
DECL_AXIS_EX(r_y_plus,   -3, "R-Stick down")

#define WIIUINPUT_PRO_CONTROLLER_DEFAULT_BINDS \
DECL_BTN_EX(up,           0, "D-Pad Up") \
DECL_BTN_EX(left,         1, "D-Pad Left") \
DECL_BTN_EX(r2,           2, "ZR") \
DECL_BTN_EX(x,            3, "X") \
DECL_BTN_EX(a,            4, "A") \
DECL_BTN_EX(y,            5, "Y") \
DECL_BTN_EX(b,            6, "B") \
DECL_BTN_EX(l2,           7, "ZL") \
DECL_BTN_EX(r,            9, "R") \
DECL_BTN_EX(start,       10, "+") \
DECL_BTN_EX(menu_toggle, 11, "Home") \
DECL_BTN_EX(select,      12, "-") \
DECL_BTN_EX(l,           13, "L") \
DECL_BTN_EX(down,        14, "D-Pad Down") \
DECL_BTN_EX(right,       15, "D-Pad Right") \
DECL_BTN_EX(r3,          16, "Right Thumb") \
DECL_BTN_EX(l3,          17, "Left Thumb") \
DECL_AXIS_EX(l_x_plus,   +0, "L-Stick right") \
DECL_AXIS_EX(l_x_minus,  -0, "L-Stick left") \
DECL_AXIS_EX(l_y_minus,  +1, "L-Stick up") \
DECL_AXIS_EX(l_y_plus,   -1, "L-Stick down") \
DECL_AXIS_EX(r_x_plus,   +2, "R-Stick right") \
DECL_AXIS_EX(r_x_minus,  -2, "R-Stick left") \
DECL_AXIS_EX(r_y_minus,  +3, "R-Stick up") \
DECL_AXIS_EX(r_y_plus,   -3, "R-Stick down")

#define WIIUINPUT_CLASSIC_CONTROLLER_DEFAULT_BINDS \
DECL_BTN_EX(up,           0, "D-Pad Up") \
DECL_BTN_EX(left,         1, "D-Pad Left") \
DECL_BTN_EX(r2,           2, "ZR") \
DECL_BTN_EX(x,            3, "X") \
DECL_BTN_EX(a,            4, "A") \
DECL_BTN_EX(y,            5, "Y") \
DECL_BTN_EX(b,            6, "B") \
DECL_BTN_EX(l2,           7, "ZL") \
DECL_BTN_EX(r,            9, "R") \
DECL_BTN_EX(start,       10, "+") \
DECL_BTN_EX(menu_toggle, 11, "Home") \
DECL_BTN_EX(select,      12, "-") \
DECL_BTN_EX(l,           13, "L") \
DECL_BTN_EX(down,        14, "D-Pad Down") \
DECL_BTN_EX(right,       15, "D-Pad Right") \
DECL_AXIS_EX(l_x_plus,   +0, "L-Stick right") \
DECL_AXIS_EX(l_x_minus,  -0, "L-Stick left") \
DECL_AXIS_EX(l_y_minus,  +1, "L-Stick up") \
DECL_AXIS_EX(l_y_plus,   -1, "L-Stick down") \
DECL_AXIS_EX(r_x_plus,   +2, "R-Stick right") \
DECL_AXIS_EX(r_x_minus,  -2, "R-Stick left") \
DECL_AXIS_EX(r_y_minus,  +3, "R-Stick up") \
DECL_AXIS_EX(r_y_plus,   -3, "R-Stick down")

#define WIIUINPUT_WIIMOTE_DEFAULT_BINDS \
DECL_BTN_EX(down,         0, "D-Pad Left") \
DECL_BTN_EX(up,           1, "D-Pad Right") \
DECL_BTN_EX(right,        2, "D-Pad Down") \
DECL_BTN_EX(left,         3, "D-Pad Up") \
DECL_BTN_EX(start,        4, "+") \
DECL_BTN_EX(a,            8, "2") \
DECL_BTN_EX(b,            9, "1") \
DECL_BTN_EX(x,           10, "B") \
DECL_BTN_EX(y,           11, "A") \
DECL_BTN_EX(select,      12, "-") \
DECL_BTN_EX(l,           13, "Z") \
DECL_BTN_EX(r,           14, "C") \
DECL_BTN_EX(menu_toggle, 15, "Home")

#define WIIUINPUT_NUNCHUK_DEFAULT_BINDS \
DECL_BTN_EX(left,         0, "D-Pad Left") \
DECL_BTN_EX(right,        1, "D-Pad Right") \
DECL_BTN_EX(down,         2, "D-Pad Down") \
DECL_BTN_EX(up,           3, "D-Pad Up") \
DECL_BTN_EX(start,        4, "+") \
DECL_BTN_EX(y,            8, "2") \
DECL_BTN_EX(x,            9, "1") \
DECL_BTN_EX(b,           10, "B") \
DECL_BTN_EX(a,           11, "A") \
DECL_BTN_EX(select,      12, "-") \
DECL_BTN_EX(l,           13, "Z") \
DECL_BTN_EX(r,           14, "C") \
DECL_BTN_EX(menu_toggle, 15, "Home") \
DECL_AXIS_EX(l_x_plus,   +0, "Stick Right") \
DECL_AXIS_EX(l_x_minus,  -0, "Stick Left") \
DECL_AXIS_EX(l_y_minus,  +1, "Stick Up") \
DECL_AXIS_EX(l_y_plus,   -1, "Stick Down") \

#endif

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

#define PS3INPUT_DEFAULT_BINDS \
DECL_BTN(a, 8) \
DECL_BTN(b, 0) \
DECL_BTN(x, 9) \
DECL_BTN(y, 1) \
DECL_BTN(start, 3) \
DECL_BTN(select, 2) \
DECL_BTN(up, 4) \
DECL_BTN(down, 5) \
DECL_BTN(left, 6) \
DECL_BTN(right, 7) \
DECL_BTN(l, 10) \
DECL_BTN(r, 11) \
DECL_BTN(l3, 14) \
DECL_BTN(r3, 15) \
DECL_BTN(l2, 12) \
DECL_BTN(r2, 13) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  +1) \
DECL_AXIS(l_y_minus, -1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)

#ifndef _XBOX
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
DECL_MENU(10) \
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
#else
#define XINPUT_DEFAULT_BINDS \
DECL_BTN(a, 8) \
DECL_BTN(b, 0) \
DECL_BTN(x, 9) \
DECL_BTN(y, 1) \
DECL_BTN(start, 3) \
DECL_BTN(select, 2) \
DECL_BTN(up, 4) \
DECL_BTN(down, 5) \
DECL_BTN(left, 6) \
DECL_BTN(right, 7) \
DECL_BTN(l, 10) \
DECL_BTN(r, 11) \
DECL_BTN(l3, 14) \
DECL_BTN(r3, 15) \
DECL_BTN(l2, 12) \
DECL_BTN(r2, 13) \
DECL_AXIS(l_x_plus,  +0) \
DECL_AXIS(l_x_minus, -0) \
DECL_AXIS(l_y_plus,  -1) \
DECL_AXIS(l_y_minus, +1) \
DECL_AXIS(r_x_plus,  +2) \
DECL_AXIS(r_x_minus, -2) \
DECL_AXIS(r_y_plus,  -3) \
DECL_AXIS(r_y_minus, +3)
#endif

const char* const input_builtin_autoconfs[] =
{
#if defined(_WIN32) && defined(_XBOX)
   DECL_AUTOCONF_DEVICE("XInput Controller", "xdk", XINPUT_DEFAULT_BINDS),
#elif defined(_WIN32)
#if !defined(__STDC_C89__) && !defined(__STDC_C89_AMENDMENT_1__)
   DECL_AUTOCONF_DEVICE("XInput Controller (User 1)", "xinput", XINPUT_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("XInput Controller (User 2)", "xinput", XINPUT_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("XInput Controller (User 3)", "xinput", XINPUT_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("XInput Controller (User 4)", "xinput", XINPUT_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("XBOX One Controller (User 1)", "xinput", XINPUT_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("XBOX One Controller (User 2)", "xinput", XINPUT_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("XBOX One Controller (User 3)", "xinput", XINPUT_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("XBOX One Controller (User 4)", "xinput", XINPUT_DEFAULT_BINDS),
#endif
#endif
#ifdef HAVE_SDL2
   DECL_AUTOCONF_DEVICE("Standard Gamepad", "sdl2", SDL2_DEFAULT_BINDS),
#endif
#if defined(ANDROID)
   DECL_AUTOCONF_DEVICE("Android Gamepad", "android", ANDROID_DEFAULT_BINDS),
#endif
#ifdef __QNX__
   DECL_AUTOCONF_DEVICE("QNX Controller", "qnx", QNX_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("DS4 Controller", "qnx", QNX_DUALSHOCK_BINDS),
#endif
#if defined(VITA) || defined(SN_TARGET_PSP2)
   DECL_AUTOCONF_DEVICE("Vita Controller", "vita", PSPINPUT_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("DS3 Controller", "vita", PSPINPUT_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("DS4 Controller", "vita", PSPINPUT_DEFAULT_BINDS),
#elif defined(PSP)
   DECL_AUTOCONF_DEVICE("PSP Controller", "psp", PSPINPUT_DEFAULT_BINDS),
#endif
#ifdef _3DS
   DECL_AUTOCONF_DEVICE("3DS Controller", "ctr", CTRINPUT_DEFAULT_BINDS),
#endif
#ifdef DJGPP
   DECL_AUTOCONF_DEVICE("DOS Controller", "dos", DOSINPUT_DEFAULT_BINDS),
#endif
#ifdef GEKKO
   DECL_AUTOCONF_DEVICE("GameCube Controller", "gx", GXINPUT_GAMECUBE_DEFAULT_BINDS),
#ifdef HW_RVL
   DECL_AUTOCONF_DEVICE("Wiimote Controller", "gx", GXINPUT_WIIMOTE_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Nunchuk Controller", "gx", GXINPUT_NUNCHUK_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Classic Controller", "gx", GXINPUT_CLASSIC_DEFAULT_BINDS),
#endif
#endif
#ifdef WIIU
   DECL_AUTOCONF_DEVICE("WIIU Gamepad", "wiiu", WIIUINPUT_GAMEPAD_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("HID Controller", "wiiu", WIIUINPUT_GAMEPAD_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("WIIU Pro Controller", "wiiu", WIIUINPUT_PRO_CONTROLLER_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Wiimote Controller", "wiiu", WIIUINPUT_WIIMOTE_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Nunchuk Controller", "wiiu", WIIUINPUT_NUNCHUK_DEFAULT_BINDS),
   DECL_AUTOCONF_DEVICE("Classic Controller", "wiiu", WIIUINPUT_CLASSIC_CONTROLLER_DEFAULT_BINDS),
#endif
#ifdef __CELLOS_LV2__
   DECL_AUTOCONF_DEVICE("SixAxis Controller", "ps3", PS3INPUT_DEFAULT_BINDS),
#endif
   NULL
};


