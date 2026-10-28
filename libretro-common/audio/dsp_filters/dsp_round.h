/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dsp_round.h).
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

#ifndef __LIBRETRO_SDK_DSP_ROUND_H
#define __LIBRETRO_SDK_DSP_ROUND_H

#include <stdint.h>
#include <retro_inline.h>

/* v / 2^s rounded to the nearest integer, halves away from zero, for
 * 1 <= s <= 62 and |v| < 2^63 - 2^(s - 1): the same number as
 *
 *    (v >= 0) ? ((v + h) >> s) : -(((-v) + h) >> s),  h = 2^(s - 1)
 *
 * without its branch, which the int16 paths took on every sample. It
 * is floor((v + h - (v < 0)) / 2^s), v >> 63 being -1 or 0. This takes
 * >> of a negative value as the arithmetic shift it is on every
 * compiler RetroArch is built with (GCC, Clang, MSVC and the console
 * toolchains); the output is the same everywhere. */
static INLINE int64_t dsp_round_shift(int64_t v, unsigned s)
{
   return (v + ((int64_t)1 << (s - 1)) + (v >> 63)) >> s;
}

#endif
