/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018-2019 - Andrés Suárez
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

#ifndef __RARCH_DISCORD_H
#define __RARCH_DISCORD_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>

enum discord_presence
{
   DISCORD_PRESENCE_NONE = 0,
   DISCORD_PRESENCE_MENU,
   DISCORD_PRESENCE_GAME,
   DISCORD_PRESENCE_GAME_PAUSED,
   DISCORD_PRESENCE_CHEEVO_UNLOCKED,
   DISCORD_PRESENCE_NETPLAY_HOSTING,
   DISCORD_PRESENCE_NETPLAY_CLIENT,
   DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED,
   DISCORD_PRESENCE_SHUTDOWN
};

typedef struct discord_userdata
{
   enum discord_presence status;
} discord_userdata_t;

void discord_init(const char *discord_app_id);

void discord_shutdown(void);

void discord_update(enum discord_presence presence, bool fuzzy_archive_match);

bool discord_is_ready(void);

void discord_avatar_set_ready(bool ready);

bool discord_avatar_is_ready(void);

char* discord_get_own_username(void);

char* discord_get_own_avatar(void);

#endif /* __RARCH_DISCORD_H */
