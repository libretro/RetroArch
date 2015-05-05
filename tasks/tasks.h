/*  RetroArch - A frontend for libretro.
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

#ifndef COMMON_TASKS_H
#define COMMON_TASKS_H

#include <stdint.h>
#include <boolean.h>
#include "../runloop_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_NETWORKING
/**
 * rarch_main_data_http_iterate_transfer:
 *
 * Resumes HTTP transfer update.
 *
 * Returns: 0 when finished, -1 when we should continue
 * with the transfer on the next frame.
 **/
void rarch_main_data_http_iterate(bool is_thread,
void *data);
#endif
    
    void data_runloop_osd_msg(const char *msg, size_t sizeof_msg);

#ifdef __cplusplus
}
#endif

#endif
