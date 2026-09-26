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

#ifndef __DISPSERV_GX_H
#define __DISPSERV_GX_H

#include <retro_common_api.h>

#include "dispserv_gx_modes.h"

RETRO_BEGIN_DECLS

/* The TV standard the console is set to, as the mode rules take it,
 * and the VI_* tv mode to program. Shared by the display server's
 * mode list and gx_set_video_mode, so both see the same console. */
void gx_display_server_query(gx_vi_standard_t *std, unsigned *tvmode);

RETRO_END_DECLS

#endif
