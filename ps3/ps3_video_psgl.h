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
#include <cell/dbgfont.h>

void ps3_video_init(void);
void ps3_video_deinit(void);

void ps3_next_resolution (void);
void ps3_previous_resolution (void);
const char * ps3_get_resolution_label(uint32_t resolution);
int ps3_check_resolution(uint32_t resolution_id);
void ps3_block_swap (void);
void ps3_unblock_swap (void);
void gl_frame_menu(void);

extern void *g_gl;

#endif
