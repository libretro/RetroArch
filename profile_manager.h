#ifndef __RARCH_PROFILE_MANAGER_H
#define __RARCH_PROFILE_MANAGER_H

#include <boolean.h>
#include <retro_miscellaneous.h>

#define MAX_PROFILES 32

typedef struct
{
   char name[128];
   char image_path[PATH_MAX_LENGTH];
   char config_name[128]; 
} rarch_profile_t;

typedef struct
{
   rarch_profile_t profiles[MAX_PROFILES];
   int count;
   int active_index;
} rarch_profile_list_t;

/* Initialise the profile manager. */
void profile_manager_init(const char *config_dir, const char *main_config_path);

/* Get the name and icon of active profile. */
int profile_manager_get_active_index(void);
void profile_manager_get_active(char *name, size_t name_len,
      char *image_path, size_t image_path_len);

/* Startup redirect call from config after init. */
bool profile_manager_get_startup_redirect(const char *loaded_path,
      char *out_path, size_t len);

/* Resolve the path for a specific profile index. Index 0 is default */
bool profile_manager_get_config_path_for_index(int index, char *path, size_t len);

/* Create a new profile from the current settings snapshot. */
bool profile_manager_create(const char *name, const char *image_name);

/* Set which profile is active. */
bool profile_manager_set_active(int index);

/* Save current memory settings to the active profile file. */
bool profile_manager_save_current(void);

/* Delete a profile */
bool profile_manager_delete(int index);

/* Return the full profile list */
const rarch_profile_list_t *profile_manager_get_list(void);

/* Return the path to the system icons directory. */
void profile_manager_get_sysicons_dir(char *dir, size_t len);

bool profile_manager_set_icon(int index, const char *image_name);

#endif
