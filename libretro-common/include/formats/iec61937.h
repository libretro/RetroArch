/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (iec61937.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_IEC61937_H__
#define __LIBRETRO_SDK_FORMAT_IEC61937_H__

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* IEC 61937 data bursts: a compressed audio frame carried as if it
 * were 2-channel 16-bit PCM, which is how AC-3, E-AC-3 and DTS reach
 * a receiver or TV over S/PDIF and HDMI. A burst is four 16-bit
 * preamble words - Pa 0xF872, Pb 0x4E1F, Pc the data type, Pd the
 * length - then the frame's bytes in 16-bit words, most significant
 * byte first, then zeros to the burst's repetition period: the
 * number of PCM frames the compressed frame stands for, times four
 * bytes. Wrote as little-endian 16-bit samples, so the frame's
 * bytes come out swapped in pairs, which is what the standard's
 * receivers expect. */

/* Pc data types */
enum
{
   IEC61937_NULL       = 0,
   IEC61937_AC3        = 1,
   IEC61937_PAUSE      = 3,
   IEC61937_DTS_I      = 11,   /* 512 PCM frames per DTS frame */
   IEC61937_DTS_II     = 12,   /* 1024 */
   IEC61937_DTS_III    = 13,   /* 2048 */
   IEC61937_EAC3       = 21
};

/* The bytes one burst takes: the repetition period, as 2 channels of
 * 16 bits. AC-3: 1536 frames -> 6144 bytes. DTS: the frame's PCM
 * frames * 4. E-AC-3: 6144 * 4, at four times the audio rate. */
#define IEC61937_AC3_BURST_BYTES   (1536 * 4)
#define IEC61937_EAC3_BURST_BYTES  (1536 * 4 * 4)
#define IEC61937_PAUSE_BURST_BYTES 64

/* One AC-3 syncframe (frame, len bytes; bsmod its bit stream mode,
 * 0 for complete main) to a burst in out. Returns the bytes written,
 * IEC61937_AC3_BURST_BYTES, or 0 if cap is too small or the frame
 * does not fit a burst. */
size_t iec61937_wrap_ac3(const uint8_t *frame, size_t len, unsigned bsmod, uint8_t *out, size_t cap);

/* One E-AC-3 frame or the set of frames of one 1536-sample access
 * unit (independent stream and its dependents, or six 256-sample
 * frames) to a burst at four times the rate. */
size_t iec61937_wrap_eac3(const uint8_t *frames, size_t len, uint8_t *out, size_t cap);

/* One DTS core frame of pcm_frames (512, 1024 or 2048) samples. */
size_t iec61937_wrap_dts(const uint8_t *frame, size_t len, unsigned pcm_frames, uint8_t *out, size_t cap);

/* A pause burst: what to send instead of silence so the receiver
 * stays locked to the bitstream rather than dropping to PCM. gap is
 * the pause length in frames, as the burst carries it; 64 bytes. */
size_t iec61937_pause_burst(unsigned gap_frames, uint8_t *out, size_t cap);

/* The general form: a burst of data type type (with any of Pc's
 * upper bits, e.g. bsmod << 8) carrying payload, at a repetition
 * period of period_bytes. length_is_bits: AC-3 and DTS give Pd in
 * bits, E-AC-3 in bytes. */
size_t iec61937_burst(unsigned type, const uint8_t *payload, size_t len, bool length_is_bits, size_t period_bytes, uint8_t *out, size_t cap);

/* Whether buf begins with a burst preamble; type and payload length
 * (bytes) out if so. For tests and for a driver checking what it was
 * handed. */
bool iec61937_probe(const uint8_t *buf, size_t len, unsigned *type, size_t *payload_bytes);

RETRO_END_DECLS

#endif
