/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwebp.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RWEBP_H__
#define __LIBRETRO_SDK_FORMAT_RWEBP_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct rwebp rwebp_t;

int rwebp_process_image(rwebp_t *rwebp, void **buf,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba);

bool rwebp_set_buf_ptr(rwebp_t *rwebp, void *data, size_t len);

void rwebp_free(rwebp_t *rwebp);

rwebp_t *rwebp_alloc(void);

/* ===== Animation (animated WebP / ANMF) =====
 * Opaque handle to a fully decoded animation. rwebp_anim_decode returns
 * NULL for non-animated or malformed input, so callers can attempt it
 * unconditionally and fall back to the still-image path. Frames are
 * complete, composited RGBA canvases (memory order R,G,B,A). */

typedef struct rwebp_anim rwebp_anim_t;

rwebp_anim_t *rwebp_anim_decode(const uint8_t *buf, size_t len);

void rwebp_anim_free(rwebp_anim_t *anim);

int rwebp_anim_num_frames(const rwebp_anim_t *anim);

void rwebp_anim_get_info(const rwebp_anim_t *anim,
      unsigned *width, unsigned *height, int *loop_count);

/* Returns the RGBA pixels of frame 'index' (0-based) and, if non-NULL,
 * writes its display duration in milliseconds. Returns NULL out of range.
 * The returned pointer is owned by the animation and valid until freed. */
const uint32_t *rwebp_anim_get_frame(const rwebp_anim_t *anim, int index,
      int *duration_ms);

/* Streaming animation iterator. Unlike rwebp_anim_decode, which holds
 * every composited canvas in memory at once, the stream keeps only two
 * canvases plus a BORROWED reference to the caller's file buffer, so
 * memory use is independent of frame count. The buffer passed to
 * rwebp_anim_stream_open must remain valid and unmodified until
 * rwebp_anim_stream_close. */

typedef struct rwebp_anim_stream rwebp_anim_stream_t;

rwebp_anim_stream_t *rwebp_anim_stream_open(const uint8_t *buf, size_t len);

void rwebp_anim_stream_close(rwebp_anim_stream_t *stream);

void rwebp_anim_stream_get_info(const rwebp_anim_stream_t *stream,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count);

/* Composites and returns the next frame as a full RGBA canvas (memory
 * order R,G,B,A), writing its display duration in milliseconds. The
 * returned pointer refers to the stream's internal canvas: it is valid
 * until the next call into the stream and must not be freed. Returns
 * NULL at the end of one pass; call rwebp_anim_stream_rewind to loop. */
const uint32_t *rwebp_anim_stream_next(rwebp_anim_stream_t *stream,
      int *duration_ms);

void rwebp_anim_stream_rewind(rwebp_anim_stream_t *stream);

RETRO_END_DECLS

#endif
