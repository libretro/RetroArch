/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ropus.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_ROPUS_H__
#define __LIBRETRO_SDK_FORMAT_ROPUS_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Standalone Opus decoder (RFC 6716): SILK, CELT and hybrid modes,
 * mono/stereo, all frame sizes, output at 48 kHz.  The s16 pipeline is
 * bit-exact with libopus 1.4 FIXED_POINT opus_decode(); the f32
 * pipeline is bit-exact with the libopus 1.4 float build's
 * opus_decode_float().  No PLC/CNG: loss-free playback only. */

typedef struct ropus ropus_t;

/* Parse an OpusHead (RFC 7845 5.1, e.g. a container's CodecPrivate) and
 * create a decoder.  Only channel mapping family 0 (mono/stereo) is
 * supported.  Returns NULL on malformed or unsupported input. */
ropus_t *ropus_open(const void *opus_head, size_t head_size);

void ropus_close(ropus_t *o);

/* Channel count from the OpusHead (1 or 2); output rate is always
 * 48000 Hz. */
unsigned ropus_channels(const ropus_t *o);

/* Pre-skip in 48 kHz frames: discard this many frames from the start
 * of the decoded stream (RFC 7845 4.2). */
unsigned ropus_preskip(const ropus_t *o);

/* Drop all decoder state (equivalent to reopening); use before seeking
 * back to the start of a stream. */
void ropus_reset(ropus_t *o);

/* Decode one Opus packet into interleaved 48 kHz PCM.  out must have
 * room for 5760 * channels samples (120 ms).  Returns the number of
 * frames produced, or < 0 on malformed input.  The OpusHead output
 * gain is applied.  The first call selects the decoder's output
 * pipeline; s16 and f32 must not be mixed on one instance (the pre-skip
 * given by the OpusHead is the caller's responsibility). */
int ropus_decode_s16(ropus_t *o, const void *pkt, size_t len,
      int16_t *out);
int ropus_decode_f32(ropus_t *o, const void *pkt, size_t len,
      float *out);

RETRO_END_DECLS

#endif
