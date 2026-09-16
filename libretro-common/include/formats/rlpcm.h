/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rlpcm.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RLPCM_H
#define __LIBRETRO_SDK_FORMAT_RLPCM_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Linear PCM: samples in the order they were taken, no model and
 * nothing to undo. What there is to get right is the shape - which
 * way round the bytes go, how many bits a sample has and how they sit
 * in the bytes it takes, which speaker each one belongs to - and the
 * two disc formats say all of that in a header of their own.
 *
 * DVD-Video carries LPCM in a private stream, each PES packet opening
 * with five bytes (ISO/IEC 13818-1's private stream 1, the LPCM
 * variant): a frame number, the first access unit's offset, and two
 * bytes saying the sample size, rate and channel count. Blu-ray's
 * HDMV LPCM opens each access unit with four: the payload's length,
 * then a channel assignment, a sampling frequency and a sample size.
 * Both are big-endian, which is the opposite of WAV and of every
 * device the frontend writes to.
 *
 * Raw LPCM has no header at all, so a caller that has one (a .pcm
 * file, a track ripped from a disc) says what it is. */

enum rlpcm_kind
{
   RLPCM_KIND_RAW = 0,   /* no header; the caller supplies the shape */
   RLPCM_KIND_DVD,       /* DVD-Video private stream 1, five-byte header */
   RLPCM_KIND_BLURAY     /* HDMV LPCM, four-byte header */
};

enum rlpcm_status
{
   RLPCM_OK        =  0,
   RLPCM_NEED_MORE =  1,   /* len does not reach the end of the header */
   RLPCM_NO_SYNC   = -1,   /* the bytes are not a header of that kind */
   RLPCM_BAD       = -2    /* a header field the format does not allow */
};

typedef struct rlpcm_format
{
   enum rlpcm_kind kind;
   unsigned sample_rate;    /* 48000, 96000, 192000; DVD also 44100 and 88200 */
   unsigned bits;           /* 16, 20 or 24 */
   unsigned channels;       /* 1 to 8 */
   /* The speaker mask (audio_upmix.h's bits: FL 1, FR 2, FC 4, LFE 8,
    * BL 0x10, BR 0x20, BC 0x100, SL 0x200, SR 0x400), in the order
    * the samples arrive - which for both disc formats is the mask's
    * own ascending-bit order. */
   uint32_t layout;
   bool     big_endian;     /* the disc formats are; a raw caller says */
   /* Bytes of header before the samples, and bytes of payload after
    * it where the header says (Blu-ray does; DVD's packet length is
    * the container's business, so 0 means "to the end of the buffer"). */
   unsigned header_bytes;
   size_t   payload_bytes;
} rlpcm_format_t;

/* Bytes one frame of every channel takes on the wire. 20-bit samples
 * are packed two to five bytes, so a frame of an odd channel count at
 * 20 bits is not a whole number of bytes and the pair is the unit;
 * rlpcm_frame_bits() is exact where that matters. */
unsigned rlpcm_frame_bits(const rlpcm_format_t *fmt);
size_t   rlpcm_frame_bytes(const rlpcm_format_t *fmt);

/* Reads the header at src. RLPCM_NEED_MORE when len is shorter than
 * the header, RLPCM_NO_SYNC when the bytes cannot be one of that
 * kind, RLPCM_BAD when a field is reserved. The samples are not read.
 *
 * For RLPCM_KIND_RAW the caller has already filled fmt with the shape
 * and this only checks and completes it. */
enum rlpcm_status rlpcm_parse_format(enum rlpcm_kind kind,
      const uint8_t *src, size_t len, rlpcm_format_t *fmt);

/* Samples to float in [-1, 1], interleaved in the layout's order, one
 * frame of every channel at a time. src points at the samples, past
 * any header. Returns the frames written, which is the lesser of the
 * frames asked for and the frames the bytes hold. */
size_t rlpcm_decode_f32(const rlpcm_format_t *fmt,
      const uint8_t *src, size_t src_len, float *out, size_t frames);

/* The same to int16, rounded and saturated. A 20- or 24-bit source
 * loses its low bits, which is what an int16 pipeline asks for. */
size_t rlpcm_decode_s16(const rlpcm_format_t *fmt,
      const uint8_t *src, size_t src_len, int16_t *out, size_t frames);

/* Float in [-1, 1] to samples of the format, for a caller writing
 * LPCM out (a disc image, or the IEC 61937 pass-through, where LPCM
 * is carried as it is rather than in bursts). Returns the frames
 * written. */
size_t rlpcm_encode_f32(const rlpcm_format_t *fmt,
      const float *src, size_t frames, uint8_t *out, size_t out_len);

RETRO_END_DECLS

#endif
