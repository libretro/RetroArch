/*  RetroArch - A frontend for libretro.
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

#ifndef __DISPSERV_PS3_H
#define __DISPSERV_PS3_H

#include <retro_common_api.h>

#include "dispserv_ps3_modes.h"

RETRO_BEGIN_DECLS

/* The video output resolution id to configure: the one chosen in the
 * menu (current_resolution_id) when the display takes it, otherwise
 * system_id - the mode the system menu has the output in, which the
 * caller reads from the output state (0 when it has none). Shared by
 * the RSX driver and the PSGL context, which configure the output. */
unsigned ps3_display_server_resolution(unsigned system_id);

RETRO_END_DECLS

#endif
