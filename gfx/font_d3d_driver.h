/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef __FONT_D3D_DRIVER_H__
#define __FONT_D3D_DRIVER_H__

#include <stdint.h>
#include <boolean.h>
#include "../driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct d3d_font_renderer
{
   void *(*init)(void *data, const char *font_path, float font_size);
   void (*free)(void *data);
   void (*render_msg)(void *data, const char *msg,
         const void *params);
   const char *ident;

   const void *(*get_glyph)(void *data, uint32_t code);
   void (*bind_block)(void *data, void *block);
   void (*flush)(void *data);
} d3d_font_renderer_t;

extern d3d_font_renderer_t d3d_xbox360_font;
extern d3d_font_renderer_t d3d_xdk1_font;
extern d3d_font_renderer_t d3d_win32_font;

bool d3d_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path, float font_size);

#ifdef __cplusplus
}
#endif

#endif

