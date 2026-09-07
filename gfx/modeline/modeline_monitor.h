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

#ifndef __VIDEO_MODELINE_MONITOR_H
#define __VIDEO_MODELINE_MONITOR_H

#include <retro_common_api.h>

#include "modeline_core.h"

RETRO_BEGIN_DECLS

/* Monitor presets. A preset is one or more ranges; the names cover
 * 15 kHz consumer and arcade sets, multi-sync arcade chassis, PC CRTs
 * and VESA GTF bands, so an LCD or fixed-frequency client is a
 * different preset on the same engine rather than a second engine.
 *
 * Fills range[] from a preset name and returns the number of ranges
 * written, 0 when the name is unknown. */
int  modeline_monitor_set_preset(const char *type,
      video_modeline_range_t *range);

/* Parse a 16-field range line into *range; 0 on "auto" or empty
 * (range untouched), -1 on a parse or validation error. */
int  modeline_monitor_fill_range(video_modeline_range_t *range,
      const char *specs_line);

/* LCD: only a vfreq band, "min-max" or "auto" (59-61). */
int  modeline_monitor_fill_lcd_range(video_modeline_range_t *range,
      const char *specs_line);

/* VESA GTF bands up to the line count in "vesa_NNN". */
int  modeline_monitor_fill_vesa_gtf(video_modeline_range_t *range,
      const char *max_lines);
int  modeline_monitor_fill_vesa_range(video_modeline_range_t *range,
      int lines_min, int lines_max);

int  modeline_monitor_evaluate_range(video_modeline_range_t *range);
int  modeline_monitor_show_range(video_modeline_range_t *range);

RETRO_END_DECLS

#endif
