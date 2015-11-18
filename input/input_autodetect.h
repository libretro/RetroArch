/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef _INPUT_AUTODETECT_H
#define _INPUT_AUTODETECT_H

#include <stdint.h>

#include <boolean.h>
#include <retro_miscellaneous.h>

typedef struct autoconfig_params
{
   char  name[PATH_MAX_LENGTH];
   char  driver[PATH_MAX_LENGTH];
   char  display_name[PATH_MAX_LENGTH];
   unsigned idx;
   int32_t vid;
   int32_t pid;
} autoconfig_params_t;

const struct retro_keybind *input_get_auto_bind(unsigned port,
      unsigned id);

bool input_config_autoconfigure_joypad(autoconfig_params_t *params);

void input_config_autoconfigure_disconnect(unsigned i, const char *ident);

extern const char* const input_builtin_autoconfs[];

#endif
