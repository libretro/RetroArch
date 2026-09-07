/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dsp_filter.h).
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

#ifndef __LIBRETRO_SDK_AUDIO_DSP_FILTER_H
#define __LIBRETRO_SDK_AUDIO_DSP_FILTER_H

#include <retro_common_api.h>

#include <stdint.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct retro_dsp_filter retro_dsp_filter_t;

retro_dsp_filter_t *retro_dsp_filter_new(const char *filter_config,
      void *string_data, float sample_rate);

void retro_dsp_filter_free(retro_dsp_filter_t *dsp);

/**
 * A struct that groups the input and output of a DSP filter.
 */
struct retro_dsp_data
{
   float *input;
   unsigned input_frames;

   /* Set by retro_dsp_filter_process(). */
   float *output;
   unsigned output_frames;
};

void retro_dsp_filter_process(retro_dsp_filter_t *dsp,
      struct retro_dsp_data *data);

/**
 * int16 counterpart of struct retro_dsp_data.
 */
struct retro_dsp_data_int16
{
   int16_t *input;
   unsigned input_frames;

   /* Set by retro_dsp_filter_process_int16(). */
   int16_t *output;
   unsigned output_frames;
};

/**
 * retro_dsp_filter_supports_int16:
 *
 * Returns true only if there is at least one filter instance and every filter
 * in the chain provides an int16 processing entry point (API version >= 2).
 * When true, the whole chain can run integer-only via
 * retro_dsp_filter_process_int16(); otherwise the float path must be used.
 */
bool retro_dsp_filter_supports_int16(retro_dsp_filter_t *dsp);

/**
 * Processes the chain entirely in int16. Only valid when
 * retro_dsp_filter_supports_int16() returns true.
 */
void retro_dsp_filter_process_int16(retro_dsp_filter_t *dsp,
      struct retro_dsp_data_int16 *data);

RETRO_END_DECLS

#endif
