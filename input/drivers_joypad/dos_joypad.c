/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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


#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#include "../../config.def.h"

#include "../input_driver.h"
#include "../input_keymaps.h"
#include "../../tasks/tasks_internal.h"
#include "../drivers_keyboard/keyboard_event_dos.h"

static const char *dos_joypad_name(unsigned pad)
{
   return "DOS Controller";
}

static void dos_joypad_autodetect_add(unsigned autoconf_pad)
{
   input_autoconfigure_connect(
         dos_joypad_name(autoconf_pad),
         NULL, NULL,
         dos_joypad.ident,
         autoconf_pad,
         0,
         0
         );
}

/* Joypad functions are no-op now, it all falls back to keyboard. 
 * Actual joystick/gamepad input may be added later. */
static void *dos_joypad_init(void *data)
{
}

static int32_t dos_joypad_button_state(
      uint16_t *buf, uint16_t joykey)
{
   return 0;
}

static int32_t dos_joypad_button(unsigned port_num, uint16_t joykey)
{
   uint16_t *buf = dos_keyboard_state_get();

   if (port_num >= DEFAULT_MAX_PADS)
      return 0;
   return dos_joypad_button_state(buf, joykey);
}

static int16_t dos_joypad_axis(unsigned port_num, uint32_t joyaxis) { return 0; }

static int16_t dos_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   int16_t ret       = 0;
   return ret;
}


static void dos_joypad_poll(void)
{
}

static bool dos_joypad_query_pad(unsigned pad)
{
   return (pad < MAX_USERS);
}

static void dos_joypad_destroy(void)
{
}

input_device_driver_t dos_joypad = {
   dos_joypad_init,
   dos_joypad_query_pad,
   dos_joypad_destroy,
   dos_joypad_button,
   dos_joypad_state,
   NULL,
   dos_joypad_axis,
   dos_joypad_poll,
   NULL, /* set_rumble */
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   dos_joypad_name,
   "dos",
};
