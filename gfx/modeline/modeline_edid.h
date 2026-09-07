/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner,
 *                            Alexandre Wodarczyk, Gil Delescluse
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

#ifndef __VIDEO_MODELINE_EDID_H
#define __VIDEO_MODELINE_EDID_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "modeline_core.h"

RETRO_BEGIN_DECLS

#define MODELINE_EDID_SIZE 128

/* One EDID 1.3 base block describing a monitor that syncs the given
 * range, with the modeline as its preferred detailed timing and the
 * name (13 characters at most) as the monitor name. This is what a
 * display without DDC - a 15 kHz CRT on a VGA or SCART adapter -
 * cannot tell the kernel or the driver itself: loaded as a firmware
 * EDID (Linux, drm.edid_firmware=<connector>:edid/<file>) or an EDID
 * override, it makes the connector report as connected, gives the
 * driver the sync limits so the CRT timings are not pruned, and puts
 * the system on a scanrate the tube can show before anything else
 * runs. The generator only writes bytes; installing the block is a
 * deliberate, separate step for the user. */
bool modeline_edid_build(const video_modeline_t *mode,
      const video_modeline_range_t *range, const char *name,
      uint8_t out[MODELINE_EDID_SIZE]);

/* The block for a generator's current preset: the preferred timing is
 * the preset's 320x240@60 (the canonical CRT mode, doublescanned on
 * a 31 kHz preset) and the limits are the first live range. */
bool modeline_edid_for_gen(video_modeline_gen_t *gen,
      uint8_t out[MODELINE_EDID_SIZE]);

RETRO_END_DECLS

#endif
