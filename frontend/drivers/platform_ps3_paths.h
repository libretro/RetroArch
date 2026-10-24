#ifndef RARCH_PLATFORM_PS3_PATHS_H
#define RARCH_PLATFORM_PS3_PATHS_H

#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

bool ps3_user_paths_init(unsigned user_id);
/* The shared layout below root (USRDIR), for when the user's
 * directory cannot be used */
void ps3_legacy_paths_init(const char *root);
/* Copies the shared layout's personal data to a user without a
 * configuration yet; false leaves them without one */
bool ps3_user_data_migrate(const char *root);
bool ps3_config_import(const char *path, char *dest, size_t len);

RETRO_END_DECLS

#endif
