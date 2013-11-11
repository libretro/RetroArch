/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "sdk_defines.h"

#ifndef __PSL1GHT__
#define MAX_PADS 7
#endif

#define DEADZONE_LOW 55
#define DEADZONE_HIGH 210

#ifdef HAVE_OSK
typedef struct ps3_osk
{
   unsigned int osk_memorycontainer;
   wchar_t init_message[CELL_OSKDIALOG_STRING_SIZE + 1];
   wchar_t message[CELL_OSKDIALOG_STRING_SIZE + 1];
   wchar_t text_buf[CELL_OSKDIALOG_STRING_SIZE + 1];
   uint32_t flags;
   sys_memory_container_t containerid;
   CellOskDialogPoint pos;
   CellOskDialogInputFieldInfo inputFieldInfo;
   CellOskDialogCallbackReturnParam outputInfo;
   CellOskDialogParam dialogParam;
} ps3_osk_t;

void oskutil_write_message(void *params, const void* msg);
void oskutil_write_initial_message(void *params, const void* msg);
void *oskutil_init(unsigned containersize);
void oskutil_free(void *data);
#endif

#endif
