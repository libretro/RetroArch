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

#ifndef _MENU_NETWORKING_H
#define _MENU_NETWORKING_H

#include <stdint.h>

#include <retro_common_api.h>
#include <retro_environment.h>

#include <lists/file_list.h>

#include "../msg_hash.h"

RETRO_BEGIN_DECLS

unsigned print_buf_lines(file_list_t *list, char *buf,
      const char *label, int buf_size,
      enum msg_file_type type, bool append, bool extended);

void cb_net_generic_subdir(retro_task_t *task,
      void *task_data, void *user_data,
      const char *err);

void cb_net_generic(retro_task_t *task,
      void *task_data, void *user_data, const char *err);

RETRO_END_DECLS

#endif
