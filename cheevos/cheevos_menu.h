/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2018 - Andre Leiradella
 *  Copyright (C) 2019-2021 - Brian Weiss
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

#ifndef __RARCH_CHEEVOS_MENU_H
#define __RARCH_CHEEVOS_MENU_H

#ifdef HAVE_MENU

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

void rcheevos_menu_populate(void* data);
void rcheevos_menu_populate_hardcore_pause_submenu(void* data);
bool rcheevos_menu_get_state(unsigned menu_offset, char* buffer, size_t buffer_size);
bool rcheevos_menu_get_sublabel(unsigned menu_offset, char* buffer, size_t buffer_size);
uintptr_t rcheevos_menu_get_badge_texture(unsigned menu_offset);

RETRO_END_DECLS

#endif /* HAVE_MENU */

#endif /* __RARCH_CHEEVOS_MENU_H */
