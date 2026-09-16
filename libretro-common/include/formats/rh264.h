/* rh264 -- self-contained H.264 decoder for libretro-common.
 *
 * Decodes H.264 (AVC) I, P and B pictures with either entropy coding
 * (CAVLC and CABAC): NAL/SPS/PPS/slice parsing; 4x4, 8x8 and 16x16 luma
 * and chroma intra prediction; inter prediction with quarter-pel motion
 * compensation, multiple reference pictures (a DPB with sliding-window
 * and MMCO marking and reference list modifications), weighted and
 * implicit bi-prediction, and spatial and temporal direct modes; the
 * 4x4 and 8x8 integer transforms with scaling matrices and the Hadamard
 * DC transforms; dequantisation with correct chroma-QP derivation; the
 * in-loop deblocking filter; and picture order count types 0, 1 and 2
 * with display-order output.  Reconstruction is 8-bit 4:2:0 and 4:2:2,
 * which covers the baseline, main and high profiles as commonly emitted.
 *
 * Out-of-scope streams are refused at the parameter-set or slice level
 * rather than decoded wrongly: 4:4:4, monochrome, high bit depths and
 * lossless transform bypass; SP/SI switching slices; FMO/ASO and
 * redundant pictures; field-coded B and CABAC pictures and field-coded
 * macroblock pairs (frame-coded MBAFF pairs decode; field pictures
 * decode for CAVLC I/P).
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
 *      if (rh264_video_decode(v, data, len) < 0) fail;   (1 = picture ready)
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
 * decoded picture leaves in display order, which with B pictures is not
 * decode order: returns 1 when a picture is ready through rh264_video_plane,
 * 0 when the data was consumed but the picture is still held for reordering,
 * and negative on malformed input or an unsupported (high-profile) stream. */
int rh264_video_decode(rh264_video *v, const uint8_t *data, size_t len);

/* Hand out the next pending picture in display order without feeding more
 * data, for end of stream. Returns 0 when a picture became available through
 * rh264_video_plane, -1 when nothing is pending. */
int rh264_video_drain(rh264_video *v);

/* Bits per sample of the decoded pictures (8..14).  Above 8 the planes
 * hold uint16_t samples: rh264_video_plane still returns a byte
 * pointer (cast it; the stride counts samples). */
int rh264_video_bit_depth(const rh264_video *v);

/* Borrow a decoded plane (0=Y, 1=U, 2=V). Valid until the next decode call. */
const uint8_t *rh264_video_plane(const rh264_video *v, int plane,
      int *stride, int *width, int *height);

void rh264_video_close(rh264_video *v);

RETRO_END_DECLS

#endif
