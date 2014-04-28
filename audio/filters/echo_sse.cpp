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
#include <algorithm>
#include <iterator>
#include <type_traits>

#include <emmintrin.h>

// 4 source echo.

#define ALIGNED __attribute__((aligned(16))) // Should use C++11 alignas(), but doesn't seem to work :(

#define ECHO_MS 150
#define AMP 0.0

struct EchoFilter
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

   EchoFilter()
   {
      unsigned i, j;

      for (i = 0; i < 4; i++)
      {
         ptr[i] = 0.0f;
         amp[i] = _mm_set1_ps(AMP);
      }

      scratch_ptr = 0;
      feedback = _mm_set1_ps(0.0f);

      input_rate = 32000.0;

      for (i = 0; i < 4; i++)
      {
         scratch_buf[i] = 0.0f;

         for (j = 0; j < 0x10000; j++)
            echo_buffer[i][j] = 0.0f;
      }
      for (i = 0; i < 4096; i++)
         buffer[i] = 0.0f;
   }

   ~EchoFilter()
   {
   }

   unsigned Process(const float *input, unsigned frames)
   {
      unsigned frames_out = 0;
      float *buffer_out = buffer;

      __m128 amp[4] = {
         this->amp[0],
         this->amp[1],
         this->amp[2],
         this->amp[3],
      };

      __m128 feedback = this->feedback;

#define DO_FILTER() \
      __m128 result[4]; \
      __m128 echo_[4]; \
      for (unsigned i = 0; i < 4; i++) \
      { \
         echo_[i] = _mm_load_ps(echo_buffer[i] + ptr[i]); \
         result[i] = _mm_mul_ps(amp[i], echo_[i]); \
      } \
      __m128 final_result = _mm_add_ps(_mm_add_ps(result[0], result[1]), _mm_add_ps(result[2], result[3])); \
      __m128 feedback_result = _mm_mul_ps(feedback, final_result); \
      final_result = _mm_add_ps(reg, final_result); \
      feedback_result = _mm_add_ps(reg, feedback_result); \
      for (unsigned i = 0; i < 4; i++) \
         _mm_store_ps(echo_buffer[i] + ptr[i], feedback_result); \
      _mm_store_ps(buffer_out, final_result); \
      for (unsigned i = 0; i < 4; i++) \
         ptr[i] = (ptr[i] + 4) % buf_size[i]


      // Fill up scratch buffer and flush.
      if (scratch_ptr)
      {
         for (unsigned i = scratch_ptr; i < 4; i += 2)
         {
            scratch_buf[i] = *input++;
            scratch_buf[i + 1] = *input++;
            frames--;
         }

         scratch_ptr = 0;

         __m128 reg = _mm_load_ps(scratch_buf);

         DO_FILTER();

         frames_out += 2;
         buffer_out += 4;
      }

      // Main processing.
      unsigned i;
      for (i = 0; (i + 4) <= (frames * 2); i += 4, input += 4, buffer_out += 4, frames_out += 2)
      {
         __m128 reg = _mm_loadu_ps(input); // Might not be aligned.
         DO_FILTER();
      }

      // Flush rest to scratch buffer.
      for (; i < (frames * 2); i++)
         scratch_buf[scratch_ptr++] = *input++;

      return frames_out;
   }
};

static void dsp_process(void *data, rarch_dsp_output_t *output,
      const rarch_dsp_input_t *input)
{
   EchoFilter *echo = reinterpret_cast<EchoFilter*>(data);
   output->samples = echo->buffer;

   output->frames = echo->Process(input->samples, input->frames);
}

static void dsp_free(void *data)
{
   delete reinterpret_cast<EchoFilter*>(data);
}

static void *dsp_init(const rarch_dsp_info_t *info)
{
   EchoFilter *echo = new EchoFilter;

   echo->input_rate = info->input_rate;

   for (unsigned i = 0; i < 4; i++)
      echo->buf_size[i] = ECHO_MS * (info->input_rate * 2) / 1000;

   fprintf(stderr, "[Echo] loaded!\n");

   return echo;
}

static void dsp_config(void *)
{}

static const rarch_dsp_plugin_t dsp_plug = {
   dsp_init,
   dsp_process,
   dsp_free,
   RARCH_DSP_API_VERSION,
   dsp_config,
   "Echo plugin (SSE2)"
};

RARCH_API_EXPORT const rarch_dsp_plugin_t* RARCH_API_CALLTYPE
   rarch_dsp_plugin_init(void) { return &dsp_plug; }

