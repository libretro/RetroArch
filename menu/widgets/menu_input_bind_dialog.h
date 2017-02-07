/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef _MENU_INPUT_BIND_DIALOG_H
#define _MENU_INPUT_BIND_DIALOG_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>

#include <retro_common_api.h>

#include "../menu_input.h"

RETRO_BEGIN_DECLS

enum menu_input_binds_ctl_state
{
   MENU_INPUT_BINDS_CTL_BIND_NONE = 0,
   MENU_INPUT_BINDS_CTL_BIND_SINGLE,
   MENU_INPUT_BINDS_CTL_BIND_ALL
};

typedef struct menu_input_ctx_bind
{
   char *s;
   size_t len;
} menu_input_ctx_bind_t;

typedef struct menu_input_ctx_bind_limits
{
   unsigned min;
   unsigned max;
} menu_input_ctx_bind_limits_t;

bool menu_input_key_bind_set_mode(
      enum menu_input_binds_ctl_state state, void *data);

bool menu_input_key_bind_set_min_max(menu_input_ctx_bind_limits_t *lim);

bool menu_input_key_bind_iterate(menu_input_ctx_bind_t *bind);

RETRO_END_DECLS

#endif
