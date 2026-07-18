/* rh264 -- self-contained H.264 decoder for libretro-common.
 *
 * Decodes H.264 (AVC) I and P pictures: NAL/SPS/PPS/slice parsing, CAVLC and
 * CABAC residual decoding, 4x4 and 16x16 luma intra prediction and chroma
 * intra prediction, inter prediction (motion vector prediction, sub-pel
 * luma and chroma interpolation, a single reference picture), the inverse
 * 4x4 transform and Hadamard DC transforms, dequantisation (with correct
 * chroma-QP derivation), and the in-loop deblocking filter.  B pictures,
 * CABAC-coded P slices, multiple reference pictures and the higher profiles
 * are not present.
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
 * I420 planes. Accepts Annex-B or length-prefixed AVCC data. IDR pictures and
 * CAVLC-coded P pictures are decoded, the latter predicted from the previously
 * decoded picture; returns 0 on success, nonzero on malformed input or an
 * unsupported (B-slice/CABAC-P/high-profile) stream. */
int rh264_video_decode(rh264_video *v, const uint8_t *data, size_t len);

/* Borrow a decoded plane (0=Y, 1=U, 2=V). Valid until the next decode call. */
const uint8_t *rh264_video_plane(const rh264_video *v, int plane,
      int *stride, int *width, int *height);

void rh264_video_close(rh264_video *v);

RETRO_END_DECLS

#endif
