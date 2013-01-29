/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#ifndef __GFX_COMMON_H
#define __GFX_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "../general.h"
#include "../boolean.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

// If always_write is true, will always update FPS value
// If always_write is false, returns true if FPS value was updated.
bool gfx_get_fps(char *buf, size_t size, bool always_write);
void gfx_window_title_reset(void);

#ifdef _WIN32
void gfx_set_dwm(void);
#endif

void gfx_scale_integer(struct rarch_viewport *vp, unsigned win_width, unsigned win_height,
      float aspect_ratio, bool keep_aspect);

#ifdef __cplusplus
}
#endif

#endif
