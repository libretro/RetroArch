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

#include "video_pixel_converter.h"
#include <gfx/scaler/pixconv.h>
#include "../general.h"

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
   global_t *global = global_get_ptr();

   /* This function can be called multiple times
    * without deiniting first on consoles. */
   deinit_pixel_converter();

   /* If pixel format is not 0RGB1555, we don't need to do
    * any internal pixel conversion. */
   if (global->system.pix_fmt != RETRO_PIXEL_FORMAT_0RGB1555)
      return true;

   RARCH_WARN("0RGB1555 pixel format is deprecated, and will be slower. For 15/16-bit, RGB565 format is preferred.\n");

   driver->scaler.scaler_type = SCALER_TYPE_POINT;
   driver->scaler.in_fmt      = SCALER_FMT_0RGB1555;

   /* TODO: Pick either ARGB8888 or RGB565 depending on driver. */
   driver->scaler.out_fmt     = SCALER_FMT_RGB565;

   if (!scaler_ctx_gen_filter(&driver->scaler))
      return false;

   driver->scaler_out = calloc(sizeof(uint16_t), size * size);

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
