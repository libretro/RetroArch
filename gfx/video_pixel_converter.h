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

#ifndef _VIDEO_PIXEL_CONVERTER_H
#define _VIDEO_PIXEL_CONVERTER_H

#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct video_pixel_scaler
{
   struct scaler_ctx *scaler;
   void *scaler_out;
} video_pixel_scaler_t;

void deinit_pixel_converter(void);

bool init_video_pixel_converter(unsigned size);

unsigned video_pixel_get_alignment(unsigned pitch);

bool video_pixel_frame_scale(const void *data,
      unsigned width, unsigned height,
      size_t pitch);

video_pixel_scaler_t *scaler_get_ptr(void);

#ifdef __cplusplus
}
#endif

#endif
