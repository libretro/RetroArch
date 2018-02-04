#ifndef __RARCH_BADGE_H
#define __RARCH_BADGE_H

#include "../menu/menu_driver.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define CHEEVOS_BADGE_LIMIT 256

typedef struct
{
  bool badge_locked[CHEEVOS_BADGE_LIMIT];
  const char * badge_id_list[CHEEVOS_BADGE_LIMIT];
  menu_texture_item menu_texture_list[CHEEVOS_BADGE_LIMIT];
} badges_ctx_t;

bool badge_exists(const char* filepath);
void set_badge_menu_texture(badges_ctx_t * badges, int i);
extern void set_badge_info (badges_ctx_t *badge_struct, int id, const char *badge_id, bool active);
extern menu_texture_item get_badge_texture(int id);

extern badges_ctx_t badges_ctx;
static badges_ctx_t new_badges_ctx;

RETRO_END_DECLS

#endif
