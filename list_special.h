/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef _LIST_SPECIAL_H
#define _LIST_SPECIAL_H

#include <stdint.h>
#include <stddef.h>

#include <lists/string_list.h>
#include <retro_environment.h>

RETRO_BEGIN_DECLS

enum dir_list_type
{
   DIR_LIST_NONE = 0,
   DIR_LIST_CORES,
   DIR_LIST_CORE_INFO,
   DIR_LIST_DATABASES,
   DIR_LIST_COLLECTIONS,
   DIR_LIST_PLAIN,
   DIR_LIST_SHADERS,
   DIR_LIST_AUTOCONFIG,
   DIR_LIST_RECURSIVE
};

enum string_list_type
{
   STRING_LIST_NONE = 0,
   STRING_LIST_MENU_DRIVERS,
   STRING_LIST_CAMERA_DRIVERS,
   STRING_LIST_WIFI_DRIVERS,
   STRING_LIST_LOCATION_DRIVERS,
   STRING_LIST_AUDIO_DRIVERS,
   STRING_LIST_AUDIO_RESAMPLER_DRIVERS,
   STRING_LIST_VIDEO_DRIVERS,
   STRING_LIST_INPUT_DRIVERS,
   STRING_LIST_INPUT_JOYPAD_DRIVERS,
   STRING_LIST_INPUT_HID_DRIVERS,
   STRING_LIST_RECORD_DRIVERS,
   STRING_LIST_MIDI_DRIVERS,
   STRING_LIST_SUPPORTED_CORES_PATHS,
   STRING_LIST_SUPPORTED_CORES_NAMES
};

struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter);

struct string_list *string_list_new_special(enum string_list_type type,
      void *data, unsigned *len, size_t *list_size);

const char *char_list_new_special(enum string_list_type type, void *data);

RETRO_END_DECLS

#endif
