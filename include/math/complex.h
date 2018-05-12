/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (complex.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __LIBRETRO_SDK_MATH_COMPLEX_H__
#define __LIBRETRO_SDK_MATH_COMPLEX_H__

#include <stdint.h>

#include <retro_inline.h>

typedef struct
{
   float real;
   float imag;
} fft_complex_t;

static INLINE fft_complex_t fft_complex_mul(fft_complex_t a,
      fft_complex_t b)
{
   fft_complex_t out;
   out.real = a.real * b.real - a.imag * b.imag;
   out.imag = a.imag * b.real + a.real * b.imag;

   return out;

}

static INLINE fft_complex_t fft_complex_add(fft_complex_t a,
      fft_complex_t b)
{
   fft_complex_t out;
   out.real = a.real + b.real;
   out.imag = a.imag + b.imag;

   return out;

}

static INLINE fft_complex_t fft_complex_sub(fft_complex_t a,
      fft_complex_t b)
{
   fft_complex_t out;
   out.real = a.real - b.real;
   out.imag = a.imag - b.imag;

   return out;

}

static INLINE fft_complex_t fft_complex_conj(fft_complex_t a)
{
   fft_complex_t out;
   out.real = a.real;
   out.imag = -a.imag;

   return out;
}

#endif
