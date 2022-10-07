/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#ifndef __RARCH_CHEEVOS_CHEEVOS_H
#define __RARCH_CHEEVOS_CHEEVOS_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

bool rcheevos_load(const void *data);
void rcheevos_change_disc(const char* new_disc_path, bool initial_disc);

size_t rcheevos_get_serialize_size(void);
bool rcheevos_get_serialized_data(void* buffer);
bool rcheevos_set_serialized_data(void* buffer);

bool rcheevos_unload(void);

void rcheevos_test(void);

void rcheevos_reset_game(bool widgets_ready);

void rcheevos_pause_hardcore(void);
void rcheevos_hardcore_enabled_changed(void);
void rcheevos_toggle_hardcore_paused(void);
bool rcheevos_hardcore_active(void);

void rcheevos_validate_config_settings(void);

void rcheevos_leaderboards_enabled_changed(void);

void rcheevos_set_support_cheevos(bool state);
bool rcheevos_get_support_cheevos(void);

const char* rcheevos_get_hash(void);
int rcheevos_get_richpresence(char *s, size_t len);
uintptr_t rcheevos_get_badge_texture(const char *badge, bool locked);

uint8_t* rcheevos_patch_address(unsigned address);

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_CHEEVOS_H */
