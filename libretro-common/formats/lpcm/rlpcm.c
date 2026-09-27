/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rlpcm.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>

#include <formats/rlpcm.h>

/* The speaker bits, as audio_upmix.h numbers them. Kept here by value
 * so the format does not depend on the frontend's header. */
#define L_FL   0x001u
#define L_FR   0x002u
#define L_FC   0x004u
#define L_LFE  0x008u
#define L_BL   0x010u
#define L_BR   0x020u
#define L_BC   0x100u
#define L_SL   0x200u
#define L_SR   0x400u

#define L_STEREO (L_FL | L_FR)
#define L_3_0    (L_STEREO | L_FC)
#define L_QUAD   (L_STEREO | L_BL | L_BR)
#define L_5_0    (L_3_0 | L_SL | L_SR)
#define L_5_1    (L_5_0 | L_LFE)
#define L_7_1    (L_5_1 | L_BL | L_BR)

static unsigned rlpcm_count_bits(uint32_t v)
{
   unsigned n = 0;
   while (v)
   {
      n += v & 1u;
      v >>= 1;
   }
   return n;
}

/* Channel counts map to the layout the disc formats use: one channel
 * is the centre, two the front pair, and the rest fill the room in
 * the mask's own order. DVD says only how many; Blu-ray names the
 * arrangement and its codes are below. */
static uint32_t rlpcm_layout_of_count(unsigned channels)
{
   switch (channels)
   {
      case 1: return L_FC;
      case 2: return L_STEREO;
      case 3: return L_3_0;
      case 4: return L_QUAD;
      case 5: return L_5_0;
      case 6: return L_5_1;
      case 7: return L_5_1 | L_BC;
      case 8: return L_7_1;
      default: break;
   }
   return 0;
}

unsigned rlpcm_frame_bits(const rlpcm_format_t *fmt)
{
   if (!fmt)
      return 0;
   return fmt->bits * fmt->channels;
}

size_t rlpcm_frame_bytes(const rlpcm_format_t *fmt)
{
   unsigned bits = rlpcm_frame_bits(fmt);
   return (size_t)((bits + 7u) / 8u);
}

/* ---- headers ------------------------------------------------------ */

static enum rlpcm_status rlpcm_parse_dvd(const uint8_t *src, size_t len,
      rlpcm_format_t *fmt)
{
   unsigned quant, rate, channels;

   if (len < 5)
      return RLPCM_NEED_MORE;

   /* [0] frame number, [1..2] first access unit pointer, [3] the
    * shape, [4] dynamic range. The pointer counts from the byte after
    * itself and is one-based; zero means no access unit begins here,
    * which is a continuation packet and not a header to read a shape
    * from. */
   if (((unsigned)src[1] << 8 | src[2]) == 0)
      return RLPCM_NO_SYNC;

   quant    = (src[3] >> 6) & 3u;
   rate     = (src[3] >> 4) & 3u;
   channels = (src[3] & 7u) + 1u;

   switch (quant)
   {
      case 0: fmt->bits = 16; break;
      case 1: fmt->bits = 20; break;
      case 2: fmt->bits = 24; break;
      default: return RLPCM_BAD;   /* 3 is reserved */
   }
   switch (rate)
   {
      case 0: fmt->sample_rate = 48000; break;
      case 1: fmt->sample_rate = 96000; break;
      case 2: fmt->sample_rate = 44100; break;
      case 3: fmt->sample_rate = 32000; break;
   }
   if (channels > 8)
      return RLPCM_BAD;

   fmt->kind          = RLPCM_KIND_DVD;
   fmt->channels      = channels;
   fmt->layout        = rlpcm_layout_of_count(channels);
   fmt->big_endian    = true;
   fmt->header_bytes  = 5;
   /* The packet's length belongs to the container above this one. */
   fmt->payload_bytes = 0;
   return RLPCM_OK;
}

/* Blu-ray's channel assignment codes (HDMV LPCM). The samples arrive
 * in the order the mask counts, which is what makes these a mask and
 * not a permutation. */
static uint32_t rlpcm_bluray_layout(unsigned code)
{
   switch (code)
   {
      case 1:  return L_FC;                 /* mono */
      case 3:  return L_STEREO;
      case 4:  return L_3_0;                /* L R C  (surround 3/0) */
      case 5:  return L_STEREO | L_BC;      /* L R S */
      case 6:  return L_3_0 | L_BC;         /* L R C S */
      case 7:  return L_QUAD;               /* L R Ls Rs */
      case 8:  return L_5_0;                /* L R C Ls Rs */
      case 9:  return L_5_1;
      case 10: return L_7_1 & ~L_LFE;       /* L R C Ls Rs Lb Rb */
      case 11: return L_7_1;
      default: break;
   }
   return 0;
}

static enum rlpcm_status rlpcm_parse_bluray(const uint8_t *src, size_t len,
      rlpcm_format_t *fmt)
{
   unsigned assign, rate, bits;
   uint32_t layout;

   if (len < 4)
      return RLPCM_NEED_MORE;

   /* [0..1] payload bytes, [2] channel assignment and sampling
    * frequency, [3] bits per sample in the top two bits. */
   assign = (src[2] >> 4) & 0x0Fu;
   rate   = src[2] & 0x0Fu;
   bits   = (src[3] >> 6) & 3u;

   layout = rlpcm_bluray_layout(assign);
   if (!layout)
      return RLPCM_BAD;

   switch (rate)
   {
      case 1: fmt->sample_rate = 48000;  break;
      case 4: fmt->sample_rate = 96000;  break;
      case 5: fmt->sample_rate = 192000; break;
      default: return RLPCM_BAD;
   }
   switch (bits)
   {
      case 1: fmt->bits = 16; break;
      case 2: fmt->bits = 20; break;
      case 3: fmt->bits = 24; break;
      default: return RLPCM_BAD;
   }

   fmt->kind          = RLPCM_KIND_BLURAY;
   fmt->layout        = layout;
   fmt->channels      = rlpcm_count_bits(layout);
   fmt->big_endian    = true;
   fmt->header_bytes  = 4;
   fmt->payload_bytes = (size_t)((unsigned)src[0] << 8 | src[1]);
   return RLPCM_OK;
}

enum rlpcm_status rlpcm_parse_format(enum rlpcm_kind kind,
      const uint8_t *src, size_t len, rlpcm_format_t *fmt)
{
   if (!fmt)
      return RLPCM_BAD;

   switch (kind)
   {
      case RLPCM_KIND_DVD:
         return rlpcm_parse_dvd(src, len, fmt);
      case RLPCM_KIND_BLURAY:
         return rlpcm_parse_bluray(src, len, fmt);
      case RLPCM_KIND_RAW:
         break;
   }

   /* Raw: the caller said what it is, so this is the check. */
   if (fmt->bits != 16 && fmt->bits != 20 && fmt->bits != 24)
      return RLPCM_BAD;
   if (fmt->channels < 1 || fmt->channels > 8)
      return RLPCM_BAD;
   if (!fmt->sample_rate)
      return RLPCM_BAD;
   if (!fmt->layout)
      fmt->layout = rlpcm_layout_of_count(fmt->channels);
   if (rlpcm_count_bits(fmt->layout) != fmt->channels)
      return RLPCM_BAD;
   fmt->kind         = RLPCM_KIND_RAW;
   fmt->header_bytes = 0;
   return RLPCM_OK;
}

/* ---- samples ------------------------------------------------------ */

/* One sample, sign-extended into an int32 whose full scale is the
 * format's own: 16, 20 or 24 bits.
 *
 * 16 and 24 bits are whole bytes and go either way round. 20-bit is
 * the awkward one: both disc formats carry it as pairs of samples in
 * five bytes - two whole bytes of each sample's top bits, then a
 * fifth byte holding the two sets of low nibbles, the first sample's
 * in the high half. That is why the pair, not the sample, is the unit
 * there. */
static int32_t rlpcm_sample_16(const uint8_t *p, bool be)
{
   int32_t v = be ? ((int32_t)p[0] << 8 | p[1])
                  : ((int32_t)p[1] << 8 | p[0]);
   return (int32_t)(int16_t)v;
}

static int32_t rlpcm_sample_24(const uint8_t *p, bool be)
{
   int32_t v = be ? ((int32_t)p[0] << 16 | (int32_t)p[1] << 8 | p[2])
                  : ((int32_t)p[2] << 16 | (int32_t)p[1] << 8 | p[0]);
   if (v & 0x800000)
      v -= 0x1000000;
   return v;
}

static void rlpcm_pair_20(const uint8_t *p, bool be, int32_t *a, int32_t *b)
{
   int32_t x, y;
   if (be)
   {
      x = ((int32_t)p[0] << 12) | ((int32_t)p[1] << 4) | ((p[4] >> 4) & 0x0Fu);
      y = ((int32_t)p[2] << 12) | ((int32_t)p[3] << 4) | (p[4] & 0x0Fu);
   }
   else
   {
      x = ((int32_t)p[1] << 12) | ((int32_t)p[0] << 4) | (p[4] & 0x0Fu);
      y = ((int32_t)p[3] << 12) | ((int32_t)p[2] << 4) | ((p[4] >> 4) & 0x0Fu);
   }
   if (x & 0x80000) x -= 0x100000;
   if (y & 0x80000) y -= 0x100000;
   *a = x;
   *b = y;
}

/* How many whole frames the bytes hold. At 20 bits the samples come in
 * pairs, so what the bytes hold is a whole number of pairs of samples
 * and the frames are however many of those cover the channels. */
static size_t rlpcm_frames_available(const rlpcm_format_t *fmt, size_t bytes)
{
   if (fmt->bits == 20)
   {
      size_t pairs   = bytes / 5;
      size_t samples = pairs * 2;
      return samples / fmt->channels;
    }
   return bytes / ((fmt->bits / 8) * fmt->channels);
}

size_t rlpcm_decode_f32(const rlpcm_format_t *fmt,
      const uint8_t *src, size_t src_len, float *out, size_t frames)
{
   size_t have, f;
   unsigned c;

   if (!fmt || !src || !out || !fmt->channels)
      return 0;
   have = rlpcm_frames_available(fmt, src_len);
   if (frames > have)
      frames = have;
   if (!frames)
      return 0;

   if (fmt->bits == 20)
   {
      /* Walked as a run of samples, since the pair straddles the
       * frame boundary whenever the channel count is odd. */
      const float scale   = 1.0f / 524288.0f;   /* 2^19 */
      size_t      samples = frames * fmt->channels;
      size_t      i       = 0;
      while (i + 1 < samples)
      {
         int32_t a, b;
         rlpcm_pair_20(src + (i / 2) * 5, fmt->big_endian, &a, &b);
         out[i]     = (float)a * scale;
         out[i + 1] = (float)b * scale;
         i += 2;
      }
      if (i < samples)
      {
         int32_t a, b;
         rlpcm_pair_20(src + (i / 2) * 5, fmt->big_endian, &a, &b);
         out[i] = (float)a * scale;
      }
      return frames;
   }

   if (fmt->bits == 16)
   {
      const float scale = 1.0f / 32768.0f;
      for (f = 0; f < frames; f++)
         for (c = 0; c < fmt->channels; c++)
            *out++ = (float)rlpcm_sample_16(
                  src + (f * fmt->channels + c) * 2, fmt->big_endian) * scale;
      return frames;
   }

   {
      const float scale = 1.0f / 8388608.0f;   /* 2^23 */
      for (f = 0; f < frames; f++)
         for (c = 0; c < fmt->channels; c++)
            *out++ = (float)rlpcm_sample_24(
                  src + (f * fmt->channels + c) * 3, fmt->big_endian) * scale;
   }
   return frames;
}

size_t rlpcm_decode_s16(const rlpcm_format_t *fmt,
      const uint8_t *src, size_t src_len, int16_t *out, size_t frames)
{
   size_t have, f;
   unsigned c;

   if (!fmt || !src || !out || !fmt->channels)
      return 0;
   have = rlpcm_frames_available(fmt, src_len);
   if (frames > have)
      frames = have;
   if (!frames)
      return 0;

   if (fmt->bits == 16)
   {
      /* Already the target's width: only the byte order to undo. */
      for (f = 0; f < frames; f++)
         for (c = 0; c < fmt->channels; c++)
            *out++ = (int16_t)rlpcm_sample_16(
                  src + (f * fmt->channels + c) * 2, fmt->big_endian);
      return frames;
   }

   if (fmt->bits == 20)
   {
      size_t samples = frames * fmt->channels;
      size_t i       = 0;
      while (i < samples)
      {
         int32_t a, b;
         rlpcm_pair_20(src + (i / 2) * 5, fmt->big_endian, &a, &b);
         /* >> 4 with rounding: the four bits the target has no room
          * for, taken to nearest rather than towards silence. */
         out[i] = (int16_t)((a + 8) >> 4);
         if (i + 1 < samples)
            out[i + 1] = (int16_t)((b + 8) >> 4);
         i += 2;
      }
      return frames;
   }

   for (f = 0; f < frames; f++)
      for (c = 0; c < fmt->channels; c++)
      {
         int32_t v = rlpcm_sample_24(
               src + (f * fmt->channels + c) * 3, fmt->big_endian);
         v = (v + 128) >> 8;
         if (v > 32767)  v = 32767;
         if (v < -32768) v = -32768;
         *out++ = (int16_t)v;
      }
   return frames;
}

/* ---- out ---------------------------------------------------------- */

static void rlpcm_put_16(uint8_t *p, int32_t v, bool be)
{
   if (be) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
   else    { p[1] = (uint8_t)(v >> 8); p[0] = (uint8_t)v; }
}

static void rlpcm_put_24(uint8_t *p, int32_t v, bool be)
{
   if (be) { p[0] = (uint8_t)(v >> 16); p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)v; }
   else    { p[2] = (uint8_t)(v >> 16); p[1] = (uint8_t)(v >> 8); p[0] = (uint8_t)v; }
}

static int32_t rlpcm_clip(double v, int32_t lo, int32_t hi)
{
   double r = v < 0.0 ? v - 0.5 : v + 0.5;
   if (r < (double)lo) return lo;
   if (r > (double)hi) return hi;
   return (int32_t)r;
}

size_t rlpcm_encode_f32(const rlpcm_format_t *fmt,
      const float *src, size_t frames, uint8_t *out, size_t out_len)
{
   size_t room, f;
   unsigned c;

   if (!fmt || !src || !out || !fmt->channels)
      return 0;
   room = rlpcm_frames_available(fmt, out_len);
   if (frames > room)
      frames = room;
   if (!frames)
      return 0;

   if (fmt->bits == 20)
   {
      size_t samples = frames * fmt->channels;
      size_t i       = 0;
      while (i < samples)
      {
         int32_t a = rlpcm_clip((double)src[i] * 524288.0, -524288, 524287);
         int32_t b = (i + 1 < samples)
               ? rlpcm_clip((double)src[i + 1] * 524288.0, -524288, 524287) : 0;
         uint8_t *p = out + (i / 2) * 5;
         if (fmt->big_endian)
         {
            p[0] = (uint8_t)(a >> 12); p[1] = (uint8_t)(a >> 4);
            p[2] = (uint8_t)(b >> 12); p[3] = (uint8_t)(b >> 4);
            p[4] = (uint8_t)(((a & 0x0F) << 4) | (b & 0x0F));
         }
         else
         {
            p[1] = (uint8_t)(a >> 12); p[0] = (uint8_t)(a >> 4);
            p[3] = (uint8_t)(b >> 12); p[2] = (uint8_t)(b >> 4);
            p[4] = (uint8_t)(((b & 0x0F) << 4) | (a & 0x0F));
         }
         i += 2;
      }
      return frames;
   }

   if (fmt->bits == 16)
   {
      for (f = 0; f < frames; f++)
         for (c = 0; c < fmt->channels; c++)
            rlpcm_put_16(out + (f * fmt->channels + c) * 2,
                  rlpcm_clip((double)*src++ * 32768.0, -32768, 32767),
                  fmt->big_endian);
      return frames;
   }

   for (f = 0; f < frames; f++)
      for (c = 0; c < fmt->channels; c++)
         rlpcm_put_24(out + (f * fmt->channels + c) * 3,
               rlpcm_clip((double)*src++ * 8388608.0, -8388608, 8388607),
               fmt->big_endian);
   return frames;
}
