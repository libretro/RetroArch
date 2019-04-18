/* CRT SwitchRes Core
 * Copyright (C) 2018 Alphanu / Ben Templeman.
 *
 * RetroArch - A frontend for libretro.
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

#ifndef __VIDEO_CRT_SWITCH_H__
#define __VIDEO_CRT_SWITCH_H__

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

void crt_switch_res_core(unsigned width, unsigned height, float hz, unsigned crt_mode, int crt_switch_center_adjust, int monitor_index, bool dynamic);

void crt_aspect_ratio_switch(unsigned width, unsigned height);

void crt_video_restore(void);

int crt_compute_dynamic_width(int width);

RETRO_END_DECLS

#endif
