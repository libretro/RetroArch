/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __SSNES_RESAMPLER_H
#define __SSNES_RESAMPLER_H

#include <stddef.h>
#include <math.h>

// M_PI is left out of ISO C99 :(
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif

typedef struct ssnes_resampler ssnes_resampler_t;

struct resampler_data
{
   const float *data_in;
   float *data_out;

   size_t input_frames;
   size_t output_frames;

   double ratio;
};

ssnes_resampler_t *resampler_new(void);
void resampler_process(ssnes_resampler_t *re, struct resampler_data *data);
void resampler_free(ssnes_resampler_t *re);

// Generate a starting cosine pulse with given frequency for testing (SNR, etc) purposes.
void resampler_preinit(ssnes_resampler_t *re, double omega, unsigned *samples_offset);

#endif

