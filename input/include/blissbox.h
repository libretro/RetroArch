/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef __BLISSBOX_H
#define __BLISSBOX_H

#include <retro_common_api.h>

#define BLISSBOX_VID 0x16d0 /* requires firmware 2.0 */
#define BLISSBOX_PID 0x0d04 /* first of 4 controllers, each one increments PID by 1 */
#define BLISSBOX_UPDATE_MODE_PID 0x0a5f
#define BLISSBOX_OLD_PID 0x0a60
#define BLISSBOX_MAX_PADS 4
#define BLISSBOX_MAX_PAD_INDEX (BLISSBOX_MAX_PADS - 1)

#define BLISSBOX_USB_FEATURE_REPORT_ID 17

RETRO_BEGIN_DECLS

typedef struct {
   const char *name;
   int index;
} blissbox_pad_type_t;

const blissbox_pad_type_t blissbox_pad_types[] =
{
   {"A5200", 6},
   {"A5200_TB", 50},
   {"A7800", 4},
   {"ATARI", 0},
   {"ATARI_KEYPAD", 43},
   {"ATMARK", 10},
   {"BALLY", 42},
   {"CD32", 24},
   {"CDI", 33},
   {"COL", 1},
   {"COL_FLASHBACK", 48}, /* 3.0 */
   {"DC_ASCI", 15},
   {"DC_PAD", 16},
   {"DC_TWIN", 35}, /* 3.0 */
   {"FC_ARKANOID", 53},
   {"FC_NES", 52},
   {"GC", 9},
   {"GC_WHEEL", 18},
   {"GEN_3", 20},
   {"GEN_6", 21},
   {"GRAVIS_EX", 38},
   {"HAMMERHEAD", 40},
   {"HPD", 7},
   {"INTELI", 14},
   {"JAG", 11},
   {"MSSW", 39},
   {"N64", 19},
   {"NEO", 49},
   {"NES", 17},
   {"NES_ARKANOID", 30},
   {"NES_GUN", 28},
   {"NES_POWERPAD", 36},
   {"PADDLES", 41},
   {"PC_FX", 26},
   {"PC_GAMEPAD", 46},
   {"PSX_DIGITAL", 65},
   {"PSX_DS", 115},
   {"PSX_DS2", 121},
   {"PSX_FS", 83},
   {"PSX_JOGCON", 227}, /* 3.0 */
   {"PSX_NEGCON", 51},
   {"PSX_WHEEL", 12},
   {"SAC", 34},
   {"SATURN_ANALOG", 8},
   {"SATURN_DIGITAL", 3},
   {"SMS", 22},
   {"SNES", 27},
   {"SNESS_NTT", 47}, /* 3.0 */
   {"SPEEK", 45},
   {"TG16", 23},
   {"TG16_6BUTTON", 54}, /* 3.0 */
   {"THREE_DO", 25},
   {"THREE_DO_ANALOG", 37},
   {"VEC", 5},
   {"V_BOY", 29},
   {"WII_CLASSIC", 31},
   {"WII_DRUM", 55}, /* 3.0 */
   {"WII_MPLUS", 32},
   {"WII_NUNCHUK", 13},
   {"ZXSINC", 44},
   {"gx4000", 2},
   {NULL, 0}, /* used to mark unconnected ports, do not remove */
};

RETRO_END_DECLS

#endif
