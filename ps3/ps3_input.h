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

#define DEADZONE_LOW 55
#define DEADZONE_HIGH 210

#ifdef HAVE_OSKUTIL

typedef struct
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
} oskutil_params;

void oskutil_write_message(oskutil_params *params, const wchar_t* msg);
void oskutil_write_initial_message(oskutil_params *params, const wchar_t* msg);
void oskutil_init(oskutil_params *params, unsigned containersize);

#endif

#endif
