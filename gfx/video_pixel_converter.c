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
#include "../verbosity.h"
#include "video_pixel_converter.h"

/* Used for 16-bit -> 16-bit conversions that take place before
 * being passed to video driver. */
static video_pixel_scaler_t *scaler_ptr;

video_pixel_scaler_t *scaler_get_ptr(void)
{
   return scaler_ptr;
}

void deinit_pixel_converter(void)
{
   if (!scaler_ptr)
      return;

   scaler_ctx_gen_reset(scaler_ptr->scaler);

   if (scaler_ptr->scaler)
      free(scaler_ptr->scaler);
   if (scaler_ptr->scaler_out)
      free(scaler_ptr->scaler_out);
   if (scaler_ptr)
      free(scaler_ptr);

   scaler_ptr->scaler     = NULL;
   scaler_ptr->scaler_out = NULL;
   scaler_ptr             = NULL;
}

bool init_video_pixel_converter(unsigned size)
{
   /* This function can be called multiple times
    * without deiniting first on consoles. */
   deinit_pixel_converter();

   /* If pixel format is not 0RGB1555, we don't need to do
    * any internal pixel conversion. */
   if (video_driver_get_pixel_format() != RETRO_PIXEL_FORMAT_0RGB1555)
      return true;

   RARCH_WARN("0RGB1555 pixel format is deprecated, and will be slower. For 15/16-bit, RGB565 format is preferred.\n");

   scaler_ptr = (video_pixel_scaler_t*)calloc(1, sizeof(*scaler_ptr));

   if (!scaler_ptr)
      goto error;

   scaler_ptr->scaler = (struct scaler_ctx*)calloc(1, sizeof(*scaler_ptr->scaler));

   if (!scaler_ptr->scaler)
      goto error;

   scaler_ptr->scaler->scaler_type = SCALER_TYPE_POINT;
   scaler_ptr->scaler->in_fmt      = SCALER_FMT_0RGB1555;

   /* TODO: Pick either ARGB8888 or RGB565 depending on driver. */
   scaler_ptr->scaler->out_fmt     = SCALER_FMT_RGB565;

   if (!scaler_ctx_gen_filter(scaler_ptr->scaler))
      goto error;

   scaler_ptr->scaler_out = calloc(sizeof(uint16_t), size * size);

   if (!scaler_ptr->scaler_out)
      goto error;

   return true;

error:
   if (scaler_ptr->scaler_out)
      free(scaler_ptr->scaler_out);
   if (scaler_ptr->scaler)
      free(scaler_ptr->scaler);
   if (scaler_ptr)
      free(scaler_ptr);

   scaler_ptr = NULL;

   return false;
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
   static struct retro_perf_counter video_frame_conv = {0};
   video_pixel_scaler_t *scaler = scaler_get_ptr();

   rarch_perf_init(&video_frame_conv, "video_frame_conv");

   if (!data)
      return false;
   if (video_driver_get_pixel_format() != RETRO_PIXEL_FORMAT_0RGB1555)
      return false;
   if (data == RETRO_HW_FRAME_BUFFER_VALID)
      return false;

   retro_perf_start(&video_frame_conv);

   scaler->scaler->in_width      = width;
   scaler->scaler->in_height     = height;
   scaler->scaler->out_width     = width;
   scaler->scaler->out_height    = height;
   scaler->scaler->in_stride     = pitch;
   scaler->scaler->out_stride    = width * sizeof(uint16_t);

   scaler_ctx_scale(scaler->scaler, scaler->scaler_out, data);

   retro_perf_stop(&video_frame_conv);

   return true;
}
