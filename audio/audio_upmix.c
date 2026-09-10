/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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
 */

#include <math.h>
#include <string.h>

#include "audio_upmix.h"

#define UPMIX_LFE_HZ      120.0
#define UPMIX_CENTRE_GAIN 0.5f        /* -6 dB on the sum */
#define UPMIX_REAR_GAIN   0.70710678f /* -3 dB */

unsigned audio_layout_channels(uint32_t layout)
{
   unsigned n = 0;
   while (layout)
   {
      n     += layout & 1u;
      layout >>= 1;
   }
   return n;
}

bool audio_layout_supported(uint32_t layout)
{
   return layout == AUDIO_LAYOUT_STEREO
       || layout == AUDIO_LAYOUT_QUAD
       || layout == AUDIO_LAYOUT_5POINT1
       || layout == AUDIO_LAYOUT_5POINT1_SURROUND
       || layout == AUDIO_LAYOUT_7POINT1;
}

/* The slot of a position in a frame of the layout: the count of set
 * bits below it, or -1 when the layout lacks it. */
static int upmix_slot(uint32_t layout, uint32_t position)
{
   if (!(layout & position))
      return -1;
   return (int)audio_layout_channels(layout & (position - 1u));
}

bool audio_upmix_init(audio_upmix_t *up, uint32_t layout, unsigned rate)
{
   double x;
   memset(up, 0, sizeof(*up));
   up->layout   = AUDIO_LAYOUT_STEREO;
   up->channels = 2;
   up->fl = 0; up->fr = 1;
   up->fc = up->lfe = up->bl = up->br = up->sl = up->sr = -1;
   if (!audio_layout_supported(layout))
      return false;
   if (layout == AUDIO_LAYOUT_STEREO)
      return true;
   if (!rate)
      rate = 48000;
   /* One-pole low-pass: y += a * (x - y), a from the corner. */
   x             = exp(-2.0 * 3.14159265358979 * UPMIX_LFE_HZ / (double)rate);
   up->lfe_coeff = (float)(1.0 - x);
   up->layout    = layout;
   up->channels  = audio_layout_channels(layout);
   up->fl  = upmix_slot(layout, AUDIO_SPEAKER_FRONT_LEFT);
   up->fr  = upmix_slot(layout, AUDIO_SPEAKER_FRONT_RIGHT);
   up->fc  = upmix_slot(layout, AUDIO_SPEAKER_FRONT_CENTER);
   up->lfe = upmix_slot(layout, AUDIO_SPEAKER_LOW_FREQUENCY);
   up->bl  = upmix_slot(layout, AUDIO_SPEAKER_BACK_LEFT);
   up->br  = upmix_slot(layout, AUDIO_SPEAKER_BACK_RIGHT);
   up->sl  = upmix_slot(layout, AUDIO_SPEAKER_SIDE_LEFT);
   up->sr  = upmix_slot(layout, AUDIO_SPEAKER_SIDE_RIGHT);
   return true;
}

void audio_upmix_process(audio_upmix_t *up, float *out, const float *in, size_t frames)
{
   size_t i;
   float  lfe   = up->lfe_state;
   const float a = up->lfe_coeff;
   const unsigned ch = up->channels;

   if (up->layout == AUDIO_LAYOUT_STEREO)
   {
      memcpy(out, in, frames * 2 * sizeof(float));
      return;
   }
   for (i = 0; i < frames; i++)
   {
      float l = in[2 * i], r = in[2 * i + 1];
      float m = (l + r) * UPMIX_CENTRE_GAIN;
      out[up->fl] = l;
      out[up->fr] = r;
      if (up->fc  >= 0) out[up->fc]  = m;
      if (up->lfe >= 0)
      {
         lfe += a * (m - lfe);
         out[up->lfe] = lfe;
      }
      if (up->bl  >= 0) out[up->bl]  = l * UPMIX_REAR_GAIN;
      if (up->br  >= 0) out[up->br]  = r * UPMIX_REAR_GAIN;
      if (up->sl  >= 0) out[up->sl]  = l * UPMIX_REAR_GAIN;
      if (up->sr  >= 0) out[up->sr]  = r * UPMIX_REAR_GAIN;
      out += ch;
   }
   up->lfe_state = lfe;
}
