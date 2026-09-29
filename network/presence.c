#include <stdint.h>
#include "presence.h"

#ifdef HAVE_DISCORD
#include "discord.h"
#endif

#ifdef HAVE_MIST
#include "steam/steam.h"
#endif

void presence_update(enum presence presence)
{
#ifdef HAVE_DISCORD
   discord_state_t *discord_st = discord_state_get_ptr();

   if (discord_st->ready)
      discord_update(presence);
#endif
#ifdef HAVE_MIST
   steam_update_presence(presence, false);
#endif
}

void presence_poll(enum presence presence, int64_t now_us)
{
   static int64_t last_us;
   if (last_us && now_us - last_us < PRESENCE_POLL_INTERVAL_US)
      return;
   last_us = now_us;
   presence_update(presence);
}
