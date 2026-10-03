#ifndef __RARCH_PRESENCE_H
#define __RARCH_PRESENCE_H

enum presence
{
   PRESENCE_NONE = 0,
   PRESENCE_MENU,
   PRESENCE_GAME,
   PRESENCE_GAME_PAUSED,
   PRESENCE_NETPLAY_HOSTING,
   PRESENCE_NETPLAY_CLIENT,
   PRESENCE_NETPLAY_NETPLAY_STOPPED,
   PRESENCE_RETROACHIEVEMENTS,
   PRESENCE_SHUTDOWN
};

typedef struct presence_userdata
{
   enum presence status;
} presence_userdata_t;

void presence_update(enum presence presence);

/* The per-frame entry: presence_update() at most every
 * PRESENCE_POLL_INTERVAL_US; a state edge between polls is not lost,
 * since each sink compares against its own last status. now_us is
 * the frame's cpu_features_get_time_usec(). */
#define PRESENCE_POLL_INTERVAL_US 100000
void presence_poll(enum presence presence, int64_t now_us);

#endif /* __RARCH_PRESENCE_H */
