/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PS3_VIDEO_PSGL_H
#define _PS3_VIDEO_PSGL_H

#include "../gfx/gl_common.h"
#include "../gfx/gfx_common.h"
#include <cell/dbgfont.h>

enum
{
	ASPECT_RATIO_4_3,
	ASPECT_RATIO_5_4,
	ASPECT_RATIO_8_7,
	ASPECT_RATIO_16_9,
	ASPECT_RATIO_16_10,
	ASPECT_RATIO_16_15,
	ASPECT_RATIO_19_14,
	ASPECT_RATIO_2_1,
	ASPECT_RATIO_3_2,
	ASPECT_RATIO_3_4,
	ASPECT_RATIO_1_1,
	ASPECT_RATIO_AUTO,
	ASPECT_RATIO_CUSTOM,
	LAST_ASPECT_RATIO
};

bool ps3_setup_texture(void);
const char * ps3_get_resolution_label(uint32_t resolution);
int ps3_check_resolution(uint32_t resolution_id);
void gl_frame_menu(void);
void ps3_previous_resolution (void);
void ps3_next_resolution (void);
void ps3_set_filtering(unsigned index, bool set_smooth);
void ps3_video_deinit(void);
void ps3graphics_block_swap (void);
void ps3graphics_set_aspect_ratio(uint32_t aspectratio_index);
void ps3graphics_set_orientation(uint32_t orientation);
void ps3graphics_set_vsync(uint32_t vsync);
void ps3graphics_unblock_swap (void);
void ps3graphics_video_init(bool get_all_resolutions);
void ps3graphics_video_reinit(void);

extern void *g_gl;

#endif
