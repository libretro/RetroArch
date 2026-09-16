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

#include "audio_binaural.h"

#define PI_F 3.14159265358979f

/* A speaker's direction and how the head treats it. Azimuth in
 * degrees, 0 ahead, positive to the right; the far-ear level and
 * low-pass corner by how far round it is; the rear low-pass for
 * everything behind the ears. */
static void speaker_at(audio_binaural_speaker_t *s, int slot, float azimuth_deg,
      bool behind, unsigned rate)
{
   float az     = azimuth_deg * PI_F / 180.0f;
   float sa     = sinf(az);                  /* -1 left .. +1 right */
   /* Woodworth's head: the far ear's extra path is r * (a + sin a) for
    * a head of radius r, about 8.75 cm; at the side that is about
    * 0.65 ms. Whole samples at the rate. */
   float itd_s  = 0.0875f * (fabsf(az) + fabsf(sa)) / 343.0f;
   int   itd    = (int)(itd_s * (float)rate + 0.5f);
   /* Far-ear level: down to -6 dB at the side. Far-ear low-pass:
    * from open ahead down to about 2 kHz at the side. */
   float far_g  = 1.0f - 0.5f * fabsf(sa);
   float far_hz = 12000.0f - 10000.0f * fabsf(sa);
   float far_lp = 1.0f - expf(-2.0f * PI_F * far_hz / (float)rate);
   float rear_lp = behind ? 1.0f - expf(-2.0f * PI_F * 6000.0f / (float)rate) : 1.0f;

   if (itd > AUDIO_BINAURAL_MAX_DELAY - 1)
      itd = AUDIO_BINAURAL_MAX_DELAY - 1;
   s->slot = slot;
   s->lp_state_l = s->lp_state_r = 0.0f;
   if (sa < 0.0f)   /* on the left: left ear near */
   {
      s->gain_l = 1.0f;       s->gain_r = far_g;
      s->delay_l = 0;         s->delay_r = itd;
      s->lp_l = rear_lp;      s->lp_r = far_lp < rear_lp ? far_lp : rear_lp;
   }
   else if (sa > 0.0f)
   {
      s->gain_r = 1.0f;       s->gain_l = far_g;
      s->delay_r = 0;         s->delay_l = itd;
      s->lp_r = rear_lp;      s->lp_l = far_lp < rear_lp ? far_lp : rear_lp;
   }
   else                       /* ahead or behind on the midline */
   {
      s->gain_l = s->gain_r = 0.70710678f;   /* -3 dB each: the sum is unity power */
      s->delay_l = s->delay_r = 0;
      s->lp_l = s->lp_r = rear_lp;
   }
}

bool audio_binaural_init(audio_binaural_t *b, uint32_t layout, unsigned rate)
{
   audio_upmix_t slots;
   memset(b, 0, sizeof(*b));
   if (!audio_layout_supported(layout))
      return false;
   if (!rate)
      rate = 48000;
   /* The slot of each position, from the upmix's layout logic. */
   audio_upmix_init(&slots, layout, rate);
   b->layout   = layout;
   b->channels = slots.channels;
   /* Directions: ITU-R BS.775 for the fronts and rears; sides at 90. */
   if (slots.fl  >= 0) speaker_at(&b->speakers[b->nspeakers++], slots.fl,  -30.0f, false, rate);
   if (slots.fr  >= 0) speaker_at(&b->speakers[b->nspeakers++], slots.fr,   30.0f, false, rate);
   if (slots.fc  >= 0) speaker_at(&b->speakers[b->nspeakers++], slots.fc,    0.0f, false, rate);
   if (slots.lfe >= 0)
   {
      /* The LFE has no direction and no shadow: both ears, no filter. */
      audio_binaural_speaker_t *s = &b->speakers[b->nspeakers++];
      speaker_at(s, slots.lfe, 0.0f, false, rate);
      s->lp_l = s->lp_r = 1.0f;
   }
   if (slots.bl  >= 0) speaker_at(&b->speakers[b->nspeakers++], slots.bl, -110.0f, true,  rate);
   if (slots.br  >= 0) speaker_at(&b->speakers[b->nspeakers++], slots.br,  110.0f, true,  rate);
   if (slots.sl  >= 0) speaker_at(&b->speakers[b->nspeakers++], slots.sl,  -90.0f, true,  rate);
   if (slots.sr  >= 0) speaker_at(&b->speakers[b->nspeakers++], slots.sr,   90.0f, true,  rate);
   /* Each ear hears its near front speaker at unity and the far one
    * at its shadowed level, so a source in the middle of the stereo
    * would arrive louder than it left; the whole render is scaled so
    * that it does not. */
   b->gain = 1.0f / (1.0f + b->speakers[0].gain_r);
   return true;
}

void audio_binaural_process(audio_binaural_t *b, float *out, const float *in, size_t frames)
{
   size_t i;
   unsigned ch = b->channels;
   for (i = 0; i < frames; i++)
   {
      unsigned k;
      float l = 0.0f, r = 0.0f;
      unsigned at = b->at;
      /* This frame into every channel's delay line, then each speaker
       * read at its own two delays. */
      for (k = 0; k < ch; k++)
         b->delay[k][at] = in[i * ch + k];
      for (k = 0; k < b->nspeakers; k++)
      {
         audio_binaural_speaker_t *s = &b->speakers[k];
         const float *line = b->delay[s->slot];
         float xl = line[(at + AUDIO_BINAURAL_MAX_DELAY - s->delay_l) & (AUDIO_BINAURAL_MAX_DELAY - 1)];
         float xr = line[(at + AUDIO_BINAURAL_MAX_DELAY - s->delay_r) & (AUDIO_BINAURAL_MAX_DELAY - 1)];
         s->lp_state_l += s->lp_l * (xl - s->lp_state_l);
         s->lp_state_r += s->lp_r * (xr - s->lp_state_r);
         l += s->gain_l * s->lp_state_l;
         r += s->gain_r * s->lp_state_r;
      }
      out[2 * i]     = l * b->gain;
      out[2 * i + 1] = r * b->gain;
      b->at = (at + 1) & (AUDIO_BINAURAL_MAX_DELAY - 1);
   }
}
