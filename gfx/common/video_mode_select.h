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

#ifndef __RARCH_VIDEO_MODE_SELECT_H
#define __RARCH_VIDEO_MODE_SELECT_H

#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

#include "../video_display_server.h"

RETRO_BEGIN_DECLS

/* Choosing among a display's modes, as every display server does:
 * which listed mode a requested size and rate means, and the order the
 * menu shows them in. The backends keep their own mode structures, so
 * a mode is read through a callback rather than copied. */

typedef struct video_mode_desc
{
   /* VIDEO_SCALE_PACK(width, height) */
   unsigned dims;
   float    refresh;
   bool     interlaced;
   bool     current;
} video_mode_desc_t;

/* Fills 'out' for mode 'index'; false to skip that mode. */
typedef bool (*video_mode_get_fn)(void *ctx, unsigned index,
      video_mode_desc_t *out);

/**
 * The listed mode of size 'dims' nearest 'hz': within half a hertz, or
 * matching 'int_hz' as a whole-hertz label, progressive before
 * interlaced, and the current mode on a tie. A rate further off than
 * that is not a mode the caller asked for, so -1 is returned rather
 * than a wildly different one. -1 also when no mode has that size.
 *
 * 'hz' or 'int_hz' at zero takes the current mode's rate.
 */
int video_mode_find_nearest(void *ctx, video_mode_get_fn get,
      unsigned count, unsigned dims, int int_hz, float hz);

/**
 * Puts a resolution list in the order the menu shows it - by size,
 * progressive before interlaced, then rate - and numbers the entries.
 */
void video_mode_list_finish(video_display_config_t *list, unsigned len);

RETRO_END_DECLS

#endif
