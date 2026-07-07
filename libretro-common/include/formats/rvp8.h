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
   int       w, h, mbw, mbh, ys, uvs;
   int       base_qp, y1dc_dq, y2dc_dq, y2ac_dq, uvdc_dq, uvac_dq;
   int       skip_enabled, prob_skip, num_parts;
   int       filter_type, lf_level, sharpness, lf_delta_enabled;
   int       ref_lf_delta[4], mode_lf_delta[4];
   int       seg_enabled, seg_abs, seg_qp[4], seg_lf[4], seg_prob[3];
   int       my;                     /* next MB row to decode               */
} rvp8_dec;

/* One-shot: decode a complete VP8 key frame from 'data' (the raw VP8
 * bitstream, i.e. the WebP 'VP8 ' chunk payload) to a freshly malloc'd
 * RGBA8888 buffer of *ow * *oh pixels. Returns NULL on malformed input.
 * The caller frees the buffer. */
uint32_t *rvp8_decode(const uint8_t *data, size_t len,
      unsigned *ow, unsigned *oh);

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

RETRO_END_DECLS

#endif
