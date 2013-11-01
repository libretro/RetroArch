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

#ifndef GL_FONT_H__
#define GL_FONT_H__

#include <stdint.h>
#include "../../boolean.h"

typedef struct gl_font_renderer
{
   bool (*init)(void *data, const char *font_path, float font_size,
         unsigned win_width, unsigned win_height);
   void (*deinit)(void *data);
   void (*render_msg)(void *data, const char *msg, void *parms);
   const char *ident;
} gl_font_renderer_t;

extern const gl_font_renderer_t gl_raster_font;
extern const gl_font_renderer_t libdbg_font;

const gl_font_renderer_t *gl_font_init_first(void *data,
      const char *font_path, float font_size, unsigned win_width, unsigned win_height);

#endif

