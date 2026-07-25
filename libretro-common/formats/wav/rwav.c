/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwav.c).
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

/* rwav -- minimal RIFF WAVE reader.
 *
 * What it implements: WAV files holding integer PCM at 8, 16 or 24
 * bits or IEEE float at 32 bits, any channel count and sample rate.  The RIFF
 * chunk list is walked, so 'fmt ' and 'data' are found wherever they
 * sit and the chunks writers put between them - LIST, fact, cue, JUNK
 * and the rest - are stepped over; a fmt chunk longer than 16 bytes
 * (the 18-byte form carrying cbSize, say) is accepted for its standard
 * first 16.
 *
 * Two ways in.  rwav_load and the resumable iterator behind it decode
 * the file whole into a buffer this file owns, delivering samples in
 * native memory order: unsigned bytes for 8-bit, host-endian int16 for
 * 16-bit, host-endian float words for 32-bit, byte order fixed up from
 * the file's little-endian layout on big-endian hosts.  24-bit is the
 * exception and is handed over packed as the file stores it, three
 * little-endian bytes a sample, since no host type is that wide;
 * rwav.h carries the accessors for reading it.  rwav_parse
 * reads the header alone and reports where the samples are, allocating
 * and copying nothing, for a caller that would rather convert frames
 * out of its own buffer as it needs them - which also lets that buffer
 * hold only part of the file, since parsing touches chunk headers and
 * never the payload.
 *
 * WAVE_FORMAT_EXTENSIBLE headers are resolved through their SubFormat
 * GUID, so a file that is only extensible because it has more than two
 * channels reads as the plain PCM or float it actually holds.
 *
 * What it does not implement: compressed codecs (ADPCM, a-law, mu-law)
 * - including as an extensible SubFormat - and writing.  A declared
 * data size larger than the buffer is taken as the buffer's worth
 * rather than an error, which covers both a truncated file and a
 * writer that stamped a placeholder length it never went back to fix;
 * a trailing partial frame is dropped.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h> /* ptrdiff_t on osx */
#include <stdlib.h>
#include <string.h>

#include <formats/rwav.h>

/* WAV is little-endian on the wire.  Assembling words a byte at a time
 * keeps that correct on a big-endian host and safe on one that faults
 * unaligned loads, which is what the copy loops below already do. */
static unsigned rwav_u16(const uint8_t *p)
{
   return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static uint32_t rwav_u32(const uint8_t *p)
{
   return  (uint32_t)p[0]         | ((uint32_t)p[1] << 8)
       | ((uint32_t)p[2] << 16)   | ((uint32_t)p[3] << 24);
}

enum
{
   ITER_BEGIN,
   ITER_COPY_SAMPLES,
   ITER_COPY_SAMPLES_8,
   ITER_COPY_SAMPLES_16,
   ITER_COPY_SAMPLES_32
};

struct rwav_iterator
{
   rwav_t *out;
   const uint8_t *data;
   size_t size;
   size_t i, j;
   int step;
};

/* Walk the RIFF chunk list for 'fmt ' and 'data' and fill everything in
 * 'out' but the samples pointer.  A chunk is a four-byte id, a 32-bit
 * little-endian body size, and a body padded to an even length, the pad
 * byte not counted in the size.  Only chunk headers are read, never the
 * payload, so a buffer holding the head of a file is enough. */
static enum rwav_state rwav_walk(rwav_t *out, const uint8_t *data,
      size_t len)
{
   size_t   off        = 12;
   size_t   fmt_off    = 0;
   size_t   fmt_size   = 0;
   size_t   data_off   = 0;
   size_t   data_size  = 0;
   int      found_data = 0;
   size_t   blockalign, avail;
   unsigned tag, channels, bits, rate;

   if (len < 12)
      return RWAV_ITERATE_ERROR;
   if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F')
      return RWAV_ITERATE_ERROR;
   if (data[8] != 'W' || data[9] != 'A' || data[10] != 'V' || data[11] != 'E')
      return RWAV_ITERATE_ERROR;

   while (off + 8 <= len)
   {
      uint32_t csize = rwav_u32(data + off + 4);
      size_t   body  = off + 8;

      if (!memcmp(data + off, "data", 4))
      {
         data_off   = body;
         data_size  = (size_t)csize;
         found_data = 1;
         break;                     /* the payload starts here          */
      }
      if (!memcmp(data + off, "fmt ", 4))
      {
         fmt_off  = body;
         fmt_size = (size_t)csize;
      }
      if ((size_t)csize > len - body)
         break;                     /* body runs off the end            */
      off = body + (size_t)csize + (csize & 1);
   }

   if (!found_data || !fmt_off || fmt_size < 16 || fmt_size > len - fmt_off)
      return RWAV_ITERATE_ERROR;

   tag      = rwav_u16(data + fmt_off);
   channels = rwav_u16(data + fmt_off + 2);
   rate     = (unsigned)rwav_u32(data + fmt_off + 4);
   bits     = rwav_u16(data + fmt_off + 14);

   /* WAVE_FORMAT_EXTENSIBLE names the real format in a GUID instead of
    * the tag, and is what writers emit once a file has more than two
    * channels or more than 16 bits - so a plain 5.1 PCM file arrives
    * this way.  Past the standard 16 bytes it carries cbSize, the
    * valid bit count, the channel mask, and a 16-byte SubFormat whose
    * first four bytes are the tag the file would otherwise have
    * carried, followed by the fixed KSDATAFORMAT suffix.  Resolve it
    * to that tag and let the checks below judge it; a suffix that is
    * not KSDATAFORMAT's belongs to a codec this reader has no business
    * guessing at. */
   if (tag == 0xFFFE)
   {
      static const uint8_t ksdataformat[12] =
      {
         0x00, 0x00, 0x10, 0x00, 0x80, 0x00,
         0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
      };
      unsigned valid;

      /* 16 standard bytes, cbSize, and the 22 the extension needs */
      if (fmt_size < 40)
         return RWAV_ITERATE_ERROR;
      if (rwav_u16(data + fmt_off + 16) < 22)            /* cbSize    */
         return RWAV_ITERATE_ERROR;
      if (memcmp(data + fmt_off + 28, ksdataformat, sizeof(ksdataformat)))
         return RWAV_ITERATE_ERROR;
      /* samples fill the container width with the unused low bits
       * zeroed, so a smaller valid count needs nothing done about it
       * here; a larger one is a malformed header */
      valid = rwav_u16(data + fmt_off + 18);
      if (valid > bits)
         return RWAV_ITERATE_ERROR;
      /* the channel mask at 20 assigns speakers to the channels and
       * does not change how they are interleaved, so it is the
       * caller's business rather than this reader's */
      tag = (unsigned)rwav_u32(data + fmt_off + 24);
   }

   /* format tag 1 is integer PCM, 3 is IEEE float; anything past the
    * standard 16 bytes of either is ignored rather than refused */
   if (tag != 1 && tag != 3)
      return RWAV_ITERATE_ERROR;
   if (tag == 1 ? (bits != 8 && bits != 16 && bits != 24) : (bits != 32))
      return RWAV_ITERATE_ERROR;
   /* a zero channel count divides by zero working out the frame count,
    * and a zero rate is unplayable: refuse both here rather than hand
    * them on */
   if (!channels || !rate)
      return RWAV_ITERATE_ERROR;

   blockalign = (size_t)channels * (size_t)(bits / 8);
   avail      = len - data_off;
   if (data_size > avail)
      data_size = avail;
   /* whole frames only - the readers step a frame at a time, and a
    * trailing partial one would run them past the payload */
   data_size -= data_size % blockalign;
   if (!data_size)
      return RWAV_ITERATE_ERROR;

   out->bitspersample = bits;
   out->numchannels   = channels;
   out->samplerate    = rate;
   out->subchunk2size = data_size;
   out->numsamples    = data_size / blockalign;
   out->dataoffset    = data_off;
   out->samples       = NULL;
   return RWAV_ITERATE_DONE;
}

enum rwav_state rwav_parse(rwav_t *out, const void *buf, size_t len)
{
   if (!out || !buf)
      return RWAV_ITERATE_ERROR;
   out->samples = NULL;
   return rwav_walk(out, (const uint8_t*)buf, len);
}

void rwav_init(rwav_iterator_t* iter, rwav_t* out, const void *s, size_t len)
{
   iter->out    = out;
   iter->data   = (const uint8_t*)s;
   iter->size   = len;
   iter->step   = ITER_BEGIN;

   out->samples = NULL;
}

enum rwav_state rwav_iterate(rwav_iterator_t *iter)
{
   size_t s;
   uint16_t *u16       = NULL;
   uint32_t *u32       = NULL;
   void *samples       = NULL;
   rwav_t *rwav        = iter->out;
   const uint8_t *data = iter->data;

   switch (iter->step)
   {
      case ITER_BEGIN:
         if (rwav_walk(rwav, data, iter->size) != RWAV_ITERATE_DONE)
            return RWAV_ITERATE_ERROR;

         /* the walk clamped the payload to what the buffer holds and
          * rounded it to whole frames, so the copy below stays inside
          * both this allocation and the source */
         samples = malloc(rwav->subchunk2size);

         if (!samples)
            return RWAV_ITERATE_ERROR;

         rwav->samples = samples;

         iter->step = ITER_COPY_SAMPLES;
         return RWAV_ITERATE_MORE;

      case ITER_COPY_SAMPLES:
         iter->i = 0;

         /* 8-bit needs no conversion, and 24-bit is handed over as
          * stored, so both are the same plain copy */
         if (rwav->bitspersample == 8 || rwav->bitspersample == 24)
         {
            iter->step = ITER_COPY_SAMPLES_8;

            /* TODO/FIXME - what is going on here? */
            case ITER_COPY_SAMPLES_8:
            s = rwav->subchunk2size - iter->i;

            if (s > RWAV_ITERATE_BUF_SIZE)
               s = RWAV_ITERATE_BUF_SIZE;

            memcpy((void*)((uint8_t*)rwav->samples + iter->i),
                  (void*)(iter->data + rwav->dataoffset + iter->i), s);
            iter->i += s;
         }
         else if (rwav->bitspersample == 16)
         {
            iter->step = ITER_COPY_SAMPLES_16;
            iter->j    = 0;

            /* TODO/FIXME - what is going on here? */
            case ITER_COPY_SAMPLES_16:
            s = rwav->subchunk2size - iter->i;

            if (s > RWAV_ITERATE_BUF_SIZE)
               s = RWAV_ITERATE_BUF_SIZE;

            u16 = (uint16_t *)rwav->samples;

            while (s != 0)
            {
               u16[iter->j++] = iter->data[rwav->dataoffset + iter->i]
                  | iter->data[rwav->dataoffset + iter->i + 1] << 8;
               iter->i += 2;
               s -= 2;
            }
         }
         else
         {
            iter->step = ITER_COPY_SAMPLES_32;
            iter->j    = 0;

            /* the samples are IEEE-float little-endian words; assemble
             * them to host order the same way the 16-bit path does */
            case ITER_COPY_SAMPLES_32:
            s = rwav->subchunk2size - iter->i;

            if (s > RWAV_ITERATE_BUF_SIZE)
               s = RWAV_ITERATE_BUF_SIZE;

            u32 = (uint32_t *)rwav->samples;

            while (s != 0)
            {
               u32[iter->j++] =
                    (uint32_t)iter->data[rwav->dataoffset + iter->i]
                  | (uint32_t)iter->data[rwav->dataoffset + iter->i + 1] << 8
                  | (uint32_t)iter->data[rwav->dataoffset + iter->i + 2] << 16
                  | (uint32_t)iter->data[rwav->dataoffset + iter->i + 3] << 24;
               iter->i += 4;
               s -= 4;
            }
         }

         if (iter->i < rwav->subchunk2size)
            return RWAV_ITERATE_MORE;
         return RWAV_ITERATE_DONE;
   }

   return RWAV_ITERATE_ERROR;
}

enum rwav_state rwav_load(rwav_t* out, const void *s, size_t len)
{
   enum rwav_state res;
   rwav_iterator_t iter;

   iter.out             = NULL;
   iter.data            = NULL;
   iter.size            = 0;
   iter.i               = 0;
   iter.j               = 0;
   iter.step            = 0;

   rwav_init(&iter, out, s, len);

   do
   {
      res = rwav_iterate(&iter);
   }while (res == RWAV_ITERATE_MORE);

   return res;
}

void rwav_free(rwav_t *rwav)
{
   free((void*)rwav->samples);
}
