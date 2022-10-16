/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2018 - Francisco Javier Trujillo Mata - fjtrujy
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

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#include "../input_driver.h"

static void ps2_input_free_input(void *data) { }
static void* ps2_input_initialize(const char *a) { return (void*)-1; }
static uint64_t ps2_input_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD)
       |  (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_ps2 = {
   ps2_input_initialize,
   NULL,                         /* poll */
   NULL,                         /* input_state */
   ps2_input_free_input,
   NULL,
   NULL,
   ps2_input_get_capabilities,
   "ps2",
   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
