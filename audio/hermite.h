/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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

// Hermite resampler based on bsnes' audio library.

#ifndef __SSNES_HERMITE_H
#define __SSNES_HERMITE_H

#include <stddef.h>

typedef struct hermite_resampler hermite_resampler_t;

hermite_resampler_t *hermite_new(void);

struct hermite_data
{
   const float *in_data;
   float *out_data; // We make it really simple and assume that there is always enough space. :)

   size_t in_frames;
   double ratio;
};

size_t hermite_process(hermite_resampler_t *re, const struct hermite_data *data);
void hermite_free(hermite_resampler_t *re);

#endif

