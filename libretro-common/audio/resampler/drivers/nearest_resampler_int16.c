/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file
 * (nearest_resampler_int16.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */

#include <stdint.h>
#include <stdlib.h>

#include <audio/nearest_resampler_int16.h>

/* The phase accumulator "fraction" and the per-output step are held in a
 * signed Q32 fixed-point value (int64_t).  This mirrors the float driver's
 * (fraction += 1 per input; fraction -= 1/ratio per output) exactly, but the
 * accumulation is pure integer so the output timing is bit-identical across
 * platforms.  Signed, so heavy downsampling (step > 1.0) can drive the
 * fraction negative without the unsigned wrap that would corrupt the loop. */
#define NEAREST_I16_FRAC_BITS 32

typedef struct nearest_resampler_int16
{
   int64_t fraction; /* Q32 */
} nearest_resampler_int16_t;

void *nearest_resampler_int16_init(void)
{
   nearest_resampler_int16_t *re = (nearest_resampler_int16_t*)
      calloc(1, sizeof(*re));
   if (!re)
      return NULL;
   re->fraction = 0;
   return re;
}

void nearest_resampler_int16_process(void *re_,
      struct resampler_data_int16 *data)
{
   nearest_resampler_int16_t *re = (nearest_resampler_int16_t*)re_;
   const int16_t *inp            = data->data_in;
   const int16_t *inp_max        = inp + data->input_frames * 2;
   int16_t       *outp           = data->data_out;
   int16_t       *outp_first     = outp;
   const int64_t  one            = (int64_t)1 << NEAREST_I16_FRAC_BITS;
   /* step = (1.0 / ratio) in Q32, i.e. input frames consumed per output. */
   int64_t        step           = (int64_t)
      ((1.0 / data->ratio) * (double)one + 0.5);

   while (inp != inp_max)
   {
      while (re->fraction > one)
      {
         outp[0]       = inp[0];
         outp[1]       = inp[1];
         outp         += 2;
         re->fraction -= step;
      }
      re->fraction += one;
      inp          += 2;
   }

   data->output_frames = (size_t)((outp - outp_first) >> 1);
}

void nearest_resampler_int16_free(void *re_)
{
   nearest_resampler_int16_t *re = (nearest_resampler_int16_t*)re_;
   if (re)
      free(re);
}
