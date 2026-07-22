/* rmp4_video -- MP4 video-to-image glue for libretro-common.
 *
 * Combines the rmp4 demuxer with the rvp8/rvp9 decoders to present an
 * MP4 file's video track through the same two interfaces the image
 * formats use, so the menu thumbnail pipeline can treat a .mp4 exactly
 * like a .webm or an animated .webp:
 *
 *   - Still image: rmp4_video_alloc / set_buf_ptr / process_image
 *     decode the first displayed frame (the rwebp_t-style contract that
 *     image_transfer.c drives for the initial texture upload).
 *
 *   - Streaming animation: rmp4_video_stream_* decode one displayed
 *     frame per call, in stream order, with per-frame durations derived
 *     from the container's sample timestamps (the rwebp_anim_stream_t
 *     contract that gfx_thumbnail_animate() drives).
 *
 * VP8 ('vp08') tracks require the rvp8 decoder (built with HAVE_RWEBP
 * or HAVE_RWEBM); VP9 ('vp09') tracks additionally require HAVE_RVP9.
 * Tracks whose codec is not compiled in (AVC, HEVC, AV1, ...) are
 * skipped.  Audio tracks are ignored: output is always opaque.
 *
 * Frames are returned as w*h uint32 pixels. The animation stream emits
 * ABGR words (memory order R,G,B,A on little-endian), matching the
 * animated WebP stream; the still-image path honours supports_rgba the
 * same way rwebp_process_image does (ARGB words when false).
 */
#ifndef __LIBRETRO_SDK_FORMAT_RMP4_VIDEO_H__
#define __LIBRETRO_SDK_FORMAT_RMP4_VIDEO_H__

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* ---- Still image (first displayed frame) ---- */

typedef struct rmp4_video rmp4_video_t;

rmp4_video_t *rmp4_video_alloc(void);

void rmp4_video_free(rmp4_video_t *mp4);

bool rmp4_video_set_buf_ptr(rmp4_video_t *mp4, void *data, size_t len);

/* Request packed XRGB2101010 (10-bit) output for 10-bit HDR sources; 8-bit
 * sources are unaffected. Off by default. */
void rmp4_video_set_want_10bit(rmp4_video_t *mp4, int want);

/* Partial-read support for the still decode: declare how many leading
 * bytes of the buffer are valid (monotonic; 0 means fully resident).
 * With a short avail, rmp4_video_process_image returns
 * IMAGE_PROCESS_WAIT instead of failing when it needs bytes that have
 * not arrived: the moov (for a trailing moov or a fragmented movie
 * this effectively means the whole file) or the first displayed
 * frame's sample. */
void rmp4_video_set_avail(rmp4_video_t *mp4, size_t avail);

/* True if the last rmp4_video_process_image() produced XRGB2101010. */
bool rmp4_video_is_10bit(const rmp4_video_t *mp4);

/* Decodes the first displayed frame of the first supported video track
 * into a freshly malloc'd buffer at *buf. Returns IMAGE_PROCESS_END on
 * success, IMAGE_PROCESS_ERROR on failure (no supported video track,
 * malformed stream, or out of memory). */
int rmp4_video_process_image(rmp4_video_t *mp4, void **buf,
      size_t len, unsigned *width, unsigned *height, bool supports_rgba);

/* ---- Streaming animation ---- */

typedef struct rmp4_video_stream rmp4_video_stream_t;

/* Take ownership of the stream a successful rmp4_video_process_image
 * left open, positioned just past the first displayed frame, so the
 * caller can continue the video as an animation without re-opening
 * (and re-pre-scanning) the file.  Returns NULL if no stream is held
 * (no process call yet, it failed, or the stream was already
 * detached).  The stream borrows the buffer given via
 * rmp4_video_set_buf_ptr, which must outlive it; close it with
 * rmp4_video_stream_close.  10-bit output requested for the still is
 * dropped: detached streams emit 8-bit frames. */
rmp4_video_stream_t *rmp4_video_detach_stream(rmp4_video_t *mp4);

/* The buffer is BORROWED and must remain valid and unmodified until
 * rmp4_video_stream_close. Returns NULL when the data is not an MP4
 * stream, has no video track with a compiled-in codec, or contains no
 * video packets. */
rmp4_video_stream_t *rmp4_video_stream_open(const uint8_t *buf,
      size_t len);

/* Open against a partially-read buffer: 'avail' leading bytes are
 * valid (raise later with the stream set_avail).  On NULL, *need_more
 * distinguishes "feed more bytes and retry" from malformed data. */
rmp4_video_stream_t *rmp4_video_stream_open_avail(const uint8_t *buf,
      size_t len, size_t avail, int *need_more);

void rmp4_video_stream_close(rmp4_video_stream_t *stream);

/* num_frames is the number of coded video packets, saturating at the
 * pre-scan cap (a few thousand): treat it as "at least this many", an
 * upper bound on displayed frames only below the cap (the stream may
 * carry non-shown frames).  loop_count is always 0: video loops
 * indefinitely. */
void rmp4_video_stream_get_info(const rmp4_video_stream_t *stream,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count);

/* Decode the next displayed frame. Returns the frame pixels (valid until
 * the next call on this stream) and writes its display duration in ms
 * (0 when unknown; the caller applies its default), or NULL at the end
 * of one pass; call rmp4_video_stream_rewind to loop.  Pixels are in
 * memory order R,G,B,A by default; see rmp4_video_stream_set_argb. */
const uint32_t *rmp4_video_stream_next(rmp4_video_stream_t *stream,
      int *duration_ms);

/* Select the channel order of subsequent frames: non-zero emits ARGB
 * words (memory order B,G,R,A on little-endian), zero restores the
 * default R,G,B,A memory order.  Applies to the 8-bit output paths;
 * 10-bit XRGB2101010 output is unaffected.  The order is baked by the
 * blit, so this costs nothing per frame - it exists so a caller whose
 * upload format is ARGB does not need its own full-frame swizzle pass.
 * Takes effect from the next decoded frame; may be changed between
 * frames.  A stream detached from a still-image transfer starts at
 * the default order. */
void rmp4_video_stream_set_argb(rmp4_video_stream_t *stream, int argb);

/* Advance past the next displayed frame without colour-converting it:
 * the picture stays inside the decoder and no work is spent on its
 * pixels.  Returns 1 when a frame was consumed (its display duration
 * written as for _next), -1 at the end of a pass or on error.  For a
 * caller pacing through several frames per tick, skip the passed-over
 * frames and render only the one presented.  rmp4_video_stream_next
 * is exactly skip followed by render. */
int rmp4_video_stream_skip(rmp4_video_stream_t *stream, int *duration_ms);

/* Colour-convert the most recently consumed displayed frame (from
 * _next or _skip) into the stream canvas and return it - the planes
 * stay valid inside the decoder until the next decode, so this can be
 * called at any point after the frame was consumed, and repeatedly
 * (idempotent).  NULL when no frame is pending (before the first
 * frame, after rewind or seek, or at end of stream). */
const uint32_t *rmp4_video_stream_render(rmp4_video_stream_t *stream);

/* Partial-read support: raise the number of leading buffer bytes that
 * are valid (monotonic).  A blocked skip/next (return 2 / NULL after
 * a step that returned 2) resumes once the needed sample's bytes are
 * inside the window.  Fully-resident streams never block. */
void rmp4_video_stream_set_avail(rmp4_video_stream_t *stream,
      size_t avail);

/* Bounded-memory streaming support (see rmp4_media_floor/consumed). */
size_t rmp4_video_stream_media_floor(rmp4_video_stream_t *s);
size_t rmp4_video_stream_consumed(rmp4_video_stream_t *s);

void rmp4_video_stream_rewind(rmp4_video_stream_t *stream);

/* Seek to the display position at or before 'ms' milliseconds: the
 * stream restarts at the preceding key frame and decodes forward,
 * discarding pictures, so the next rmp4_video_stream_next call
 * returns the first frame past the position.  Returns the position in
 * ms actually reached (the presented slot's timestamp), or -1 on a
 * stream without timing. */
int64_t rmp4_video_stream_seek_ms(rmp4_video_stream_t *stream, int64_t ms);

/* Timestamp span of the display timeline: last frame's presentation
 * time minus the first's, in ms.  (frames-1)/span is the stream's true
 * mean rate; the track duration also counts the final frame's own
 * duration, which skews a rate derived from it. */
int64_t rmp4_video_stream_span_ms(const rmp4_video_stream_t *stream);

RETRO_END_DECLS

#endif
