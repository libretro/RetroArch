/* rh265 -- self-contained H.265/HEVC decoder for libretro-common.
 *
 * This first milestone decodes Main-profile intra pictures (IDR/CRA/
 * TRAIL I slices) in 8-bit 4:2:0, with the full intra toolset: all 35
 * prediction modes, the 4x4 DST and 4/8/16/32 inverse DCTs, transform
 * skip, sign-data hiding, cu_qp_delta quantisation groups, the in-loop
 * deblocking filter and sample-adaptive offset.
 *
 * Out-of-scope streams are refused at the parameter-set or slice level
 * rather than decoded wrongly: P and B slices (the next milestone),
 * 4:2:2/4:4:4/monochrome, high bit depths, tiles, wavefront parallel
 * processing, dependent slice segments, explicit scaling lists, PCM,
 * transquant bypass and constrained intra prediction.
 *
 * The persistent video API mirrors rh264_video/rvp8_video so a demuxer
 * (e.g. the MP4 glue in rmp4_video.c) can dispatch H.265 the same way:
 *
 *   rh265_video *v = rh265_video_open();
 *   rh265_video_set_extradata(v, hvcc, hvcc_len);   (the hvcC box payload)
 *   for each frame:
 *      if (rh265_video_decode(v, data, len) < 0) fail;   (1 = picture ready)
 *      y = rh265_video_plane(v, 0, &ystride, &w, &h);    (also 1=U, 2=V)
 *   rh265_video_close(v);
 *
 * Frame data may be either Annex-B (start-code delimited) or the
 * length-prefixed HVCC form carried in MP4 'mdat'; the NAL length size
 * is taken from the hvcC extradata when present (default 4).  The
 * returned plane pointers stay valid until the next decode call. */
#ifndef __LIBRETRO_SDK_FORMAT_RH265_H__
#define __LIBRETRO_SDK_FORMAT_RH265_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct rh265_video rh265_video;

/* Create a decoder. Returns NULL on allocation failure. */
rh265_video *rh265_video_open(void);

/* Supply the hvcC (HEVCDecoderConfigurationRecord) extradata from the
 * sample description, which carries the VPS/SPS/PPS and the NAL length
 * size. Optional for Annex-B input that carries in-band parameter sets,
 * required for bare HVCC sample data. Returns 0 on success. Safe to call
 * once before decoding. */
int rh265_video_set_extradata(rh265_video *v, const uint8_t *hvcc, size_t len);

/* Decode one access unit (one coded picture worth of NAL units) to
 * internal I420 planes. Accepts Annex-B or length-prefixed HVCC data.
 * Returns 1 when a picture is ready through rh265_video_plane, 0 when
 * the data was consumed without producing a picture (parameter sets or
 * SEI only), and negative on malformed input or an unsupported (e.g.
 * inter-coded) stream. */
int rh265_video_decode(rh265_video *v, const uint8_t *data, size_t len);

/* Hand out the next pending picture in display order without feeding
 * more data, for end of stream. Returns 0 when a picture became
 * available through rh265_video_plane, -1 when nothing is pending.
 * (Intra pictures leave in decode order, so this only matters once
 * inter prediction lands.) */
int rh265_video_drain(rh265_video *v);

/* Borrow a decoded plane (0=Y, 1=U, 2=V). Valid until the next decode call. */
/* Active luma bit depth of the stream (8 or 10).  At 10 bits the
 * plane pointers reference uint16_t samples: cast the returned byte
 * pointer and index with the sample stride. */
int rh265_video_bit_depth(const rh265_video *v);

const uint8_t *rh265_video_plane(const rh265_video *v, int plane,
      int *stride, int *width, int *height);

void rh265_video_close(rh265_video *v);

RETRO_END_DECLS

#endif
