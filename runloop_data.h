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

#ifndef __RETROARCH_DATA_RUNLOOP_H
#define __RETROARCH_DATA_RUNLOOP_H

#include <retro_miscellaneous.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*transfer_cb_t)(void *data, size_t len);

enum runloop_data_type
{
   DATA_TYPE_NONE = 0,
   DATA_TYPE_FILE,
   DATA_TYPE_IMAGE,
   DATA_TYPE_HTTP,
   DATA_TYPE_OVERLAY,
   DATA_TYPE_DB
};

void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2,
      unsigned prio, unsigned duration, bool flush);

void rarch_main_data_clear_state(void);

void rarch_main_data_iterate(void);

void rarch_main_data_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
