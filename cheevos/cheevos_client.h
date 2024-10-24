/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019-2023 - Brian Weiss
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

#ifdef HAVE_RC_CLIENT

void rcheevos_client_download_placeholder_badge(void);
void rcheevos_client_download_game_badge(const rc_client_game_t* game);
void rcheevos_client_download_achievement_badges(rc_client_t* client);
void rcheevos_client_download_achievement_badge(const char* badge_name, bool locked);
void rcheevos_client_download_badge_from_url(const char* url, const char* badge_name);

void rcheevos_client_server_call(const rc_api_request_t* request,
   rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);

#else

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

void rcheevos_log_url(const char* url);

#endif /* HAVE_RC_CLIENT */

void rcheevos_get_user_agent(rcheevos_locals_t* locals, char* buffer, size_t len);

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_MENU_H */
