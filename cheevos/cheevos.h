/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
 *  Copyright (C) 2019-2026 - Brian Weiss
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
void rcheevos_idle(void);

void rcheevos_reset_game(bool widgets_ready);
void rcheevos_refresh_memory(void);

void rcheevos_pause_hardcore(void);
void rcheevos_hardcore_enabled_changed(void);
void rcheevos_toggle_hardcore_paused(void);
bool rcheevos_hardcore_active(void);

bool rcheevos_is_pause_allowed(void);
void rcheevos_spectating_changed(void);

void rcheevos_validate_config_settings(void);

void rcheevos_leaderboard_trackers_visibility_changed(void);

void rcheevos_set_support_cheevos(bool state);
bool rcheevos_get_support_cheevos(void);

const char* rcheevos_get_hash(void);
int rcheevos_get_richpresence(char *s, size_t len);
int rcheevos_get_game_badge_url(char *s, size_t len);

void rcheevos_get_local_badge_filename(char badge_file[], size_t badge_file_size, const char* badge, bool locked);
/* A texture handle, or 0 while the badge is on its way (or missing
 * and, with download_if_missing, now downloading); ask again on a
 * later frame. Never reads, decodes or uploads on the calling thread,
 * so any thread may ask. The handle belongs to the caller, who unloads
 * it. */
uintptr_t rcheevos_get_badge_texture(const char* badge, bool locked, bool download_if_missing);
/* Same, and says why a 0 is a 0: @pending is true while the file is
 * on disk and being loaded, which takes a few frames - a caller about
 * to draw a placeholder can sit those out - and false when the badge
 * is being downloaded, has failed, or was handed over. */
uintptr_t rcheevos_get_badge_texture_ex(const char* badge, bool locked,
      bool download_if_missing, bool *pending);
/* The server default badge ("00000"), same rules, except the handle is
 * lent: it stays the cache's, is good until the next
 * rcheevos_badge_cache_reset(), and the caller never unloads it. */
uintptr_t rcheevos_get_default_badge_texture(void);
/* Main thread, once a frame: start the loads asked for from other
 * threads. One atomic load when there are none. */
void rcheevos_badge_cache_service(void);
void rcheevos_badge_cache_reset(void);
void rcheevos_badge_request_download(const char* badge, bool locked);
bool rcheevos_is_badge_available(const char* badge, bool locked);
void rcheevos_update_badge_references(const char* badge_name);

uint8_t* rcheevos_patch_address(unsigned address);

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_CHEEVOS_H */
