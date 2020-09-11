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

#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef _XBOX
#include <xtl.h>
#endif

#include <boolean.h>
#include <libretro.h>

#include "../../config.def.h"

#include "../input_driver.h"

static void xdk_input_free_input(void *data) { }
static void *xdk_input_init(const char *a) { return (void*)-1; }

static uint64_t xdk_input_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_xinput = {
   xdk_input_init,
   NULL,                         /* poll */
   NULL,                         /* input_state */
   xdk_input_free_input,
   NULL,
   NULL,
   xdk_input_get_capabilities,
   "xinput",
   NULL,                         /* grab_mouse */
   NULL
};
