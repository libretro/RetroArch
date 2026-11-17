/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __CALENDAR_TIME_H
#define __CALENDAR_TIME_H

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>

RETRO_BEGIN_DECLS

/* The snapshot behind RETRO_ENVIRONMENT_GET_CALENDAR_TIME_INTERFACE.
 * These run on the main thread; the callbacks they hand out run on any
 * thread and never wait. */

/* What the callbacks report from now on; a NULL start, nothing. */
void calendar_time_publish(const struct retro_calendar_time *start,
      const struct retro_calendar_zone *zone);

/* Publishes the host clock and zone, or nothing if the host has no
 * clock. Before retro_load_game only. */
void calendar_time_capture(void);

void calendar_time_clear(void);

/* While held, get_current_time returns false. */
void calendar_time_set_held(bool held);

bool calendar_time_get_interface(struct retro_calendar_time_interface *iface);

RETRO_END_DECLS

#endif
