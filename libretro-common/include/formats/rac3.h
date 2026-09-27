/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rac3.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RAC3_H__
#define __LIBRETRO_SDK_FORMAT_RAC3_H__

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* AC-3 and E-AC-3, written from the ATSC A/52 standard and nothing
 * else. This header is the frame layer: sync, the bit stream
 * information, the frame's size and what it carries. The decoder and
 * the encoder build on it in their own translation units.
 *
 * Terms are the standard's: bsid tells AC-3 (0..8, with 6 taking the
 * Annex D extras) from E-AC-3 (11..16); acmod is the channel
 * configuration; the frame is 1536 samples per channel of AC-3, and
 * one to six 256-sample blocks of E-AC-3 as numblkscod says. */

/* Where the frame came from, by bsid. */
enum rac3_kind
{
   RAC3_KIND_AC3  = 0,
   RAC3_KIND_EAC3 = 1
};

/* E-AC-3 stream types (strmtyp). */
enum rac3_stream_type
{
   RAC3_STREAM_INDEPENDENT = 0,
   RAC3_STREAM_DEPENDENT   = 1,
   RAC3_STREAM_AC3_CONVERT = 2,
   RAC3_STREAM_RESERVED    = 3
};

typedef struct rac3_frame_info
{
   enum rac3_kind        kind;
   unsigned              bsid;
   unsigned              frame_bytes;     /* the whole frame, sync word included */
   unsigned              sample_rate;     /* 48000, 44100, 32000; E-AC-3 also the halves */
   unsigned              samples;         /* per channel in this frame: 1536, or 256 * blocks */
   unsigned              blocks;          /* audio blocks: 6, or E-AC-3's 1, 2, 3 or 6 */
   unsigned              bitrate;         /* nominal, bits per second; 0 for E-AC-3 */
   unsigned              bsmod;           /* bit stream mode: 0 complete main; IEC 61937 carries it */
   unsigned              acmod;           /* 0 = 1+1 (two mono), 1 = 1/0, 2 = 2/0, 3 = 3/0,
                                             4 = 2/1, 5 = 3/1, 6 = 2/2, 7 = 3/2 */
   bool                  lfe;
   unsigned              channels;        /* full-bandwidth channels plus the LFE */
   /* The channels as the frontend's speaker mask (audio_upmix.h's
    * bits: FL 1, FR 2, FC 4, LFE 8, BL 0x10, BR 0x20, BC 0x100, SL
    * 0x200, SR 0x400), for what leaves the decoder. A/52's surround
    * pair Ls/Rs sits at 110 degrees, which the mask calls the side
    * pair; the single surround channel of 2/1 and 3/1 is the back
    * centre. The order in the bit stream is A/52's own - L, C, R, Ls,
    * Rs, with the LFE last - and the decoder writes the mask's
    * ascending-bit order. */
   uint32_t              layout;
   /* E-AC-3 only. */
   enum rac3_stream_type stream_type;
   unsigned              substream_id;
   /* Dialogue normalisation of the first (or only) channel group, in
    * dB below full scale: 1..31. */
   unsigned              dialnorm;
} rac3_frame_info_t;

/* Parses the frame that starts at src (its sync word must be there).
 * Returns RAC3_OK with info filled, RAC3_NEED_MORE when len is too
 * short to reach the fields the size depends on (fewer than 6 bytes
 * for AC-3 or 6 for E-AC-3 as it happens), RAC3_NO_SYNC when there
 * is no sync word at src, or RAC3_BAD when the header is not one the
 * standard allows. The frame's bytes past the header are not needed
 * and not read. */
enum rac3_status
{
   RAC3_OK        =  0,
   RAC3_NEED_MORE =  1,
   RAC3_NO_SYNC   = -1,
   RAC3_BAD       = -2
};

enum rac3_status rac3_parse_frame_info(const uint8_t *src, size_t len, rac3_frame_info_t *info);

/* Finds the next byte offset at or after 'from' where a frame header
 * parses; returns len when none does. Two consecutive headers are
 * required to agree - the first's size must land on the second - so
 * a chance sync word in the middle of a frame is not taken. */
size_t rac3_find_sync(const uint8_t *src, size_t len, size_t from);

/* The 16-bit CRC the frames carry: CRC-16 with the polynomial
 * x^16 + x^15 + x^2 + 1, as the standard's crc1 and crc2 use. */
uint16_t rac3_crc16(const uint8_t *src, size_t len);

/* Whether the frame's checksums hold: AC-3's crc1 over the first
 * five-eighths and crc2 over the whole; E-AC-3's single crc over the
 * whole. Needs the entire frame. */
bool rac3_frame_crc_ok(const uint8_t *src, size_t frame_bytes);

/* ---- the decoder -------------------------------------------------- */

/* A decoder for AC-3 (bsid <= 8). Frames go in one at a time and
 * come out as interleaved float, one value per channel per sample in
 * the frame's layout, the mask's ascending-bit order - so a 3/2
 * stream comes out FL FR FC [LFE] SL SR. Output is nominal full
 * scale at +/-1.0 before dialogue normalisation; dynamic range
 * control (dynrng) is applied as the standard directs unless turned
 * off; dialnorm is reported, not applied, so the caller may. */
typedef struct rac3_decoder rac3_decoder_t;

rac3_decoder_t *rac3_decoder_new(void);
void            rac3_decoder_free(rac3_decoder_t *d);

/* Apply the stream's dynamic range control words (default on). */
void rac3_decoder_set_drc(rac3_decoder_t *d, bool on);

/* Decodes one frame. src must hold the whole frame. out must have
 * room for info->samples * info->channels floats. Returns the frames
 * (samples per channel) written, 0 on a frame the decoder refuses
 * (bad header, a CRC that does not hold, an E-AC-3 frame, a syntax
 * element the standard reserves), in which case info is still filled
 * where the header parsed. */
size_t rac3_decode_frame(rac3_decoder_t *d, const uint8_t *src, size_t len,
      float *out, rac3_frame_info_t *info);

/* ---- the encoder -------------------------------------------------- */

/* A basic AC-3 encoder in the sense of the standard's section 8: the
 * long transform only, no coupling, no rematrixing, new D15
 * exponents in block 0 reused for the frame, the core bit allocation
 * with the section's nominal parameters and the SNR offset searched
 * to fill the frame. Legal, and decodable by any decoder; not the
 * most efficient use of a bit rate, which is what coupling and the
 * finer strategies buy. Input is interleaved float in the frontend's
 * speaker order for the layout, 1536 frames per call. */
typedef struct rac3_encoder rac3_encoder_t;

/* The largest syncframe: 640 kbit/s at 32 kHz, 1920 words. */
#define RAC3_MAX_FRAME_BYTES 3840

/* The acmod a layout mask encodes as, -1 if A/52 has no
 * configuration for it (the encoder's layouts, listed below). */
int rac3_layout_acmod(uint32_t layout);

/* rate: 48000, 44100 or 32000. layout: the frontend's mask, one of
 * the configurations A/52 has (mono FC; stereo; 3.0; 2.1 as L R BC;
 * quad with the pair at the sides; 5.0 and 5.1 with the pair at the
 * sides; each with or without the LFE). kbps: one of the standard's
 * nineteen rates, 32 to 640. NULL if any is not. */
rac3_encoder_t *rac3_encoder_new(unsigned rate, uint32_t layout, unsigned kbps);
void            rac3_encoder_free(rac3_encoder_t *e);

/* The bytes one frame of this encoder takes (at 44.1 kHz the two
 * sizes alternate, so this is the larger). */
size_t rac3_encoder_frame_bytes(const rac3_encoder_t *e);

/* 1536 frames of in, interleaved at the layout's channel count, to
 * one AC-3 frame in out (cap bytes). Returns the bytes written, 0 if
 * cap is too small. */
size_t rac3_encode_frame(rac3_encoder_t *e, const float *in, uint8_t *out, size_t cap);

RETRO_END_DECLS

#endif
