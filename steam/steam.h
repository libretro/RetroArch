#ifndef __RARCH_STEAM_H
#define __RARCH_STEAM_H

#include <boolean.h>
#include <mist.h>

#include "core_info.h"
#include "network/presence.h"

#define MIST_UNPACK_RESULT(result) MIST_RESULT_CODE(result), MIST_ERROR(result)

struct steam_core_dlc {
   AppId app_id;
   char* name;
   char* name_lower;
   core_info_t* core_info; /* NOTE: This will be NULL if no core info was found for the name */
};

typedef struct steam_core_dlc steam_core_dlc_t;

typedef struct
{
   steam_core_dlc_t *list;
   size_t count;
} steam_core_dlc_list_t;

enum steam_rich_presence_running_format
{
   STEAM_RICH_PRESENCE_FORMAT_NONE = 0,
   STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   STEAM_RICH_PRESENCE_FORMAT_CORE,
   STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   STEAM_RICH_PRESENCE_FORMAT_LAST
};

void steam_init(void);

void steam_deinit(void);

void steam_poll(void);

MistResult steam_generate_core_dlcs_list(steam_core_dlc_list_t **list);
MistResult steam_get_core_dlcs(steam_core_dlc_list_t **list, bool cached);
steam_core_dlc_t *steam_core_dlc_list_get(steam_core_dlc_list_t *list, size_t i);
steam_core_dlc_t* steam_get_core_dlc_by_name(steam_core_dlc_list_t *list, const char *name);
void steam_core_dlc_list_free(steam_core_dlc_list_t *list); /* NOTE: This should not be called manually with lists from steam_get_core_dlcs */

void steam_install_core_dlc(steam_core_dlc_t *core_dlc);
void steam_uninstall_core_dlc(steam_core_dlc_t *core_dlc);

bool steam_open_osk(void);
bool steam_has_osk_open(void);

void steam_update_presence(enum presence presence, bool force);

/* Located in tasks/task_steam.c */
void task_push_steam_core_dlc_install(AppId app_id, const char *name);

#endif /* __RARCH_STEAM_H */
