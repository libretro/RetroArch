/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef _PS3_INPUT_H_
#define _PS3_INPUT_H_

#include <stdbool.h>
#include <wchar.h>

#ifdef HAVE_OSKUTIL
#ifdef __PSL1GHT__
#include <sysutil/osk.h>
#else
#include <sysutil/sysutil_oskdialog.h>
#endif
#endif

#include "sdk_defines.h"

#ifdef HAVE_SYSUTILS
#include <sysutil/sysutil_common.h>
#endif

#ifndef __PSL1GHT__
#define MAX_PADS 7
#endif

#define DEADZONE_LOW 55
#define DEADZONE_HIGH 210

#define OSK_IS_RUNNING(object) object.is_running
#define OUTPUT_TEXT_STRING(object) object.osk_text_buffer_char

enum {
   PS3_GAMEPAD_CROSS =			1 << 0,
   PS3_GAMEPAD_SQUARE =			1 << 1,
   PS3_GAMEPAD_SELECT =			1 << 2,
   PS3_GAMEPAD_START =			1 << 3,
   PS3_GAMEPAD_DPAD_UP =		1 << 4,
   PS3_GAMEPAD_DPAD_DOWN =		1 << 5,
   PS3_GAMEPAD_DPAD_LEFT =		1 << 6,
   PS3_GAMEPAD_DPAD_RIGHT =		1 << 7,
   PS3_GAMEPAD_CIRCLE =			1 << 8,
   PS3_GAMEPAD_TRIANGLE =		1 << 9,
   PS3_GAMEPAD_L1 =			1 << 10,
   PS3_GAMEPAD_R1 =			1 << 11,
   PS3_GAMEPAD_L2 =			1 << 12,
   PS3_GAMEPAD_R2 =			1 << 13,
   PS3_GAMEPAD_L3 =			1 << 14,
   PS3_GAMEPAD_R3 =			1 << 15,
   PS3_GAMEPAD_LSTICK_LEFT_MASK =	1 << 16,
   PS3_GAMEPAD_LSTICK_RIGHT_MASK =	1 << 17,
   PS3_GAMEPAD_LSTICK_UP_MASK	 =	1 << 18,
   PS3_GAMEPAD_LSTICK_DOWN_MASK =	1 << 19,
   PS3_GAMEPAD_RSTICK_LEFT_MASK =	1 << 20,
   PS3_GAMEPAD_RSTICK_RIGHT_MASK =	1 << 21,
   PS3_GAMEPAD_RSTICK_UP_MASK 	=	1 << 22,
   PS3_GAMEPAD_RSTICK_DOWN_MASK =	1 << 23,
};

enum ps3_device_id
{
   PS3_DEVICE_ID_JOYPAD_CIRCLE = 0,
   PS3_DEVICE_ID_JOYPAD_CROSS,
   PS3_DEVICE_ID_JOYPAD_TRIANGLE,
   PS3_DEVICE_ID_JOYPAD_SQUARE,
   PS3_DEVICE_ID_JOYPAD_UP,
   PS3_DEVICE_ID_JOYPAD_DOWN,
   PS3_DEVICE_ID_JOYPAD_LEFT,
   PS3_DEVICE_ID_JOYPAD_RIGHT,
   PS3_DEVICE_ID_JOYPAD_SELECT,
   PS3_DEVICE_ID_JOYPAD_START,
   PS3_DEVICE_ID_JOYPAD_L1,
   PS3_DEVICE_ID_JOYPAD_L2,
   PS3_DEVICE_ID_JOYPAD_L3,
   PS3_DEVICE_ID_JOYPAD_R1,
   PS3_DEVICE_ID_JOYPAD_R2,
   PS3_DEVICE_ID_JOYPAD_R3,
   PS3_DEVICE_ID_LSTICK_LEFT,
   PS3_DEVICE_ID_LSTICK_RIGHT,
   PS3_DEVICE_ID_LSTICK_UP,
   PS3_DEVICE_ID_LSTICK_DOWN,
   PS3_DEVICE_ID_LSTICK_LEFT_DPAD,
   PS3_DEVICE_ID_LSTICK_RIGHT_DPAD,
   PS3_DEVICE_ID_LSTICK_UP_DPAD,
   PS3_DEVICE_ID_LSTICK_DOWN_DPAD,
   PS3_DEVICE_ID_RSTICK_LEFT,
   PS3_DEVICE_ID_RSTICK_RIGHT,
   PS3_DEVICE_ID_RSTICK_UP,
   PS3_DEVICE_ID_RSTICK_DOWN,
   PS3_DEVICE_ID_RSTICK_LEFT_DPAD,
   PS3_DEVICE_ID_RSTICK_RIGHT_DPAD,
   PS3_DEVICE_ID_RSTICK_UP_DPAD,
   PS3_DEVICE_ID_RSTICK_DOWN_DPAD,

   RARCH_LAST_PLATFORM_KEY
};

#ifdef HAVE_OSKUTIL

typedef struct
{
   unsigned int osk_memorycontainer;
   wchar_t init_message[CELL_OSKDIALOG_STRING_SIZE + 1];
   wchar_t message[CELL_OSKDIALOG_STRING_SIZE + 1];
   wchar_t osk_text_buffer[CELL_OSKDIALOG_STRING_SIZE + 1];
   char osk_text_buffer_char[CELL_OSKDIALOG_STRING_SIZE + 1];
   uint32_t flags;
   bool is_running;
   bool text_can_be_fetched;
   sys_memory_container_t containerid;
   CellOskDialogPoint pos;
   CellOskDialogInputFieldInfo inputFieldInfo;
   CellOskDialogCallbackReturnParam outputInfo;
   CellOskDialogParam dialogParam;
} oskutil_params;

void oskutil_write_message(oskutil_params *params, const wchar_t* msg);
void oskutil_write_initial_message(oskutil_params *params, const wchar_t* msg);
void oskutil_init(oskutil_params *params, unsigned containersize);
bool oskutil_start(oskutil_params *params);
void oskutil_stop(oskutil_params *params);
void oskutil_finished(oskutil_params *params);
void oskutil_close(oskutil_params *params);
void oskutil_unload(oskutil_params *params);

#endif

#endif
