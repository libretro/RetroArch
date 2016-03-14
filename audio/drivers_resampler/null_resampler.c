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
typedef struct rarch_null_resampler
{
   void *empty;
} rarch_null_resampler_t;
 
static void resampler_null_process(
      void *re_, struct resampler_data *data)
{
}
 
static void resampler_null_free(void *re_)
{
}
 
static void *resampler_null_init(const struct resampler_config *config,
      double bandwidth_mod, resampler_simd_mask_t mask)
{
   return (void*)0;
}
 
rarch_resampler_t null_resampler = {
   resampler_null_init,
   resampler_null_process,
   resampler_null_free,
   RESAMPLER_API_VERSION,
   "null",
   "null"
};
