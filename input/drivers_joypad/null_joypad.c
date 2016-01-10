/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - CatalystG
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
#include <stddef.h>
#include <boolean.h>
#include "../input_joypad_driver.h"

static const char *null_joypad_name(unsigned pad)
{
   return "null";
}

static bool null_joypad_init(void *data)
{
   (void)data;
   return true;
}

static bool null_joypad_button(unsigned port_num, uint16_t joykey)
{
   return false;
}

static uint64_t null_joypad_get_buttons(unsigned port_num)
{
   return 0;
}

static int16_t null_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   return 0;
}

static void null_joypad_poll(void)
{
}

static bool null_joypad_query_pad(unsigned pad)
{
   return true;
}


static void null_joypad_destroy(void)
{
}

input_device_driver_t null_joypad = {
   null_joypad_init,
   null_joypad_query_pad,
   null_joypad_destroy,
   null_joypad_button,
   null_joypad_get_buttons,
   null_joypad_axis,
   null_joypad_poll,
   NULL,
   null_joypad_name,
   "null",
};
