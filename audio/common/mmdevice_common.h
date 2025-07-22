/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _MMDEVICE_COMMON_H
#define _MMDEVICE_COMMON_H

#include <stdlib.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

void *mmdevice_list_new(const void *u, unsigned data_flow);

/**
 * Gets the friendly name of the provided IMMDevice.
 * The string must be freed with free().
 */
char* mmdevice_name(void *data);

RETRO_END_DECLS

#endif
