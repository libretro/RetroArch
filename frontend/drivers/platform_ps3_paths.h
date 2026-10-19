#ifndef RARCH_PLATFORM_PS3_PATHS_H
#define RARCH_PLATFORM_PS3_PATHS_H

#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

bool ps3_user_paths_init(unsigned user_id);
bool ps3_config_import(const char *path, char *dest, size_t len);

RETRO_END_DECLS

#endif
