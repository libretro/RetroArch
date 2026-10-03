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

/* Prefix probe for partial reads: true once the bytes [0, avail)
 * contain the complete still-image chunk (the first VP8/VP8L,
 * standalone or inside the first ANMF frame), i.e. a decode whose
 * buffer length is bounded to avail will succeed without touching a
 * byte beyond it. */
bool rwebp_still_ready(const void *buf, size_t avail);

/* Is this an animated WebP?  Decided from the file's first chunk: an
 * extended file (VP8X first) with the animation flag.  Needs the first
 * 30 bytes; 'need_more' (may be NULL) is set when fewer are given and
 * the answer is not yet known, so a caller probing a short header
 * prefix can tell that apart from a conclusive "still". */
bool rwebp_is_animated_ex(const uint8_t *buf, size_t len, int *need_more);

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

/* num_frames is the number of ANMF frames indexed so far: the whole
 * count for a stream opened over a complete buffer, a lower bound for a
 * progressive stream (rwebp_anim_stream_open_avail) until its scan has
 * reached the end of the file. */
void rwebp_anim_stream_get_info(const rwebp_anim_stream_t *stream,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count);

/* Composites and returns the next frame as a full RGBA canvas (memory
 * order R,G,B,A), writing its display duration in milliseconds. The
 * returned pointer refers to the stream's internal canvas: it is valid
 * until the next call into the stream and must not be freed. Returns
 * NULL at the end of one pass; call rwebp_anim_stream_rewind to loop.
 * On a progressive stream NULL is also returned, with nothing consumed,
 * when the next frame's bytes lie past the wall declared by
 * rwebp_anim_stream_set_avail. */
const uint32_t *rwebp_anim_stream_next(rwebp_anim_stream_t *stream,
      int *duration_ms);

/* Progressive open over a partially-resident buffer: only the first
 * 'avail' bytes are guaranteed present.  The container is walked chunk
 * header by chunk header, never past the wall; VP8X's animation flag
 * decides "animated" (a still returns NULL with *need_more clear, from
 * its first chunk alone), and the open succeeds once two ANMF headers
 * are indexed (so the first frame is resident and the file is known to
 * animate), or one when the whole file is within 'avail'.  Otherwise
 * NULL with *need_more set (may be NULL): retry with a larger prefix.
 * Frames are indexed lazily as the wall advances; a chunk whose bytes
 * are not yet resident is not decoded (next() returns NULL, nothing
 * consumed).  rwebp_anim_stream_open(buf, len) is this with
 * avail == len. */
rwebp_anim_stream_t *rwebp_anim_stream_open_avail(const uint8_t *buf,
      size_t len, size_t avail, int *need_more);

/* Exact store of the readable bound (not a raise: a windowed caller's
 * bytes un-arrive when its feeder decommits behind the decoder or
 * rewinds the window at a loop).  Clamped to the buffer length. */
void rwebp_anim_stream_set_avail(rwebp_anim_stream_t *stream, size_t avail);

/* Byte cursor for a windowing feeder.  media_floor is the offset of the
 * first ANMF chunk header (fixed for the stream's life); consumed is
 * where the decoder's next read lands - the header of the next frame
 * to decode, the scan position once every indexed frame is out, the
 * buffer length once the file is exhausted.  rewind() drops it back to
 * the floor.  next_span reports the byte range [lo, hi) the next frame
 * occupies (chunk header to chunk end) so the feeder can make a frame
 * larger than its lookahead resident before asking for it; 0/0 when no
 * frame is indexed at the cursor. */
size_t rwebp_anim_stream_media_floor(const rwebp_anim_stream_t *stream);
size_t rwebp_anim_stream_consumed(const rwebp_anim_stream_t *stream);
void rwebp_anim_stream_next_span(const rwebp_anim_stream_t *stream,
      size_t *lo, size_t *hi);

void rwebp_anim_stream_rewind(rwebp_anim_stream_t *stream);

/* Select the channel order of subsequently emitted frames: non-zero
 * for ARGB words, zero for the default memory-order R,G,B,A.  The
 * order is a property of the compositing canvas, so a switch after
 * frames have been emitted converts the canvas in place once (a
 * single full-canvas pass); sub-frame decoding then stores the new
 * order directly at no per-frame cost.  Persists across rewind. */
void rwebp_anim_stream_set_argb(rwebp_anim_stream_t *stream, int argb);

RETRO_END_DECLS

#endif
