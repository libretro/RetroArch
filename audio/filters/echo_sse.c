/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
 *
 */

#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rarch_dsp.h"

#include <emmintrin.h>

// 4 source echo.

#ifdef __GNUC__
#define ALIGNED __attribute__((aligned(16)))
#else
#define ALIGNED
#endif

#define ECHO_MS 150
#define AMP 0.0

struct echo_filter_data
{
   float echo_buffer[4][0x10000] ALIGNED;
   float buffer[4096] ALIGNED;
   float scratch_buf[4] ALIGNED;

   unsigned buf_size[4];
   unsigned ptr[4];

   unsigned scratch_ptr;
   __m128 amp[4] ALIGNED;
   __m128 feedback ALIGNED;
   float input_rate;
};

void echo_init(void *data)
{
   unsigned i, j;
   struct echo_filter_data *echo = (struct echo_filter_data*)data;

   for (i = 0; i < 4; i++)
   {
      echo->ptr[i] = 0.0f;
      echo->amp[i] = _mm_set1_ps(AMP);
   }

   echo->scratch_ptr = 0;
   echo->feedback = _mm_set1_ps(0.0f);
   echo->input_rate = 32000.0;

   for (i = 0; i < 4; i++)
   {
      echo->scratch_buf[i] = 0.0f;

      for (j = 0; j < 0x10000; j++)
         echo->echo_buffer[i][j] = 0.0f;
   }

   for (i = 0; i < 4096; i++)
      echo->buffer[i] = 0.0f;
}

unsigned echo_sse2_process(void *data, const float *input, unsigned frames)
{
   unsigned frames_out, i;
   float *buffer_out;
   struct echo_filter_data *echo = (struct echo_filter_data*)data;

   frames_out = 0;
   buffer_out = echo->buffer;

   __m128 amp[4] = {
      echo->amp[0],
      echo->amp[1],
      echo->amp[2],
      echo->amp[3],
   };

   __m128 feedback = echo->feedback;

#define DO_FILTER() \
   __m128 result[4]; \
   __m128 echo_[4]; \
   for (i = 0; i < 4; i++) \
   { \
      echo_[i] = _mm_load_ps(echo->echo_buffer[i] + echo->ptr[i]); \
      result[i] = _mm_mul_ps(amp[i], echo_[i]); \
   } \
   __m128 final_result = _mm_add_ps(_mm_add_ps(result[0], result[1]), _mm_add_ps(result[2], result[3])); \
   __m128 feedback_result = _mm_mul_ps(feedback, final_result); \
   final_result = _mm_add_ps(reg, final_result); \
   feedback_result = _mm_add_ps(reg, feedback_result); \
   for (i = 0; i < 4; i++) \
   _mm_store_ps(echo->echo_buffer[i] + echo->ptr[i], feedback_result); \
   _mm_store_ps(buffer_out, final_result); \
   for (i = 0; i < 4; i++) \
   echo->ptr[i] = (echo->ptr[i] + 4) % echo->buf_size[i]


   // Fill up scratch buffer and flush.
   if (echo->scratch_ptr)
   {
      for (i = echo->scratch_ptr; i < 4; i += 2)
      {
         echo->scratch_buf[i]     = *input++;
         echo->scratch_buf[i + 1] = *input++;
         frames--;
      }

      echo->scratch_ptr = 0;

      __m128 reg = _mm_load_ps(echo->scratch_buf);

      DO_FILTER();

      frames_out += 2;
      buffer_out += 4;
   }

   // Main processing.
   for (i = 0; (i + 4) <= (frames * 2); i += 4, input += 4, buffer_out += 4, frames_out += 2)
   {
      __m128 reg = _mm_loadu_ps(input); // Might not be aligned.
      DO_FILTER();
   }

   // Flush rest to scratch buffer.
   for (; i < (frames * 2); i++)
      echo->scratch_buf[echo->scratch_ptr++] = *input++;

   return frames_out;
}

static void dsp_process(void *data, rarch_dsp_output_t *output,
      const rarch_dsp_input_t *input)
{
   struct echo_filter_data *echo = (struct echo_filter_data*)data;
   output->samples = echo->buffer;
   output->frames = echo_sse2_process(echo, input->samples, input->frames);
}

static void dsp_free(void *data)
{
   struct echo_filter_data *echo = (struct echo_filter_data*)data;

   if (echo)
      free(echo);
}

static void *dsp_init(const rarch_dsp_info_t *info)
{
   struct echo_filter_data *echo = (struct echo_filter_data*)calloc(1, sizeof(*echo));;

   if (!echo)
      return NULL;

   for (unsigned i = 0; i < 4; i++)
      echo->buf_size[i] = ECHO_MS * (info->input_rate * 2) / 1000;

   echo_init(echo);

   echo->input_rate = info->input_rate;

   fprintf(stderr, "[Echo] loaded!\n");

   return echo;
}

static void dsp_config(void *data)
{
   (void)data;
}

static const rarch_dsp_plugin_t dsp_plug = {
   dsp_init,
   dsp_process,
   dsp_free,
   RARCH_DSP_API_VERSION,
   dsp_config,
   "Echo (SSE2)"
};

RARCH_API_EXPORT const rarch_dsp_plugin_t* RARCH_API_CALLTYPE
   rarch_dsp_plugin_init(void)
{
   return &dsp_plug;
}

