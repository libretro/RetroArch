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

#include <discord_rpc.h>
#include "../deps/discord-rpc/include/discord_rpc.h"
#include "presence.h"

/* The Discord API specifies these variables:
- userId --------- char[24]   - the userId of the player asking to join
- username ------- char[344]  - the username of the player asking to join
- discriminator -- char[8]    - the discriminator of the player asking to join
- spectateSecret - char[128] - secret used for spectatin matches
- joinSecret     - char[128] - secret used to join matches
- partyId        - char[128] - the party you would be joining
*/

struct discord_state
{
   int64_t start_time;
   int64_t pause_time;
   int64_t elapsed_time;

   DiscordRichPresence presence;       /* int64_t alignment */

   unsigned status;

   char self_party_id[128];
   char peer_party_id[128];
   char user_name[344];
   char user_avatar[344];

   bool ready;
   bool avatar_ready;
   bool connecting;
   bool inited;
};

typedef struct discord_state discord_state_t;

void discord_update(enum presence presence);

bool discord_is_ready(void);

void discord_avatar_set_ready(bool ready);

bool discord_avatar_is_ready(void);

char* discord_get_own_avatar(void);

char *discord_get_own_username(void);

discord_state_t *discord_state_get_ptr(void);

void discord_init(const char *discord_app_id, char *args);

#endif /* __RARCH_DISCORD_H */
