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

#include "input_context.h"
#include <string.h>
#include <stdlib.h>
#include "../general.h"

rarch_joypad_driver_t *joypad_drivers[] = {
#ifdef __CELLOS_LV2__
   &ps3_joypad,
#endif
#ifdef HAVE_WINXINPUT
   &winxinput_joypad,
#endif
#ifdef GEKKO
   &gx_joypad,
#endif
#ifdef _XBOX
   &xdk_joypad,
#endif
#ifdef PSP
   &psp_joypad,
#endif
#ifdef HAVE_DINPUT
   &dinput_joypad,
#endif
#ifdef HAVE_UDEV
   &udev_joypad,
#endif
#if defined(__linux) && !defined(ANDROID)
   &linuxraw_joypad,
#endif
#ifdef HAVE_PARPORT
   &parport_joypad,
#endif
#ifdef ANDROID
   &android_joypad,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &sdl_joypad,
#endif
#ifdef __MACH__
#ifdef HAVE_HID
   &apple_hid_joypad,
#endif
#ifdef IOS
   &apple_ios_joypad,
#endif
#endif
#ifdef __QNX__
   &qnx_joypad,
#endif
   NULL,
};

const rarch_joypad_driver_t *input_joypad_init_driver(const char *ident)
{
   unsigned i;
   if (!ident || !*ident)
      return input_joypad_init_first();

   for (i = 0; joypad_drivers[i]; i++)
   {
      if (strcmp(ident, joypad_drivers[i]->ident) == 0
            && joypad_drivers[i]->init())
      {
         RARCH_LOG("Found joypad driver: \"%s\".\n",
               joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return input_joypad_init_first();
}

const rarch_joypad_driver_t *input_joypad_init_first(void)
{
   unsigned i;
   for (i = 0; joypad_drivers[i]; i++)
   {
      if (joypad_drivers[i]->init())
      {
         RARCH_LOG("Found joypad driver: \"%s\".\n",
               joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return NULL;
}
