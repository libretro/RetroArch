#ifndef __RARCH_STEAM_H
#define __RARCH_STEAM_H

#include <mist.h>

#include "core_info.h"

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

/* Located in tasks/task_steam.c */
void task_push_steam_core_dlc_install(AppId app_id, const char *name);

#endif /* __RARCH_STEAM_H */
