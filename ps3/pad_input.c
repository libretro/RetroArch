/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cell/pad.h>
#include <sdk_version.h>
#include "pad_input.h"

#define LOWER_BUTTONS 2
#define HIGHER_BUTTONS 3
#define RSTICK_X 4
#define RSTICK_Y 5
#define LSTICK_X 6
#define LSTICK_Y 7

#define DEADZONE_LOW 55
#define DEADZONE_HIGH 210

#define PRESSED_LEFT_LSTICK(state)   (CTRL_AXIS_LSTICK_X(state) <= DEADZONE_LOW)
#define PRESSED_RIGHT_LSTICK(state)  (CTRL_AXIS_LSTICK_X(state) >= DEADZONE_HIGH)
#define PRESSED_UP_LSTICK(state)     (CTRL_AXIS_LSTICK_Y(state) <= DEADZONE_LOW)
#define PRESSED_DOWN_LSTICK(state)   (CTRL_AXIS_LSTICK_Y(state) >= DEADZONE_HIGH)
#define PRESSED_LEFT_RSTICK(state)   (CTRL_AXIS_RSTICK_X(state) <= DEADZONE_LOW)
#define PRESSED_RIGHT_RSTICK(state)  (CTRL_AXIS_RSTICK_X(state) >= DEADZONE_HIGH)
#define PRESSED_UP_RSTICK(state)     (CTRL_AXIS_RSTICK_Y(state) <= DEADZONE_LOW)
#define PRESSED_DOWN_RSTICK(state)   (CTRL_AXIS_RSTICK_Y(state) >= DEADZONE_HIGH)

#define LSTICK_LEFT_SHIFT 48
#define LSTICK_RIGHT_SHIFT 49
#define LSTICK_UP_SHIFT 50
#define LSTICK_DOWN_SHIFT 51

#define RSTICK_LEFT_SHIFT 52
#define RSTICK_RIGHT_SHIFT 53
#define RSTICK_UP_SHIFT 54
#define RSTICK_DOWN_SHIFT 55

int cell_pad_input_init(void)
{
   return cellPadInit(MAX_PADS);
}

void cell_pad_input_deinit(void)
{
   cellPadEnd();
}

uint32_t cell_pad_input_pads_connected(void)
{
#if(CELL_SDK_VERSION > 0x340000)
   CellPadInfo2 pad_info;
   cellPadGetInfo2(&pad_info);
#else
   CellPadInfo pad_info;
   cellPadGetInfo(&pad_info);
#endif
   return pad_info.now_connect;
}

#define M(x) (x & 0xFF)

uint64_t cell_pad_input_poll_device(uint32_t id)
{
   CellPadData pad_data;
   static uint64_t ret[MAX_PADS];

   // Get new pad data
   cellPadGetData(id, &pad_data);

   if (pad_data.len == 0)
      return ret[id];
   else
   {
      ret[id] = 0;

      // Build the return value.
      ret[id] |= (uint64_t)M(pad_data.button[LOWER_BUTTONS]);
      ret[id] |= (uint64_t)M(pad_data.button[HIGHER_BUTTONS]) << 8;
      ret[id] |= (uint64_t)M(pad_data.button[RSTICK_X]) << 32;
      ret[id] |= (uint64_t)M(pad_data.button[RSTICK_Y]) << 40;
      ret[id] |= (uint64_t)M(pad_data.button[LSTICK_X]) << 16;
      ret[id] |= (uint64_t)M(pad_data.button[LSTICK_Y]) << 24;

      ret[id] |= (uint64_t)(PRESSED_LEFT_LSTICK(ret[id]) ? 1 : 0) << LSTICK_LEFT_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_RIGHT_LSTICK(ret[id]) ? 1 : 0) << LSTICK_RIGHT_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_UP_LSTICK(ret[id]) ? 1 : 0) << LSTICK_UP_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_DOWN_LSTICK(ret[id]) ? 1 : 0) << LSTICK_DOWN_SHIFT;

      ret[id] |= (uint64_t)(PRESSED_LEFT_RSTICK(ret[id]) ? 1 : 0) << RSTICK_LEFT_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_RIGHT_RSTICK(ret[id]) ? 1 : 0) << RSTICK_RIGHT_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_UP_RSTICK(ret[id]) ? 1 : 0) << RSTICK_UP_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_DOWN_RSTICK(ret[id]) ? 1 : 0) << RSTICK_DOWN_SHIFT;
      return ret[id];
   }
}
#undef M

