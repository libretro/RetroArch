/* Copyright  (C) 2010-2020 The RetroArch team
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
#include <math.h>

#include <retro_miscellaneous.h>
#include <libretro_dspfilter.h>

struct echo_channel
{
   float *buffer;
   /* int64 Q16 mirror of the delay line (headroom + fractional precision so
    * the feedback loop neither clips nor quantizes), and a Q16 feedback gain,
    * for the deterministic int16 path. */
   int64_t *buffer_i;
   int32_t feedback_q;
   unsigned ptr;
   unsigned frames;
   float feedback;
};

struct echo_data
{
   struct echo_channel *channels;
   unsigned num_channels;
   float amp;
   int32_t amp_q;   /* Q16 mirror of amp */
};

static void echo_free(void *data)
{
   unsigned i;
   struct echo_data *echo = (struct echo_data*)data;

   for (i = 0; i < echo->num_channels; i++)
   {
      free(echo->channels[i].buffer);
      free(echo->channels[i].buffer_i);
   }
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

      echo_left     *= echo->amp;
      echo_right    *= echo->amp;

      left           = out[0] + echo_left;
      right          = out[1] + echo_right;

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

/* Deterministic int16 path: same multi-tap feedback echo as echo_process(),
 * with the delay lines and accumulation in Q16 fixed point (int64), the amp
 * and per-tap feedback gains as Q16, and round-half-away-from-zero on every
 * narrowing shift.  Only the final dry+echo sum is saturated to s16. */
static void echo_process_i16(void *data,
      struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i, c;
   int16_t *out           = NULL;
   struct echo_data *echo = (struct echo_data*)data;

   output->samples        = input->samples;
   output->frames         = input->frames;
   out                    = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      int32_t in0        = out[0];
      int32_t in1        = out[1];
      int64_t echo_l     = 0;   /* Q16 */
      int64_t echo_r     = 0;   /* Q16 */
      int64_t p, left, right;
      int32_t v;

      for (c = 0; c < echo->num_channels; c++)
      {
         echo_l += echo->channels[c].buffer_i[(echo->channels[c].ptr << 1) + 0];
         echo_r += echo->channels[c].buffer_i[(echo->channels[c].ptr << 1) + 1];
      }

      /* echo *= amp (Q16 * Q16 -> Q16). */
      p      = echo_l * echo->amp_q;
      echo_l = (p >= 0) ? ((p + 32768) >> 16) : -(((-p) + 32768) >> 16);
      p      = echo_r * echo->amp_q;
      echo_r = (p >= 0) ? ((p + 32768) >> 16) : -(((-p) + 32768) >> 16);

      left   = ((int64_t)in0 << 16) + echo_l;   /* Q16 */
      right  = ((int64_t)in1 << 16) + echo_r;

      for (c = 0; c < echo->num_channels; c++)
      {
         int32_t fbq = echo->channels[c].feedback_q;
         int64_t fl  = echo_l * fbq;   /* Q32 */
         int64_t fr  = echo_r * fbq;
         fl = (fl >= 0) ? ((fl + 32768) >> 16) : -(((-fl) + 32768) >> 16); /* Q16 */
         fr = (fr >= 0) ? ((fr + 32768) >> 16) : -(((-fr) + 32768) >> 16);

         echo->channels[c].buffer_i[(echo->channels[c].ptr << 1) + 0] =
               ((int64_t)in0 << 16) + fl;
         echo->channels[c].buffer_i[(echo->channels[c].ptr << 1) + 1] =
               ((int64_t)in1 << 16) + fr;

         echo->channels[c].ptr =
               (echo->channels[c].ptr + 1) % echo->channels[c].frames;
      }

      v = (left >= 0) ?  (int32_t)(( left + 32768) >> 16)
                      : -(int32_t)((-left + 32768) >> 16);
      if      (v >  32767) v =  32767;
      else if (v < -32768) v = -32768;
      out[0] = (int16_t)v;

      v = (right >= 0) ?  (int32_t)(( right + 32768) >> 16)
                       : -(int32_t)((-right + 32768) >> 16);
      if      (v >  32767) v =  32767;
      else if (v < -32768) v = -32768;
      out[1] = (int16_t)v;
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
   echo->amp_q = (int32_t)floor((double)echo->amp * 65536.0 + 0.5);

   channels            = num_feedback = num_delay = MIN(num_delay, num_feedback);

   if (!(echo_channels = (struct echo_channel*)calloc(channels,
         sizeof(*echo_channels))))
      goto error;

   echo->channels      = echo_channels;
   echo->num_channels  = channels;

   for (i = 0; i < channels; i++)
   {
      unsigned frames  = (unsigned)(delay[i] * info->input_rate / 1000.0f + 0.5f);
      if (!frames)
         goto error;

      if (!(echo->channels[i].buffer = (float*)calloc(frames, 2 * sizeof(float))))
         goto error;

      if (!(echo->channels[i].buffer_i = (int64_t*)calloc(frames, 2 * sizeof(int64_t))))
         goto error;

      echo->channels[i].frames     = frames;
      echo->channels[i].feedback   = feedback[i];
      echo->channels[i].feedback_q =
            (int32_t)floor((double)feedback[i] * 65536.0 + 0.5);
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

   echo_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation echo_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   return &echo_plug;
}

#undef dspfilter_get_implementation
