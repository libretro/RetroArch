/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2020 - Daniel De Matteis
 *  Copyright (C) 2019-2020 - James Leaver
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

#include <libretro.h>

#include "../input_driver.h"

#include "../../verbosity.h"

/* Empty input driver - all functionality
 * is handled in the sdl_dingux joypad driver */

static void* sdl_dingux_input_init(const char *joypad_driver)
{
   return (void*)-1;
}

static void sdl_dingux_input_free_input(void *data)
{
}

static uint64_t sdl_dingux_input_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_sdl_dingux = {
   sdl_dingux_input_init,
   NULL,                              /* poll */
   NULL,                              /* input_state */
   sdl_dingux_input_free_input,
   NULL,                              /* set_sensor_state */
   NULL,                              /* get_sensor_input */
   sdl_dingux_input_get_capabilities,
   "sdl_dingux",
   NULL,                              /* grab_mouse */
   NULL                               /* grab_stdin */
};
