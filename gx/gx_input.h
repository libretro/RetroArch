/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012 - Michael Lelli
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

#ifndef _GX_INPUT_H
#define _GX_INPUT_H

enum
{
   GX_GC_A                 = 0,
   GX_GC_B                 = 1,
   GX_GC_X                 = 2,
   GX_GC_Y                 = 3,
   GX_GC_START             = 4,
   GX_GC_Z_TRIGGER         = 5,
   GX_GC_L_TRIGGER         = 6,
   GX_GC_R_TRIGGER         = 7,
   GX_GC_UP                = 8,
   GX_GC_DOWN              = 9,
   GX_GC_LEFT              = 10,
   GX_GC_RIGHT             = 11,
#ifdef HW_RVL
   GX_CLASSIC_A            = 20,
   GX_CLASSIC_B            = 21,
   GX_CLASSIC_X            = 22,
   GX_CLASSIC_Y            = 23,
   GX_CLASSIC_PLUS         = 24,
   GX_CLASSIC_MINUS        = 25,
   GX_CLASSIC_HOME         = 26,
   GX_CLASSIC_L_TRIGGER    = 27,
   GX_CLASSIC_R_TRIGGER    = 28,
   GX_CLASSIC_ZL_TRIGGER   = 29,
   GX_CLASSIC_ZR_TRIGGER   = 30,
   GX_CLASSIC_UP           = 31,
   GX_CLASSIC_DOWN         = 32,
   GX_CLASSIC_LEFT         = 33,
   GX_CLASSIC_RIGHT        = 34,
   GX_WIIMOTE_A            = 43,
   GX_WIIMOTE_B            = 44,
   GX_WIIMOTE_1            = 45,
   GX_WIIMOTE_2            = 46,
   GX_WIIMOTE_PLUS         = 47,
   GX_WIIMOTE_MINUS        = 48,
   //GX_WIIMOTE_HOME         = 49,
   GX_WIIMOTE_UP           = 50,
   GX_WIIMOTE_DOWN         = 51,
   GX_WIIMOTE_LEFT         = 52,
   GX_WIIMOTE_RIGHT        = 53,
   GX_NUNCHUK_Z            = 54,
   GX_NUNCHUK_C            = 55,
   GX_NUNCHUK_UP           = 56,
   GX_NUNCHUK_DOWN         = 57,
   GX_NUNCHUK_LEFT         = 58,
   GX_NUNCHUK_RIGHT        = 59,
#endif
   GX_WIIMOTE_HOME         = 49, // needed on GameCube as "fake" menu button
   GX_QUIT_KEY             = 60,

   // special binds for the menu
   GX_MENU_A               = 61,
   GX_MENU_B               = 62,
   GX_MENU_X               = 63,
   GX_MENU_Y               = 64,
   GX_MENU_START           = 65,
   GX_MENU_SELECT          = 66,
   GX_MENU_UP              = 67,
   GX_MENU_DOWN            = 68,
   GX_MENU_LEFT            = 69,
   GX_MENU_RIGHT           = 70,
   GX_MENU_L               = 71,
   GX_MENU_R               = 72,
   GX_MENU_HOME            = 73,

   GX_MENU_FIRST           = GX_MENU_A,
   GX_MENU_LAST            = GX_MENU_HOME
};

enum gx_device_id
{
   GX_DEVICE_GC_ID_JOYPAD_A = 0,
   GX_DEVICE_GC_ID_JOYPAD_B,
   GX_DEVICE_GC_ID_JOYPAD_X,
   GX_DEVICE_GC_ID_JOYPAD_Y,
   GX_DEVICE_GC_ID_JOYPAD_UP,
   GX_DEVICE_GC_ID_JOYPAD_DOWN,
   GX_DEVICE_GC_ID_JOYPAD_LEFT,
   GX_DEVICE_GC_ID_JOYPAD_RIGHT,
   GX_DEVICE_GC_ID_JOYPAD_Z_TRIGGER,
   GX_DEVICE_GC_ID_JOYPAD_START,
   GX_DEVICE_GC_ID_JOYPAD_L_TRIGGER,
   GX_DEVICE_GC_ID_JOYPAD_R_TRIGGER,

#ifdef HW_RVL
   // CLASSIC CONTROLLER
   GX_DEVICE_CLASSIC_ID_JOYPAD_A,
   GX_DEVICE_CLASSIC_ID_JOYPAD_B,
   GX_DEVICE_CLASSIC_ID_JOYPAD_X,
   GX_DEVICE_CLASSIC_ID_JOYPAD_Y,
   GX_DEVICE_CLASSIC_ID_JOYPAD_UP,
   GX_DEVICE_CLASSIC_ID_JOYPAD_DOWN,
   GX_DEVICE_CLASSIC_ID_JOYPAD_LEFT,
   GX_DEVICE_CLASSIC_ID_JOYPAD_RIGHT,
   GX_DEVICE_CLASSIC_ID_JOYPAD_PLUS,
   GX_DEVICE_CLASSIC_ID_JOYPAD_MINUS,
   GX_DEVICE_CLASSIC_ID_JOYPAD_HOME,
   GX_DEVICE_CLASSIC_ID_JOYPAD_L_TRIGGER,
   GX_DEVICE_CLASSIC_ID_JOYPAD_R_TRIGGER,
   GX_DEVICE_CLASSIC_ID_JOYPAD_ZL_TRIGGER,
   GX_DEVICE_CLASSIC_ID_JOYPAD_ZR_TRIGGER,

   // WIIMOTE (PLUS OPTIONAL NUNCHUK)
   GX_DEVICE_WIIMOTE_ID_JOYPAD_A,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_B,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_1,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_2,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_UP,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_DOWN,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_LEFT,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_RIGHT,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_PLUS,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_MINUS,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_HOME,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_Z,
   GX_DEVICE_WIIMOTE_ID_JOYPAD_C,
#endif

   RARCH_LAST_PLATFORM_KEY
};

#endif
