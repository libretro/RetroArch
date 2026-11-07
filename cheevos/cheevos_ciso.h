/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __RARCH_CHEEVOS_CISO_H
#define __RARCH_CHEEVOS_CISO_H

#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* RC_CONSOLE_GAMECUBE or RC_CONSOLE_WII for a CISO file of either's
 * disc, RC_CONSOLE_UNKNOWN for anything else */
uint32_t rcheevos_ciso_get_console_id(const char *path);

/* Has rc_hash read files as CISO disc images */
void rcheevos_ciso_filereader_init(void);

RETRO_END_DECLS

#endif
