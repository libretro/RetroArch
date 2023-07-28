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

#ifndef __WEBHOOKS_H
#define __WEBHOOKS_H

#include <stdint.h>
#include <stdlib.h>

#include "cheevos_client.h"
#include "rc_api_request.h"

#include "webhooks_game.h"

void webhooks_send_presence();

void webhooks_send_game_event(int game_id, game_event_t game_event);

#endif /* __WEBHOOKS_H */
