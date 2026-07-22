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
 * (hable curve, auto exposure, MaxCLL-driven peak with a
 * 1000-nit fallback, BT.2020 -> 709
 * gamut); anything else is converted as SDR and rounded to 8 bits.
 * abgr nonzero packs R in the low byte (thumbnail convention); zero
 * packs XRGB8888 (libretro frontend convention). */
void rwebm_video_blit_i420_hbd(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h, const uint16_t *y, int ys,
      const uint16_t *u, const uint16_t *v, int uvs,
      unsigned matrix, unsigned transfer, unsigned range,
      unsigned max_cll, int abgr);

/* Native 10-bit variant: same SDR-encoded colour as the _hbd path (PQ/HDR10
 * tone-mapped, BT.2020 -> 709 gamut, sRGB transfer) but emitted at 10-bit
 * precision into packed XRGB2101010 (bits [29:20]=R [19:10]=G [9:0]=B, native
 * endian) instead of narrowing to 8 bits, so gradients band less. Used when
 * the frontend accepts RETRO_PIXEL_FORMAT_XRGB2101010. The frontend HDR path
 * expects an SDR-encoded source, so this raises bit depth only and does not
 * pass PQ through. */
void rwebm_video_blit_i420_10bit(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h, const uint16_t *y, int ys,
      const uint16_t *u, const uint16_t *v, int uvs,
      unsigned matrix, unsigned transfer, unsigned range,
      unsigned max_cll);

bool rwebm_video_set_buf_ptr(rwebm_video_t *webm, void *data, size_t len);

/* Request packed XRGB2101010 (10-bit) output for 10-bit HDR sources; 8-bit
 * sources are unaffected. Off by default. */
void rwebm_video_set_want_10bit(rwebm_video_t *webm, int want);

/* Partial-read support for the still decode: declare how many leading
 * bytes of the buffer are valid (monotonic; 0 means fully resident).
 * With a short avail, rwebm_video_process_image returns
 * IMAGE_PROCESS_WAIT instead of failing when it needs bytes that have
 * not arrived (the header, at least two scanned frames, or the first
 * displayed frame's blocks). */
void rwebm_video_set_avail(rwebm_video_t *webm, size_t avail);

/* True if the last rwebm_video_process_image() produced XRGB2101010. */
bool rwebm_video_is_10bit(const rwebm_video_t *webm);

/* Decodes the first displayed frame of the first supported video track
 * into a freshly malloc'd buffer at *buf. Returns IMAGE_PROCESS_END on
 * success, IMAGE_PROCESS_ERROR on failure (no supported video track,
 * malformed stream, or out of memory). */
int rwebm_video_process_image(rwebm_video_t *webm, void **buf,
      size_t len, unsigned *width, unsigned *height, bool supports_rgba);

/* ---- Streaming animation ---- */

typedef struct rwebm_video_stream rwebm_video_stream_t;

/* Take ownership of the stream a successful rwebm_video_process_image
 * left open, positioned just past the first displayed frame, so the
 * caller can continue the video as an animation without re-opening
 * (and re-pre-scanning) the file.  Returns NULL if no stream is held
 * (no process call yet, it failed, or the stream was already
 * detached).  The stream borrows the buffer given via
 * rwebm_video_set_buf_ptr, which must outlive it; close it with
 * rwebm_video_stream_close.  10-bit output requested for the still is
 * dropped: detached streams emit 8-bit frames. */
rwebm_video_stream_t *rwebm_video_detach_stream(rwebm_video_t *webm);

/* The buffer is BORROWED and must remain valid and unmodified until
 * rwebm_video_stream_close. Returns NULL when the data is not a WebM
 * stream, has no video track with a compiled-in codec, or contains no
 * video packets. */
rwebm_video_stream_t *rwebm_video_stream_open(const uint8_t *buf,
      size_t len);

void rwebm_video_stream_close(rwebm_video_stream_t *stream);

/* num_frames is the number of coded video packets, saturating at the
 * pre-scan cap (a few thousand): treat it as "at least this many", an
 * upper bound on displayed frames only below the cap (the stream may
 * carry non-shown frames).  loop_count is always 0: video loops
 * indefinitely. */
void rwebm_video_stream_get_info(const rwebm_video_stream_t *stream,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count);

/* Decode the next displayed frame. Returns the frame pixels (valid until
 * the next call on this stream) and writes its display duration in ms
 * (0 when unknown; the caller applies its default), or NULL at the end
 * of one pass; call rwebm_video_stream_rewind to loop.  Pixels are in
 * memory order R,G,B,A by default; see rwebm_video_stream_set_argb. */
const uint32_t *rwebm_video_stream_next(rwebm_video_stream_t *stream,
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
void rwebm_video_stream_set_argb(rwebm_video_stream_t *stream, int argb);

/* Partial-read support: raise the number of leading buffer bytes that
 * are valid (monotonic).  A blocked step resumes once the needed
 * block's bytes are inside the window; fully-resident streams never
 * block. */
void rwebm_video_stream_set_avail(rwebm_video_stream_t *stream,
      size_t avail);

/* For a stream adopted from a still decoded against a partial read:
 * once the whole file is in the buffer, finish the timestamp
 * pre-scan the partial open truncated at its byte wall, making every
 * per-frame duration identical to a stream opened over the complete
 * file (a wall-truncated table otherwise paces frames past it by a
 * single-interval estimate - for ms-rounded 30 fps content, a
 * constant 33 where the true cadence alternates 33/34, i.e. a third
 * of a millisecond of drift per frame until the table's cap).
 * A bounded, header-only walk of at most the table cap's packets. */
void rwebm_video_stream_complete_scan(rwebm_video_stream_t *stream,
      const uint8_t *buf, size_t len);

void rwebm_video_stream_rewind(rwebm_video_stream_t *stream);

RETRO_END_DECLS

#endif
