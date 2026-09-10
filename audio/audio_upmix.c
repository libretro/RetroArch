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

#define AUDIO_KNOWN_POSITIONS 0x7FFu

bool audio_layout_known(uint32_t layout)
{
   return layout != 0 && (layout & ~AUDIO_KNOWN_POSITIONS) == 0;
}

/* The fold's gains per position into left and right, in the
 * position's bit order. */
static void downmix_gains(uint32_t layout, float *gl, float *gr, unsigned *nslots)
{
   static const float table[11][2] = {
      { 1.0f, 0.0f },              /* FL */
      { 0.0f, 1.0f },              /* FR */
      { 0.70710678f, 0.70710678f },/* FC */
      { 0.0f, 0.0f },              /* LFE: dropped */
      { 0.70710678f, 0.0f },       /* BL */
      { 0.0f, 0.70710678f },       /* BR */
      { 0.70710678f, 0.0f },       /* FLC */
      { 0.0f, 0.70710678f },       /* FRC */
      { 0.5f, 0.5f },              /* BC */
      { 0.70710678f, 0.0f },       /* SL */
      { 0.0f, 0.70710678f }        /* SR */
   };
   unsigned bit, n = 0;
   for (bit = 0; bit < 11; bit++)
      if (layout & (1u << bit))
      {
         gl[n] = table[bit][0];
         gr[n] = table[bit][1];
         n++;
      }
   /* mono: the one channel to both sides at unity */
   if (n == 1)
      gl[0] = gr[0] = 1.0f;
   *nslots = n;
}

void audio_downmix_f32(float *out, const float *in, size_t frames, uint32_t layout, unsigned channels)
{
   float gl[11], gr[11];
   unsigned n, c;
   size_t f;
   downmix_gains(layout, gl, gr, &n);
   if (n != channels)
      return;
   for (f = 0; f < frames; f++)
   {
      float l = 0.0f, r = 0.0f;
      for (c = 0; c < n; c++)
      {
         l += in[c] * gl[c];
         r += in[c] * gr[c];
      }
      out[0] = l;
      out[1] = r;
      out   += 2;
      in    += n;
   }
}

void audio_downmix_s16(int16_t *out, const int16_t *in, size_t frames, uint32_t layout, unsigned channels)
{
   float gl[11], gr[11];
   int32_t ql[11], qr[11];
   unsigned n, c;
   size_t f;
   downmix_gains(layout, gl, gr, &n);
   if (n != channels)
      return;
   for (c = 0; c < n; c++)
   {
      ql[c] = (int32_t)(gl[c] * 32768.0f + 0.5f);
      qr[c] = (int32_t)(gr[c] * 32768.0f + 0.5f);
   }
   for (f = 0; f < frames; f++)
   {
      int64_t l = 0, r = 0;
      for (c = 0; c < n; c++)
      {
         l += (int64_t)in[c] * ql[c];
         r += (int64_t)in[c] * qr[c];
      }
      l = (l + 16384) >> 15;
      r = (r + 16384) >> 15;
      out[0] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
      out[1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
      out   += 2;
      in    += n;
   }
}

void audio_layout_remap_s16(int16_t *out, uint32_t out_layout,
      const int16_t *in, uint32_t in_layout, size_t frames)
{
   const unsigned oc = audio_layout_channels(out_layout);
   const unsigned ic = audio_layout_channels(in_layout);
   int  dst[11];            /* per source slot: the destination slot, or -1 */
   float gl[11], gr[11];    /* the fold of a source slot without a destination */
   int  ol = -1, orr = -1;
   unsigned bit, n = 0, k;
   size_t f;
   if (in_layout == out_layout)
   {
      memcpy(out, in, frames * ic * sizeof(int16_t));
      return;
   }
   downmix_gains(in_layout, gl, gr, &n);
   n = 0;
   for (bit = 0; bit < 11; bit++)
   {
      uint32_t p = 1u << bit;
      if (!(in_layout & p))
         continue;
      dst[n++] = (out_layout & p) ? (int)audio_layout_channels(out_layout & (p - 1)) : -1;
   }
   if (out_layout & AUDIO_SPEAKER_FRONT_LEFT)  ol  = 0;
   if (out_layout & AUDIO_SPEAKER_FRONT_RIGHT) orr = (int)audio_layout_channels(out_layout & (AUDIO_SPEAKER_FRONT_RIGHT - 1));
   for (f = 0; f < frames; f++)
   {
      int32_t l = 0, r = 0;
      memset(out, 0, oc * sizeof(int16_t));
      for (k = 0; k < n; k++)
      {
         if (dst[k] >= 0)
            out[dst[k]] = in[k];
         else
         {
            l += (int32_t)(in[k] * gl[k]);
            r += (int32_t)(in[k] * gr[k]);
         }
      }
      if (ol >= 0 && l)
      {
         int32_t v = out[ol] + l;
         out[ol] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
      }
      if (orr >= 0 && r)
      {
         int32_t v = out[orr] + r;
         out[orr] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
      }
      out += oc;
      in  += ic;
   }
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
   up->lfe_coeff_q30 = (int32_t)((1.0 - x) * 1073741824.0 + 0.5);
   up->lfe_state_q30 = 0;
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

void audio_upmix_process_s16(audio_upmix_t *up, int16_t *out, const int16_t *in, size_t frames)
{
   size_t i;
   int64_t lfe = up->lfe_state_q30;   /* the state: an int16 in Q30 */
   const int64_t a = up->lfe_coeff_q30;
   const int32_t centre = (int32_t)(UPMIX_CENTRE_GAIN * 32768.0f + 0.5f);
   const int32_t rear   = (int32_t)(UPMIX_REAR_GAIN * 32768.0f + 0.5f);
   const unsigned ch = up->channels;

   if (up->layout == AUDIO_LAYOUT_STEREO)
   {
      memcpy(out, in, frames * 2 * sizeof(int16_t));
      return;
   }
   for (i = 0; i < frames; i++)
   {
      int32_t l = in[2 * i], r = in[2 * i + 1];
      /* the centre: (l + r) * gain, rounded; in Q15 for the filter */
      int32_t m_q15 = (l + r) * centre;                 /* |l+r| < 2^16, centre 2^14: fits */
      int32_t m     = (m_q15 + 16384) >> 15;
      out[up->fl] = (int16_t)l;
      out[up->fr] = (int16_t)r;
      if (up->fc  >= 0) out[up->fc]  = (int16_t)m;
      if (up->lfe >= 0)
      {
         /* lfe += a * (m - lfe): the state in Q30, the coefficient in
          * Q30, the difference taken to Q15 for the product to fit in
          * 64 bits. A Q15 coefficient was five LSB off on a 60 Hz
          * tone: at a hundredth, its own rounding was a tenth of a
          * percent of the cutoff. */
         lfe += (a * (((int64_t)m_q15 * 32768 - lfe) >> 15)) >> 15;
         out[up->lfe] = (int16_t)((lfe + (1 << 29)) >> 30);
      }
      if (up->bl  >= 0) out[up->bl]  = (int16_t)((l * rear + 16384) >> 15);
      if (up->br  >= 0) out[up->br]  = (int16_t)((r * rear + 16384) >> 15);
      if (up->sl  >= 0) out[up->sl]  = (int16_t)((l * rear + 16384) >> 15);
      if (up->sr  >= 0) out[up->sr]  = (int16_t)((r * rear + 16384) >> 15);
      out += ch;
   }
   up->lfe_state_q30 = lfe;
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
