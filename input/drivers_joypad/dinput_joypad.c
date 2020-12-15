/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2020 - Daniel De Matteis
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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <windowsx.h>

#include <dinput.h>
#include <mmsystem.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../tasks/tasks_internal.h"
#include "../input_keymaps.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "dinput_joypad.h"

/* TODO/FIXME - globals referenced outside */
struct dinput_joypad_data g_pads[MAX_USERS];
unsigned g_joypad_cnt;

/* TODO/FIXME - forward declaration */
extern LPDIRECTINPUT8 g_dinput_ctx;

#include "dinput_joypad_inl.h"
#include "dinput_joypad_excl.h"

input_device_driver_t dinput_joypad = {
   dinput_joypad_init,
   dinput_joypad_query_pad,
   dinput_joypad_destroy,
   dinput_joypad_button,
   dinput_joypad_state,
   NULL,
   dinput_joypad_axis,
   dinput_joypad_poll,
   dinput_joypad_set_rumble,
   NULL,
   dinput_joypad_name,
   "dinput",
};
