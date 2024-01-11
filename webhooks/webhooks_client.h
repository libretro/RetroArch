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

#ifndef __WEBHOOKS_CLIENT_H
#define __WEBHOOKS_CLIENT_H

void wc_send_game_event
(
  unsigned int console_id,
  const char* rom_hash,
  unsigned short game_event,
  unsigned long frame_number,
  retro_time_t time
);

void wc_send_achievement_event
(
  unsigned int console_id,
  const char* rom_hash,
  unsigned short game_event,
  unsigned int active_achievements,
  unsigned int total_achievements,
  unsigned long frame_number,
  retro_time_t time
);

void wc_send_keep_alive_event
(
  unsigned int console_id,
  const char* rom_hash,
  unsigned short game_event,
  unsigned long frame_number,
  retro_time_t time
);

void wc_update_progress
(
  unsigned int console_id,
  const char* rom_hash,
  const char* progress,
  unsigned long frame_number,
  retro_time_t time
);

#endif /* __WEBHOOKS_CLIENT_H */
