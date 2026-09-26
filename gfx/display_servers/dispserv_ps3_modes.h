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

/* The PS3 video output modes RetroArch offers and the rules for which
 * one is in use. Nothing here touches an SDK: whether the display
 * takes a mode comes in through a callback (dispserv_ps3.c asks the
 * video output), so the Cell SDK and PSL1GHT builds share it and a
 * host build can test it.
 *
 * A mode is named by its video output resolution id, the value both
 * SDKs use (CELL_VIDEO_OUT_RESOLUTION_* / VIDEO_RESOLUTION_*: 1080 is
 * 1, 720 is 2, 480 is 4, 576 is 5, 1600x1080 to 960x1080 are 10 to
 * 13). That id is what current_resolution_id stores in the config;
 * 0 there means "whatever the system menu is set to". */

#ifndef __DISPSERV_PS3_MODES_H
#define __DISPSERV_PS3_MODES_H

#include <stddef.h>

#include <retro_common_api.h>
#include <boolean.h>

#include "../video_display_server.h"

RETRO_BEGIN_DECLS

#define PS3_MODE_ID_SYSTEM 0

/* Whether the display takes the mode with this resolution id */
typedef bool (*ps3_mode_available_t)(void *user, unsigned id);

/* The mode's size as VIDEO_SCALE_PACKed dims, 0 for an unknown id. */
unsigned ps3_modes_dims(unsigned id);

/* The field rate the video output runs the mode at. */
float ps3_modes_hz(unsigned id);

/* The id in use: the stored one when it is a known mode the display
 * takes, otherwise system_id (the system menu's mode; 0 if unknown). */
unsigned ps3_modes_effective(ps3_mode_available_t avail, void *user,
      unsigned current_id, unsigned system_id);

/* The id of an available mode of exactly these dims, or -1. */
int ps3_modes_find(ps3_mode_available_t avail, void *user, unsigned dims);

/* The available modes, smallest first, with the one in use marked
 * current. Writes at most max entries to out and returns how many
 * there are in total; out may be NULL to count. */
unsigned ps3_modes_list(ps3_mode_available_t avail, void *user,
      unsigned current_id, unsigned system_id,
      video_display_config_t *out, unsigned max);

RETRO_END_DECLS

#endif
