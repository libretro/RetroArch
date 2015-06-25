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

#include <stddef.h>

#include <gfx/scaler/pixconv.h>

#include "../general.h"
#include "../performance.h"
#include "video_pixel_converter.h"

void deinit_pixel_converter(void)
{
   driver_t *driver = driver_get_ptr();

   scaler_ctx_gen_reset(&driver->scaler);
   memset(&driver->scaler, 0, sizeof(driver->scaler));
   free(driver->scaler_out);
   driver->scaler_out = NULL;
}

bool init_video_pixel_converter(unsigned size)
{
   driver_t *driver = driver_get_ptr();

   /* This function can be called multiple times
    * without deiniting first on consoles. */
   deinit_pixel_converter();

   /* If pixel format is not 0RGB1555, we don't need to do
    * any internal pixel conversion. */
   if (video_driver_get_pixel_format() != RETRO_PIXEL_FORMAT_0RGB1555)
      return true;

   RARCH_WARN("0RGB1555 pixel format is deprecated, and will be slower. For 15/16-bit, RGB565 format is preferred.\n");

   driver->scaler.scaler_type = SCALER_TYPE_POINT;
   driver->scaler.in_fmt      = SCALER_FMT_0RGB1555;

   /* TODO: Pick either ARGB8888 or RGB565 depending on driver. */
   driver->scaler.out_fmt     = SCALER_FMT_RGB565;

   if (!scaler_ctx_gen_filter(&driver->scaler))
      return false;

   driver->scaler_out = calloc(sizeof(uint16_t), size * size);

   if (!driver->scaler_out)
      return false;

   return true;
}

unsigned video_pixel_get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

bool video_pixel_frame_scale(const void *data,
      unsigned width, unsigned height,
      size_t pitch)
{
   driver_t *driver = driver_get_ptr();

   RARCH_PERFORMANCE_INIT(video_frame_conv);

   if (!data)
      return false;
   if (video_driver_get_pixel_format() != RETRO_PIXEL_FORMAT_0RGB1555)
      return false;
   if (data == RETRO_HW_FRAME_BUFFER_VALID)
      return false;

   RARCH_PERFORMANCE_START(video_frame_conv);

   driver->scaler.in_width      = width;
   driver->scaler.in_height     = height;
   driver->scaler.out_width     = width;
   driver->scaler.out_height    = height;
   driver->scaler.in_stride     = pitch;
   driver->scaler.out_stride    = width * sizeof(uint16_t);

   scaler_ctx_scale(&driver->scaler, driver->scaler_out, data);

   RARCH_PERFORMANCE_STOP(video_frame_conv);

   return true;
}
