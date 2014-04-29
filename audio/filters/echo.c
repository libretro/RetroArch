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

// 4 source echo.

#ifdef __GNUC__
#define ALIGNED __attribute__((aligned(16)))
#else
#define ALIGNED
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

struct echo_filter
{
   float *history; // history buffer
   int pos; // current position in history buffer
   int amp; // amplification of echoes (0-256)
   int delay; // delay in number of samples
   int ms; // delay in miliseconds
   int rate; // sample rate

   float f_amp; // amplification (0-1)
   float input_rate;
};

struct echo_filter_data
{
   struct echo_filter echo_l;
   struct echo_filter echo_r;
   float buf[4096];
};

#ifdef RARCH_INTERNAL
#define rarch_dsp_plugin_init echo_dsp_plugin_init
#endif

static float echo_process(void *data, float in)
{
   struct echo_filter *echo = (struct echo_filter*)data;

   float smp = echo->history[echo->pos];
   smp *= echo->f_amp;	
   smp += in;
   echo->history[echo->pos] = smp;
   echo->pos = (echo->pos + 1) % echo->delay;
   return smp;
}

static void echo_dsp_process(void *data, rarch_dsp_output_t *output,
      const rarch_dsp_input_t *input)
{
   int num_samples, i;
   struct echo_filter_data *echo = (struct echo_filter_data*)data;
   output->samples = echo->buf;
   num_samples = input->frames * 2;

   for (i = 0; i < num_samples;)
   {
      echo->buf[i] = echo_process(&echo->echo_l, input->samples[i]);
      i++;
      echo->buf[i] = echo_process(&echo->echo_r, input->samples[i]);
      i++;
   }
   output->frames = input->frames;
}

static void echo_dsp_free(void *data)
{
   struct echo_filter_data *echo = (struct echo_filter_data*)data;

   if (echo->echo_l.history)
      free(echo->echo_l.history);
   if (echo->echo_r.history)
      free(echo->echo_r.history);

   if (echo)
      free(echo);
}

static void echo_set_delay(void *data, int ms)
{
   int new_delay, how_much, i;
   float *new_history;
   struct echo_filter *echo = (struct echo_filter*)data;

   new_delay = ms * echo->input_rate / 1000;
   if (new_delay == 0)
      new_delay = 1;

   new_history = (float*)malloc(new_delay * sizeof(float));
   memset(new_history, 0, new_delay * sizeof(float));

   if (echo->history)
   {
      how_much = echo->delay - echo->pos;
      how_much = min(how_much, new_delay);
      memcpy(new_history, echo->history + echo->pos, how_much * sizeof(float));

      if (how_much < new_delay)
      {
         i = how_much;
         how_much = new_delay - how_much;
         how_much = min(how_much, echo->delay);
         how_much = min(how_much, echo->pos);
         memcpy(new_history + i, echo->history, how_much * sizeof(float));
      }

      if (echo->history)
         free(echo->history);
   }
   echo->history = new_history;
   echo->pos = 0;
   echo->delay = new_delay;
   echo->ms = ms;
}

static void *echo_dsp_init(const rarch_dsp_info_t *info)
{
   struct echo_filter_data *echo = (struct echo_filter_data*)calloc(1, sizeof(*echo));;

   if (!echo)
      return NULL;

   echo->echo_l.history = NULL;
   echo->echo_l.input_rate = info->input_rate;
   echo_set_delay(&echo->echo_l, 200);
   echo->echo_l.amp = 128;
   echo->echo_l.f_amp = (float)echo->echo_l.amp / 256.0f;
   echo->echo_l.pos = 0;

   echo->echo_r.history = NULL;
   echo->echo_r.input_rate = info->input_rate;
   echo_set_delay(&echo->echo_r, 200);
   echo->echo_r.amp = 128;
   echo->echo_r.f_amp = (float)echo->echo_r.amp / 256.0f;
   echo->echo_r.pos = 0;

   fprintf(stderr, "[Echo] loaded!\n");

   return echo;
}

static void echo_dsp_config(void *data)
{
   (void)data;
}

static const struct dspfilter_implementation generic_echo_dsp = {
   echo_dsp_init,
   echo_dsp_process,
   echo_dsp_free,
   RARCH_DSP_API_VERSION,
   echo_dsp_config,
   "Echo",
   NULL
};

const struct dspfilter_implementation *rarch_dsp_plugin_init(void)
{
   return &generic_echo_dsp;
}

#ifdef RARCH_INTERNAL
#undef rarch_dsp_plugin_init
#endif
