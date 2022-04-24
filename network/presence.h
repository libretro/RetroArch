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

#endif /* __RARCH_PRESENCE_H */
