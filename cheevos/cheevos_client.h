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

#ifndef __RARCH_CHEEVOS_CLIENT_H
#define __RARCH_CHEEVOS_CLIENT_H

#include "cheevos_locals.h"


/* Define this macro to log URLs. */
#undef CHEEVOS_LOG_URLS

/* Define this macro to have the password and token logged. THIS WILL DISCLOSE
 * THE USER'S PASSWORD, TAKE CARE! */
#undef CHEEVOS_LOG_PASSWORD


RETRO_BEGIN_DECLS

void rcheevos_start_session(unsigned game_id);
void rcheevos_award_achievement(rcheevos_racheevo_t* cheevo);
void rcheevos_lboard_submit(rcheevos_ralboard_t* lboard, int value);

void rcheevos_log_url(const char* api, const char* url);
void rcheevos_get_user_agent(rcheevos_locals_t *locals, char *buffer, size_t len);

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_MENU_H */
