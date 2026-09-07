/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018-2019 - Andres Suarez
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
#include <retro_common_api.h>

#include "presence.h"

RETRO_BEGIN_DECLS

/* ======================================================================== */
/*  Discord-RPC public types (formerly deps/discord-rpc/include/discord_rpc.h) */
/* ======================================================================== */

/* The Discord API specifies these variables:
 * - userId         - char[24]  - the userId of the player asking to join
 * - username       - char[344] - the username of the player asking to join
 * - discriminator  - char[8]   - the discriminator of the player asking to join
 * - spectateSecret - char[128] - secret used for spectating matches
 * - joinSecret     - char[128] - secret used to join matches
 * - partyId        - char[128] - the party you would be joining
 */

typedef struct DiscordRichPresence
{
   const char *state;          /* max 128 bytes */
   const char *details;        /* max 128 bytes */
   int64_t     startTimestamp;
   int64_t     endTimestamp;
   const char *largeImageKey;  /* max 32  bytes */
   const char *largeImageText; /* max 128 bytes */
   const char *smallImageKey;  /* max 32  bytes */
   const char *smallImageText; /* max 128 bytes */
   const char *partyId;        /* max 128 bytes */
   int         partySize;
   int         partyMax;
   const char *matchSecret;    /* max 128 bytes */
   const char *joinSecret;     /* max 128 bytes */
   const char *spectateSecret; /* max 128 bytes */
   int8_t      instance;
} DiscordRichPresence;

typedef struct DiscordUser
{
   const char *userId;
   const char *username;
   const char *discriminator;
   const char *avatar;
} DiscordUser;

typedef struct DiscordEventHandlers
{
   void (*ready)        (const DiscordUser *request);
   void (*disconnected) (int errorCode, const char *message);
   void (*errored)      (int errorCode, const char *message);
   void (*joinGame)     (const char *joinSecret);
   void (*spectateGame) (const char *spectateSecret);
   void (*joinRequest)  (const DiscordUser *request);
} DiscordEventHandlers;

#define DISCORD_REPLY_NO     0
#define DISCORD_REPLY_YES    1
#define DISCORD_REPLY_IGNORE 2

/* ======================================================================== */
/*  Discord-RPC public API (implementations in network/discord.c)            */
/* ======================================================================== */

void Discord_Initialize(const char *application_id,
      DiscordEventHandlers *handlers,
      int auto_register,
      const char *optional_steam_id);

void Discord_Shutdown(void);

void Discord_RunCallbacks(void);

void Discord_UpdateConnection(void);

void Discord_UpdatePresence(const DiscordRichPresence *presence);

void Discord_ClearPresence(void);

void Discord_Respond(const char *user_id, int reply);

void Discord_UpdateHandlers(DiscordEventHandlers *handlers);

/* Auto-register stubs (no-ops; retained for ABI compatibility). */
void Discord_Register         (const char *application_id, const char *command);
void Discord_RegisterSteamGame(const char *application_id, const char *steam_id);

/* ======================================================================== */
/*  RetroArch-facing discord_* API                                           */
/* ======================================================================== */

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

#ifdef HAVE_CHEEVOS
   char cheevos_richpresence[256];
   char cheevos_badge_url[256];
#endif
   char game_state[128];
   char game_image_key[32];
   char game_image_text[128];

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

char *discord_get_own_avatar(void);

char *discord_get_own_username(void);

discord_state_t *discord_state_get_ptr(void);

void discord_init(const char *discord_app_id, char *args);

RETRO_END_DECLS

#endif /* __RARCH_DISCORD_H */
