/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
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

#ifndef _VIDEO_TEXTURE_H
#define _VIDEO_TEXTURE_H

#include "video_driver.h"

enum texture_backend_type
{
   TEXTURE_BACKEND_DEFAULT = 0,
   TEXTURE_BACKEND_OPENGL,
   TEXTURE_BACKEND_DIRECT3D
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_OPENGL
#include "drivers/gl_common.h"

void gl_load_texture_data(GLuint id,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame,
      unsigned base_size);
#endif

unsigned video_texture_load(void *data,
      enum texture_backend_type type,
      enum texture_filter_type  filter_type);

void video_texture_unload(enum texture_backend_type type, uintptr_t *id);

#ifdef __cplusplus
}
#endif

#endif
