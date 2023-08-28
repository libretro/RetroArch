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

#ifndef __WEBHOOKS_RICH_PRESENCE_H
#define __WEBHOOKS_RICH_PRESENCE_H

#include "rc_runtime.h"

void wc_send_event
(
  unsigned int console_id,
  const char* game_hash,
  bool is_loaded
);

void wc_update_progress
(
  unsigned int console_id,
  const char* game_hash,
  const char* progress,
  unsigned long frame_number
);

#endif /* __WEBHOOKS_RICH_PRESENCE_H */
