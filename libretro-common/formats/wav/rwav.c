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
#include <retro_endianness.h>

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
      unsigned spb, max_spb;
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
         max_spb = (unsigned)((blockalign - 7 * channels) * 2 / channels
               + 2);
      }
      else
         max_spb = (unsigned)((blockalign - 4 * channels) * 2 / channels
               + 1);
      if (!spb)
         spb = max_spb;
      /* samples-per-block comes from the file, but a coded block can
       * only hold as many frames as remain after its per-channel
       * predictor header.  Trusting a larger declaration lets a small
       * payload expand the frame count enough to wrap callers' output
       * allocations. */
      if (!spb || spb > max_spb)
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
   avail = data_size / blockalign;
   if (avail > SIZE_MAX / (size_t)out->samplesperblock)
      return RWAV_ITERATE_ERROR;
   out->numsamples    = avail * (size_t)out->samplesperblock;
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
#ifdef MSB_FIRST
   uint16_t *u16       = NULL;
   uint32_t *u32       = NULL;
#endif
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
#ifndef MSB_FIRST
         /* File order is host order here; the byte assembly below is
          * only the big-endian host's swab. */
         memcpy((uint8_t*)rwav->samples + iter->i,
               iter->data + rwav->dataoffset + iter->i, s & ~(size_t)1);
         iter->i += s & ~(size_t)1;
         break;
#else
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
#endif

      case ITER_COPY_SAMPLES_32:
         s = rwav->subchunk2size - iter->i;
         if (s > RWAV_ITERATE_BUF_SIZE)
            s = RWAV_ITERATE_BUF_SIZE;
#ifndef MSB_FIRST
         memcpy((uint8_t*)rwav->samples + iter->i,
               iter->data + rwav->dataoffset + iter->i, s & ~(size_t)3);
         iter->i += s & ~(size_t)3;
         break;
#else
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
#endif

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

/* Selects rather than branches, so compilers emit conditional moves:
 * the ADPCM decoders run it once a sample on a dependency chain, where
 * a mispredicted branch costs several times the arithmetic. */
static INLINE short rwav_clamp16(int v)
{
   if (v >  32767)
      return  32767;
   if (v < -32768)
      return -32768;
   return (short)v;
}

/* IMA index table (IMA ADPCM, ANSI/IEEE); the step table is folded
 * into rwav_ima_diff below. */
static const signed char rwav_ima_index[16] = {
   -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8
};
/* The signed difference each nibble makes at each step index,
 * ((2*(nib & 7) + 1) * step) >> 3 negated for the upper eight: the
 * product, the shift and the sign are a lookup, done once here instead
 * of once a sample. */
static const int rwav_ima_diff[89 * 16] = {
   0,2,4,6,7,9,11,13,0,-2,-4,-6,-7,-9,-11,-13,
   1,3,5,7,9,11,13,15,-1,-3,-5,-7,-9,-11,-13,-15,
   1,3,5,7,10,12,14,16,-1,-3,-5,-7,-10,-12,-14,-16,
   1,3,6,8,11,13,16,18,-1,-3,-6,-8,-11,-13,-16,-18,
   1,4,6,9,12,15,17,20,-1,-4,-6,-9,-12,-15,-17,-20,
   1,4,7,10,13,16,19,22,-1,-4,-7,-10,-13,-16,-19,-22,
   1,4,8,11,14,17,21,24,-1,-4,-8,-11,-14,-17,-21,-24,
   1,5,8,12,15,19,22,26,-1,-5,-8,-12,-15,-19,-22,-26,
   2,6,10,14,18,22,26,30,-2,-6,-10,-14,-18,-22,-26,-30,
   2,6,10,14,19,23,27,31,-2,-6,-10,-14,-19,-23,-27,-31,
   2,7,11,16,21,26,30,35,-2,-7,-11,-16,-21,-26,-30,-35,
   2,7,13,18,23,28,34,39,-2,-7,-13,-18,-23,-28,-34,-39,
   2,8,14,20,25,31,37,43,-2,-8,-14,-20,-25,-31,-37,-43,
   3,9,15,21,28,34,40,46,-3,-9,-15,-21,-28,-34,-40,-46,
   3,10,17,24,31,38,45,52,-3,-10,-17,-24,-31,-38,-45,-52,
   3,11,19,27,34,42,50,58,-3,-11,-19,-27,-34,-42,-50,-58,
   4,12,21,29,38,46,55,63,-4,-12,-21,-29,-38,-46,-55,-63,
   4,13,23,32,41,50,60,69,-4,-13,-23,-32,-41,-50,-60,-69,
   5,15,25,35,46,56,66,76,-5,-15,-25,-35,-46,-56,-66,-76,
   5,16,28,39,50,61,73,84,-5,-16,-28,-39,-50,-61,-73,-84,
   6,18,31,43,56,68,81,93,-6,-18,-31,-43,-56,-68,-81,-93,
   6,20,34,48,61,75,89,103,-6,-20,-34,-48,-61,-75,-89,-103,
   7,22,37,52,67,82,97,112,-7,-22,-37,-52,-67,-82,-97,-112,
   8,24,41,57,74,90,107,123,-8,-24,-41,-57,-74,-90,-107,-123,
   9,27,45,63,82,100,118,136,-9,-27,-45,-63,-82,-100,-118,-136,
   10,30,50,70,90,110,130,150,-10,-30,-50,-70,-90,-110,-130,-150,
   11,33,55,77,99,121,143,165,-11,-33,-55,-77,-99,-121,-143,-165,
   12,36,60,84,109,133,157,181,-12,-36,-60,-84,-109,-133,-157,-181,
   13,40,66,93,120,147,173,200,-13,-40,-66,-93,-120,-147,-173,-200,
   14,44,73,103,132,162,191,221,-14,-44,-73,-103,-132,-162,-191,-221,
   16,48,81,113,146,178,211,243,-16,-48,-81,-113,-146,-178,-211,-243,
   17,53,89,125,160,196,232,268,-17,-53,-89,-125,-160,-196,-232,-268,
   19,58,98,137,176,215,255,294,-19,-58,-98,-137,-176,-215,-255,-294,
   21,64,108,151,194,237,281,324,-21,-64,-108,-151,-194,-237,-281,-324,
   23,71,118,166,213,261,308,356,-23,-71,-118,-166,-213,-261,-308,-356,
   26,78,130,182,235,287,339,391,-26,-78,-130,-182,-235,-287,-339,-391,
   28,86,143,201,258,316,373,431,-28,-86,-143,-201,-258,-316,-373,-431,
   31,94,158,221,284,347,411,474,-31,-94,-158,-221,-284,-347,-411,-474,
   34,104,174,244,313,383,453,523,-34,-104,-174,-244,-313,-383,-453,-523,
   38,115,191,268,345,422,498,575,-38,-115,-191,-268,-345,-422,-498,-575,
   42,126,210,294,379,463,547,631,-42,-126,-210,-294,-379,-463,-547,-631,
   46,139,231,324,417,510,602,695,-46,-139,-231,-324,-417,-510,-602,-695,
   51,153,255,357,459,561,663,765,-51,-153,-255,-357,-459,-561,-663,-765,
   56,168,280,392,505,617,729,841,-56,-168,-280,-392,-505,-617,-729,-841,
   61,185,308,432,555,679,802,926,-61,-185,-308,-432,-555,-679,-802,-926,
   68,204,340,476,612,748,884,1020,-68,-204,-340,-476,-612,-748,-884,-1020,
   74,224,373,523,672,822,971,1121,-74,-224,-373,-523,-672,-822,-971,-1121,
   82,246,411,575,740,904,1069,1233,-82,-246,-411,-575,-740,-904,-1069,-1233,
   90,271,452,633,814,995,1176,1357,-90,-271,-452,-633,-814,-995,-1176,-1357,
   99,298,497,696,895,1094,1293,1492,-99,-298,-497,-696,-895,-1094,-1293,-1492,
   109,328,547,766,985,1204,1423,1642,-109,-328,-547,-766,-985,-1204,-1423,-1642,
   120,361,601,842,1083,1324,1564,1805,-120,-361,-601,-842,-1083,-1324,-1564,-1805,
   132,397,662,927,1192,1457,1722,1987,-132,-397,-662,-927,-1192,-1457,-1722,-1987,
   145,437,728,1020,1311,1603,1894,2186,-145,-437,-728,-1020,-1311,-1603,-1894,-2186,
   160,480,801,1121,1442,1762,2083,2403,-160,-480,-801,-1121,-1442,-1762,-2083,-2403,
   176,529,881,1234,1587,1940,2292,2645,-176,-529,-881,-1234,-1587,-1940,-2292,-2645,
   194,582,970,1358,1746,2134,2522,2910,-194,-582,-970,-1358,-1746,-2134,-2522,-2910,
   213,640,1066,1493,1920,2347,2773,3200,-213,-640,-1066,-1493,-1920,-2347,-2773,-3200,
   234,704,1173,1643,2112,2582,3051,3521,-234,-704,-1173,-1643,-2112,-2582,-3051,-3521,
   258,774,1291,1807,2324,2840,3357,3873,-258,-774,-1291,-1807,-2324,-2840,-3357,-3873,
   284,852,1420,1988,2556,3124,3692,4260,-284,-852,-1420,-1988,-2556,-3124,-3692,-4260,
   312,937,1561,2186,2811,3436,4060,4685,-312,-937,-1561,-2186,-2811,-3436,-4060,-4685,
   343,1030,1718,2405,3092,3779,4467,5154,-343,-1030,-1718,-2405,-3092,-3779,-4467,-5154,
   378,1134,1890,2646,3402,4158,4914,5670,-378,-1134,-1890,-2646,-3402,-4158,-4914,-5670,
   415,1247,2079,2911,3742,4574,5406,6238,-415,-1247,-2079,-2911,-3742,-4574,-5406,-6238,
   457,1372,2287,3202,4117,5032,5947,6862,-457,-1372,-2287,-3202,-4117,-5032,-5947,-6862,
   503,1509,2516,3522,4529,5535,6542,7548,-503,-1509,-2516,-3522,-4529,-5535,-6542,-7548,
   553,1660,2767,3874,4981,6088,7195,8302,-553,-1660,-2767,-3874,-4981,-6088,-7195,-8302,
   608,1826,3044,4262,5479,6697,7915,9133,-608,-1826,-3044,-4262,-5479,-6697,-7915,-9133,
   669,2009,3348,4688,6027,7367,8706,10046,-669,-2009,-3348,-4688,-6027,-7367,-8706,-10046,
   736,2210,3683,5157,6630,8104,9577,11051,-736,-2210,-3683,-5157,-6630,-8104,-9577,-11051,
   810,2431,4052,5673,7294,8915,10536,12157,-810,-2431,-4052,-5673,-7294,-8915,-10536,-12157,
   891,2674,4457,6240,8023,9806,11589,13372,-891,-2674,-4457,-6240,-8023,-9806,-11589,-13372,
   980,2941,4903,6864,8825,10786,12748,14709,-980,-2941,-4903,-6864,-8825,-10786,-12748,-14709,
   1078,3236,5393,7551,9708,11866,14023,16181,-1078,-3236,-5393,-7551,-9708,-11866,-14023,-16181,
   1186,3559,5933,8306,10679,13052,15426,17799,-1186,-3559,-5933,-8306,-10679,-13052,-15426,-17799,
   1305,3915,6526,9136,11747,14357,16968,19578,-1305,-3915,-6526,-9136,-11747,-14357,-16968,-19578,
   1435,4307,7179,10051,12922,15794,18666,21538,-1435,-4307,-7179,-10051,-12922,-15794,-18666,-21538,
   1579,4738,7896,11055,14214,17373,20531,23690,-1579,-4738,-7896,-11055,-14214,-17373,-20531,-23690,
   1737,5212,8686,12161,15636,19111,22585,26060,-1737,-5212,-8686,-12161,-15636,-19111,-22585,-26060,
   1911,5733,9555,13377,17200,21022,24844,28666,-1911,-5733,-9555,-13377,-17200,-21022,-24844,-28666,
   2102,6306,10511,14715,18920,23124,27329,31533,-2102,-6306,-10511,-14715,-18920,-23124,-27329,-31533,
   2312,6937,11562,16187,20812,25437,30062,34687,-2312,-6937,-11562,-16187,-20812,-25437,-30062,-34687,
   2543,7631,12718,17806,22893,27981,33068,38156,-2543,-7631,-12718,-17806,-22893,-27981,-33068,-38156,
   2798,8394,13990,19586,25183,30779,36375,41971,-2798,-8394,-13990,-19586,-25183,-30779,-36375,-41971,
   3077,9233,15389,21545,27700,33856,40012,46168,-3077,-9233,-15389,-21545,-27700,-33856,-40012,-46168,
   3385,10157,16928,23700,30471,37243,44014,50786,-3385,-10157,-16928,-23700,-30471,-37243,-44014,-50786,
   3724,11172,18621,26069,33518,40966,48415,55863,-3724,-11172,-18621,-26069,-33518,-40966,-48415,-55863,
   4095,12287,20479,28671,36862,45054,53246,61438,-4095,-12287,-20479,-28671,-36862,-45054,-53246,-61438
};
/* MS ADPCM adaptation table, and the coefficients a file may omit. */
static const short rwav_ms_adapt[16] = {
   230,230,230,230,307,409,512,614,768,614,512,409,307,230,230,230
};

/* MS ADPCM's step, kept inside an int.  The file states each block's
 * opening delta, and a run of large nibbles grows it by 768/256 a
 * sample without bound, so a crafted block overflows the multiply
 * within a few dozen samples; the cap is the largest delta that the
 * next adaptation and the nibble times delta (at most 8x) still hold,
 * as reference decoders cap it.  No real encoder gets near it: past
 * 32767 every sample already clamps. */
#define RWAV_MS_DELTA_MAX (0x7FFFFFFF / 768)
static int rwav_ms_adapt_delta(int nib, int delta)
{
   if (delta > RWAV_MS_DELTA_MAX)
      delta = RWAV_MS_DELTA_MAX;
   delta = (rwav_ms_adapt[nib] * delta) / 256;
   return (delta < 16) ? 16 : delta;
}

/* The prediction from the two previous samples.  Samples are 16-bit
 * but the coefficients are the file's, so two products at -32768 each
 * sum to 2^31: summed wide, then divided as before. */
static int rwav_ms_predict(int s1, int co1, int s2, int co2)
{
   return (int)(((int64_t)s1 * co1 + (int64_t)s2 * co2) / 256);
}

/* ---- block decoding ----
 *
 * A block is decoded from its start - its preamble states the
 * predictor the samples continue from - but only the frames in
 * [first, end) are stored, at @out relative to @first, and nothing
 * past @end is decoded.  So a window landing inside a block needs no
 * scratch to be trimmed afterwards, and one ending early stops there.
 *
 * Each sample depends on the one before it, so the decode is a chain
 * of dependent steps; the state of that chain is kept in locals for a
 * run of samples at a time - eight nibbles of one IMA channel, or a
 * whole block of MS mono or stereo - since state kept in arrays
 * indexed by a runtime channel count goes through memory every sample,
 * which costs several times the arithmetic. */

/* One IMA nibble: the step from the index, ((2*delta + 1) * step) >> 3
 * rather than the shift-and-accumulate form - the two disagree once
 * the step is small enough for the shifts to truncate, step 7 giving 1
 * against 2, and this is the one every reference decoder uses. */
#define RWAV_IMA_STEP(p, x, nib)                                         \
   do {                                                                  \
      (p)  = rwav_clamp16((p) + rwav_ima_diff[(x) * 16 + (nib)]);         \
      (x) += rwav_ima_index[nib];                                        \
      (x)  = ((x) < 0) ? 0 : (((x) > 88) ? 88 : (x));                     \
   } while (0)

/* One channel's run of eight IMA samples from the four bytes at @q,
 * low nibble first.  All eight are decoded, for the state; sample k is
 * stored at o[(k - klo) * stride] for klo <= k < khi. */
static INLINE void rwav_ima_run(const unsigned char *q, int *pred,
      int *index, short *o, unsigned stride, unsigned klo, unsigned khi)
{
   int      p = *pred;
   int      x = *index;
   unsigned k;
   if (!klo && khi == 8)
      /* the whole run is in the window: no test a sample */
      for (k = 0; k < 8; k++)
      {
         unsigned nib = (q[k >> 1] >> ((k & 1) << 2)) & 0x0f;
         RWAV_IMA_STEP(p, x, nib);
         o[k * stride] = (short)p;
      }
   else
      for (k = 0; k < 8; k++)
      {
         unsigned nib = (q[k >> 1] >> ((k & 1) << 2)) & 0x0f;
         RWAV_IMA_STEP(p, x, nib);
         if (k >= klo && k < khi)
            o[(k - klo) * stride] = (short)p;
      }
   *pred  = p;
   *index = x;
}

/* Stereo's two runs at once, whole runs only: two independent chains
 * the core can overlap.  Left at @q, right at @q + 4, stored to @o,
 * @o + 1 every two shorts. */
static INLINE void rwav_ima_run_pair(const unsigned char *q, int *pred,
      int *index, short *o)
{
   int      pl = pred[0],  pr = pred[1];
   int      xl = index[0], xr = index[1];
   unsigned k;
   for (k = 0; k < 8; k++)
   {
      unsigned sh = (k & 1) << 2;
      unsigned nl = (q[k >> 1]     >> sh) & 0x0f;
      unsigned nr = (q[4 + (k >> 1)] >> sh) & 0x0f;
      RWAV_IMA_STEP(pl, xl, nl);
      RWAV_IMA_STEP(pr, xr, nr);
      o[k * 2]     = (short)pl;
      o[k * 2 + 1] = (short)pr;
   }
   pred[0]  = pl; pred[1]  = pr;
   index[0] = xl; index[1] = xr;
}

#undef RWAV_IMA_STEP

/* IMA, any channel count: a preamble of four bytes a channel (the
 * first sample, the step index, a reserved byte), then the nibbles in
 * runs of four bytes of one channel, then four of the next. */
static unsigned rwav_ima_block(const rwav_t *wav, const unsigned char *b,
      size_t blen, unsigned first, unsigned end, short *out)
{
   int      pred[16], idx[16];
   unsigned ch   = wav->numchannels;
   size_t   step = 4 * (size_t)ch;
   size_t   off  = step;
   unsigned c, i;
   if (ch > 16 || blen < step)
      return 0;
   for (c = 0; c < ch; c++)
   {
      pred[c] = (short)rwav_u16(b + c * 4);
      idx[c]  = (b[c * 4 + 2] > 88) ? 88 : b[c * 4 + 2];
      if (!first)
         out[c] = (short)pred[c];
   }
   for (i = 1; i < end && off + step <= blen; i += 8, off += step)
   {
      /* the part of this run inside the window, as nibble indices */
      unsigned klo = (first > i) ? first - i : 0;
      unsigned khi = (end - i < 8) ? end - i : 8;
      short   *o   = (klo < khi) ? out + (size_t)(i + klo - first) * ch
                                 : out;
      if (ch == 2 && !klo && khi == 8)
         rwav_ima_run_pair(b + off, pred, idx, o);
      else
         for (c = 0; c < ch; c++)
            rwav_ima_run(b + off + c * 4, &pred[c], &idx[c], o + c, ch,
                  klo, khi);
   }
   return end - first;
}

/* One MS ADPCM sample from a nibble, on a channel's state in locals. */
#define RWAV_MS_STEP(p, nib, s1, s2, co1, co2, delta)                     \
   do {                                                                  \
      int rwav__n = (int)(nib);                                          \
      (p)     = rwav_clamp16(rwav_ms_predict((s1), (co1), (s2), (co2))    \
              + ((rwav__n ^ 8) - 8) * (delta));                          \
      (s2)    = (s1);                                                    \
      (s1)    = (p);                                                     \
      (delta) = rwav_ms_adapt_delta(rwav__n, (delta));                   \
   } while (0)

/* MS ADPCM preamble: per channel a coefficient index, then per channel
 * the step, the newer and the older priming sample, so each field is
 * a run of all channels.  Returns 0 when a coefficient index is out of
 * range. */
static int rwav_ms_preamble(const rwav_t *wav, const unsigned char *b,
      unsigned ch, unsigned c, int *co1, int *co2, int *delta,
      int *s1, int *s2)
{
   unsigned pi = b[c];
   if (pi >= wav->numcoef)
      return 0;
   *co1   = wav->coef[pi][0];
   *co2   = wav->coef[pi][1];
   *delta = (short)rwav_u16(b + ch + c * 2);
   *s1    = (short)rwav_u16(b + ch * 3 + c * 2);
   *s2    = (short)rwav_u16(b + ch * 5 + c * 2);
   return 1;
}

/* MS mono: two samples a byte, high nibble first. */
static unsigned rwav_ms_block_mono(const rwav_t *wav, const unsigned char *b,
      size_t blen, unsigned first, unsigned end, short *out)
{
   int      co1, co2, delta, s1, s2, p;
   size_t   off = 7;
   unsigned i;
   if (blen < 7 || !rwav_ms_preamble(wav, b, 1, 0,
            &co1, &co2, &delta, &s1, &s2))
      return 0;
   if (first == 0)
      out[0] = (short)s2;
   if (first <= 1 && end > 1)
      out[1 - first] = (short)s1;
   for (i = 2; i < end && off < blen; off++)
   {
      unsigned byte = b[off];
      RWAV_MS_STEP(p, byte >> 4, s1, s2, co1, co2, delta);
      if (i >= first)
         out[i - first] = (short)p;
      if (++i >= end)
         break;
      RWAV_MS_STEP(p, byte & 0x0f, s1, s2, co1, co2, delta);
      if (i >= first)
         out[i - first] = (short)p;
      i++;
   }
   return end - first;
}

/* MS stereo: a byte a frame, the left channel in the high nibble. */
static unsigned rwav_ms_block_stereo(const rwav_t *wav,
      const unsigned char *b, size_t blen, unsigned first, unsigned end,
      short *out)
{
   int      l1, l2, ld, lc1, lc2, r1, r2, rd, rc1, rc2, pl, pr;
   size_t   off = 14;
   unsigned i;
   if (     blen < 14
         || !rwav_ms_preamble(wav, b, 2, 0, &lc1, &lc2, &ld, &l1, &l2)
         || !rwav_ms_preamble(wav, b, 2, 1, &rc1, &rc2, &rd, &r1, &r2))
      return 0;
   if (first == 0)
   {
      out[0] = (short)l2;
      out[1] = (short)r2;
   }
   if (first <= 1 && end > 1)
   {
      out[(1 - first) * 2]     = (short)l1;
      out[(1 - first) * 2 + 1] = (short)r1;
   }
   for (i = 2; i < end && off < blen; i++, off++)
   {
      unsigned byte = b[off];
      RWAV_MS_STEP(pl, byte >> 4,   l1, l2, lc1, lc2, ld);
      RWAV_MS_STEP(pr, byte & 0x0f, r1, r2, rc1, rc2, rd);
      if (i >= first)
      {
         out[(i - first) * 2]     = (short)pl;
         out[(i - first) * 2 + 1] = (short)pr;
      }
   }
   return end - first;
}

/* MS, any other channel count: the nibbles are one stream, high nibble
 * first, a frame's channels in turn - so with an odd count a frame
 * starts mid-byte. */
static unsigned rwav_ms_block(const rwav_t *wav, const unsigned char *b,
      size_t blen, unsigned first, unsigned end, short *out)
{
   int      co1[16], co2[16], delta[16], s1[16], s2[16];
   unsigned ch   = wav->numchannels;
   size_t   need = 7 * (size_t)ch;
   size_t   n    = 0;   /* nibbles read since the preamble */
   unsigned c, i;
   if (ch > 16 || blen < need)
      return 0;
   for (c = 0; c < ch; c++)
   {
      if (!rwav_ms_preamble(wav, b, ch, c,
               &co1[c], &co2[c], &delta[c], &s1[c], &s2[c]))
         return 0;
      if (first == 0)
         out[c] = (short)s2[c];
      if (first <= 1 && end > 1)
         out[(1 - first) * ch + c] = (short)s1[c];
   }
   for (i = 2; i < end; i++)
   {
      for (c = 0; c < ch; c++, n++)
      {
         size_t o = need + (n >> 1);
         int    p;
         if (o >= blen)
            return end - first;
         RWAV_MS_STEP(p, (n & 1) ? (b[o] & 0x0f) : (b[o] >> 4),
               s1[c], s2[c], co1[c], co2[c], delta[c]);
         if (i >= first)
            out[(size_t)(i - first) * ch + c] = (short)p;
      }
   }
   return end - first;
}

#undef RWAV_MS_STEP

/* Decode frames [first, first + count) of one block into @out.
 * Returns the frames stored, 0 on a malformed block. */
static unsigned rwav_decode_block(const rwav_t *wav, const unsigned char *b,
      size_t blen, unsigned first, unsigned count, short *out)
{
   unsigned spb = wav->samplesperblock;
   unsigned end = (count < spb - first) ? first + count : spb;
   if (first >= spb)
      return 0;
   if (wav->format == RWAV_FORMAT_IMA_ADPCM)
      return rwav_ima_block(wav, b, blen, first, end, out);
   if (spb < 2)
      return 0;
   if (wav->numchannels == 1)
      return rwav_ms_block_mono(wav, b, blen, first, end, out);
   if (wav->numchannels == 2)
      return rwav_ms_block_stereo(wav, b, blen, first, end, out);
   return rwav_ms_block(wav, b, blen, first, end, out);
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

#ifndef MSB_FIRST
   /* On little-endian hosts interleaved PCM16 is already in output
    * order: the general loop below spends ~13 instructions a sample
    * reassembling each one from bytes, which is a memcpy spelled
    * slowly.  Only when the stride is the payload width, though -
    * the loop semantics for an unusual blockalign are "read ch*2
    * bytes at each blockalign step", which a flat copy would not
    * reproduce.  Big-endian keeps the loop, whose byte assembly is
    * the swab it needs anyway. */
   if (       wav->format == RWAV_FORMAT_PCM
       &&     wav->bitspersample == 16
       &&     wav->blockalign == 2u * ch)
   {
      memcpy(out, d + frame * (size_t)wav->blockalign,
            frames * (size_t)wav->blockalign);
      return frames;
   }
#endif

   if (   wav->format == RWAV_FORMAT_MS_ADPCM
       || wav->format == RWAV_FORMAT_IMA_ADPCM)
   {
      /* Start at the block holding 'frame' and step forward within
       * it: a block restates its own predictor, so nothing before it
       * has to be decoded. */
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
         take = (frames - done > (size_t)spb)
            ? spb : (unsigned)(frames - done);
         got  = rwav_decode_block(wav, d + boff, wav->blockalign,
               within, take, out + done * ch);
         if (!got)
            break;
         done += got;
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
                  /* 8-bit PCM is unsigned with a bias of 128, so the
                   * centred value is negative for the lower half of
                   * the range and shifting it left is undefined.
                   * Multiply instead; the result is identical on every
                   * representation and defined on all of them. */
                  out[done * ch + c] =
                        (short)(((int)p[c] - 128) * 256);
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

/* ---- windowed reading ------------------------------------------------
 *
 * rwav_decode_s16 addresses frames, but a caller streaming a file has
 * bytes: it must know which ones a frame range needs before it can
 * fetch them.  For the uncompressed and companded formats that is
 * blockalign arithmetic the caller could do itself; for the block-coded
 * ones it is not, because a frame lives inside a coded block that only
 * rwav can locate, and fetching the wrong span yields silence or noise
 * rather than an error.  So the mapping is stated here once.
 */

static int rwav_block_coded(const rwav_t *wav)
{
   return    wav->format == RWAV_FORMAT_MS_ADPCM
          || wav->format == RWAV_FORMAT_IMA_ADPCM;
}

int rwav_frame_extent(const rwav_t *wav, size_t frame, size_t frames,
      size_t *offset, size_t *length)
{
   size_t first, last, off, len;

   if (!wav || !wav->blockalign || frame >= wav->numsamples)
      return 0;
   if (frames > wav->numsamples - frame)
      frames = wav->numsamples - frame;
   if (!frames)
      return 0;

   if (rwav_block_coded(wav))
   {
      unsigned spb = wav->samplesperblock;
      if (!spb)
         return 0;
      first = frame / spb;
      last  = (frame + frames - 1) / spb;
   }
   else
   {
      first = frame;
      last  = frame + frames - 1;
   }

   off = (size_t)wav->blockalign * first;
   len = (size_t)wav->blockalign * (last - first + 1);
   /* A declared payload may overrun the file; subchunk2size is already
    * clamped to what is really there, so bound against it rather than
    * describing bytes the caller cannot read. */
   if (off >= wav->subchunk2size)
      return 0;
   if (len > wav->subchunk2size - off)
      len = wav->subchunk2size - off;

   if (offset)
      *offset = wav->dataoffset + off;
   if (length)
      *length = len;
   return 1;
}

size_t rwav_decode_s16_at(const rwav_t *wav, const void *chunk,
      size_t chunk_offset, size_t frame, size_t frames, short *out)
{
   rwav_t h;
   size_t want, rel;

   if (!wav || !chunk || !out || !wav->blockalign)
      return 0;
   if (frame >= wav->numsamples)
      return 0;
   if (frames > wav->numsamples - frame)
      frames = wav->numsamples - frame;
   if (!frames)
      return 0;

   /* Re-express the payload as though the chunk were the whole file:
    * the decode below is position-independent once told where the data
    * starts and how much of it there is. */
   h = *wav;
   h.dataoffset = 0;
   h.samples    = NULL;

   if (rwav_block_coded(wav))
   {
      unsigned spb   = wav->samplesperblock;
      size_t   first = frame / spb;
      if (!spb)
         return 0;
      /* The chunk must begin on the block the frame lives in - that is
       * what rwav_frame_extent hands back - because the block carries
       * the predictor state the frame continues from. */
      if (chunk_offset != wav->dataoffset + (size_t)wav->blockalign * first)
         return 0;
      rel  = frame - first * (size_t)spb;
      want = rel + frames;
   }
   else
   {
      if (chunk_offset != wav->dataoffset + (size_t)wav->blockalign * frame)
         return 0;
      rel  = 0;
      want = frames;
   }

   h.numsamples    = want;
   h.subchunk2size = (size_t)wav->blockalign
      * (rwav_block_coded(wav)
            ? (want + wav->samplesperblock - 1) / wav->samplesperblock
            : want);
   return rwav_decode_s16(&h, chunk, rel, frames, out);
}
