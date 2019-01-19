/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (echo.c).
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

#include <stdlib.h>

#include <retro_miscellaneous.h>
#include <libretro_dspfilter.h>

struct echo_channel
{
   float *buffer;
   unsigned ptr;
   unsigned frames;
   float feedback;
};

struct echo_data
{
   struct echo_channel *channels;
   unsigned num_channels;
   float amp;
};

static void echo_free(void *data)
{
   unsigned i;
   struct echo_data *echo = (struct echo_data*)data;

   for (i = 0; i < echo->num_channels; i++)
      free(echo->channels[i].buffer);
   free(echo->channels);
   free(echo);
}

static void echo_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i, c;
   float *out             = NULL;
   struct echo_data *echo = (struct echo_data*)data;

   output->samples        = input->samples;
   output->frames         = input->frames;

   out                    = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float left, right;
      float echo_left  = 0.0f;
      float echo_right = 0.0f;

      for (c = 0; c < echo->num_channels; c++)
      {
         echo_left  += echo->channels[c].buffer[(echo->channels[c].ptr << 1) + 0];
         echo_right += echo->channels[c].buffer[(echo->channels[c].ptr << 1) + 1];
      }

      echo_left  *= echo->amp;
      echo_right *= echo->amp;

      left        = out[0] + echo_left;
      right       = out[1] + echo_right;

      for (c = 0; c < echo->num_channels; c++)
      {
         float feedback_left  = out[0] + echo->channels[c].feedback * echo_left;
         float feedback_right = out[1] + echo->channels[c].feedback * echo_right;

         echo->channels[c].buffer[(echo->channels[c].ptr << 1) + 0] = feedback_left;
         echo->channels[c].buffer[(echo->channels[c].ptr << 1) + 1] = feedback_right;

         echo->channels[c].ptr = (echo->channels[c].ptr + 1) % echo->channels[c].frames;
      }

      out[0] = left;
      out[1] = right;
   }
}

static void *echo_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   unsigned i, channels;
   struct echo_channel *echo_channels    = NULL;
   float *delay                          = NULL;
   float *feedback                       = NULL;
   unsigned num_delay                    = 0;
   unsigned num_feedback                 = 0;

   static const float default_delay[]    = { 200.0f };
   static const float default_feedback[] = { 0.5f };
   struct echo_data *echo                = (struct echo_data*)
      calloc(1, sizeof(*echo));

   if (!echo)
      return NULL;

   config->get_float_array(userdata, "delay", &delay,
         &num_delay, default_delay, 1);
   config->get_float_array(userdata, "feedback", &feedback,
         &num_feedback, default_feedback, 1);
   config->get_float(userdata, "amp", &echo->amp, 0.2f);

   channels       = num_feedback = num_delay = MIN(num_delay, num_feedback);

   echo_channels = (struct echo_channel*)calloc(channels,
         sizeof(*echo_channels));

   if (!echo_channels)
      goto error;

   echo->channels     = echo_channels;
   echo->num_channels = channels;

   for (i = 0; i < channels; i++)
   {
      unsigned frames = (unsigned)(delay[i] * info->input_rate / 1000.0f + 0.5f);
      if (!frames)
         goto error;

      echo->channels[i].buffer = (float*)calloc(frames, 2 * sizeof(float));
      if (!echo->channels[i].buffer)
         goto error;

      echo->channels[i].frames = frames;
      echo->channels[i].feedback = feedback[i];
   }

   config->free(delay);
   config->free(feedback);
   return echo;

error:
   config->free(delay);
   config->free(feedback);
   echo_free(echo);
   return NULL;
}

static const struct dspfilter_implementation echo_plug = {
   echo_init,
   echo_process,
   echo_free,

   DSPFILTER_API_VERSION,
   "Multi-Echo",
   "echo",
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation echo_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &echo_plug;
}

#undef dspfilter_get_implementation
