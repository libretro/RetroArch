/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2016 - Daniel De Matteis
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

#ifndef __VIDEO_COORD_ARRAY_H
#define __VIDEO_COORD_ARRAY_H

#include <stdint.h>
#include <string.h>

#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gfx_fbo_rect
{
   unsigned img_width;
   unsigned img_height;
   unsigned max_img_width;
   unsigned max_img_height;
   unsigned width;
   unsigned height;
};

struct gfx_ortho
{
   float left;
   float right;
   float bottom;
   float top;
   float znear;
   float zfar;
};

struct gfx_tex_info
{
   unsigned int tex;
   float input_size[2];
   float tex_size[2];
   float coord[8];
};

typedef struct gfx_coords
{
   const float *vertex;
   const float *color;
   const float *tex_coord;
   const float *lut_tex_coord;
   unsigned vertices;
   const unsigned *index;
   unsigned indexes;
} gfx_coords_t;

typedef struct gfx_mut_coords
{
   float *vertex;
   float *color;
   float *tex_coord;
   float *lut_tex_coord;
   unsigned vertices;
   unsigned *index;
   unsigned indexes;
} gfx_mut_coords_t;

typedef struct gfx_coord_array
{
   gfx_mut_coords_t coords;
   unsigned allocated;
} gfx_coord_array_t;

typedef struct gfx_raster_block
{
   bool fullscreen;
   gfx_coord_array_t carr;
} gfx_font_raster_block_t;

bool gfx_coord_array_append(gfx_coord_array_t *ca,
      const gfx_coords_t *coords, unsigned count);

void gfx_coord_array_free(gfx_coord_array_t *ca);

#ifdef __cplusplus
}
#endif

#endif
