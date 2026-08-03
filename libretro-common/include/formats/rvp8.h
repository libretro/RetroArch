/* rvp8 -- self-contained VP8 key-frame (intra) decoder for libretro-common.
 *
 * Extracted from the WebP decoder (formats/webp/rwebp.c), where it decodes
 * the lossy VP8 image chunk.  VP8 key frames are exactly what WebP carries,
 * and the same decoder is the intra-frame core a VP8 video stream needs.
 *
 * Decodes coefficient tokens, dequantisation, the DCT and Walsh-Hadamard
 * inverse transforms, 4x4 and 16x16 intra prediction, both the simple and
 * normal loop filters, fancy chroma upsampling and YUV->RGB.  Inter-frame
 * prediction (motion vectors, golden/altref reference frames) is not
 * present: a WebP VP8 chunk is always a single key frame.
 *
 * Two APIs are provided:
 *   - rvp8_decode(): one-shot; decode a whole key frame to RGBA.
 *   - the resumable rvp8_* primitives (begin/rows/output/filter/upsample):
 *     row-at-a-time decode, for callers that want to drive the decoder
 *     incrementally (e.g. a future video demuxer).
 */
#ifndef __LIBRETRO_SDK_FORMAT_RVP8_H__
#define __LIBRETRO_SDK_FORMAT_RVP8_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* VP8 boolean entropy decoder (RFC 6386 s7). Public because the resumable
 * decode state embeds it; callers do not manipulate it directly. */
typedef struct
{
   const uint8_t *buf, *end;
   uint32_t range;
   uint64_t value;
   int count;
} rvp8_bool;

/* Resumable VP8 key-frame decode state. Opaque in practice -- callers use
 * the primitives below -- but defined here so it can live on the stack. */
typedef struct rvp8_dec
{
   rvp8_bool br;
   rvp8_bool tbr[8];                 /* token partitions                    */
   uint8_t   cprob[4][8][3][11];
   uint8_t  *seg_map_buf;
   uint8_t  *skip_lf_buf;
   uint8_t  *bpred_buf;
   uint8_t  *yb, *ub, *vb;
   uint8_t  *above_nz_y, *above_nz_u, *above_nz_v, *above_nz_dc;
   uint8_t  *above_bmodes;
   uint8_t  *fancy_uv;               /* 2*w chroma-interp scratch (or NULL) */
   int       swap_rb;                /* upsample emits memory-order R,G,B,A
                                        words (ABGR32 on LE) instead of the
                                        default ARGB when nonzero          */
   int       w, h, mbw, mbh, ys, uvs;
   int       base_qp, y1dc_dq, y2dc_dq, y2ac_dq, uvdc_dq, uvac_dq;
   int       skip_enabled, prob_skip, num_parts;
   int       filter_type, lf_level, sharpness, lf_delta_enabled;
   int       ref_lf_delta[4], mode_lf_delta[4];
   int       seg_enabled, seg_abs, seg_qp[4], seg_lf[4], seg_prob[3];
   int       my;                     /* next MB row to decode               */
   /* Inter-frame state (0/unused for key frames). */
   int       is_inter;               /* this frame is an inter frame        */
   int       use_bilinear;           /* version 1-3: 2-tap MC filter        */
   int       full_pixel;             /* version 3: integer-pel chroma MVs   */
   int       refresh_golden, refresh_altref, refresh_last;
   int       copy_golden, copy_altref;
   int       refresh_entropy;
   int       sign_bias[4];           /* indexed by ref_frame: 0=intra(unused),
                                        1=last(always 0), 2=golden, 3=altref */
   int       prob_intra, prob_last, prob_golden;
   uint8_t   ymode_prob[4];          /* inter-frame intra Y mode probs       */
   uint8_t   uvmode_prob[3];         /* inter-frame intra UV mode probs      */
   uint8_t   mvc[2][19];             /* MV component probabilities           */
   /* Reference frame planes (borrowed from the persistent decoder; the
    * inter MB predictor reads from these). NULL for a key frame. */
   const uint8_t *ref_y[3], *ref_u[3], *ref_v[3];
   int       ref_ys[3], ref_uvs[3];
   /* Per-MB motion vectors for the current frame, mbw*mbh entries, so the
    * MV predictor can read decoded neighbours. Owned here. */
   void     *mb_info;
} rvp8_dec;

/* One-shot: decode a complete VP8 key frame from 'data' (the raw VP8
 * bitstream, i.e. the WebP 'VP8 ' chunk payload) to a freshly malloc'd
 * buffer of *ow * *oh pixels. Channel order is selected at store time:
 * 0xAARRGGBB words when swap_rb is 0, memory-order R,G,B,A (ABGR32
 * words on LE) when nonzero - no post-pass swizzle is needed either
 * way. Returns NULL on malformed input. The caller frees the buffer. */
uint32_t *rvp8_decode(const uint8_t *data, size_t len,
      unsigned *ow, unsigned *oh, int swap_rb);

/* Resumable primitives. Typical drive loop mirrors rvp8_decode():
 *   if (rvp8_begin(data, len, &s) != 0) fail;
 *   while (rvp8_rows(&s, s.mbh) > 0) ;
 *   pix = rvp8_output(&s);
 *   rvp8_filter_rows(&s, 0, s.mbh);
 *   rvp8_upsample_rows(&s, pix, 0, s.h);
 *   rvp8_abort(&s);
 */
int       rvp8_begin(const uint8_t *data, size_t len, rvp8_dec *s);
int       rvp8_rows(rvp8_dec *s, int nrows);
uint32_t *rvp8_output(rvp8_dec *s);
void      rvp8_filter_rows(rvp8_dec *s, int my0, int my1);
int       rvp8_upsample_rows(rvp8_dec *s, uint32_t *pix, int j0, int max_rows);
void      rvp8_abort(rvp8_dec *s);

/* Persistent VP8 video decoder: decodes a sequence of frames (key and
 * inter), maintaining the last/golden/altref reference frames and the
 * inherited probability context across frames.  Feed raw VP8 frame
 * payloads (e.g. WebM block data) in stream order; the first frame must
 * be a key frame.
 *
 *   rvp8_video *v = rvp8_video_open();
 *   for each frame:
 *      if (rvp8_video_decode(v, data, len) != 0) fail;
 *      y = rvp8_video_plane(v, 0, &ystride, &w, &h);   (also 1=U, 2=V)
 *   rvp8_video_close(v);
 *
 * The returned plane pointers stay valid until the next decode call. */
typedef struct rvp8_video rvp8_video;

rvp8_video    *rvp8_video_open(void);
int            rvp8_video_decode(rvp8_video *v, const uint8_t *data, size_t len);
const uint8_t *rvp8_video_plane(const rvp8_video *v, int plane,
      int *stride, int *width, int *height);
void           rvp8_video_close(rvp8_video *v);

RETRO_END_DECLS

#endif
