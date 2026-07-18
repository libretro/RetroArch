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

/* The buffer is BORROWED and must remain valid and unmodified until
 * rmp4_video_stream_close. Returns NULL when the data is not an MP4
 * stream, has no video track with a compiled-in codec, or contains no
 * video packets. */
rmp4_video_stream_t *rmp4_video_stream_open(const uint8_t *buf,
      size_t len);

void rmp4_video_stream_close(rmp4_video_stream_t *stream);

/* num_frames is the number of coded video packets (an upper bound on
 * displayed frames when the stream carries non-shown frames).
 * loop_count is always 0: video loops indefinitely. */
void rmp4_video_stream_get_info(const rmp4_video_stream_t *stream,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count);

/* Decode the next displayed frame. Returns the frame pixels (valid until
 * the next call on this stream) and writes its display duration in ms
 * (0 when unknown; the caller applies its default), or NULL at the end
 * of one pass; call rmp4_video_stream_rewind to loop. */
const uint32_t *rmp4_video_stream_next(rmp4_video_stream_t *stream,
      int *duration_ms);

void rmp4_video_stream_rewind(rmp4_video_stream_t *stream);

RETRO_END_DECLS

#endif
