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


#ifndef __RARCH_RESAMPLER_H
#define __RARCH_RESAMPLER_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <math.h>

// M_PI is left out of ISO C99 :(
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif

typedef struct rarch_resampler rarch_resampler_t;
typedef float sample_t;

struct resampler_data
{
   const sample_t *data_in;
   sample_t *data_out;

   size_t input_frames;
   size_t output_frames;

   double ratio;
};

rarch_resampler_t *resampler_new(void);
void resampler_process(rarch_resampler_t *re, struct resampler_data *data);
void resampler_free(rarch_resampler_t *re);

#endif

