/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#ifndef __WEBHOOKS_PROGRESS_TRACKER_H
#define __WEBHOOKS_PROGRESS_TRACKER_H

#include "../deps/rcheevos/include/rc_runtime.h"

enum {
    PROGRESS_UNCHANGED = 0,
    PROGRESS_UPDATED = 1
};

const char* wpt_get_last_progress();

void wpt_clear_progress();

int wpt_process_frame
(
    rc_runtime_t* runtime
);

#endif /* __WEBHOOKS_PROGRESS_TRACKER_H */
