/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2018 - Andre Leiradella
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RARCH_CHEEVOS_BADGE_H
#define __RARCH_CHEEVOS_BADGE_H

#include <retro_common_api.h>

#include "../menu/menu_driver.h"

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
