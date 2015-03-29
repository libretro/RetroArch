/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef RARCH_FFT_H__
#define RARCH_FFT_H__

#include <retro_inline.h>

typedef struct fft fft_t;

// C99 <complex.h> would be nice.
typedef struct
{
   float real;
   float imag;
} fft_complex_t;

static INLINE fft_complex_t fft_complex_mul(fft_complex_t a,
      fft_complex_t b)
{
   fft_complex_t out = {
      a.real * b.real - a.imag * b.imag,
      a.imag * b.real + a.real * b.imag,
   };

   return out;

}

static INLINE fft_complex_t fft_complex_add(fft_complex_t a,
      fft_complex_t b)
{
   fft_complex_t out = {
      a.real + b.real,
      a.imag + b.imag,
   };

   return out;

}

static INLINE fft_complex_t fft_complex_sub(fft_complex_t a,
      fft_complex_t b)
{
   fft_complex_t out = {
      a.real - b.real,
      a.imag - b.imag,
   };

   return out;

}

static INLINE fft_complex_t fft_complex_conj(fft_complex_t a)
{
   fft_complex_t out = {
      a.real, -a.imag,
   };

   return out;
}

fft_t *fft_new(unsigned block_size_log2);

void fft_free(fft_t *fft);

void fft_process_forward_complex(fft_t *fft,
      fft_complex_t *out, const fft_complex_t *in, unsigned step);

void fft_process_forward(fft_t *fft,
      fft_complex_t *out, const float *in, unsigned step);

void fft_process_inverse(fft_t *fft,
      float *out, const fft_complex_t *in, unsigned step);


#endif

