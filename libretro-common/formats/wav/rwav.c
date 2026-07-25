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
 * bits, IEEE float at 32, G.711 a-law or mu-law at 8, or MS or IMA/DVI
 * ADPCM at 4 - any channel count and sample rate.  The RIFF chunk list
 * is walked, so 'fmt ' and 'data' are found wherever they sit and the
 * chunks writers put between them - LIST, fact, cue, JUNK and the rest
 * - are stepped over; a fmt chunk longer than 16 bytes is accepted for
 * its standard first 16, and read further where the format states more
 * (a block size and a frame count for the ADPCM pair, and predictor
 * coefficients for the MS one).
 *
 * Three ways in.
 *
 * rwav_parse reads the header alone and reports where the payload is,
 * allocating and copying nothing, for a caller that would rather take
 * frames out of its own buffer as it needs them - which also lets that
 * buffer hold only part of the file, since parsing touches chunk
 * headers and never the payload.
 *
 * rwav_decode_s16 turns any of those payloads into interleaved s16,
 * from a frame index the caller chooses, so it need not know which
 * format it has.  It is the only way to read the ADPCM pair, whose
 * samples continue from predictor state a block establishes and which
 * therefore cannot be read a frame at a time from the buffer; the
 * companded pair need it for their curve.  A block restates its own
 * predictor, so a read starting anywhere decodes only the block it
 * lands in.
 *
 * rwav_load and the resumable iterator behind it copy the payload
 * whole into a buffer this file owns, in native memory order where
 * that means anything: unsigned bytes for 8-bit, host-endian int16 for
 * 16-bit, host-endian float words for 32-bit, byte order fixed up from
 * the file's little-endian layout on big-endian hosts.  24-bit is
 * handed over packed as the file stores it, three little-endian bytes
 * a sample, since no host type is that wide; rwav.h carries the
 * accessors.  The companded and coded payloads are copied verbatim and
 * stay coded - there is no host order for a curve or a nibble - and
 * rwav_decode_s16 reads them from there as it would from the file.
 *
 * WAVE_FORMAT_EXTENSIBLE headers are resolved through their SubFormat
 * GUID, so a file that is only extensible because it has more than two
 * channels reads as the plain PCM or float it actually holds, and one
 * naming a compressed format reads as that.
 *
 * What it does not implement: writing, and any format not listed above
 * - which now means the rarer codecs a WAV can nominally carry, GSM
 * and the like, rather than the common compressed ones.  A declared
 * data size larger than the buffer is taken as the buffer's worth
 * rather than an error, which covers both a truncated file and a
 * writer that stamped a placeholder length it never went back to fix;
 * a trailing partial unit - a frame, or a block where a block is the
 * unit - is dropped.
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

   /* 1 is integer PCM and 3 IEEE float; 6 and 7 are the G.711
    * companded pair, eight bits carrying a logarithmic sixteen; 2 and
    * 17 are the two ADPCM layouts, four bits a sample in blocks that
    * each restate their predictor.  Anything past the standard 16
    * bytes of a format is ignored rather than refused. */
   switch (tag)
   {
      case 1:
         out->format = RWAV_FORMAT_PCM;
         if (bits != 8 && bits != 16 && bits != 24)
            return RWAV_ITERATE_ERROR;
         break;
      case 3:
         out->format = RWAV_FORMAT_FLOAT;
         if (bits != 32)
            return RWAV_ITERATE_ERROR;
         break;
      case 6:
      case 7:
         out->format = (tag == 6) ? RWAV_FORMAT_ALAW : RWAV_FORMAT_MULAW;
         if (bits != 8)
            return RWAV_ITERATE_ERROR;
         break;
      case 2:
      case 17:
         out->format = (tag == 2) ? RWAV_FORMAT_MS_ADPCM
                                  : RWAV_FORMAT_IMA_ADPCM;
         if (bits != 4)
            return RWAV_ITERATE_ERROR;
         break;
      default:
         return RWAV_ITERATE_ERROR;
   }
   /* a zero channel count divides by zero working out the frame count,
    * and a zero rate is unplayable: refuse both here rather than hand
    * them on */
   if (!channels || !rate)
      return RWAV_ITERATE_ERROR;

   out->numcoef          = 0;
   out->samplesperblock  = 1;

   if (   out->format == RWAV_FORMAT_MS_ADPCM
       || out->format == RWAV_FORMAT_IMA_ADPCM)
   {
      /* A coded block is the unit here, and its size and the frames
       * it holds come from the header rather than the sample width.
       * Both are stated; neither is inferable, an MS block's nibble
       * count depending on how many bytes of preamble each channel
       * spends. */
      /* fmt_off is the chunk body: nBlockAlign at 12, and the
       * extension past cbSize at 18, where both layouts state the
       * frames a block holds. */
      unsigned spb;
      blockalign = (size_t)rwav_u16(data + fmt_off + 12);
      spb        = (fmt_size >= 20)
                 ? (unsigned)rwav_u16(data + fmt_off + 18) : 0;
      if (blockalign < (size_t)channels * 8 || blockalign > 65535)
         return RWAV_ITERATE_ERROR;
      if (out->format == RWAV_FORMAT_MS_ADPCM)
      {
         /* wNumCoef at 20, the pairs from 22. */
         unsigned i, n;
         if (fmt_size < 22)
            return RWAV_ITERATE_ERROR;
         n = (unsigned)rwav_u16(data + fmt_off + 20);
         if (n > 16 || fmt_size < 22 + n * 4)
            return RWAV_ITERATE_ERROR;
         for (i = 0; i < n; i++)
         {
            out->coef[i][0] =
                  (short)rwav_u16(data + fmt_off + 22 + i * 4);
            out->coef[i][1] =
                  (short)rwav_u16(data + fmt_off + 24 + i * 4);
         }
         out->numcoef = n;
         if (!spb)
            spb = (unsigned)((blockalign - 7 * channels) * 2 / channels
                  + 2);
      }
      else if (!spb)
         spb = (unsigned)((blockalign - 4 * channels) * 2 / channels + 1);
      if (!spb)
         return RWAV_ITERATE_ERROR;
      out->samplesperblock = spb;
   }
   else
      blockalign = (size_t)channels * (size_t)(bits / 8);

   avail      = len - data_off;
   if (data_size > avail)
      data_size = avail;
   /* whole units only - the readers step one at a time, and a
    * trailing partial one would run them past the payload */
   data_size -= data_size % blockalign;
   if (!data_size)
      return RWAV_ITERATE_ERROR;

   out->bitspersample = bits;
   out->numchannels   = channels;
   out->samplerate    = rate;
   out->subchunk2size = data_size;
   out->blockalign    = (unsigned)blockalign;
   out->numsamples    = (data_size / blockalign)
                      * (size_t)out->samplesperblock;
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

/* Copy the payload a bounded chunk at a time, resuming where the last
 * call stopped.
 *
 * This used to put the case labels inside the if-blocks that choose
 * the copy, so that the switch re-entered mid-block and skipped the
 * choice on every call after the first.  It is legal C and it is what
 * the two "what is going on here" notes were about; it is also not
 * buying anything, the choice being three comparisons made once.  The
 * labels are at the top of the switch now and the choice happens when
 * the payload is allocated, which reads as what it is.
 *
 * Which copy an payload gets follows its format, not its sample
 * width.  Sixteen-bit PCM and 32-bit float are little-endian words
 * that need assembling into host order; everything else is bytes -
 * eight- and 24-bit PCM the host reads as stored, and the companded
 * and block-coded payloads, which stay coded here and are decoded by
 * rwav_decode_s16 when a caller asks for samples.  Choosing by width
 * sent four-bit ADPCM down the float path, which assembled its coded
 * bytes into words on a big-endian host and stepped four at a time
 * through a length that need not be a multiple of four.
 */
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
          * rounded it to whole units, so the copy below stays inside
          * both this allocation and the source */
         if (!(samples = malloc(rwav->subchunk2size)))
            return RWAV_ITERATE_ERROR;

         rwav->samples = samples;
         iter->i       = 0;
         iter->j       = 0;

         if (rwav->format == RWAV_FORMAT_PCM
               && rwav->bitspersample == 16)
            iter->step = ITER_COPY_SAMPLES_16;
         else if (rwav->format == RWAV_FORMAT_FLOAT)
            iter->step = ITER_COPY_SAMPLES_32;
         else
            iter->step = ITER_COPY_SAMPLES_8;
         return RWAV_ITERATE_MORE;

      case ITER_COPY_SAMPLES_8:
         s = rwav->subchunk2size - iter->i;
         if (s > RWAV_ITERATE_BUF_SIZE)
            s = RWAV_ITERATE_BUF_SIZE;
         memcpy((uint8_t*)rwav->samples + iter->i,
               iter->data + rwav->dataoffset + iter->i, s);
         iter->i += s;
         break;

      case ITER_COPY_SAMPLES_16:
         s = rwav->subchunk2size - iter->i;
         if (s > RWAV_ITERATE_BUF_SIZE)
            s = RWAV_ITERATE_BUF_SIZE;
         u16 = (uint16_t*)rwav->samples;
         while (s >= 2)
         {
            u16[iter->j++] =
                 (uint16_t)iter->data[rwav->dataoffset + iter->i]
               | (uint16_t)iter->data[rwav->dataoffset + iter->i + 1] << 8;
            iter->i += 2;
            s       -= 2;
         }
         break;

      case ITER_COPY_SAMPLES_32:
         s = rwav->subchunk2size - iter->i;
         if (s > RWAV_ITERATE_BUF_SIZE)
            s = RWAV_ITERATE_BUF_SIZE;
         u32 = (uint32_t*)rwav->samples;
         while (s >= 4)
         {
            u32[iter->j++] =
                 (uint32_t)iter->data[rwav->dataoffset + iter->i]
               | (uint32_t)iter->data[rwav->dataoffset + iter->i + 1] << 8
               | (uint32_t)iter->data[rwav->dataoffset + iter->i + 2] << 16
               | (uint32_t)iter->data[rwav->dataoffset + iter->i + 3] << 24;
            iter->i += 4;
            s       -= 4;
         }
         break;

      default:
         return RWAV_ITERATE_ERROR;
   }

   if (iter->i < rwav->subchunk2size)
      return RWAV_ITERATE_MORE;
   return RWAV_ITERATE_DONE;
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

/* ---- decoding -------------------------------------------------------
 *
 * One entry point for every payload rwav accepts, so a caller need
 * not know which it has.  The uncompressed and companded formats are
 * a per-sample conversion; the two ADPCM layouts are block coded and
 * are the reason this exists at all, their samples continuing from
 * predictor state that only a block's own preamble establishes.
 */

static short rwav_alaw_to_s16(unsigned char a)
{
   int t, seg;
   a  ^= 0x55;
   t   = (a & 0x0f) << 4;
   seg = (a & 0x70) >> 4;
   if (seg == 0)
      t += 8;
   else
   {
      t += 0x108;
      t <<= seg - 1;
   }
   return (short)((a & 0x80) ? t : -t);
}

static short rwav_mulaw_to_s16(unsigned char u)
{
   int t;
   u = (unsigned char)~u;
   t = (((u & 0x0f) << 3) + 0x84) << ((u & 0x70) >> 4);
   t -= 0x84;
   return (short)((u & 0x80) ? (0x84 - (t + 0x84)) : t);
}

static short rwav_clamp16(int v)
{
   if (v >  32767)
      return  32767;
   if (v < -32768)
      return -32768;
   return (short)v;
}

/* IMA step and index tables (IMA ADPCM, ANSI/IEEE). */
static const short rwav_ima_step[89] = {
   7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,
   73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,
   408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,
   1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,
   7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,
   22385,24623,27086,29794,32767
};
static const signed char rwav_ima_index[16] = {
   -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8
};
/* MS ADPCM adaptation table, and the coefficients a file may omit. */
static const short rwav_ms_adapt[16] = {
   230,230,230,230,307,409,512,614,768,614,512,409,307,230,230,230
};

/* Decode one block into 'out', which must hold samplesperblock frames.
 * Returns frames produced, 0 on a malformed block. */
static unsigned rwav_decode_block(const rwav_t *wav, const unsigned char *b,
      size_t blen, short *out)
{
   unsigned ch  = wav->numchannels;
   unsigned spb = wav->samplesperblock;
   unsigned c, i;

   if (wav->format == RWAV_FORMAT_IMA_ADPCM)
   {
      int pred[16], idx[16];
      unsigned need = 4 * ch;
      if (ch > 16 || blen < need)
         return 0;
      for (c = 0; c < ch; c++)
      {
         pred[c] = (short)rwav_u16(b + c * 4);
         idx[c]  = b[c * 4 + 2];
         if (idx[c] > 88)
            idx[c] = 88;
         out[c]  = (short)pred[c];
      }
      /* After the preamble the nibbles come in eight-sample runs per
       * channel: four bytes of one channel, then four of the next. */
      i = 1;
      {
         size_t off = need;
         while (i < spb && off + 4 * ch <= blen)
         {
            for (c = 0; c < ch; c++)
            {
               unsigned k;
               for (k = 0; k < 8; k++)
               {
                  unsigned nib = (k & 1)
                        ? (b[off + c * 4 + (k >> 1)] >> 4)
                        : (b[off + c * 4 + (k >> 1)] & 0x0f);
                  /* ((2*delta + 1) * step) >> 3, not the
                   * shift-and-accumulate form.  The two disagree
                   * once the step is small enough for the shifts to
                   * truncate - step 7 gives 1 against 2 - and this
                   * is the one every reference decoder uses. */
                  int step  = rwav_ima_step[idx[c]];
                  int delta = (int)(nib & 7);
                  int diff  = ((2 * delta + 1) * step) >> 3;
                  if (nib & 8)
                     diff = -diff;
                  pred[c] = rwav_clamp16(pred[c] + diff);
                  idx[c] += rwav_ima_index[nib];
                  if (idx[c] < 0)  idx[c] = 0;
                  if (idx[c] > 88) idx[c] = 88;
                  if (i + k < spb)
                     out[(i + k) * ch + c] = (short)pred[c];
               }
            }
            off += 4 * ch;
            i   += 8;
         }
      }
      return spb;
   }

   /* MS ADPCM: each channel states a coefficient index, a delta and
    * two priming samples, and the nibbles follow interleaved. */
   {
      int co1[16], co2[16], delta[16], s1[16], s2[16];
      unsigned need = 7 * ch;
      size_t   off;
      if (ch > 16 || blen < need || spb < 2)
         return 0;
      for (c = 0; c < ch; c++)
      {
         unsigned pi = b[c];
         if (pi >= wav->numcoef)
            return 0;
         co1[c] = wav->coef[pi][0];
         co2[c] = wav->coef[pi][1];
      }
      for (c = 0; c < ch; c++)
         delta[c] = (short)rwav_u16(b + ch + c * 2);
      for (c = 0; c < ch; c++)
         s1[c] = (short)rwav_u16(b + ch * 3 + c * 2);
      for (c = 0; c < ch; c++)
         s2[c] = (short)rwav_u16(b + ch * 5 + c * 2);
      for (c = 0; c < ch; c++)
      {
         out[c]      = (short)s2[c];
         out[ch + c] = (short)s1[c];
      }
      off = need;
      i   = 2;
      while (i < spb && off < blen)
      {
         for (c = 0; c < ch && i < spb; c++)
         {
            unsigned byte = b[off];
            int      nib  = (c & 1) ? (byte & 0x0f) : (byte >> 4);
            int      sn   = (nib & 8) ? nib - 16 : nib;
            int      p    = (s1[c] * co1[c] + s2[c] * co2[c]) / 256
                          + sn * delta[c];
            p        = rwav_clamp16(p);
            s2[c]    = s1[c];
            s1[c]    = p;
            delta[c] = (rwav_ms_adapt[nib] * delta[c]) / 256;
            if (delta[c] < 16)
               delta[c] = 16;
            out[i * ch + c] = (short)p;
            if (c & 1)
               off++;
            if (c == ch - 1)
               i++;
         }
         if (ch == 1)
         {
            /* one channel packs two samples per byte */
            if (i < spb)
            {
               unsigned byte = b[off];
               int nib = byte & 0x0f;
               int sn  = (nib & 8) ? nib - 16 : nib;
               int p   = (s1[0] * co1[0] + s2[0] * co2[0]) / 256
                       + sn * delta[0];
               p        = rwav_clamp16(p);
               s2[0]    = s1[0];
               s1[0]    = p;
               delta[0] = (rwav_ms_adapt[nib] * delta[0]) / 256;
               if (delta[0] < 16)
                  delta[0] = 16;
               out[i * ch] = (short)p;
               i++;
            }
            off++;
         }
      }
      return spb;
   }
}

size_t rwav_decode_s16(const rwav_t *wav, const void *base, size_t frame,
      size_t frames, short *out)
{
   const unsigned char *d;
   unsigned ch;
   size_t   done = 0;

   if (!wav || !base || !out)
      return 0;
   ch = wav->numchannels;
   d  = (const unsigned char*)base + wav->dataoffset;

   if (frame >= wav->numsamples)
      return 0;
   if (frames > wav->numsamples - frame)
      frames = wav->numsamples - frame;

   if (   wav->format == RWAV_FORMAT_MS_ADPCM
       || wav->format == RWAV_FORMAT_IMA_ADPCM)
   {
      /* Start at the block holding 'frame' and step forward within
       * it: a block restates its own predictor, so nothing before it
       * has to be decoded. */
      static short blk[8192 * 16];
      unsigned spb = wav->samplesperblock;
      while (done < frames)
      {
         size_t   f      = frame + done;
         size_t   bidx   = f / spb;
         unsigned within = (unsigned)(f % spb);
         size_t   boff   = bidx * wav->blockalign;
         unsigned got, take;
         if (boff + wav->blockalign > wav->subchunk2size)
            break;
         if ((size_t)spb * ch > sizeof(blk) / sizeof(blk[0]))
            break;
         got = rwav_decode_block(wav, d + boff, wav->blockalign, blk);
         if (!got || within >= got)
            break;
         take = got - within;
         if ((size_t)take > frames - done)
            take = (unsigned)(frames - done);
         memcpy(out + done * ch, blk + (size_t)within * ch,
               (size_t)take * ch * sizeof(short));
         done += take;
      }
      return done;
   }

   for (done = 0; done < frames; done++)
   {
      const unsigned char *p = d + (frame + done) * wav->blockalign;
      unsigned c;
      for (c = 0; c < ch; c++)
      {
         switch (wav->format)
         {
            case RWAV_FORMAT_ALAW:
               out[done * ch + c] = rwav_alaw_to_s16(p[c]);
               break;
            case RWAV_FORMAT_MULAW:
               out[done * ch + c] = rwav_mulaw_to_s16(p[c]);
               break;
            case RWAV_FORMAT_FLOAT:
            {
               float v = rwav_f32(p + c * 4) * 32768.0f;
               out[done * ch + c] = rwav_clamp16(
                     (int)(v >= 0.0f ? v + 0.5f : v - 0.5f));
               break;
            }
            default:
               if (wav->bitspersample == 8)
                  out[done * ch + c] =
                        (short)(((int)p[c] - 128) << 8);
               else if (wav->bitspersample == 24)
                  out[done * ch + c] = rwav_s24_to_s16(p + c * 3);
               else
                  out[done * ch + c] = rwav_s16(p + c * 2);
               break;
         }
      }
   }
   return done;
}
