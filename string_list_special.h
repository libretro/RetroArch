/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef _STRING_LIST_SPECIAL_H
#define _STRING_LIST_SPECIAL_H

#include <stddef.h>

#include <string/string_list.h>

enum string_list_type
{
   STRING_LIST_NONE = 0,
   STRING_LIST_MENU_DRIVERS,
   STRING_LIST_CAMERA_DRIVERS,
   STRING_LIST_LOCATION_DRIVERS,
   STRING_LIST_AUDIO_DRIVERS,
   STRING_LIST_AUDIO_RESAMPLER_DRIVERS,
   STRING_LIST_VIDEO_DRIVERS,
   STRING_LIST_INPUT_DRIVERS,
   STRING_LIST_INPUT_JOYPAD_DRIVERS,
   STRING_LIST_INPUT_HID_DRIVERS,
   STRING_LIST_RECORD_DRIVERS,
   STRING_LIST_SUPPORTED_CORES_PATHS,
   STRING_LIST_SUPPORTED_CORES_NAMES,
   STRING_LIST_CORES_PATHS,
   STRING_LIST_CORES_NAMES
};

struct string_list *string_list_new_special(enum string_list_type type,
      void *data, unsigned *len, size_t *list_size);

const char *char_list_new_special(enum string_list_type type, void *data);

#endif
