/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (raac.h).
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

/* raac -- MPEG-4 AAC Low Complexity decoder (ISO/IEC 14496-3).
 *
 * Decodes raw AAC-LC access units as MP4 files carry them ('mp4a'
 * sample entries with object type 0x40, one raw_data_block per
 * sample; no ADTS framing).  The decoder is created from the
 * AudioSpecificConfig found in the esds DecoderSpecificInfo and
 * outputs interleaved signed 16-bit PCM, 1024 samples per channel
 * per access unit.
 *
 * Scope: AAC-LC (audioObjectType 2), mono and stereo, the 1024-sample
 * frame length, every standard sampling rate, M/S and intensity
 * stereo, PNS, pulse data and TNS.  Not decoded (open or decode
 * fails cleanly): other object types (Main, SSR, LTP, HE-AAC's
 * explicit SBR signalling), the 960-sample frame length, coupling
 * channel elements and multichannel programs.  Implicitly signalled
 * SBR in fill elements is skipped, decoding the LC core band, as
 * plain LC decoders do.
 */
#ifndef __LIBRETRO_SDK_FORMAT_RAAC_H__
#define __LIBRETRO_SDK_FORMAT_RAAC_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct raac raac_t;

/* Create a decoder from an AudioSpecificConfig (the esds
 * DecoderSpecificInfo payload). Returns NULL when the configuration
 * is malformed or outside the supported scope. */
raac_t *raac_open(const uint8_t *asc, size_t asc_size);

unsigned raac_channels(const raac_t *a);
unsigned raac_sample_rate(const raac_t *a);

/* Decode one access unit (one MP4 sample). out must hold at least
 * 1024 * channels samples. Returns the number of samples produced
 * per channel (1024), or -1 on a bitstream or scope error - the
 * decoder stays usable for following packets.
 *
 * The synthesis pipeline is float throughout; the two entry points
 * differ only in the output conversion.  s16 rounds and saturates
 * once at the edge.  f32 emits the float samples unquantised at unit
 * scale (full scale = +-1.0, ffmpeg/libopus float convention) and is
 * not clamped, so float consumers avoid the 16-bit round trip.  The
 * overlap state is shared, so the entry points may be mixed freely
 * on one instance. */
int raac_decode_s16(raac_t *a, const uint8_t *pkt, size_t size,
      int16_t *out);
int raac_decode_f32(raac_t *a, const uint8_t *pkt, size_t size,
      float *out);

void raac_close(raac_t *a);

RETRO_END_DECLS

#endif
