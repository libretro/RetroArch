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

typedef struct rarch_fft rarch_fft_t;

// C99 <complex.h> would be nice.
typedef struct
{
   float real;
   float imag;
} rarch_fft_complex_t;

static inline rarch_fft_complex_t rarch_fft_complex_mul(rarch_fft_complex_t a,
      rarch_fft_complex_t b)
{
   rarch_fft_complex_t out = {
      a.real * b.real - a.imag * b.imag,
      a.imag * b.real + a.real * b.imag,
   };

   return out;

}

static inline rarch_fft_complex_t rarch_fft_complex_add(rarch_fft_complex_t a,
      rarch_fft_complex_t b)
{
   rarch_fft_complex_t out = {
      a.real + b.real,
      a.imag + b.imag,
   };

   return out;

}

static inline rarch_fft_complex_t rarch_fft_complex_sub(rarch_fft_complex_t a,
      rarch_fft_complex_t b)
{
   rarch_fft_complex_t out = {
      a.real - b.real,
      a.imag - b.imag,
   };

   return out;

}

static inline rarch_fft_complex_t rarch_fft_complex_conj(rarch_fft_complex_t a)
{
   rarch_fft_complex_t out = {
      a.real, -a.imag,
   };

   return out;
}

rarch_fft_t *rarch_fft_new(unsigned block_size_log2);

void rarch_fft_free(rarch_fft_t *fft);

void rarch_fft_process_forward_complex(rarch_fft_t *fft,
      rarch_fft_complex_t *out, const rarch_fft_complex_t *in);

void rarch_fft_process_forward(rarch_fft_t *fft,
      rarch_fft_complex_t *out, const float *in);

void rarch_fft_process_inverse(rarch_fft_t *fft,
      float *out, const rarch_fft_complex_t *in);


#endif

