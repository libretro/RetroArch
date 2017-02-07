/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _MENU_EVENT_H
#define _MENU_EVENT_H

#include <stdint.h>
#include <compat/strl.h>

#include <retro_common_api.h>
#include <libretro.h>

RETRO_BEGIN_DECLS

/* Send input code to menu for one frame.
 *
 * TODO/FIXME - needs to be overhauled so we can send multiple
 * events per frame if we want to, and we shouldn't send the
 * entire button state either but do a separate event per button
 * state.
 */
unsigned menu_event(uint64_t input, uint64_t trigger_state);

void menu_event_kb_set(bool down, enum retro_key key);

unsigned char menu_event_kb_is_set(enum retro_key key);

RETRO_END_DECLS

#endif
