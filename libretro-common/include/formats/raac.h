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
 * Scope: AAC-LC (audioObjectType 2), the 1024- and 960-sample frame
 * lengths,
 * every standard sampling rate as well as explicitly coded ones
 * (samplingFrequencyIndex 15), channel configurations 1 through 7 and
 * configuration 0 with an embedded PCE, up to eight output channels,
 * M/S and intensity stereo, PNS, pulse data and TNS.  In-stream PCE
 * and DSE elements are parsed and skipped.  Coupling channel
 * elements are decoded and mixed into their targets, dependently or
 * independently switched, with up to four concurrent coupling tags.
 * HE-AAC's SBR and HE-AAC v2's parametric stereo are decoded, signalled
 * explicitly or implicitly (see raac_frame_len).  Channels are emitted in element transmission
 * order, which for the standard configurations is the ISO order (e.g.
 * 5.1: C, L, R, Ls, Rs, LFE), or in WAVE order on request
 * (raac_set_channel_order); no downmix is applied.  ADTS framing is
 * the caller's, with raac_adts_parse reading its headers.  Not decoded
 * (open or decode fails cleanly): other object types (Main, SSR, LTP)
 * and the 960-sample frame length with SBR.
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

/* Samples per channel each access unit decodes to, and the return
 * value of the decode calls: 1024, or 960 when the stream signals the
 * short frame length, or 2048 when the doubled output rate is active.
 *
 * The doubled rate turns on at open and never changes mid-stream:
 * either the configuration signals SBR explicitly (hierarchical or
 * syncExtension form) at twice the core rate, or an unsignaled
 * 1024-frame stream has a core rate of 16-24 kHz, the range where
 * HE-AAC streams hide behind plain LC configurations. In that window
 * genuinely plain LC decodes upsampled and reports the doubled rate;
 * raac_sample_rate() always reports the rate the output is actually
 * at. SBR data, when the stream carries it, rebuilds the high band.
 * Mono SBR decodes to two channels (raac_channels reports 2): HE-AAC
 * v2's parametric stereo when the stream carries it, else the mono
 * signal in both. Explicitly signaled
 * downsampled SBR (extension rate equal to the core rate) decodes as
 * plain LC at the core rate. Size output buffers from
 * raac_frame_len() and raac_channels(), never a constant. */
#define RAAC_MAX_FRAME 2048   /* upper bound of raac_frame_len() */
unsigned raac_frame_len(const raac_t *a);
unsigned raac_sample_rate(const raac_t *a);

/* Decode one access unit (one MP4 sample). out must hold at least
 * raac_frame_len(a) * raac_channels(a) samples - the core frame
 * length (1024, or 960) doubles when the SBR path is active, so a
 * fixed buffer must be sized for RAAC_MAX_FRAME per channel.  Returns
 * the number of samples produced per channel (raac_frame_len(a)), or
 * -1 on a bitstream or scope error - the decoder stays usable for
 * following packets.
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

/* The same decode in two halves, for a caller that hands the output
 * on in slices or straight into a buffer of its own: raac_decode
 * returns what the two entry points above return and keeps the frame
 * inside the decoder, and the copies interleave frames [first,
 * first + count) of it into out, converted exactly as raac_decode_s16
 * and raac_decode_f32 would have.  The frame stays valid until the
 * next raac_decode, raac_decode_s16, raac_decode_f32, raac_reset or
 * raac_load_state; first + count must not pass what raac_decode
 * returned, and nothing is held after a failed decode. */
int raac_decode(raac_t *a, const uint8_t *pkt, size_t size);
void raac_copy_s16(const raac_t *a, size_t first, size_t count,
      int16_t *out);
void raac_copy_f32(const raac_t *a, size_t first, size_t count,
      float *out);

/* The order channels are interleaved in by the copies above.
 * RAAC_ORDER_STREAM, the default, is element transmission order (5.1:
 * C, L, R, Ls, Rs, LFE). RAAC_ORDER_WAVE is the order of the
 * WAVEFORMATEXTENSIBLE speaker bits, as WAV files, most audio APIs and
 * ffmpeg use (5.1: L, R, C, LFE, Ls, Rs; channelConfiguration 7 reads
 * as L, R, C, LFE, back L, back R, side L, side R). It is known for
 * channelConfiguration 1-7 and for a PCE whose elements each map to a
 * distinct speaker; for other layouts the call returns -1 and the order
 * stays as it was. Returns 0 otherwise. */
enum raac_channel_order
{
   RAAC_ORDER_STREAM = 0,
   RAAC_ORDER_WAVE
};
int raac_set_channel_order(raac_t *a, enum raac_channel_order order);

/* An ADTS frame header (ISO/IEC 13818-7 / 14496-3 1.A.3.2), for
 * streams that carry raw_data_blocks in ADTS framing. asc is the
 * AudioSpecificConfig to raac_open with (a channel configuration of 0,
 * whose PCE travels in the first raw_data_block, makes raac_open
 * refuse it). The access unit is the frame_size - header_size bytes
 * after the header; a frame of more than one raw_data_block carries
 * them back to back, raac_last_size telling where each ends, with
 * CRC words between them when header_size is 9. */
typedef struct raac_adts
{
   unsigned frame_size;   /* whole frame, header included   */
   unsigned header_size;  /* 7, or 9 with the CRC word      */
   unsigned raw_blocks;   /* raw_data_blocks in the frame   */
   uint8_t  asc[2];
} raac_adts_t;

/* Parses the header at buf (7 bytes are enough). Returns 0, or -1
 * when buf holds no valid ADTS header. */
int raac_adts_parse(const uint8_t *buf, size_t size, raac_adts_t *h);

/* Return the decoder to its just-opened state (clears the overlap-add
 * history and reseeds the noise generator) so a stream can be decoded
 * again from its first packet, e.g. for looping. */
void raac_reset(raac_t *a);

/* What the frames decoded so far leave the next ones, as bytes, for
 * a savestate: raac_state_size bytes out, and back into a decoder
 * opened with the same configuration, which then decodes on exactly
 * as the saved one. Load returns -1, changing nothing, for any other
 * size or configuration. */
size_t raac_state_size(const raac_t *a);
void raac_save_state(const raac_t *a, void *data);
int raac_load_state(raac_t *a, const void *data, size_t size);

/* Bytes the last decoded access unit took, through its END element:
 * less than the size given when more follows it. 0 after a failure. */
size_t raac_last_size(const raac_t *a);

void raac_close(raac_t *a);

RETRO_END_DECLS

#endif
