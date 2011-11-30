/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 * 
 *  RSound is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RSound is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RSound.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "resampler.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define SAMPLES_TO_FRAMES(x,y) ((x)/(y)->channels)
#define FRAMES_TO_SAMPLES(x,y) ((x)*(y)->channels)

struct resampler
{
   float *data;
   double ratio;
   size_t data_ptr;
   size_t data_size;
   void *cb_data;
   int channels;
   resampler_cb_t func;
   uint64_t sum_output_frames;
   uint64_t sum_input_frames;
};

resampler_t* resampler_new(resampler_cb_t func, double ratio, int channels, void* cb_data)
{
   if (func == NULL)
      return NULL;

   if (channels < 1)
      return NULL;

   resampler_t* state = calloc(1, sizeof(resampler_t));
   if (state == NULL)
      return NULL;

   state->func = func;
   state->ratio = ratio;
   state->channels = channels;
   state->cb_data = cb_data;

   return state;
}

void resampler_free(resampler_t* state)
{
   if (state && state->data)
      free(state->data);
   if (state)
      free(state);
}

void resampler_float_to_s16(int16_t * restrict out, const float * restrict in, size_t samples)
{
   for (int i = 0; i < (int)samples; i++)
   {
      int32_t temp = in[i] * 0x7FFF; 
      if (temp > 0x7FFE)
         out[i] = 0x7FFE;
      else if (temp < -0x7FFF)
         out[i] = -0x7FFF;
      else
         out[i] = (int16_t)temp;
   }
}

void resampler_s16_to_float(float * restrict out, const int16_t * restrict in, size_t samples)
{
   for (int i = 0; i < (int)samples; i++)
      out[i] = (float)in[i]/0x7FFF;
}

static size_t resampler_get_required_frames(resampler_t* state, size_t frames)
{
   assert(state);

   size_t after_sum = state->sum_output_frames + frames;

   size_t min_input_frames = (size_t)((after_sum / state->ratio) + 2.0);
   return min_input_frames - state->sum_input_frames;
}

static void poly_create_3(float *poly, float *y)
{
   poly[2] = (y[0] - 2*y[1] + y[2])/2;
   poly[1] = -1.5*y[0] + 2*y[1] - 0.5*y[2];
   poly[0] = y[0];
}

static size_t resampler_process(resampler_t *state, size_t frames, float *out_data)
{
   size_t frames_used = 0;
   uint64_t pos_out;
   double pos_in = 0.0;

#if 0
   fprintf(stderr, "=========================================\n");
   fprintf(stderr, "Output: %zu frames.\n", frames);
   fprintf(stderr, "Output frames: %zu - %zu\n", state->sum_output_frames, state->sum_output_frames + frames);
   fprintf(stderr, "Needed input frames: %zu - %zu\n", (size_t)(state->sum_output_frames/state->ratio), (size_t)((state->sum_output_frames + frames)/state->ratio + 1.0));
   fprintf(stderr, "Current input frames: %zu - %zu\n", state->sum_input_frames, state->sum_input_frames + SAMPLES_TO_FRAMES(state->data_ptr, state));
   fprintf(stderr, "=========================================\n");

   assert(state->sum_input_frames <= (size_t)(state->sum_output_frames/state->ratio));
   assert(state->sum_input_frames + SAMPLES_TO_FRAMES(state->data_ptr, state) - 1 >= (size_t)((state->sum_output_frames + frames - 1)/state->ratio + 1.0));
#endif

   for (uint64_t x = state->sum_output_frames; x < state->sum_output_frames + frames; x++)
   {
      pos_out = x - state->sum_output_frames;
      pos_in  = ((double)x / state->ratio) - (double)state->sum_input_frames;
      //pos_in = pos_out / state->ratio;
      //fprintf(stderr, "pos_in: %15.7lf\n", pos_in + state->sum_input_frames);
      for (int c = 0; c < state->channels; c++)
      {


         float poly[3];
         float data[3];
         float x_val;

         if ((int)pos_in == 0)
         {
            data[0] = state->data[0 * state->channels + c];
            data[1] = state->data[1 * state->channels + c];
            data[2] = state->data[2 * state->channels + c];
            x_val = pos_in;
         }
         else
         {
            data[0] = state->data[((int)pos_in - 1) * state->channels + c];
            data[1] = state->data[((int)pos_in + 0) * state->channels + c];
            data[2] = state->data[((int)pos_in + 1) * state->channels + c];
            x_val = pos_in - (int)pos_in + 1.0;
         }

         poly_create_3(poly, data);
         
         out_data[pos_out * state->channels + c] = poly[2] * x_val * x_val + poly[1] * x_val + poly[0];
      }
   }
   frames_used = (int)pos_in;
   return frames_used;
}

ssize_t resampler_cb_read(resampler_t *state, size_t frames, float *data)
{
   assert(state);
   assert(data);


   // How many frames must we have to resample?
   size_t req_frames = resampler_get_required_frames(state, frames);

   // Do we need to read more data?
   if (SAMPLES_TO_FRAMES(state->data_ptr, state) < req_frames)
   {
      size_t must_read = req_frames - SAMPLES_TO_FRAMES(state->data_ptr, state);
      float temp_buf[FRAMES_TO_SAMPLES(must_read, state)];

      size_t has_read = 0;

      size_t copy_size = 0;
      size_t ret = 0;
      float *ptr = NULL;
      while (has_read < must_read)
      {
         ret = state->func(state->cb_data, &ptr);

         if (ret == 0 || ptr == NULL) // We're done.
            return -1;

         copy_size = (ret > must_read - has_read) ? (must_read - has_read) : ret;
         memcpy(temp_buf + FRAMES_TO_SAMPLES(has_read, state), ptr, FRAMES_TO_SAMPLES(copy_size, state) * sizeof(float));

         has_read += ret;
      }

      // We might have gotten a lot of data from the callback. We should realloc our buffer if needed.
      size_t req_buffer_frames = SAMPLES_TO_FRAMES(state->data_ptr, state) + has_read;

      if (req_buffer_frames > SAMPLES_TO_FRAMES(state->data_size, state))
      {
         state->data = realloc(state->data, FRAMES_TO_SAMPLES(req_buffer_frames, state) * sizeof(float));
         if (state->data == NULL)
            return -1;

         state->data_size = FRAMES_TO_SAMPLES(req_buffer_frames, state);
      }
      
      memcpy(state->data + state->data_ptr, temp_buf, FRAMES_TO_SAMPLES(must_read, state) * sizeof(float));
      state->data_ptr += FRAMES_TO_SAMPLES(must_read, state);

      // We have some data from the callback we need to copy over as well.
      if (ret > copy_size)
      {
         memcpy(state->data + state->data_ptr, ptr + FRAMES_TO_SAMPLES(copy_size, state), FRAMES_TO_SAMPLES(ret - copy_size, state) * sizeof(float));
         state->data_ptr += FRAMES_TO_SAMPLES(ret - copy_size, state);
      }
   }

   // Phew. We should have enough data in our buffer now to be able to process the data we need.

   size_t frames_used = resampler_process(state, frames, data);
   state->sum_input_frames += frames_used;
   memmove(state->data, state->data + FRAMES_TO_SAMPLES(frames_used, state), (state->data_ptr - FRAMES_TO_SAMPLES(frames_used, state)) * sizeof(float));
   state->data_ptr -= FRAMES_TO_SAMPLES(frames_used, state);
   state->sum_output_frames += frames;

   return frames;
}
