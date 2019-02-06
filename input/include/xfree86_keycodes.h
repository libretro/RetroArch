/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

/* generated from /usr/share/X11/xkb/keycodes/evdev */

#ifndef __XFREE86_KEYCODES__H
#define __XFREE86_KEYCODES__H

enum xfvk_key
{
   XFVK_UNKNOWN        = 0,
   XFVK_FIRST          = 0,

   XFVK_ESC            = 9,
   XFVK_FK01           = 67,
   XFVK_FK02           = 68,
   XFVK_FK03           = 69,
   XFVK_FK04           = 70,
   XFVK_FK05           = 71,
   XFVK_FK06           = 72,
   XFVK_FK07           = 73,
   XFVK_FK08           = 74,
   XFVK_FK09           = 75,
   XFVK_FK10           = 76,
   XFVK_FK11           = 95,
   XFVK_FK12           = 96,

   /* Added for pc105 compatibility */
   XFVK_LSGT           = 94,
   XFVK_TLDE           = 49,
   XFVK_AE01           = 10,
   XFVK_AE02           = 11,
   XFVK_AE03           = 12,
   XFVK_AE04           = 13,
   XFVK_AE05           = 14,
   XFVK_AE06           = 15,
   XFVK_AE07           = 16,
   XFVK_AE08           = 17,
   XFVK_AE09           = 18,
   XFVK_AE10           = 19,
   XFVK_AE11           = 20,
   XFVK_AE12           = 21,
   XFVK_BKSP           = 22,

   XFVK_TAB            = 23,
   XFVK_AD01           = 24,
   XFVK_AD02           = 25,
   XFVK_AD03           = 26,
   XFVK_AD04           = 27,
   XFVK_AD05           = 28,
   XFVK_AD06           = 29,
   XFVK_AD07           = 30,
   XFVK_AD08           = 31,
   XFVK_AD09           = 32,
   XFVK_AD10           = 33,
   XFVK_AD11           = 34,
   XFVK_AD12           = 35,
   XFVK_BKSL           = 51,
   XFVK_AC12           = XFVK_BKSL,
   XFVK_RTRN           = 36,

   XFVK_CAPS           = 66,
   XFVK_AC01           = 38,
   XFVK_AC02           = 39,
   XFVK_AC03           = 40,
   XFVK_AC04           = 41,
   XFVK_AC05           = 42,
   XFVK_AC06           = 43,
   XFVK_AC07           = 44,
   XFVK_AC08           = 45,
   XFVK_AC09           = 46,
   XFVK_AC10           = 47,
   XFVK_AC11           = 48,

   XFVK_LFSH           = 50,
   XFVK_AB01           = 52,
   XFVK_AB02           = 53,
   XFVK_AB03           = 54,
   XFVK_AB04           = 55,
   XFVK_AB05           = 56,
   XFVK_AB06           = 57,
   XFVK_AB07           = 58,
   XFVK_AB08           = 59,
   XFVK_AB09           = 60,
   XFVK_AB10           = 61,
   XFVK_RTSH           = 62,

   XFVK_LALT           = 64,
   XFVK_LCTL           = 37,
   XFVK_SPCE           = 65,
   XFVK_RCTL           = 105,
   XFVK_RALT           = 108,

   XFVK_PRSC           = 107,
   /* SYRQ                = 107, */
   XFVK_SCLK           = 78,
   XFVK_PAUS           = 127,
   /* BRK                 = 419, */

   XFVK_INS            = 118,
   XFVK_HOME           = 110,
   XFVK_PGUP           = 112,
   XFVK_DELE           = 119,
   XFVK_END            = 115,
   XFVK_PGDN           = 117,

   XFVK_UP             = 111,
   XFVK_LEFT           = 113,
   XFVK_DOWN           = 116,
   XFVK_RGHT           = 114,

   XFVK_NMLK           = 77,
   XFVK_KPDV           = 106,
   XFVK_KPMU           = 63,
   XFVK_KPSU           = 82,

   XFVK_KP7            = 79,
   XFVK_KP8            = 80,
   XFVK_KP9            = 81,
   XFVK_KPAD           = 86,

   XFVK_KP4            = 83,
   XFVK_KP5            = 84,
   XFVK_KP6            = 85,

   XFVK_KP1            = 87,
   XFVK_KP2            = 88,
   XFVK_KP3            = 89,
   XFVK_KPEN           = 104,

   XFVK_KP0            = 90,
   XFVK_KPDL           = 91,
   XFVK_KPEQ           = 125,

   /* Microsoft keyboard extra keys */
   XFVK_LWIN           = 133,
   XFVK_RWIN           = 134,
   XFVK_COMP           = 135,
   XFVK_MENU           = XFVK_COMP,

   /* Extended keys */
   XFVK_CALC           = 148,

   XFVK_FK13           = 191,
   XFVK_FK14           = 192,
   XFVK_FK15           = 193,
   XFVK_FK16           = 194,
   XFVK_FK17           = 195,
   XFVK_FK18           = 196,
   XFVK_FK19           = 197,
   XFVK_FK20           = 198,
   XFVK_FK21           = 199,
   XFVK_FK22           = 200,
   XFVK_FK23           = 201,
   XFVK_FK24           = 202,

   XFVK_LAST,
   XFVK_DUMMY          = 255
};

#endif /* __XFREE86_KEYCODES__H */
