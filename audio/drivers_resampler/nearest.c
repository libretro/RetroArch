/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2016 - Ali Bouhlel ( aliaspider@gmail.com )
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

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
 
#include "../audio_resampler_driver.h"
typedef struct rarch_nearest_resampler
{
   float fraction;
} rarch_nearest_resampler_t;
 
static void resampler_nearest_process(
      void *re_, struct resampler_data *data)
{
   rarch_nearest_resampler_t *re = (rarch_nearest_resampler_t*)re_;
   audio_frame_float_t  *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t  *inp_max = (audio_frame_float_t*)inp + data->input_frames;
   audio_frame_float_t  *outp    = (audio_frame_float_t*)data->data_out;
   float                   ratio = 1.0 / data->ratio;
 
   while(inp != inp_max)
   {
      while(re->fraction > 1)
      {
         *outp++ = *inp;
         re->fraction -= ratio;
      }
      re->fraction++;
      inp++;      
   }
   
   data->output_frames = (outp - (audio_frame_float_t*)data->data_out);
}
 
static void resampler_nearest_free(void *re_)
{
   rarch_nearest_resampler_t *re = (rarch_nearest_resampler_t*)re_;
   if (re)
      free(re);
}
 
static void *resampler_nearest_init(const struct resampler_config *config,
      double bandwidth_mod, resampler_simd_mask_t mask)
{
   rarch_nearest_resampler_t *re = (rarch_nearest_resampler_t*)
      calloc(1, sizeof(rarch_nearest_resampler_t));

   (void)config;
   (void)mask;

   if (!re)
      return NULL;
   
   re->fraction = 0;
   
   return re;
}
 
rarch_resampler_t nearest_resampler = {
   resampler_nearest_init,
   resampler_nearest_process,
   resampler_nearest_free,
   RESAMPLER_API_VERSION,
   "nearest",
   "nearest"
};
