/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef _PS3_VIDEO_PSGL_H
#define _PS3_VIDEO_PSGL_H

#include "../gfx/gl_common.h"
#include "../gfx/gfx_common.h"
#include "../gfx/image.h"
#include "../console/console_ext.h"

enum
{
   FBO_DEINIT = 0,
   FBO_INIT,
   FBO_REINIT
};

#define MIN_SCALING_FACTOR (1.0f)
#define MAX_SCALING_FACTOR (4.0f)

const char * ps3_get_resolution_label(uint32_t resolution);
void ps3_previous_resolution (void);
void ps3_next_resolution (void);

void gl_deinit_fbo(gl_t * gl);
void gl_init_fbo(gl_t * gl, unsigned width, unsigned height);

bool gl_cg_reinit(const char *path);
bool gl_cg_save_cgp(const char *path, const struct gl_cg_cgp_info *info);
bool gl_cg_load_shader(unsigned index, const char *path);

unsigned gl_cg_get_lut_info(struct gl_cg_lut_info *info, unsigned elems);

#endif
