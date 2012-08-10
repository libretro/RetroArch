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

#ifndef _PS3_CTX_H
#define _PS3_CTX_H

void gfx_ctx_get_available_resolutions (void);
int gfx_ctx_check_resolution(unsigned resolution_id);
unsigned gfx_ctx_get_resolution_width(unsigned resolution_id);
unsigned gfx_ctx_get_resolution_height(unsigned resolution_id);
void gfx_ctx_set_projection(gl_t *gl, const struct gl_ortho *ortho, bool allow_rotate);
void gfx_ctx_set_aspect_ratio(void *data, unsigned aspectratio_index);
void gfx_ctx_set_overscan(void);
void gfx_ctx_set_fbo(bool enable);
void gfx_ctx_clear(void);
void gfx_ctx_set_blend(bool enable);

#endif
