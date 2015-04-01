/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

static const char *ctr_joypad_name(unsigned pad)
{
   return "ctr";
}

static bool ctr_joypad_init(void)
{
   return true;
}

static bool ctr_joypad_button(unsigned port_num, uint16_t joykey)
{
   return false;
}

static uint64_t ctr_joypad_get_buttons(unsigned port_num)
{
   return 0;
}

static int16_t ctr_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   return 0;
}

static void ctr_joypad_poll(void)
{
}

static bool ctr_joypad_query_pad(unsigned pad)
{
   return true;
}


static void ctr_joypad_destroy(void)
{
}

rarch_joypad_driver_t ctr_joypad = {
   ctr_joypad_init,
   ctr_joypad_query_pad,
   ctr_joypad_destroy,
   ctr_joypad_button,
   ctr_joypad_get_buttons,
   ctr_joypad_axis,
   ctr_joypad_poll,
   NULL,
   ctr_joypad_name,
   "ctr",
};
