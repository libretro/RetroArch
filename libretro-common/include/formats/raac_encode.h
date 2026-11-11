/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (raac_encode.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RAAC_ENCODE_H
#define __LIBRETRO_SDK_FORMAT_RAAC_ENCODE_H

/* AAC-LC encoder (ISO/IEC 14496-3): what a recording's audio is when
 * AAC is asked for.
 *
 * Mono or stereo, any of AAC's sampling rates, long windows (sine),
 * a single channel element or a channel pair sharing its window. Each
 * band's quantiser follows the band's level, all of them scaled frame
 * by frame to the bit rate, up to a bandwidth the rate allows;
 * codebooks picked band by band for the fewest bits. The bit rate is a
 * ceiling: what needs fewer bits - tones, silence - takes fewer.
 *
 * A frame is 1024 samples a channel in, one raw_data_block out (an MP4
 * sample), or that block behind an ADTS header. The output lags by one
 * frame - the transform's overlap: the first frame decodes to stand for
 * the 1024 samples of silence before the input, and the last needs one more
 * call, without samples, to come out. Nothing is allocated after
 * raac_encoder_new(); the output goes into the caller's buffer,
 * RAAC_ENCODE_MAX_FRAME bytes being always enough. */

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

/* 6144 bits a channel, AAC's limit, for two channels, and an ADTS header */
#define RAAC_ENCODE_MAX_FRAME (2 * 768 + 7)
#define RAAC_ENCODE_FRAME     1024

typedef struct
{
   unsigned sample_rate;   /* one of AAC's: 8000 to 96000 */
   unsigned channels;      /* 1 or 2 */
   uint32_t bitrate;       /* bits a second, all channels */
} raac_encoder_config_t;

typedef struct raac_encoder raac_encoder_t;

raac_encoder_t *raac_encoder_new(const raac_encoder_config_t *config);
void            raac_encoder_free(raac_encoder_t *e);

/* The AudioSpecificConfig (an MP4 esds' decoder configuration): 2 bytes */
size_t raac_encoder_asc(const raac_encoder_t *e, uint8_t *dst, size_t cap);

/**
 * raac_encoder_encode_s16:
 * @e    : the encoder
 * @pcm  : 1024 frames, interleaved; NULL for the call after the last
 * @dst  : the output, @cap bytes
 * @adts : behind an ADTS header
 *
 * Returns: the bytes written, 0 if @cap was too small.
 */
size_t raac_encoder_encode_s16(raac_encoder_t *e, const int16_t *pcm,
      uint8_t *dst, size_t cap, bool adts);

/* The same, from floats in -1..1 */
size_t raac_encoder_encode_f32(raac_encoder_t *e, const float *pcm,
      uint8_t *dst, size_t cap, bool adts);

RETRO_END_DECLS

#endif
