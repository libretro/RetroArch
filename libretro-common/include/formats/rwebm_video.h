/* rwebm_video -- WebM video-to-image glue for libretro-common.
 *
 * Combines the rwebm demuxer with the rvp8/rvp9 decoders to present a
 * WebM file's video track through the same two interfaces the image
 * formats use, so the menu thumbnail pipeline can treat a .webm exactly
 * like an animated .webp:
 *
 *   - Still image: rwebm_video_alloc / set_buf_ptr / process_image
 *     decode the first displayed frame (the rwebp_t-style contract that
 *     image_transfer.c drives for the initial texture upload).
 *
 *   - Streaming animation: rwebm_video_stream_* decode one displayed
 *     frame per call, in stream order, with per-frame durations derived
 *     from the container's block timestamps (the rwebp_anim_stream_t
 *     contract that gfx_thumbnail_animate() drives).
 *
 * VP8 tracks require the rvp8 decoder (built with HAVE_RWEBP or
 * HAVE_RWEBM); VP9 tracks additionally require HAVE_RVP9. Tracks whose
 * codec is not compiled in are skipped. Audio tracks are ignored, and
 * alpha (BlockAdditions) is ignored: output is always opaque.
 *
 * Frames are returned as w*h uint32 pixels. The animation stream emits
 * ABGR words (memory order R,G,B,A on little-endian), matching the
 * animated WebP stream; the still-image path honours supports_rgba the
 * same way rwebp_process_image does (ARGB words when false).
 */
#ifndef __LIBRETRO_SDK_FORMAT_RWEBM_VIDEO_H__
#define __LIBRETRO_SDK_FORMAT_RWEBM_VIDEO_H__

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* ---- Still image (first displayed frame) ---- */

typedef struct rwebm_video rwebm_video_t;

rwebm_video_t *rwebm_video_alloc(void);

void rwebm_video_free(rwebm_video_t *webm);

/* 10-bit 4:2:0 -> 32-bit RGB.  Planes are uint16 samples with strides in
 * PIXELS.  matrix / transfer / range are ISO/IEC 23001-8 code points as
 * carried by the webm Colour element (0 = absent; HD-appropriate
 * defaults are chosen).  transfer 16 (PQ / HDR10) is tone-mapped to SDR
 * (hable curve, auto exposure, 1000-nit assumed peak, BT.2020 -> 709
 * gamut); anything else is converted as SDR and rounded to 8 bits.
 * abgr nonzero packs R in the low byte (thumbnail convention); zero
 * packs XRGB8888 (libretro frontend convention). */
void rwebm_video_blit_i420_hbd(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h, const uint16_t *y, int ys,
      const uint16_t *u, const uint16_t *v, int uvs,
      unsigned matrix, unsigned transfer, unsigned range, int abgr);

bool rwebm_video_set_buf_ptr(rwebm_video_t *webm, void *data, size_t len);

/* Decodes the first displayed frame of the first supported video track
 * into a freshly malloc'd buffer at *buf. Returns IMAGE_PROCESS_END on
 * success, IMAGE_PROCESS_ERROR on failure (no supported video track,
 * malformed stream, or out of memory). */
int rwebm_video_process_image(rwebm_video_t *webm, void **buf,
      size_t len, unsigned *width, unsigned *height, bool supports_rgba);

/* ---- Streaming animation ---- */

typedef struct rwebm_video_stream rwebm_video_stream_t;

/* The buffer is BORROWED and must remain valid and unmodified until
 * rwebm_video_stream_close. Returns NULL when the data is not a WebM
 * stream, has no video track with a compiled-in codec, or contains no
 * video packets. */
rwebm_video_stream_t *rwebm_video_stream_open(const uint8_t *buf,
      size_t len);

void rwebm_video_stream_close(rwebm_video_stream_t *stream);

/* num_frames is the number of coded video packets (an upper bound on
 * displayed frames when the stream carries non-shown frames).
 * loop_count is always 0: video loops indefinitely. */
void rwebm_video_stream_get_info(const rwebm_video_stream_t *stream,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count);

/* Decode the next displayed frame. Returns the frame pixels (valid until
 * the next call on this stream) and writes its display duration in ms
 * (0 when unknown; the caller applies its default), or NULL at the end
 * of one pass; call rwebm_video_stream_rewind to loop. */
const uint32_t *rwebm_video_stream_next(rwebm_video_stream_t *stream,
      int *duration_ms);

void rwebm_video_stream_rewind(rwebm_video_stream_t *stream);

RETRO_END_DECLS

#endif
