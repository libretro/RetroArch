/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ratrac9.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RATRAC9_H__
#define __LIBRETRO_SDK_FORMAT_RATRAC9_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

/* Decode errors (negative). */
#define RATRAC9_ERR_CONFIG            -1  /* bad configuration word */
#define RATRAC9_ERR_SUPERFRAME        -2  /* first-in-superframe block mid superframe */
#define RATRAC9_ERR_REUSE_PARAMS      -3  /* band parameter reuse in a superframe's first frame */
#define RATRAC9_ERR_BAND_PARAMS       -4  /* band parameters out of range */
#define RATRAC9_ERR_GRAD_BOUNDARY     -5  /* gradient boundary above the unit count */
#define RATRAC9_ERR_GRAD_START_UNIT   -6  /* gradient start unit out of range */
#define RATRAC9_ERR_GRAD_END_UNIT     -7  /* gradient end unit out of range */
#define RATRAC9_ERR_GRAD_START_VALUE  -8  /* gradient start value out of range */
#define RATRAC9_ERR_GRAD_END_VALUE    -9  /* gradient end value out of range */
#define RATRAC9_ERR_GRAD_ORDER        -10 /* gradient start unit after end unit */
#define RATRAC9_ERR_SF_MODE           -11 /* scale factor mode needs a previous frame */
#define RATRAC9_ERR_SF_RANGE          -12 /* scale factor out of range */
#define RATRAC9_ERR_EXT_LENGTH        -13 /* extension data longer than declared */

typedef struct ratrac9 ratrac9_t;

/* The 4 configuration bytes. NULL for a bad config or no memory. */
ratrac9_t *ratrac9_open(const uint8_t config[4]);
void       ratrac9_close(ratrac9_t *d);
/* Back to the state just after ratrac9_open, without allocating. */
void       ratrac9_reset(ratrac9_t *d);

unsigned ratrac9_channels(const ratrac9_t *d);
unsigned ratrac9_sample_rate(const ratrac9_t *d);
unsigned ratrac9_frame_samples(const ratrac9_t *d);      /* per channel */
unsigned ratrac9_frame_bytes(const ratrac9_t *d);
unsigned ratrac9_frames_per_superframe(const ratrac9_t *d);
unsigned ratrac9_superframe_bytes(const ratrac9_t *d);
unsigned ratrac9_channel_config(const ratrac9_t *d);     /* channel_config_index */

/* Decode one frame from in[0..len): frame_samples per channel into out, interleaved,
 * or planar when planar is true. *used gets the bytes the frame took (it can exceed
 * len for a frame cut short; the decoder never reads past len).
 * Returns 0, or one of the negative RATRAC9_ERR_* codes. */
int ratrac9_decode_s16(ratrac9_t *d, const uint8_t *in, size_t len,
      int16_t *out, bool planar, size_t *used);
int ratrac9_decode_s32(ratrac9_t *d, const uint8_t *in, size_t len,
      int32_t *out, bool planar, size_t *used);
int ratrac9_decode_f32(ratrac9_t *d, const uint8_t *in, size_t len,
      float *out, bool planar, size_t *used);

/* The decoder's history as bytes, for a savestate: ratrac9_state_size bytes
 * out, and back into a decoder opened with the same config. Load refuses any
 * other size, a state from another config, or out-of-range fields (and then
 * leaves the decoder unchanged). Fixed-width little-endian fields. */
size_t ratrac9_state_size(const ratrac9_t *d);
void   ratrac9_save_state(const ratrac9_t *d, void *data);
bool   ratrac9_load_state(ratrac9_t *d, const void *data, size_t size);

/* Reads a WAVE 'fmt ' chunk's body: format tag 0xFFFE, 52 bytes, the ATRAC9
 * sub-format GUID at bytes 24..39, the config bytes at 44..47. Copies the
 * config bytes out; false if the chunk is not that. */
bool ratrac9_parse_wave_fmt(const uint8_t *fmt, size_t len, uint8_t config[4]);

RETRO_END_DECLS

#endif
