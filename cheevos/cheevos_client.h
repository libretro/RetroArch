/*  RetroArch - A frontend for libretro.
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

#ifndef __RARCH_CHEEVOS_CLIENT_H
#define __RARCH_CHEEVOS_CLIENT_H

#include "cheevos_locals.h"

RETRO_BEGIN_DECLS

typedef void (*rcheevos_client_callback)(void* userdata);

void rcheevos_client_initialize(void);

void rcheevos_client_login_with_password(const char* username, const char* password,
                                         rcheevos_client_callback callback, void* userdata);
void rcheevos_client_login_with_token(const char* username, const char* token,
                                      rcheevos_client_callback callback, void* userdata);

void rcheevos_client_identify_game(const char* hash, rcheevos_client_callback callback, void* userdata);

void rcheevos_client_initialize_runtime(unsigned game_id, rcheevos_client_callback callback, void* userdata);

void rcheevos_client_start_session(unsigned game_id);
void rcheevos_client_award_achievement(unsigned achievement_id);
void rcheevos_client_submit_lboard_entry(unsigned leaderboard_id, int value);

void rcheevos_client_fetch_badges(rcheevos_client_callback callback, void* userdata);

void rcheevos_log_url(const char* api, const char* url);
void rcheevos_get_user_agent(rcheevos_locals_t *locals, char *buffer, size_t len);


RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_MENU_H */
