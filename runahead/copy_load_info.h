#ifndef __COPY_LOAD_INFO_H__
#define __COPY_LOAD_INFO_H__

#include <boolean.h>
#include <libretro.h>

#include <retro_common_api.h>

#include "../core.h"

RETRO_BEGIN_DECLS

void set_load_content_info(const retro_ctx_load_content_info_t *ctx);

void set_last_core_type(enum rarch_core_type type);

RETRO_END_DECLS

#endif
