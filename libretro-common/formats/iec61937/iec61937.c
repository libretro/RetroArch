/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (iec61937.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>

#include <formats/iec61937.h>

/* The preamble words are written as little-endian 16-bit samples;
 * the payload's bytes are swapped in pairs, so the frame's first
 * byte is the high byte of the first 16-bit word. */
static void put16(uint8_t *p, unsigned v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
}

size_t iec61937_burst(unsigned type, const uint8_t *payload, size_t len, bool length_is_bits,
      size_t period_bytes, uint8_t *out, size_t cap)
{
   size_t i;
   if (!out || cap < period_bytes || !payload)
      return 0;
   if (8 + len > period_bytes)
      return 0;
   put16(out + 0, 0xF872);
   put16(out + 2, 0x4E1F);
   put16(out + 4, type);
   put16(out + 6, (unsigned)(length_is_bits ? len * 8 : len));
   for (i = 0; i + 1 < len; i += 2)
   {
      out[8 + i]     = payload[i + 1];
      out[8 + i + 1] = payload[i];
   }
   if (i < len)
   {
      /* an odd byte: it is the high byte of a word whose low byte is zero */
      out[8 + i]     = 0;
      out[8 + i + 1] = payload[i];
      i += 2;
   }
   memset(out + 8 + i, 0, period_bytes - 8 - i);
   return period_bytes;
}

size_t iec61937_wrap_ac3(const uint8_t *frame, size_t len, unsigned bsmod, uint8_t *out, size_t cap)
{
   return iec61937_burst(IEC61937_AC3 | ((bsmod & 7) << 8), frame, len, true,
         IEC61937_AC3_BURST_BYTES, out, cap);
}

size_t iec61937_wrap_eac3(const uint8_t *frames, size_t len, uint8_t *out, size_t cap)
{
   return iec61937_burst(IEC61937_EAC3, frames, len, false,
         IEC61937_EAC3_BURST_BYTES, out, cap);
}

size_t iec61937_wrap_dts(const uint8_t *frame, size_t len, unsigned pcm_frames, uint8_t *out, size_t cap)
{
   unsigned type;
   switch (pcm_frames)
   {
      case 512:  type = IEC61937_DTS_I;   break;
      case 1024: type = IEC61937_DTS_II;  break;
      case 2048: type = IEC61937_DTS_III; break;
      default:   return 0;
   }
   return iec61937_burst(type, frame, len, true, (size_t)pcm_frames * 4, out, cap);
}

size_t iec61937_pause_burst(unsigned gap_frames, uint8_t *out, size_t cap)
{
   /* Pd is the gap in frames as 32 bits' worth: the pause burst's
    * payload is one 32-bit word holding the gap length */
   uint8_t gap[4];
   gap[0] = (uint8_t)(gap_frames >> 24);
   gap[1] = (uint8_t)(gap_frames >> 16);
   gap[2] = (uint8_t)(gap_frames >> 8);
   gap[3] = (uint8_t)gap_frames;
   return iec61937_burst(IEC61937_PAUSE, gap, sizeof(gap), true, IEC61937_PAUSE_BURST_BYTES, out, cap);
}

bool iec61937_probe(const uint8_t *buf, size_t len, unsigned *type, size_t *payload_bytes)
{
   unsigned pc, pd, t;
   if (!buf || len < 8)
      return false;
   if (buf[0] != 0x72 || buf[1] != 0xF8 || buf[2] != 0x1F || buf[3] != 0x4E)
      return false;
   pc = buf[4] | ((unsigned)buf[5] << 8);
   pd = buf[6] | ((unsigned)buf[7] << 8);
   t  = pc & 0x1F;
   if (type) *type = t;
   if (payload_bytes) *payload_bytes = (t == IEC61937_EAC3) ? pd : (pd + 7) / 8;
   return true;
}
