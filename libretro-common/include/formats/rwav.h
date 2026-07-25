/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwav.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RWAV_H__
#define __LIBRETRO_SDK_FORMAT_RWAV_H__

#include <retro_common_api.h>
#include <retro_inline.h>
#include <stdint.h>

RETRO_BEGIN_DECLS

/* Samples as the file stores them: little-endian words, read a byte at
 * a time so that a reader working straight out of the file's bytes -
 * rather than out of rwav_load's native-order copy - stays correct on a
 * big-endian host and safe where an unaligned load would fault. A data
 * chunk is only guaranteed even-aligned, so a float sample in one is
 * not necessarily four-aligned. */
static INLINE int16_t rwav_s16(const uint8_t *p)
{
   unsigned v = (unsigned)p[0] | ((unsigned)p[1] << 8);
   return (int16_t)(v < 0x8000u ? (int)v : (int)v - 0x10000);
}

static INLINE float rwav_f32(const uint8_t *p)
{
   union { uint32_t u; float f; } bits;
   bits.u =  (uint32_t)p[0]        | ((uint32_t)p[1] << 8)
          | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
   return bits.f;
}

/* One 24-bit sample: three little-endian bytes, sign-extended. There is
 * no host type of that width, so rwav_load hands 24-bit data over
 * packed and it is read through these in either case. */
static INLINE int32_t rwav_s24(const uint8_t *p)
{
   uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8)
              | ((uint32_t)p[2] << 16);
   return (u & 0x800000u) ? (int32_t)u - 0x1000000 : (int32_t)u;
}

/* Rounded to 16 bits, saturating. Biased to unsigned first so that
 * neither the rounding nor the shift is applied to a negative value,
 * both being implementation-defined there. */
static INLINE int16_t rwav_s24_to_s16(const uint8_t *p)
{
   uint32_t u = ((uint32_t)p[0] | ((uint32_t)p[1] << 8)
              | ((uint32_t)p[2] << 16)) ^ 0x800000u;
   uint32_t r = (u + 128u) >> 8;
   if (r > 65535u)
      r = 65535u;
   return (int16_t)((int32_t)r - 32768);
}

/* At the same unit scale the 16-bit and float paths use. */
static INLINE float rwav_s24_to_float(const uint8_t *p)
{
   return (float)rwav_s24(p) * (1.0f / 8388608.0f);
}

/* What the samples are, which the width alone no longer says: a-law
 * and mu-law are eight bits like PCM, and ADPCM is four. */
enum rwav_format
{
   RWAV_FORMAT_PCM = 0,     /* 8, 16 or 24-bit integer                 */
   RWAV_FORMAT_FLOAT,       /* 32-bit IEEE                             */
   RWAV_FORMAT_ALAW,        /* G.711 A-law, eight bits in, 16 out      */
   RWAV_FORMAT_MULAW,       /* G.711 mu-law                            */
   RWAV_FORMAT_MS_ADPCM,    /* Microsoft ADPCM, four bits, block coded */
   RWAV_FORMAT_IMA_ADPCM    /* IMA/DVI ADPCM, four bits, block coded   */
};

typedef struct
{
   /* bits per sample: 8, 16 and 24 are integer PCM, 32 is IEEE float,
    * eight for the companded formats and four for ADPCM.  Read
    * 'format' to tell those apart. */
   unsigned int bitspersample;

   /* which of the above the payload holds */
   unsigned int format;

   /* bytes per block: a frame for the uncompressed and companded
    * formats, a coded block of samplesperblock frames for the ADPCM
    * ones, whose payload is not addressable frame by frame */
   unsigned int blockalign;

   /* frames a coded block decodes to; 1 for everything else */
   unsigned int samplesperblock;

   /* MS ADPCM predictor coefficient pairs, from the fmt chunk */
   unsigned int numcoef;
   short        coef[16][2];

   /* number of channels */
   unsigned int numchannels;

   /* sample rate */
   unsigned int samplerate;

   /* number of *samples* */
   size_t numsamples;

   /* number of *bytes* in the pointer below, i.e. numsamples * numchannels * bitspersample/8 */
   size_t subchunk2size;

   /* Sample data, owned by rwav and freed by rwav_free. 24-bit data is
    * handed over exactly as the file stores it - packed little-endian
    * three-byte samples, for rwav_s24 and friends above - there being
    * no host type to convert it to. NULL after
    * rwav_parse, which allocates nothing: read the samples out of your
    * own buffer at dataoffset instead. */
   const void* samples;

   /* byte offset of the sample data within the buffer it was parsed
    * from. Set by both entry points; only useful with rwav_parse. */
   size_t dataoffset;
} rwav_t;

enum rwav_state
{
   RWAV_ITERATE_ERROR    = -1,
   RWAV_ITERATE_MORE     = 0,
   RWAV_ITERATE_DONE     = 1,
   RWAV_ITERATE_BUF_SIZE = 4096
};

typedef struct rwav_iterator rwav_iterator_t;

/**
 * Initializes the iterator to fill the out structure with data parsed from buf.
 */
void rwav_init(rwav_iterator_t *iter, rwav_t *out, const void* buf, size_t len);

/**
 * Parses a piece of the data. Continue calling as long as it returns RWAV_ITERATE_MORE.
 * Stop calling otherwise, and check for errors. If RWAV_ITERATE_DONE is returned,
 * the rwav_t structure passed to rwav_init is ready to be used. The iterator does not
 * have to be freed.
 */
enum rwav_state rwav_iterate(rwav_iterator_t *iter);

/**
 * Loads the entire payload in one go, into a buffer rwav owns and
 * rwav_free releases.  Words are put in host order where the format
 * has any - 16-bit PCM and 32-bit float; 24-bit stays packed as the
 * file stores it, and the companded and block-coded payloads stay
 * coded, there being no host order for a curve or a nibble.  Read
 * those back with rwav_decode_s16, which takes the loaded buffer as
 * readily as the file it came from.
 */
enum rwav_state rwav_load(rwav_t *out, const void *buf, size_t len);

/* Decode frames to interleaved s16, whatever the payload holds.
 *
 * 'base' is the buffer the header was parsed from, so dataoffset
 * applies; 'frame' is the first frame wanted and 'frames' how many.
 * Returns how many were produced, short only at the end.
 *
 * This is the only way to read the block-coded formats, whose payload
 * is not addressable a frame at a time - a block carries the
 * predictor state its samples continue from, so the decode starts at
 * the block containing 'frame' and steps forward inside it.  Blocks
 * being self-contained, that costs at most one block however far in
 * the caller asks, and no state is carried between calls.
 *
 * The uncompressed formats go through it too, so a caller need not
 * branch on the format at all.
 */
size_t rwav_decode_s16(const rwav_t *wav, const void *base, size_t frame,
      size_t frames, short *out);

/**
 * Parses the header alone: walks the RIFF chunk list, fills the format
 * fields, and reports the sample data's size and its byte offset within
 * buf. Allocates nothing and copies nothing - samples is left NULL and
 * there is nothing to free - so the caller converts frames out of its
 * own buffer as it wants them, in whatever output format it wants,
 * rather than taking rwav's native-order copy of the lot.
 *
 * len describes the whole span buf addresses, and bounds the payload:
 * subchunk2size comes back as the declared data size clamped to it and
 * rounded down to whole units - frames, or blocks where a block is the
 * unit. Reads, though, stop at the end of the
 * 'data' chunk header - the payload itself is never touched - so a
 * caller streaming a file may pass its full length while keeping only
 * the head resident, and page the rest in behind the read cursor.
 *
 * Returns RWAV_ITERATE_DONE on success.
 */
enum rwav_state rwav_parse(rwav_t *out, const void *buf, size_t len);

/**
 * Frees parsed wave data. Safe, and unnecessary, after rwav_parse.
 */
void rwav_free(rwav_t *rwav);

RETRO_END_DECLS

#endif
