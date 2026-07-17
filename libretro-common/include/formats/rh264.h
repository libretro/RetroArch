/* rh264 -- self-contained H.264 baseline intra-frame decoder for
 * libretro-common.
 *
 * Decodes H.264 (AVC) baseline-profile intra frames: NAL/SPS/PPS/slice
 * parsing, CAVLC residual decoding, 4x4 and 16x16 luma intra prediction and
 * chroma intra prediction, the inverse 4x4 transform and Hadamard DC
 * transforms, dequantisation (with correct chroma-QP derivation), and the
 * in-loop deblocking filter.  Inter-frame prediction (motion vectors,
 * reference frames), CABAC entropy coding, and the higher profiles are not
 * present: a thumbnail decoder only needs the first key frame, which in a
 * baseline avc1 stream is an IDR intra frame.
 *
 * The CAVLC VLC tables are extracted verbatim from the encoder tables in
 * libopenh264 (verified prefix-free); no table is hand-transcribed.
 *
 * The persistent video API mirrors rvp8_video so a demuxer (e.g. the MP4
 * glue in rmp4_video.c) can dispatch H.264 exactly like it dispatches VP8/
 * VP9:
 *
 *   rh264_video *v = rh264_video_open();
 *   rh264_video_set_extradata(v, avcc, avcc_len);   (the avcC box payload)
 *   for each frame:
 *      if (rh264_video_decode(v, data, len) != 0) fail;
 *      y = rh264_video_plane(v, 0, &ystride, &w, &h);   (also 1=U, 2=V)
 *   rh264_video_close(v);
 *
 * Frame data may be either Annex-B (start-code delimited) or the
 * length-prefixed AVCC form carried in MP4 'mdat'; the NAL length size is
 * taken from the avcC extradata when present (default 4).  The returned
 * plane pointers stay valid until the next decode call. */
#ifndef __LIBRETRO_SDK_FORMAT_RH264_H__
#define __LIBRETRO_SDK_FORMAT_RH264_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct rh264_video rh264_video;

/* Create a decoder. Returns NULL on allocation failure. */
rh264_video *rh264_video_open(void);

/* Supply the avcC (AVCDecoderConfigurationRecord) extradata from the sample
 * description, which carries the SPS/PPS and the NAL length size. Optional
 * for Annex-B input that carries in-band SPS/PPS, required for bare AVCC
 * sample data. Returns 0 on success. Safe to call once before decoding. */
int rh264_video_set_extradata(rh264_video *v, const uint8_t *avcc, size_t len);

/* Decode one access unit (one coded picture worth of NAL units) to internal
 * I420 planes. Accepts Annex-B or length-prefixed AVCC data. Only the intra
 * (IDR) frame is decoded; returns 0 on success, nonzero on malformed input
 * or an unsupported (inter/CABAC/high-profile) stream. */
int rh264_video_decode(rh264_video *v, const uint8_t *data, size_t len);

/* Borrow a decoded plane (0=Y, 1=U, 2=V). Valid until the next decode call. */
const uint8_t *rh264_video_plane(const rh264_video *v, int plane,
      int *stride, int *width, int *height);

void rh264_video_close(rh264_video *v);

RETRO_END_DECLS

#endif
