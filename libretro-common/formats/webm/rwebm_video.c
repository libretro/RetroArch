/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (rwebm_video.c).
 * ---------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* WebM video-to-image glue: rwebm demuxer + rvp8/rvp9 decoders exposed
 * through the still-image and streaming-animation contracts that
 * image_transfer.c dispatches on (see rwebm_video.h). */

#include <stdlib.h>
#include <string.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rwebm.h>
#include <formats/rvp8.h>
#include <formats/rh264.h>
#include <formats/rh265.h>
#ifdef HAVE_RVP9
#include <formats/rvp9_hw.h>
#include <formats/rvp9_color.h>
#endif
#include <formats/image_yuv_blit.h>
#include <formats/rwebm_video.h>
#include <formats/image_blit_bands.h>
#ifdef HAVE_RAV1
#include <formats/rav1_hw_stream.h>
#include <features/features_cpu.h>
#endif

/* Per-packet timestamps are pre-scanned at open so every frame's display
 * duration is known without lookahead decoding; cap the table so a
 * pathological stream cannot balloon it. Software playback looks ahead
 * through a separate bounded cursor past this prefix. Hardware uses
 * the same cursor before submitting the held packet. */
#ifndef RWEBM_VIDEO_MAX_TS
#define RWEBM_VIDEO_MAX_TS       8192
#endif

/* A VP9 superframe carries at most 8 sub-frames. */
#define RWEBM_VIDEO_MAX_SUPER    8
#define RWEBM_VIDEO_MAX_REORDER 64

#ifdef HAVE_RAV1
typedef struct rwebm_av1_hdr_point
{
   rav1_hdr_metadata_t hdr;
   size_t offset;
} rwebm_av1_hdr_point_t;
#endif

struct rwebm_video_stream
{
   rwebm_t     *timing;
   const uint8_t *buf;
   size_t len, avail;
   rwebm_packet lookahead_packet;
   size_t lookahead_floor;
   int completed;
   int lookahead_ready;
   rwebm_packet soft_packet;
   size_t soft_floor;
   int soft_pending, timing_active, timing_ready, timing_duration, timing_eof;
   rwebm_t     *demux;
   rvp8_video  *vp8;
   rh264_video *h264;
   rh265_video *h265;
   rh265_output_timing_t h265_timing;
   int64_t h26x_timestamp[RWEBM_VIDEO_MAX_REORDER];
   int64_t h26x_span[RWEBM_VIDEO_MAX_REORDER];
   unsigned h26x_count;
   unsigned h26x_remainder;
   int h26x_duration, h26x_ready;
   int h26x_picture_pending, h26x_consumed;
   int h26x_timeline_complete;
   unsigned h265_scan_packets, h265_scan_count, h265_scan_first;
   unsigned h265_scan_pending[RWEBM_VIDEO_MAX_REORDER];
   unsigned h26x_probe_count;
   int64_t h26x_picture_span, h26x_next_timestamp;
#ifdef HAVE_RVP9
   rvp9_dec    *vp9;
   rvp9_hw_session_t *hw;
   rvp9_hw_backend_t hw_backend;
   size_t super_sizes[RWEBM_VIDEO_MAX_SUPER], super_offset;
   int super_count, super_index, last_show, submitted, hw_restart;
#endif
#ifdef HAVE_RAV1
   rav1_hw_stream_t *av1;
   int64_t av1_display_timestamp;
   int av1_display_duration, av1_display_recorded;
   uint8_t *av1_sequence_bytes;
   size_t av1_sequence_size;
   rav1_sequence_t av1_scan_sequence, av1_initial_sequence;
   int av1_initial_sequence_set;
   rav1_hdr_metadata_t av1_hdr_initial, av1_hdr_seed;
   rwebm_t *av1_hdr_cursor;
   rwebm_av1_hdr_point_t *av1_hdr_points;
   rav1_hdr_metadata_t av1_hdr_work;
   rwebm_packet av1_hdr_packet;
   size_t av1_hdr_count, av1_hdr_capacity, av1_hdr_offset, av1_hdr_packets;
   size_t av1_hdr_done, av1_hdr_target, av1_hdr_floor;
   int av1_hdr_active, av1_hdr_pending, av1_hdr_yield;
   int av1_hdr_initial_set, av1_hdr_initial_error;
#endif
#ifdef HAVE_RVP9
   rvp9_hdr hw_initial_header;
   int hw_initial_header_valid;
#endif
   uint32_t hw_marker;
   video_hw_frame_t hw_frame;
   size_t pending_floor;
   rwebm_packet pending_packet;
   int pending, pending_idx, pending_started;
   int hw_want, hw_untaken, hw_skip;
   unsigned hw_yuv_mask, hw_yuv;
   video_color_metadata_t hw_color;
   int seek_active, seek_ready, seek_key, seek_scan;
   int seek_index_pending, seek_index_verified, seek_scan_from_cue, seek_scan_shown;
   size_t seek_index_lo, seek_index_hi;
   int64_t seek_target;
   int64_t movie_seek_target_ns;
   size_t seek_floor;
   rwebm_packet seek_packet;
   int waiting;
   rwebm_video_hdr_tone_t tone;
   uint32_t    *frame;      /* width * height ABGR words              */
   uint32_t    *out;        /* caller's frame to blit into instead of
                               'frame'; NULL for the stream's own    */
   void        *blit_pool;  /* tpool_t the blit's row bands run on   */
   unsigned     out_stride; /* caller row stride in pixels, 0: width */
   unsigned     blit_bands; /* how many; <= 1 blits on this thread   */
   int64_t     *ts;         /* pre-scanned packet timestamps (ns)     */
   int          endpoint_known, have_last_shown;
   int64_t      last_shown_ts, first_shown_ts;
   int64_t      frame_time_ns;
   int64_t      decode_time_ns;
   int          have_frame_time;
   int          have_decode_time;
   int          ts_count;   /* entries stored in ts                   */
   int          num_frames; /* total video packets in the stream      */
   int          pkt_idx;    /* ordinal of the next video packet       */
   int          track;      /* index of the chosen video track        */
   enum rwebm_codec codec;
   unsigned     width;
   unsigned     height;
   int          want10;     /* caller requested 10-bit output         */
   int          is10;       /* last decoded frame written as 10-bit    */
   int          want_fp16;  /* caller takes linear scRGB half floats
                               for an HDR source, into 'out'         */
   int          is_fp16;    /* last decoded frame written so          */
   int          emit_argb;  /* emit ARGB words instead of the default
                               R,G,B,A memory order (8-bit paths)     */
};

/* Still-image decode progress across sliced process calls. */
enum
{
   RWEBM_VIDEO_STILL_IDLE = 0,   /* nothing in progress (or done)      */
   RWEBM_VIDEO_STILL_SCAN,       /* stream open, pre-scan running      */
   RWEBM_VIDEO_STILL_DECODE      /* decoding to the first shown frame  */
};

/* Container packets walked per pre-scan slice of the sliced still
 * path: cheap header parses, sized so a slice stays well under a
 * display frame even on weak hardware while the full bounded scan
 * still completes in a handful of slices. */
#define RWEBM_VIDEO_SCAN_SLICE   1024

struct rwebm_video
{
   const uint8_t *buf;
   size_t         len;
   /* The stream opened for the still frame, kept so a caller that wants
    * to continue the video as an animation can detach it instead of
    * re-opening (and re-pre-scanning) the file.  Owned by this handle
    * until rwebm_video_detach_stream; closed by rwebm_video_free
    * otherwise.  Borrows 'buf', which must outlive it either way.
    * While still_stage is not IDLE this holds the partially-opened
    * stream the sliced still decode is building. */
   rwebm_video_stream_t *stream;
   int            still_stage; /* enum above */
   int            want10;     /* caller requested 10-bit thumbnail output */
   int            last_10bit; /* last processed frame was XRGB2101010 */
   int            want_fp16;  /* caller takes half floats for HDR      */
   int            last_fp16;  /* last processed frame was half floats  */
   /* An HDR still's frame: width * height 8-byte pixels the stream
    * decodes into from the decode stage on, handed out as the still -
    * the canvas and its copy are skipped. NULL otherwise. */
   uint16_t      *still_fp16;
   /* Bytes of 'buf' actually read so far, for decoding a still from a
    * file whose read is in progress: 0 means fully resident (the
    * default), and the still decode returns IMAGE_PROCESS_WAIT
    * instead of erroring at the wall.  Raised by
    * rwebm_video_set_avail. */
   size_t         avail;
   int            partial;    /* set_avail was called: 'avail' is live
                                 (0 is a valid value - nothing read
                                 yet - so it cannot double as the
                                 unset sentinel) */
};

/* ------------------------------------------------------------------ */
/* 8-bit limited-range YCbCr coefficient sets, <<8: {re, gd, ge, bd}.  */
/* Untagged content defaults to BT.601 below 720 lines and BT.709 at   */
/* or above it, matching industry convention.                          */
/* ------------------------------------------------------------------ */
static const int16_t rwebm_video_coef_601[4]  = { 409, 100, 208, 516 };
static const int16_t rwebm_video_coef_709[4]  = { 459,  55, 136, 541 };
static const int16_t rwebm_video_coef_2020[4] = { 431,  48, 167, 548 };

static const int16_t *rwebm_video_coefs(unsigned matrix, unsigned height)
{
   switch (matrix)
   {
      case 1:            return rwebm_video_coef_709;
      case 5: case 6:    return rwebm_video_coef_601;
      case 9: case 10:   return rwebm_video_coef_2020;
      default:           return height >= 720
                            ? rwebm_video_coef_709 : rwebm_video_coef_601;
   }
}

/* ------------------------------------------------------------------ */
/* BT.601 limited-range I420 -> ABGR words (memory R,G,B,A on LE),     */
/* the packing the animated-WebP stream emits.                         */
/* ------------------------------------------------------------------ */
static INLINE uint32_t rwebm_video_yuv_range_px(int y, int u, int v,
      const int16_t *k, int argb, int full)
{
   int c = (full ? 256 : 298) * (y - (full ? 0 : 16));
   int d = u - 128;
   int e = v - 128;
   int r = (c + k[0] * e + 128) >> 8;
   int g = (c - k[1] * d - k[2] * e + 128) >> 8;
   int b = (c + k[3] * d + 128) >> 8;
   if (r < 0)
      r = 0;
   else if (r > 255)
      r = 255;
   if (g < 0)
      g = 0;
   else if (g > 255)
      g = 255;
   if (b < 0)
      b = 0;
   else if (b > 255)
      b = 255;
   if (argb)
      return 0xFF000000u
           | ((uint32_t)r << 16)
           | ((uint32_t)g << 8)
           |  (uint32_t)b;
   return 0xFF000000u
        | ((uint32_t)b << 16)
        | ((uint32_t)g << 8)
        |  (uint32_t)r;
}

static INLINE uint32_t rwebm_video_yuv_px(int y, int u, int v,
      const int16_t *k, int argb)
{
   return rwebm_video_yuv_range_px(y, u, v, k, argb, 0);
}

#if defined(__SSE2__)
/* 8 pixels per iteration with pmaddwd pairs. Bit-exact with the scalar
 * path: pmaddwd/paddd/psrad reproduce the integer arithmetic (psrad is
 * an arithmetic shift, as the scalar's >> is on int), and the
 * packs/packus saturation chain is exactly the scalar's clamp - the
 * pre-clamp channel range (about -223..481 for 8-bit input) fits int16
 * without distortion. */
static void rwebm_video_yuv_row_sse2(uint32_t *dr,
      const uint8_t *yr, const uint8_t *ur, const uint8_t *vr, unsigned w, const int16_t *k, int argb)
{
   const __m128i k16   = _mm_set1_epi16(16);
   const __m128i k128  = _mm_set1_epi16(128);
   const __m128i zero  = _mm_setzero_si128();
   const __m128i ones  = _mm_set1_epi16(1);
   const __m128i a255  = _mm_set1_epi8((char)0xFF);
   /* Packs two int16 coefficients into the int32 lane pmaddwd expects,
    * without shifting a negative value (all arithmetic unsigned). */
#define RWEBM_PAIR16(hi, lo) \
   ((int32_t)(((uint32_t)(uint16_t)(int16_t)(hi) << 16) \
            |  (uint32_t)(uint16_t)(int16_t)(lo)))
   const __m128i c_r   = _mm_set1_epi32(RWEBM_PAIR16( k[0], 298));
   const __m128i c_g1  = _mm_set1_epi32(RWEBM_PAIR16(-k[1], 298));
   const __m128i c_g2  = _mm_set1_epi32(RWEBM_PAIR16( 128, -k[2]));
   const __m128i c_b   = _mm_set1_epi32(RWEBM_PAIR16( k[3], 298));
#undef RWEBM_PAIR16
   const __m128i rnd   = _mm_set1_epi32(128);
   unsigned i;

   for (i = 0; i + 8 <= w; i += 8)
   {
      int32_t utmp, vtmp;
      __m128i y8, ysub, u4, v4, d, e;
      __m128i ye_lo, ye_hi, yd_lo, yd_hi, e1_lo, e1_hi;
      __m128i r_lo, r_hi, g_lo, g_hi, b_lo, b_hi;
      __m128i r16, g16, b16, r8, g8, b8, rg, ba;

      /* ysub: 8 x i16 = y - 16 */
      y8   = _mm_loadl_epi64((const __m128i*)(yr + i));
      ysub = _mm_sub_epi16(_mm_unpacklo_epi8(y8, zero), k16);
      /* d/e: 4 chroma samples each duplicated to 8 x i16, minus 128
       * (memcpy avoids an unaligned int load) */
      memcpy(&utmp, ur + (i >> 1), sizeof(utmp));
      memcpy(&vtmp, vr + (i >> 1), sizeof(vtmp));
      u4 = _mm_cvtsi32_si128(utmp);
      v4 = _mm_cvtsi32_si128(vtmp);
      d  = _mm_sub_epi16(
            _mm_unpacklo_epi8(_mm_unpacklo_epi8(u4, u4), zero), k128);
      e  = _mm_sub_epi16(
            _mm_unpacklo_epi8(_mm_unpacklo_epi8(v4, v4), zero), k128);

      ye_lo = _mm_unpacklo_epi16(ysub, e);
      ye_hi = _mm_unpackhi_epi16(ysub, e);
      yd_lo = _mm_unpacklo_epi16(ysub, d);
      yd_hi = _mm_unpackhi_epi16(ysub, d);
      e1_lo = _mm_unpacklo_epi16(e, ones);
      e1_hi = _mm_unpackhi_epi16(e, ones);

      /* r = (298*ysub + 409*e + 128) >> 8 */
      r_lo = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(ye_lo, c_r), rnd), 8);
      r_hi = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(ye_hi, c_r), rnd), 8);
      /* g = (298*ysub - 100*d - 208*e + 128) >> 8
       *   = (madd(ysub,d; 298,-100) + madd(e,1; -208,128)) >> 8 */
      g_lo = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_lo, c_g1), _mm_madd_epi16(e1_lo, c_g2)), 8);
      g_hi = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_hi, c_g1), _mm_madd_epi16(e1_hi, c_g2)), 8);
      /* b = (298*ysub + 516*d + 128) >> 8 */
      b_lo = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_lo, c_b), rnd), 8);
      b_hi = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_hi, c_b), rnd), 8);

      /* Saturating packs implement the 0..255 clamp */
      r16 = _mm_packs_epi32(r_lo, r_hi);
      g16 = _mm_packs_epi32(g_lo, g_hi);
      b16 = _mm_packs_epi32(b_lo, b_hi);
      r8  = _mm_packus_epi16(r16, r16);
      g8  = _mm_packus_epi16(g16, g16);
      b8  = _mm_packus_epi16(b16, b16);

      /* Interleave to memory order R,G,B,A (ABGR words), or B,G,R,A
       * (ARGB words) when the caller asked for ARGB: the swap costs
       * only operand selection, the arithmetic is shared. */
      rg  = _mm_unpacklo_epi8(argb ? b8 : r8, g8);
      ba  = _mm_unpacklo_epi8(argb ? r8 : b8, a255);
      _mm_storeu_si128((__m128i*)(dr + i),
            _mm_unpacklo_epi16(rg, ba));
      _mm_storeu_si128((__m128i*)(dr + i + 4),
            _mm_unpackhi_epi16(rg, ba));
   }
   for (; i < w; i++)
      dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], k, argb);
}
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
/* NEON translation of the SSE2 kernel above: identical integer
 * arithmetic (widening multiply-accumulate into i32, arithmetic shift,
 * saturating narrows for the clamp), so results are byte-identical to
 * the scalar path. */
static void rwebm_video_yuv_row_neon(uint32_t *dr,
      const uint8_t *yr, const uint8_t *ur, const uint8_t *vr, unsigned w,
      const int16_t *kc, int argb)
{
   const int16x8_t k16  = vdupq_n_s16(16);
   const int16x8_t k128 = vdupq_n_s16(128);
   const int32x4_t rnd  = vdupq_n_s32(128);
   unsigned i;

   for (i = 0; i + 8 <= w; i += 8)
   {
      uint8x8_t y8, u8, v8;
      int16x8_t ysub, d, e;
      int32x4_t c_lo, c_hi, r_lo, r_hi, g_lo, g_hi, b_lo, b_hi;
      int16x8_t r16, g16, b16;
      uint8x8x4_t out;
      uint8_t utmp[8], vtmp[8];
      unsigned k;

      y8   = vld1_u8(yr + i);
      ysub = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(y8)), k16);
      for (k = 0; k < 4; k++)
      {
         utmp[2*k] = utmp[2*k+1] = ur[(i >> 1) + k];
         vtmp[2*k] = vtmp[2*k+1] = vr[(i >> 1) + k];
      }
      u8 = vld1_u8(utmp);
      v8 = vld1_u8(vtmp);
      d  = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(u8)), k128);
      e  = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(v8)), k128);

      /* c = 298*ysub + 128 (rounding folded in) */
      c_lo = vmlal_n_s16(rnd, vget_low_s16(ysub),  298);
      c_hi = vmlal_n_s16(rnd, vget_high_s16(ysub), 298);

      r_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(e),  kc[0]), 8);
      r_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(e), kc[0]), 8);
      g_lo = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_lo,
               vget_low_s16(d), kc[1]), vget_low_s16(e), kc[2]), 8);
      g_hi = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_hi,
               vget_high_s16(d), kc[1]), vget_high_s16(e), kc[2]), 8);
      b_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(d),  kc[3]), 8);
      b_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(d), kc[3]), 8);

      /* Saturating narrows implement the 0..255 clamp */
      r16 = vcombine_s16(vqmovn_s32(r_lo), vqmovn_s32(r_hi));
      g16 = vcombine_s16(vqmovn_s32(g_lo), vqmovn_s32(g_hi));
      b16 = vcombine_s16(vqmovn_s32(b_lo), vqmovn_s32(b_hi));

      /* R,G,B,A memory order, or B,G,R,A for ARGB words. */
      out.val[0] = vqmovun_s16(argb ? b16 : r16);
      out.val[1] = vqmovun_s16(g16);
      out.val[2] = vqmovun_s16(argb ? r16 : b16);
      out.val[3] = vdup_n_u8(0xFF);
      vst4_u8((uint8_t*)(dr + i), out);
   }
   for (; i < w; i++)
      dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], kc, argb);
}
#endif

static void rwebm_video_blit_i420(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h,
      const uint8_t *y, int ys,
      const uint8_t *u, const uint8_t *v, int uvs,
      unsigned matrix, int argb)
{
   const int16_t *k = rwebm_video_coefs(matrix, h);
   unsigned j;
   for (j = 0; j < h; j++)
   {
      const uint8_t *yr = y + (size_t)j * ys;
      const uint8_t *ur = u + (size_t)(j >> 1) * uvs;
      const uint8_t *vr = v + (size_t)(j >> 1) * uvs;
      uint32_t      *dr = dst + (size_t)j * dst_stride;
#if defined(__SSE2__)
      rwebm_video_yuv_row_sse2(dr, yr, ur, vr, w, k, argb);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
      rwebm_video_yuv_row_neon(dr, yr, ur, vr, w, k, argb);
#else
      {
         unsigned i;
         for (i = 0; i < w; i++)
            dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], k, argb);
      }
#endif
   }
}



/* ------------------------------------------------------------------ */
/* VP9 superframe index (parsed from the trailing marker byte).        */
/* Returns the number of sub-frames and their sizes; 1 = whole chunk.  */
/* ------------------------------------------------------------------ */
#ifdef HAVE_RVP9
static int rwebm_video_vp9_superframe(const uint8_t *data, size_t size,
      size_t *sizes, int max_frames)
{
   uint8_t marker;
   int frames, bytes_per_size, i, k;
   size_t index_size, total;
   const uint8_t *x;

   if (size < 2)
      goto whole;
   marker = data[size - 1];
   if ((marker & 0xe0) != 0xc0)
      goto whole;
   frames         = (marker & 0x7) + 1;
   bytes_per_size = ((marker >> 3) & 0x3) + 1;
   index_size     = 2 + (size_t)frames * bytes_per_size;
   if (size < index_size || data[size - index_size] != marker)
      goto whole;
   if (frames > max_frames)
      goto whole;
   x     = data + size - index_size + 1;
   total = 0;
   for (i = 0; i < frames; i++)
   {
      size_t sz = 0;
      for (k = 0; k < bytes_per_size; k++)
         sz |= (size_t)*x++ << (k * 8);
      sizes[i] = sz;
      total   += sz;
   }
   if (total > size - index_size)
      goto whole;
   return frames;

whole:
   sizes[0] = size;
   return 1;
}
#endif

/* ------------------------------------------------------------------ */
/* Decoder lifecycle                                                   */
/* ------------------------------------------------------------------ */
/* 2 marks a random-access picture; 1 is another coded picture. */
static int rwebm_video_h26x_vcl(const rwebm_video_stream_t *s,
      const rwebm_packet *packet, int *shown, rh265_slice_info_t *info)
{
   const rwebm_track *track = rwebm_get_track(s->demux, s->track);
   size_t offset = 0;
   unsigned width, i;
   if (shown) *shown = 0;
   if (!track || !track->codec_private) return -1;
   i = s->codec == RWEBM_CODEC_H264 ? 4 : 21;
   if (track->codec_private_size <= i || track->codec_private[0] != 1)
      return -1;
   width = 1 + (track->codec_private[i] & 3);
   while (offset < packet->size)
   {
      size_t length = 0;
      unsigned type;
      if (packet->size - offset < width) return -1;
      for (i = 0; i < width; i++)
         length = (length << 8) | packet->data[offset++];
      if (!length || length > packet->size - offset) return -1;
      if (s->codec == RWEBM_CODEC_H264)
      {
         type = packet->data[offset] & 31;
         if (type >= 1 && type <= 5)
         {
            if (shown) *shown = 1;
            return type == 5 ? 2 : 1;
         }
      }
      else
      {
         if (length < 2) return -1;
         type = (packet->data[offset] >> 1) & 63;
         if (type <= 31)
         {
            if (shown || info)
            {
               rh265_slice_info_t slice;
               if (rh265_video_get_slice(s->h265, packet->data + offset,
                        length, &slice) < 0)
                  return -1;
               if (shown) *shown = slice.pic_output_flag;
               if (info) *info = slice;
            }
            return type >= 16 && type <= 21 ? 2 : 1;
         }
      }
      offset += length;
   }
   return 0;
}

static void rwebm_video_h26x_color(rwebm_video_stream_t *s, const rwebm_track *t)
{
   unsigned pps;
   rh265_video_color_t color;
   rh264_video_color_t avc;
   memset(&s->hw_color, 0, sizeof(s->hw_color));
   if (!t) return;
   s->hw_color.primaries = t->primaries;
   s->hw_color.transfer = t->transfer_characteristics;
   s->hw_color.matrix = t->matrix_coefficients;
   s->hw_color.range = t->colour_range;
   s->hw_color.mastering = t->mastering;
   s->hw_color.max_cll = t->max_cll;
   s->hw_color.max_fall = t->max_fall;
   if (s->h264)
   {
      if (rh264_video_get_color(s->h264, &avc)) return;
      color.primaries = avc.primaries;
      color.transfer = avc.transfer;
      color.matrix = avc.matrix;
      color.full_range = avc.full_range;
      color.video_signal_present = avc.video_signal_present;
      color.description_present = avc.description_present;
   }
   else
   {
      if (!s->h265) return;
      for (pps = 0; pps < 64; pps++)
         if (!rh265_video_get_color(s->h265, pps, &color)) break;
      if (pps == 64) return;
   }
   if (color.description_present)
   {
      if (!t->primaries || t->primaries == 2)
         s->hw_color.primaries = color.primaries;
      if (!t->transfer_characteristics || t->transfer_characteristics == 2)
         s->hw_color.transfer = color.transfer;
      if (!t->matrix_coefficients || t->matrix_coefficients == 2)
         s->hw_color.matrix = color.matrix == 2 ? 0 : color.matrix;
   }
   if (!t->colour_range && color.video_signal_present)
      s->hw_color.range = color.full_range ? 2 : 1;
}

static bool rwebm_video_stream_open_decoder(rwebm_video_stream_t *s)
{
   switch (s->codec)
   {
      case RWEBM_CODEC_H264:
      {
         const rwebm_track *t = rwebm_get_track(s->demux, s->track);
         if (!(s->h264 = rh264_video_open()))
            return false;
         if (!t || !t->codec_private_size
               || rh264_video_set_extradata(s->h264, t->codec_private,
                  t->codec_private_size) < 0)
            return false;
         if (s->blit_pool && s->blit_bands > 1)
            rh264_video_set_thread_pool(s->h264, s->blit_pool,
                  (int)s->blit_bands);
         rwebm_video_h26x_color(s, t);
         if (s->hw_want) rh264_video_set_hw(s->h264, 1);
         return true;
      }
      case RWEBM_CODEC_H265:
      {
         const rwebm_track *t = rwebm_get_track(s->demux, s->track);
         if (!(s->h265 = rh265_video_open()))
            return false;
         if (s->hw_want && rh265_video_retain_parameter_sets(s->h265) < 0)
            return false;
         if (!t || !t->codec_private_size
               || rh265_video_set_extradata(s->h265, t->codec_private,
                  t->codec_private_size) < 0)
            return false;
         rh265_video_set_thread_pool(s->h265, s->blit_pool, s->blit_bands);
         rwebm_video_h26x_color(s, t);
         if (s->hw_want) rh265_video_set_hw(s->h265, 1);
         return true;
      }
#ifdef HAVE_RAV1
      case RWEBM_CODEC_AV1:
         return true;
#endif
      case RWEBM_CODEC_VP8:
         if (!(s->vp8 = rvp8_video_open()))
            return false;
         return true;
#ifdef HAVE_RVP9
      case RWEBM_CODEC_VP9:
         if (!(s->vp9 = (rvp9_dec*)calloc(1, sizeof(*s->vp9))))
            return false;
         rvp9_set_tile_pool(s->vp9, s->blit_pool, s->blit_bands);
         return true;
#endif
      default:
         break;
   }
   return false;
}

static void rwebm_video_stream_close_decoder(rwebm_video_stream_t *s)
{
   s->soft_pending = s->timing_active = s->timing_ready = s->lookahead_ready = 0;
   if (s->hw_untaken) video_hw_frame_abandon(&s->hw_frame);
   s->hw_untaken = 0;
   memset(&s->hw_frame, 0, sizeof(s->hw_frame));
   rh264_video_close(s->h264);
   s->h264 = NULL;
   rh265_video_close(s->h265);
   s->h265 = NULL;
   s->h26x_count = 0;
   s->h26x_remainder = 0;
   s->h26x_ready = 0;
   s->h26x_picture_pending = 0;
   if (s->vp8)
   {
      rvp8_video_close(s->vp8);
      s->vp8 = NULL;
   }
   s->pending = s->waiting = 0;
#ifdef HAVE_RAV1
   s->av1_display_recorded = 0;
   if (s->av1)
   {
      rav1_hw_stream_close(s->av1);
      free(s->av1);
      s->av1 = NULL;
   }
#endif
#ifdef HAVE_RVP9
   s->hw_restart = 0;
   if (s->hw)
   {
      rvp9_hw_session_cancel(s->hw);
      s->hw_backend.close(&s->hw->ops);
      free(s->hw); s->hw = NULL;
   }
   if (s->vp9)
   {
      rvp9_free(s->vp9);
      free(s->vp9);
      s->vp9 = NULL;
   }
#endif
}

/* ------------------------------------------------------------------ */
/* Streaming animation                                                 */
/* ------------------------------------------------------------------ */

/* Staged open, so a caller that must not stall (the sliced still-image
 * path) can spread the work over several calls:
 *   begin      demuxer + track selection + timestamp-table allocation
 *   scan_step  up to max_packets of the bounded pre-scan
 *   finish     validate, rewind, decoder + frame-canvas allocation
 * rwebm_video_stream_open composes all three for callers that want the
 * one-shot behaviour. */

static rwebm_video_stream_t *rwebm_video_stream_open_begin(
      const uint8_t *buf, size_t len, size_t avail, int *need_more,
      int allow_hw)
{
   rwebm_video_stream_t *s;
   const rwebm_track *trk = NULL;
   int i, num_tracks;

   if (need_more)
      *need_more = 0;
   if (!buf || !len)
      return NULL;

   if (!(s = (rwebm_video_stream_t*)calloc(1, sizeof(*s))))
      return NULL;
   s->buf = buf; s->len = len; s->avail = avail > len ? len : avail;

   if (!(s->demux = rwebm_open_memory_avail(buf, len, avail, need_more)))
      goto fail;

   /* Pick the first video track whose codec we can decode. */
   s->track   = -1;
   num_tracks = rwebm_num_tracks(s->demux);
   for (i = 0; i < num_tracks; i++)
   {
      const rwebm_track *t = rwebm_get_track(s->demux, i);
      if (!t || t->type != RWEBM_TRACK_VIDEO)
         continue;
      if (t->width < 1 || t->height < 1)
         continue;
      if (t->codec != RWEBM_CODEC_VP8
          && t->codec != RWEBM_CODEC_H264 && t->codec != RWEBM_CODEC_H265
#ifdef HAVE_RVP9
          && t->codec != RWEBM_CODEC_VP9
#endif
#ifdef HAVE_RAV1
          && !(allow_hw && t->codec == RWEBM_CODEC_AV1)
#endif
         )
         continue;
      s->track  = i;
      s->codec  = t->codec;
      trk       = t;
      break;
   }
   if (s->track < 0)
      goto fail;

   (void)allow_hw;
   s->width  = trk->width;
   s->height = trk->height;

   if (!(s->ts = (int64_t*)malloc(RWEBM_VIDEO_MAX_TS * sizeof(int64_t))))
      goto fail;
   if (!(s->timing = rwebm_cursor_clone(s->demux))) goto fail;
   if ((s->codec == RWEBM_CODEC_H264 || s->codec == RWEBM_CODEC_H265)
         && !rwebm_video_stream_open_decoder(s))
      goto fail;

   return s;

fail:
   rwebm_video_stream_close(s);
   return NULL;
}

static int rwebm_video_packet_shown(rwebm_video_stream_t *s,
      const rwebm_packet *packet);

static int rwebm_video_scan_h265(rwebm_video_stream_t *s,
      const rwebm_packet *packet)
{
   rh265_slice_info_t slice;
   rh265_sequence_info_t sequence;
   unsigned i, first, depth;
   int shown, vcl = rwebm_video_h26x_vcl(s, packet, &shown, &slice);
   if (vcl <= 0) return vcl;
   if (rh265_video_get_sequence(s->h265, (unsigned)slice.pps_id, &sequence))
      return -1;
   depth = sequence.max_num_reorder_pics[sequence.max_sub_layers_minus1];
   if (depth >= RWEBM_VIDEO_MAX_REORDER) return -1;
   if ((slice.nal_type >= 16 && slice.nal_type <= 20)
         || (slice.nal_type == 21 && !s->h265_scan_first))
   {
      while (s->h265_scan_count && slice.no_output_of_prior_pics)
      {
         unsigned index;
         first = 0;
         for (i = 1; i < s->h265_scan_count; i++)
            if (s->h265_scan_pending[i] > s->h265_scan_pending[first]) first = i;
         index = s->h265_scan_pending[first];
         memmove(s->ts + index, s->ts + index + 1,
               (size_t)(s->ts_count - (int)index - 1) * sizeof(*s->ts));
         s->ts_count--;
         s->num_frames--;
         s->h265_scan_pending[first] = s->h265_scan_pending[--s->h265_scan_count];
      }
      s->h265_scan_count = 0;
      s->have_last_shown = s->ts_count != 0;
      if (s->ts_count)
      {
         s->first_shown_ts = s->last_shown_ts = s->ts[0];
         for (i = 1; i < (unsigned)s->ts_count; i++)
         {
            if (s->ts[i] < s->first_shown_ts) s->first_shown_ts = s->ts[i];
            if (s->ts[i] > s->last_shown_ts) s->last_shown_ts = s->ts[i];
         }
      }
   }
   s->h265_scan_first = 1;
   if (!shown) return 0;
   if (!s->have_last_shown || packet->timestamp < s->first_shown_ts)
      s->first_shown_ts = packet->timestamp;
   if (!s->have_last_shown || packet->timestamp > s->last_shown_ts)
      s->last_shown_ts = packet->timestamp;
   s->have_last_shown = 1;
   s->h265_scan_pending[s->h265_scan_count++] = (unsigned)s->ts_count;
   s->ts[s->ts_count++] = packet->timestamp;
   s->num_frames++;
   if (s->h265_scan_count > depth)
   {
      first = 0;
      for (i = 1; i < s->h265_scan_count; i++)
         if (s->ts[s->h265_scan_pending[i]] < s->ts[s->h265_scan_pending[first]]) first = i;
      s->h265_scan_pending[first] = s->h265_scan_pending[--s->h265_scan_count];
   }
   return 1;
}

/* Pre-scan: count the track's packets and record their timestamps so
 * frame durations come straight from the container. This walks block
 * headers only (no decode), but Matroska has no sample table, so the
 * walk touches every cluster in the file.  Stop at the timestamp
 * table's capacity: frames past the cap reuse the last stored delta
 * anyway, and the only external consumer of num_frames (the >= 2
 * animation admission in gfx_thumbnail) is unaffected by the count
 * saturating.  Unbounded, this loop swept the whole buffer of a
 * long recording on the thread that opened the stream.
 *
 * Walks at most max_packets container packets (any track; <= 0 means
 * no limit) and returns 1 when the scan is complete, 0 when there is
 * more to walk. */
static int rwebm_video_stream_scan_step(rwebm_video_stream_t *s,
      int max_packets)
{
   rwebm_packet pkt;
   int walked = 0;

   while (s->num_frames < RWEBM_VIDEO_MAX_TS)
   {
      int r, shown;
      if (max_packets > 0 && walked >= max_packets)
         return 0;
      r = rwebm_read_packet(s->demux, &pkt);
      if (r == RWEBM_READ_AGAIN)
         return 2;   /* blocked at the partial-read wall; resumable */
      if (r != 1)
      {
         if (!r) s->endpoint_known = 1;
         break;
      }
      walked++;
      if (pkt.track != s->track)
         continue;
      if (s->codec == RWEBM_CODEC_H265)
      {
         if (s->h265_scan_packets == (unsigned)-1) return -1;
         s->h265_scan_packets++;
         if (rwebm_video_scan_h265(s, &pkt) < 0) return -1;
         continue;
      }
      shown = rwebm_video_packet_shown(s, &pkt);
      if ((s->codec == RWEBM_CODEC_H264 || s->codec == RWEBM_CODEC_H265) && !shown)
         continue;
      if (shown)
      {
         if (!s->have_last_shown || pkt.timestamp < s->first_shown_ts)
            s->first_shown_ts = pkt.timestamp;
         if (!s->have_last_shown || pkt.timestamp > s->last_shown_ts)
            s->last_shown_ts = pkt.timestamp;
         s->have_last_shown = 1;
      }
      s->ts[s->ts_count++] = pkt.timestamp;
      s->num_frames++;
   }
   return 1;
}

static int rwebm_video_compare_timestamp(const void *a, const void *b)
{
   int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
   return x < y ? -1 : x > y;
}

static void rwebm_video_h26x_sort(rwebm_video_stream_t *s)
{
   if ((s->codec == RWEBM_CODEC_H264 || s->codec == RWEBM_CODEC_H265)
         && s->endpoint_known && s->num_frames < RWEBM_VIDEO_MAX_TS)
   {
      qsort(s->ts, (size_t)s->ts_count, sizeof(*s->ts),
            rwebm_video_compare_timestamp);
      s->h26x_timeline_complete = 1;
   }
}

static int rwebm_video_stream_open_finish(rwebm_video_stream_t *s)
{
   if (s->num_frames < 1)
      return -1;
   rwebm_video_h26x_sort(s);
   rwebm_rewind(s->demux);

   if (!s->h264 && !s->h265 && !rwebm_video_stream_open_decoder(s))
      return -1;

   if ((size_t)s->width > SIZE_MAX / sizeof(uint32_t) / s->height)
      return -1;

   return 0;
}

rwebm_video_stream_t *rwebm_video_stream_open(const uint8_t *buf,
      size_t len)
{
   rwebm_video_stream_t *s;

   if (!(s = rwebm_video_stream_open_begin(buf, len, len, NULL, 0)))
      return NULL;
   if (rwebm_video_stream_scan_step(s, 0) < 0)
   {
      rwebm_video_stream_close(s);
      return NULL;
   }
   if (rwebm_video_stream_open_finish(s) != 0)
   {
      rwebm_video_stream_close(s);
      return NULL;
   }
   return s;
}

static rwebm_video_stream_t *rwebm_video_stream_open_avail_internal(
      const uint8_t *buf, size_t len, size_t avail, int *need_more,
      int allow_hw)
{
   rwebm_video_stream_t *s;
   int sr;

   if (need_more)
      *need_more = 0;
   if (!(s = rwebm_video_stream_open_begin(buf, len, avail, need_more, allow_hw)))
      return NULL;
   /* Same wall policy as the partial still decode: timestamps live in
    * the block headers, so the pre-scan cannot run ahead of the media
    * bytes.  Proceed once two frames are known (enough for a
    * first-frame duration) with a table truncated at the wall - the
    * duration helper degrades to its last-interval estimate past it,
    * and rwebm_video_stream_complete_scan finishes the table once the
    * file has fully arrived.  Below two frames the caller feeds more
    * bytes and retries. */
   sr = rwebm_video_stream_scan_step(s, 0);
   if (sr < 0)
   {
      rwebm_video_stream_close(s);
      return NULL;
   }
   if (sr == 2 && s->num_frames < 2)
   {
      if (need_more)
         *need_more = 1;
      rwebm_video_stream_close(s);
      return NULL;
   }
   if (rwebm_video_stream_open_finish(s) != 0)
   {
      rwebm_video_stream_close(s);
      return NULL;
   }
   return s;
}

rwebm_video_stream_t *rwebm_video_stream_open_avail(const uint8_t *buf,
      size_t len, size_t avail, int *need_more)
{
   return rwebm_video_stream_open_avail_internal(buf, len, avail, need_more, 0);
}

rwebm_video_stream_t *rwebm_video_stream_open_hw_avail(const uint8_t *buf,
      size_t len, size_t avail, int *need_more)
{
   return rwebm_video_stream_open_avail_internal(buf, len, avail, need_more, 1);
}

void rwebm_video_stream_close(rwebm_video_stream_t *s)
{
   if (!s)
      return;
   rwebm_video_stream_close_decoder(s);
#ifdef HAVE_RAV1
   rwebm_close(s->av1_hdr_cursor);
   free(s->av1_hdr_points);
   free(s->av1_sequence_bytes);
#endif
   rwebm_close(s->timing);
   rwebm_close(s->demux);
   free(s->ts);
   free(s->frame);
   free(s);
}

void rwebm_video_stream_get_info(const rwebm_video_stream_t *s,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count)
{
   if (!s)
      return;
   if (width)
      *width      = s->width;
   if (height)
      *height     = s->height;
   if (num_frames)
      *num_frames = s->num_frames;
   if (loop_count)
      *loop_count = 0;   /* video loops indefinitely */
}

void rwebm_video_stream_set_argb(rwebm_video_stream_t *s, int argb)
{
   if (s)
      s->emit_argb = argb ? 1 : 0;
}

int rwebm_video_stream_set_output_stride(rwebm_video_stream_t *s,
      uint32_t *out, unsigned stride)
{
   if (!s)
      return 0;
   if (!stride)
      stride = s->width;
   if (out && (stride < s->width
            || (s->height && (size_t)stride > SIZE_MAX / 8 / s->height)))
      return 0;
   s->out        = out;
   s->out_stride = out ? stride : 0;
   return 1;
}

void rwebm_video_stream_set_output(rwebm_video_stream_t *s, uint32_t *out)
{
   rwebm_video_stream_set_output_stride(s, out, 0);
}

void rwebm_video_stream_set_want_fp16(rwebm_video_stream_t *s, int want)
{
   if (s)
      s->want_fp16 = want ? 1 : 0;
}

int rwebm_video_stream_is_fp16(const rwebm_video_stream_t *s)
{
   return s && s->is_fp16;
}

int rwebm_video_stream_is_hdr(const rwebm_video_stream_t *s)
{
   const rwebm_track *t = (s && s->demux)
      ? rwebm_get_track(s->demux, s->track) : NULL;
   if (s && (s->h264 || s->h265))
      return s->hw_color.transfer == 16 || s->hw_color.transfer == 18;
   return t && (     t->transfer_characteristics == 16
                  || t->transfer_characteristics == 18);
}

void rwebm_video_stream_set_blit_pool(rwebm_video_stream_t *s,
      void *pool, unsigned bands)
{
   if (!s)
      return;
   s->blit_pool  = pool;
   s->blit_bands = bands;
   if (s->h264)
      rh264_video_set_thread_pool(s->h264, pool, (int)bands);
   if (s->h265)
      rh265_video_set_thread_pool(s->h265, pool, bands);
#ifdef HAVE_RVP9
   /* VP9 tile columns decode on the same pool, one per thread. */
   if (s->vp9)
      rvp9_set_tile_pool(s->vp9, pool, bands);
#endif
}

/* The blit as a row-band job (image_blit_bands): every parameter of
 * the frame's blit, with the row offset applied to the plane and
 * destination pointers per band. The matrix is resolved before the
 * split so a band's height does not pick the coefficients. */
typedef struct
{
   uint32_t *dst;
   const uint8_t *y, *u, *v;
   unsigned dst_stride, w;
   int ys, uvs;
   unsigned matrix, transfer, range, max_cll, primaries;
   const rwebm_video_hdr_tone_t *tone;
   int argb;
   int chsh, cvsh, depth;
   int kind;                /* 0: 8-bit, 1: hbd to 8-bit, 2: 10-bit,
                               3: HDR to scRGB half floats, whose
                               rows are dst_stride 8-byte pixels;
                               4: generic SDR chroma/depth/range */
} rwebm_blit_ctx_t;

static void rwebm_video_blit_generic_rows(const rwebm_blit_ctx_t *c,
      unsigned row0, unsigned rows)
{
   static const int16_t full601[4] = {359, 88, 183, 454};
   static const int16_t full709[4] = {403, 48, 120, 475};
   static const int16_t full2020[4] = {377, 42, 146, 482};
   const int16_t *k = rwebm_video_coefs(c->matrix, 0);
   unsigned j, i;
   int full = c->range == 2, shift = c->depth - 8;
   if (full)
      k = c->matrix == 1 ? full709
         : (c->matrix == 9 || c->matrix == 10 ? full2020 : full601);
   for (j = row0; j < row0 + rows; j++)
   {
      uint32_t *dst = c->dst + (size_t)j * c->dst_stride;
      size_t luma = (size_t)j * c->ys;
      size_t chroma = (size_t)(j >> c->cvsh) * c->uvs;
      for (i = 0; i < c->w; i++)
      {
         size_t at = chroma + (i >> c->chsh);
         int y, u, v;
         if (c->depth == 8)
         {
            y = c->y[luma + i]; u = c->u[at]; v = c->v[at];
         }
         else
         {
            y = ((const uint16_t*)c->y)[luma + i] >> shift;
            u = ((const uint16_t*)c->u)[at] >> shift;
            v = ((const uint16_t*)c->v)[at] >> shift;
         }
         dst[i] = rwebm_video_yuv_range_px(y, u, v, k, c->argb, full);
      }
   }
}

static void rwebm_video_blit_rows(void *arg, unsigned row0, unsigned rows)
{
   const rwebm_blit_ctx_t *c = (const rwebm_blit_ctx_t*)arg;
   uint32_t *dst             = c->dst + (size_t)row0 * c->dst_stride;
   switch (c->kind)
   {
      case 4:
         rwebm_video_blit_generic_rows(c, row0, rows);
         break;
      case 0:
         rwebm_video_blit_i420(dst, c->dst_stride, c->w, rows,
               c->y + (size_t)row0 * c->ys, c->ys,
               c->u + (size_t)(row0 >> 1) * c->uvs,
               c->v + (size_t)(row0 >> 1) * c->uvs, c->uvs,
               c->matrix, c->argb);
         break;
      case 1:
         rwebm_video_blit_i420_hbd_tone(dst, c->dst_stride, c->w, rows,
               (const uint16_t*)c->y + (size_t)row0 * c->ys, c->ys,
               (const uint16_t*)c->u + (size_t)(row0 >> 1) * c->uvs,
               (const uint16_t*)c->v + (size_t)(row0 >> 1) * c->uvs, c->uvs,
               c->matrix, c->transfer, c->range, c->max_cll, c->primaries, c->argb, c->tone);
         break;
      case 2:
         rwebm_video_blit_i420_10bit_tone(dst, c->dst_stride, c->w, rows,
               (const uint16_t*)c->y + (size_t)row0 * c->ys, c->ys,
               (const uint16_t*)c->u + (size_t)(row0 >> 1) * c->uvs,
               (const uint16_t*)c->v + (size_t)(row0 >> 1) * c->uvs, c->uvs,
               c->matrix, c->transfer, c->range, c->max_cll, c->primaries, c->tone);
         break;
      default:
         rwebm_video_blit_i420_fp16_color(
               (uint16_t*)c->dst + (size_t)row0 * c->dst_stride * 4,
               c->dst_stride, c->w, rows,
               (const uint16_t*)c->y + (size_t)row0 * c->ys, c->ys,
               (const uint16_t*)c->u + (size_t)(row0 >> 1) * c->uvs,
               (const uint16_t*)c->v + (size_t)(row0 >> 1) * c->uvs, c->uvs,
               c->matrix, c->transfer, c->range, c->primaries);
         break;
   }
}

/* Decide coefficients for the full frame and prepare its stream-owned
 * immutable PQ table before dispatching chroma-aligned row bands. */
static void rwebm_video_blit_frame(rwebm_video_stream_t *s,
      rwebm_blit_ctx_t *c, unsigned h)
{
   const rwebm_track *ct = rwebm_get_track(s->demux, s->track);
   const video_color_metadata_t *color = (s->h264 || s->h265) ? &s->hw_color : NULL;
   c->primaries = color ? color->primaries : (ct ? ct->primaries : 0);
   c->max_cll = video_color_tonemap_peak_nits(c->transfer,
         color ? color->max_cll : (ct ? ct->max_cll : 0),
         color ? &color->mastering : (ct ? &ct->mastering : NULL));
   c->tone = &s->tone;
   if (c->kind > 0 && c->kind < 3 && c->transfer == 16
         && (!s->tone.ready || s->tone.peak != c->max_cll))
      rwebm_video_hdr_tone_init(&s->tone, c->max_cll);
   if ((c->kind == 0 || c->kind == 4
            || ((s->h264 || s->h265) && c->transfer != 16 && c->transfer != 18))
         && !(c->matrix == 1 || c->matrix == 5
            || c->matrix == 6 || c->matrix == 9 || c->matrix == 10))
      c->matrix = h >= 720 ? 1 : 5;
   image_blit_bands(s->blit_pool, s->blit_bands, h, 2,
         rwebm_video_blit_rows, c);
}

void rwebm_video_stream_set_avail(rwebm_video_stream_t *s, size_t avail)
{
   if (s)
   {
      rwebm_set_avail(s->demux, avail);
      rwebm_set_avail(s->timing, avail);
#ifdef HAVE_RAV1
      rwebm_set_avail(s->av1_hdr_cursor, avail);
#endif
      s->avail = avail > s->len ? s->len : avail;
   }
}

size_t rwebm_video_stream_media_floor(rwebm_video_stream_t *s)
{
   return s ? rwebm_media_floor(s->demux) : 0;
}

size_t rwebm_video_stream_consumed(rwebm_video_stream_t *s)
{
   if (!s) return 0;
#ifdef HAVE_RAV1
   if (s->av1_hdr_active)
      return s->av1_hdr_pending ? s->av1_hdr_floor
         : rwebm_consumed(s->av1_hdr_cursor);
#endif
   if (s->seek_scan) return rwebm_consumed(s->timing);
   if (s->soft_pending) return s->soft_floor;
   if (s->seek_ready) return s->seek_floor;
#if defined(HAVE_RVP9) || defined(HAVE_RAV1)
   if (s->pending) return s->pending_floor;
#endif
   if (s->lookahead_ready) return s->lookahead_floor;
   return rwebm_consumed(s->demux);
}

size_t rwebm_video_stream_feed_frontier(rwebm_video_stream_t *s)
{
   if (s->seek_index_pending)
      return s->seek_index_lo;
   if (!s) return 0;
#ifdef HAVE_RAV1
   if (s->av1_hdr_active)
      return s->av1_hdr_pending ? s->av1_hdr_floor
         : rwebm_consumed(s->av1_hdr_cursor);
#endif
   if (s->lookahead_ready) return s->lookahead_floor;
   if (s->seek_scan || s->timing_active) return rwebm_consumed(s->timing);
   return rwebm_video_stream_consumed(s);
}

int rwebm_video_stream_seek_span(rwebm_video_stream_t *s, size_t *lo, size_t *hi)
{
   if (!s || !lo || !hi || !s->seek_index_pending)
      return 0;
   *lo = s->seek_index_lo;
   *hi = s->seek_index_hi;
   return *hi > *lo;
}

void rwebm_video_stream_pin_span(rwebm_video_stream_t *s, size_t *lo, size_t *hi)
{
   const rwebm_packet *packet = NULL;
   if (lo) *lo = 0;
   if (hi) *hi = 0;
   if (!s || !lo || !hi) return;
#ifdef HAVE_RAV1
   if (s->av1_hdr_active && s->av1_hdr_pending) packet = &s->av1_hdr_packet;
   else
#endif
   if (s->soft_pending) packet = &s->soft_packet;
#if defined(HAVE_RVP9) || defined(HAVE_RAV1)
   else if (s->pending) packet = &s->pending_packet;
#endif
   else if (s->lookahead_ready) packet = &s->lookahead_packet;
   else if (s->seek_ready) packet = &s->seek_packet;
   if (packet)
   {
      *lo = (size_t)(packet->data - s->buf);
      *hi = *lo + packet->size;
   }
}

void rwebm_video_stream_read_span(rwebm_video_stream_t *s, size_t *lo, size_t *hi)
{
#ifdef HAVE_RAV1
   if (s && s->av1_hdr_active)
   {
      rwebm_read_span(s->av1_hdr_cursor, lo, hi);
      return;
   }
#endif
   rwebm_read_span(s ? ((s->seek_scan || s->timing_active)
            ? s->timing : s->demux) : NULL, lo, hi);
}

int64_t rwebm_video_stream_duration_ns(rwebm_video_stream_t *s)
{
   return (s && s->demux) ? rwebm_duration_ns(s->demux) : 0;
}

void rwebm_video_stream_complete_scan(rwebm_video_stream_t *s,
      const uint8_t *buf, size_t len)
{
   rwebm_t *d;
   rwebm_packet pkt;
   unsigned skip;
   int ret;

   if (!s || s->num_frames >= RWEBM_VIDEO_MAX_TS)
      return;
   /* A separate demuxer over the (now fully read) buffer: the live
    * one's cursor is mid-playback and cannot be borrowed.  Skip the
    * packets the original pre-scan already recorded - the walk they
    * take is identical, so the appended timestamps are exactly the
    * values an untruncated scan would have recorded, and every
    * per-frame duration becomes bit-identical to a stream opened
    * over the complete file.  If the original scan in fact ended at
    * the file's end (not the wall), the skip consumes everything and
    * nothing is appended - harmless. */
   if (!(d = rwebm_open_memory(buf, len)))
      return;
   skip = s->codec == RWEBM_CODEC_H265 ? s->h265_scan_packets : (unsigned)s->ts_count;
   while ((ret = rwebm_read_packet(d, &pkt)) == 1)
   {
      int shown = -1;
      if (pkt.track != s->track)
         continue;
      if (s->codec == RWEBM_CODEC_H265)
      {
         if (skip > 0) { skip--; continue; }
         if (s->h265_scan_packets == (unsigned)-1) { ret = -1; break; }
         s->h265_scan_packets++;
         if (rwebm_video_scan_h265(s, &pkt) < 0) { ret = -1; break; }
         if (s->num_frames >= RWEBM_VIDEO_MAX_TS) break;
         continue;
      }
      if (s->codec == RWEBM_CODEC_H264 || s->codec == RWEBM_CODEC_H265)
      {
         shown = rwebm_video_packet_shown(s, &pkt);
         if (!shown) continue;
      }
      if (skip > 0)
      {
         skip--;
         continue;
      }
      if (shown < 0) shown = rwebm_video_packet_shown(s, &pkt);
      if (shown)
      {
         if (!s->have_last_shown || pkt.timestamp < s->first_shown_ts)
            s->first_shown_ts = pkt.timestamp;
         if (!s->have_last_shown || pkt.timestamp > s->last_shown_ts)
            s->last_shown_ts = pkt.timestamp;
         s->have_last_shown = 1;
      }
      s->ts[s->ts_count++] = pkt.timestamp;
      s->num_frames++;
      if (s->num_frames >= RWEBM_VIDEO_MAX_TS)
         break;
   }
   if (!ret) s->endpoint_known = 1;
   rwebm_video_h26x_sort(s);
   rwebm_close(d);
}

/* Display duration of packet 'idx', in ms, from the pre-scanned
 * timestamp table; 0 when unknown (caller applies its default). */
static int rwebm_video_duration_ms(const rwebm_video_stream_t *s, int idx)
{
   /* Quantise against the accumulated timeline, not per delta: flooring
    * each delta independently loses the fractional millisecond every
    * frame - a 33.333 ms (30 fps) stream came out 33+33+33..., running
    * one percent fast and drifting further each loop. Differencing the
    * floored absolute times emits 33/33/34 so the sum stays within a
    * millisecond of the container's timeline. */
   int64_t t0, t1;
   if (idx + 1 < s->ts_count)
   { t0 = s->ts[idx]; t1 = s->ts[idx + 1]; }
   else if (s->ts_count >= 2)
   { t0 = s->ts[s->ts_count - 2]; t1 = s->ts[s->ts_count - 1]; }
   else
      return 0;
   if (t1 <= t0)
      return 0;
   return (int)(t1 / 1000000 - t0 / 1000000);
}

static int rwebm_video_read_packet(rwebm_video_stream_t *s,
      rwebm_packet *packet, size_t *floor)
{
   if (s->lookahead_ready)
   {
      size_t offset = (size_t)(s->lookahead_packet.data - s->buf);
      if (offset > s->avail || s->lookahead_packet.size > s->avail - offset)
         return RWEBM_READ_AGAIN;
      *packet = s->lookahead_packet;
      *floor = s->lookahead_floor;
      s->lookahead_ready = 0;
      return 1;
   }
   *floor = rwebm_consumed(s->demux);
   return rwebm_read_packet(s->demux, packet);
}

/* The prefix table is an opening optimisation, not the media timeline's
 * length. Beyond it, look ahead with a separate preallocated cursor. A
 * bounded header slice can yield without consuming or decoding the held
 * video packet. Resident-wall retries keep the same two cursor positions. */
static int rwebm_video_packet_duration(
      rwebm_video_stream_t *s, int index, int64_t timestamp, int *duration)
{
   int walked;
   if (index + 1 < s->ts_count)
   {
      s->timing_eof = 0;
      *duration = rwebm_video_duration_ms(s, index);
      return 1;
   }
   if (s->timing_ready)
   {
      *duration = s->timing_duration;
      return 1;
   }
   if (!s->timing_active)
   {
      rwebm_cursor_copy(s->timing, s->demux);
      s->timing_active = 1;
      s->timing_eof    = 0;
   }
   for (walked = 0; walked < 256; walked++)
   {
      rwebm_packet next;
      int ret = rwebm_read_packet(s->timing, &next);
      if (ret == RWEBM_READ_AGAIN)
         return 2;
      if (ret < 0)
         return -1;
      if (!ret)
      {
         rwebm_cursor_copy(s->demux, s->timing);
         *duration          = rwebm_video_duration_ms(s, index);
         s->timing_duration = *duration;
         s->timing_ready    = 1;
         s->timing_eof      = 1;
         return 1;
      }
      if (next.track == s->track)
      {
         int64_t delta = next.timestamp / 1000000 - timestamp / 1000000;
         /* Keep the found packet, not the headers between it and the
          * held picture. Playback can then move its window without ever
          * revisiting the old interleaved/padding region. */
         s->lookahead_packet = next;
         s->lookahead_floor  = (size_t)(next.data - s->buf);
         s->lookahead_ready  = 1;
         rwebm_cursor_copy(s->demux, s->timing);
         *duration          = delta > 0 && delta <= 0x7fffffff ? (int)delta : 0;
         s->timing_duration = *duration;
         s->timing_ready    = 1;
         return 1;
      }
   }
   return 2;
}

#ifdef HAVE_RAV1
static bool rwebm_video_av1_sequence(rwebm_video_stream_t *s,
      rav1_config_t *config)
{
   rwebm_t *probe;
   rwebm_packet packet;
   unsigned walked;
   size_t offset = 0;
   bool found = false;
   if (s->av1_sequence_bytes)
      return rav1_obu_next(&config->sequence, s->av1_sequence_bytes,
            s->av1_sequence_size, &offset, true) == 1;
   if (config->sequence.data)
   {
      if (config->sequence.temporal_id || config->sequence.spatial_id)
         return false;
      goto retain;
   }
   probe = rwebm_open_memory_avail(s->buf, s->len, s->avail, NULL);
   if (!probe)
      return false;
   for (walked = 0; walked < RWEBM_VIDEO_MAX_TS; walked++)
   {
      rav1_obu_t obu;
      size_t offset = 0;
      if (rwebm_read_packet(probe, &packet) != 1)
         break;
      if (packet.track != s->track)
         continue;
      while (rav1_obu_next(&obu, packet.data, packet.size, &offset, true) == 1)
      {
         if (obu.temporal_id || obu.spatial_id)
            break;
         if (obu.type == 1)
         {
            config->sequence = obu;
            found = true;
            break;
         }
         if (obu.type != 2 && obu.type != 5 && obu.type != 15)
            break;
      }
      break;
   }
   rwebm_close(probe);
   if (!found)
      return false;
retain:
   if (!config->sequence.size || config->sequence.size > 1024u * 1024u)
      return false;
   s->av1_sequence_bytes = (uint8_t*)malloc(config->sequence.size);
   if (!s->av1_sequence_bytes)
      return false;
   s->av1_sequence_size = config->sequence.size;
   memcpy(s->av1_sequence_bytes, config->sequence.data, s->av1_sequence_size);
   return rav1_obu_next(&config->sequence, s->av1_sequence_bytes,
         s->av1_sequence_size, &offset, true) == 1;
}

static bool rwebm_video_av1_retain(void *user,
      const rav1_sequence_t *sequence, const rav1_obu_t *obu)
{
   rwebm_video_stream_t *s = (rwebm_video_stream_t*)user;
   if (!rav1_hw_stream_retain_config(
            &s->av1_sequence_bytes, &s->av1_sequence_size, obu))
      return false;
   s->av1_initial_sequence = *sequence;
   s->av1_initial_sequence_set = 1;
   return true;
}

static int rwebm_video_av1_open(rwebm_video_stream_t *s)
{
   const rwebm_track *t = rwebm_get_track(s->demux, s->track);
   rav1_config_t config;
   rav1_sequence_t *seq;
   unsigned matrix, transfer, primaries;
   if (s->av1_hdr_initial_error || !t || !rav1_config_parse(&config, t->codec_private,
            t->codec_private_size) || !rwebm_video_av1_sequence(s, &config))
      return 0;
   s->av1 = (rav1_hw_stream_t*)calloc(1, sizeof(*s->av1));
   if (!s->av1)
      return 0;
   rav1_hw_stream_init(s->av1, NULL);
   s->av1->retain_sequence = rwebm_video_av1_retain;
   s->av1->sequence_user = s;
   seq = &s->av1->sequence;
   if (s->av1_initial_sequence_set) *seq = s->av1_initial_sequence;
   else if (!rav1_config_sequence(seq, &config)) goto fail;
   if (seq->max_width != s->width
         || seq->max_height != s->height || t->colour_range > 2)
      goto fail;
   if ((t->primaries && t->primaries != 2 && seq->color_primaries != 2
            && seq->color_primaries != t->primaries)
         || (t->matrix_coefficients && t->matrix_coefficients != 2
            && seq->matrix_coefficients != 2
            && seq->matrix_coefficients != t->matrix_coefficients)
         || (t->transfer_characteristics && t->transfer_characteristics != 2
            && seq->transfer_characteristics != 2
            && seq->transfer_characteristics != t->transfer_characteristics)
         || (t->colour_range && (t->colour_range == 2) != seq->color_range))
      goto fail;
   matrix = seq->matrix_coefficients != 2 ? seq->matrix_coefficients
      : (t->matrix_coefficients == 2 ? 0 : t->matrix_coefficients);
   transfer = seq->transfer_characteristics != 2 ? seq->transfer_characteristics
      : t->transfer_characteristics;
   primaries = seq->color_primaries != 2
      ? seq->color_primaries : t->primaries;
   if (transfer == 16 || transfer == 18)
   {
      if ((matrix != 0 && matrix != 1 && matrix != 9) ||
          (primaries != 0 && primaries != 1 && primaries != 2 && primaries != 9))
         goto fail;
   }
   else if (matrix != 0 && matrix != 1 && matrix != 5 && matrix != 6)
      goto fail;
   if (!rav1_hdr_metadata_config(&s->av1->hdr, &config))
      goto fail;
   /* The bounded opening scan can precede the first decoded picture. */
   s->av1_hdr_seed = s->av1->hdr;
   if (s->av1_hdr_initial_set) s->av1->hdr = s->av1_hdr_initial;
   s->hw_color.primaries = primaries;
   s->hw_color.transfer = transfer;
   s->hw_color.matrix = matrix;
   s->hw_color.range = seq->color_range ? 2 : 1;
   s->hw_color.mastering = t->mastering;
   s->hw_color.max_cll = t->max_cll;
   s->hw_color.max_fall = t->max_fall;
   s->av1->sequence_obu = config.sequence;
   s->av1->color = s->hw_color;
   if (!rav1_hdr_metadata_merge(&s->av1->color, &s->av1->hdr)
         || !rav1_hw_stream_open_sequence(s->av1))
      goto fail;
   s->hw_yuv_mask = IMAGE_YUV_FLAG_BT709 | IMAGE_YUV_FLAG_FULL_RANGE;
   s->hw_yuv = seq->color_range ? IMAGE_YUV_FLAG_FULL_RANGE : 0;
   if (matrix == 1 || (!matrix && s->height >= 720))
      s->hw_yuv |= IMAGE_YUV_FLAG_BT709;
   s->av1_initial_sequence = *seq;
   s->av1_initial_sequence_set = 1;
   s->hw_want = 1;
   return 1;
fail:
   rav1_hw_stream_close(s->av1);
   free(s->av1);
   s->av1 = NULL;
   return 0;
}

static int rwebm_video_av1_hdr_point(rwebm_video_stream_t *s, size_t offset);

static int rwebm_video_av1_learn_hdr(rwebm_video_stream_t *s)
{
   size_t end = (size_t)(s->pending_packet.data - s->buf) + s->pending_packet.size;
   if (s->pending_idx < 0 || (size_t)s->pending_idx != s->av1_hdr_packets)
      return 1;
   if (s->av1_hdr_pending && (s->av1_hdr_packet.data != s->pending_packet.data
            || s->av1_hdr_packet.size != s->pending_packet.size)) return 1;
   if (!s->av1_hdr_count)
   {
      s->av1_hdr_work = s->av1_hdr_seed;
      if (!rwebm_video_av1_hdr_point(s, 0)) return 0;
   }
   if (!s->av1_hdr_cursor)
   {
      s->av1_hdr_cursor = rwebm_cursor_clone(s->demux);
      if (!s->av1_hdr_cursor) return 0;
   }
   s->av1_hdr_work = s->av1->hdr;
   if (!rwebm_video_av1_hdr_point(s, end)) return 0;
   rwebm_cursor_copy(s->av1_hdr_cursor, s->demux);
   s->av1_hdr_done = end;
   s->av1_hdr_packets++;
   s->av1_hdr_offset = 0;
   s->av1_hdr_pending = s->lookahead_ready;
   if (s->lookahead_ready)
   {
      s->av1_hdr_packet = s->lookahead_packet;
      s->av1_hdr_floor = s->lookahead_floor;
   }
   return 1;
}

static int rwebm_video_av1_result(rwebm_video_stream_t *s,
      int ret, video_hw_frame_t *frame, int *duration_ms,
      int duration, int64_t timestamp)
{
   if (s->av1_display_recorded)
   {
      duration = s->av1_display_duration;
      timestamp = s->av1_display_timestamp;
   }
   s->av1_display_recorded = 0;
   if (ret == RAV1_HW_STREAM_FRAME)
   {
      frame->yuv = (frame->yuv & ~s->hw_yuv_mask) | s->hw_yuv;
      frame->color = s->hw_color;
      if (!rav1_hdr_metadata_merge(&frame->color, &s->av1->output_hdr))
      {
         video_hw_frame_abandon(frame);
         s->av1->error = -1;
         return -1;
      }
   }
   if (s->hw_untaken)
      video_hw_frame_abandon(&s->hw_frame);
   s->hw_untaken = ret == RAV1_HW_STREAM_FRAME;
   if (s->hw_untaken)
      s->hw_frame = *frame;
   else
      memset(&s->hw_frame, 0, sizeof(s->hw_frame));
   if (duration_ms)
      *duration_ms = duration;
   s->frame_time_ns = timestamp;
   s->have_frame_time = 1;
   return 1;
}

static int rwebm_video_av1_step(rwebm_video_stream_t *s,
      int *duration_ms, int discard)
{
   video_hw_frame_t frame;
   int ret, duration;
   if (!s->av1)
      return -1;
   if (!s->pending)
   {
      unsigned walked;
      for (walked = 0; walked < RWEBM_VIDEO_SCAN_SLICE; walked++)
      {
         size_t floor = rwebm_consumed(s->demux);
         if (s->seek_ready)
         {
            s->pending_packet = s->seek_packet;
            floor = s->seek_floor;
            s->seek_ready = 0;
            ret = 1;
         }
         else
            ret = rwebm_video_read_packet(s, &s->pending_packet, &floor);
         if (ret == RWEBM_READ_AGAIN)
         {
            if (s->av1->backend.drain && s->av1_display_recorded)
            {
               int output = rav1_hw_stream_output(s->av1, &frame, discard != 0);
               if (output < 0) return -1;
               if (output == RAV1_HW_STREAM_FRAME || output == RAV1_HW_STREAM_DISCARDED)
                  return rwebm_video_av1_result(s, output, &frame, duration_ms, 0, 0);
            }
            s->waiting = 1;
            return 2;
         }
         if (ret != 1)
         {
            if (!ret)
            {
               ret = rav1_hw_stream_finish(s->av1, &frame, discard != 0);
               if (ret == RAV1_HW_STREAM_BUSY)
               {
                  s->waiting = 1;
                  return 2;
               }
               if (ret == RAV1_HW_STREAM_FRAME || ret == RAV1_HW_STREAM_DISCARDED)
                  return rwebm_video_av1_result(s, ret, &frame, duration_ms, 0, 0);
               s->completed = ret == RAV1_HW_STREAM_END;
            }
            return -1;
         }
         if (s->pending_packet.track != s->track)
            continue;
         s->pending_floor = (size_t)(s->pending_packet.data - s->buf);
         if (floor < s->pending_floor)
            s->pending_floor = floor;
         s->pending_idx = s->pkt_idx++;
         s->pending = 1;
         s->pending_started = 0;
         s->timing_active = s->timing_ready = 0;
         break;
      }
      if (!s->pending)
         return 0;
   }
   if ((size_t)(s->pending_packet.data - s->buf) > s->avail
         || s->pending_packet.size > s->avail
            - (size_t)(s->pending_packet.data - s->buf))
   {
      s->waiting = 1;
      return 2;
   }
   ret = rwebm_video_packet_duration(s, s->pending_idx,
         s->pending_packet.timestamp, &duration);
   if (ret == 2) { s->waiting = 1; return 2; }
   if (ret < 0) return -1;
   if (!s->pending_started)
   {
      if (rav1_hw_stream_begin(s->av1, s->pending_packet.data,
               s->pending_packet.size)) return -1;
      s->pending_started = 1;
   }
   ret = rav1_hw_stream_step(s->av1, &frame, discard != 0);
   if (!s->av1_hdr_initial_set &&
       (ret == RAV1_HW_STREAM_FRAME || ret == RAV1_HW_STREAM_DISCARDED
             || ret == RAV1_HW_STREAM_PENDING))
   {
      s->av1_hdr_initial = s->av1->output_hdr;
      s->av1_hdr_initial_set = 1;
   }
   if (ret == RAV1_HW_STREAM_PENDING)
   {
      s->av1_display_recorded = 1;
      s->av1_display_timestamp = s->pending_packet.timestamp;
      s->av1_display_duration = duration;
      return 0;
   }
   if (ret == RAV1_HW_STREAM_BUSY)
   {
      s->waiting = 1;
      return 2;
   }
   if (ret == RAV1_HW_STREAM_END)
   {
      if (!rwebm_video_av1_learn_hdr(s))
      {
         s->av1->error = -1;
         return -1;
      }
      s->pending = 0;
   }
   if (ret == RAV1_HW_STREAM_PROGRESS || ret == RAV1_HW_STREAM_END)
      return 0;
   if (ret != RAV1_HW_STREAM_FRAME && ret != RAV1_HW_STREAM_DISCARDED)
      return -1;
   return rwebm_video_av1_result(s, ret, &frame, duration_ms,
         duration, s->pending_packet.timestamp);
}
#endif

int rwebm_video_stream_set_hw(rwebm_video_stream_t *s, int enable)
{
#ifdef HAVE_RVP9
   const rwebm_track *t;
   rwebm_t *probe;
   rwebm_packet packet;
   rvp9_hw_session_t *candidate;
   rvp9_hw_backend_t backend;
   rvp9_hw_ops_t ops;
   video_color_metadata_t color;
   rvp9_config_t config;
   const rvp9_config_t *stream_config = NULL;
   size_t sizes[RWEBM_VIDEO_MAX_SUPER];
   unsigned walked;
   int found = 0;
#endif
   if (s && (s->h264 || s->h265))
   {
      if (enable && !s->hw_want)
      {
         const rwebm_track *track = rwebm_get_track(s->demux, s->track);
         if (!track || track->colour_range > 2 || track->chroma_siting_horz > 1
               || track->chroma_siting_vert > 2)
            return 0;
         if (rwebm_video_stream_is_hdr(s))
         {
            if (!(s->hw_color.matrix == 0 || s->hw_color.matrix == 2
                     || s->hw_color.matrix == 1 || s->hw_color.matrix == 9)
                  || !(s->hw_color.primaries == 0 || s->hw_color.primaries == 2
                     || s->hw_color.primaries == 1 || s->hw_color.primaries == 9))
               return 0;
         }
         else if (!(s->hw_color.matrix == 0 || s->hw_color.matrix == 2
                  || s->hw_color.matrix == 1 || s->hw_color.matrix == 5
                  || s->hw_color.matrix == 6))
            return 0;
         s->hw_want = 1;
         rwebm_video_stream_rewind(s);
      }
      return s->h265 ? rh265_video_is_hw(s->h265)
         : s->h264 && rh264_video_is_hw(s->h264);
   }
#ifdef HAVE_RVP9
#ifdef HAVE_RAV1
   if (s && s->codec == RWEBM_CODEC_AV1)
      return s->av1 ? 1 : (enable ? rwebm_video_av1_open(s) : 0);
#endif
   if (!s || s->codec != RWEBM_CODEC_VP9)
      return 0;
   if (!enable || s->hw)
      return s->hw != NULL;
   t = rwebm_get_track(s->demux, s->track);
   if (!t || t->colour_range > 2 || t->chroma_siting_horz > 1
         || t->chroma_siting_vert > 2)
      return 0;
   if (rwebm_video_stream_is_hdr(s))
   {
      /* HDR importers consume the source metadata attached below. */
      if (!(t->matrix_coefficients == 0 || t->matrix_coefficients == 2
                || t->matrix_coefficients == 1 || t->matrix_coefficients == 9)
            || !(t->primaries == 0 || t->primaries == 2 || t->primaries == 1
                  || t->primaries == 9))
         return 0;
   }
   else if (!(t->matrix_coefficients == 0 || t->matrix_coefficients == 2
                  || t->matrix_coefficients == 1 || t->matrix_coefficients == 5
                  || t->matrix_coefficients == 6))
      return 0;
   candidate = (rvp9_hw_session_t *)calloc(1, sizeof(*candidate));
   if (!candidate)
      return 0;
   if (s->hw_initial_header_valid)
   {
      candidate->pending.header = s->hw_initial_header;
      found = 1;
   }
   else
   {
      /* The metadata is already owned by the demuxer. A retained header
       * lets later seeks reopen without touching released file pages. */
      probe = rwebm_cursor_clone(s->demux);
      if (!probe)
      {
         free(candidate);
         return 0;
      }
      rwebm_rewind(probe);
      for (walked = 0; walked < RWEBM_VIDEO_MAX_TS; walked++)
      {
         if (rwebm_read_packet(probe, &packet) != 1)
            break;
         if (packet.track != s->track)
            continue;
         rwebm_video_vp9_superframe(
               packet.data, packet.size, sizes, RWEBM_VIDEO_MAX_SUPER);
         found = !rvp9_parse_frame_header(
               packet.data, sizes[0], NULL, NULL, &candidate->pending.header);
         break;
      }
      rwebm_close(probe);
   }
   memset(&color, 0, sizeof(color));
   color.primaries = t->primaries;
   color.transfer  = t->transfer_characteristics;
   color.matrix    = t->matrix_coefficients;
   color.range     = t->colour_range;
   color.mastering = t->mastering;
   color.max_cll   = t->max_cll;
   color.max_fall  = t->max_fall;
   if (!found || !rvp9_color_resolve(&candidate->pending.header, &color, &color))
   {
      free(candidate);
      return 0;
   }
   if (t->chroma_siting_horz || t->chroma_siting_vert)
   {
      if (!found || color.primaries > 255 || color.transfer > 255
            || color.matrix > 255)
      {
         free(candidate);
         return 0;
      }
      memset(&config, 0, sizeof(config));
      config.profile = (uint8_t)candidate->pending.header.profile;
      config.bit_depth = (uint8_t)candidate->pending.header.bit_depth;
      config.full_range = (uint8_t)candidate->pending.header.color_range;
      /* VPCC 0 is left/half-row; 1 is top-left. Horizontal half
       * siting has no representation in either supported 4:2:0 code. */
      config.chroma = t->chroma_siting_vert == 2 ? 0 : 1;
      config.primaries = (uint8_t)(color.primaries ? color.primaries : 2);
      config.transfer = (uint8_t)(color.transfer ? color.transfer : 2);
      config.matrix = (uint8_t)(color.matrix ? color.matrix : 2);
      stream_config = &config;
   }
   if (!found
         || candidate->pending.header.frame_type
         /* The software HDR fallback currently requires ten-bit planes. */
         || (rwebm_video_stream_is_hdr(s)
               && candidate->pending.header.bit_depth != 10)
         || candidate->pending.header.w != (int)s->width
         || candidate->pending.header.h != (int)s->height
         || !rvp9_hw_backend_open_config(&candidate->pending.header,
               &color, stream_config, 10, &ops, &backend))
   {
      free(candidate);
      return 0;
   }
   s->hw_initial_header = candidate->pending.header;
   s->hw_initial_header_valid = 1;
   if (!rvp9_hw_session_init_mode(candidate, &ops, 10, backend.packet_decoder))
   {
      backend.close(&ops);
      free(candidate);
      return 0;
   }
   if (s->vp9)
   {
      rvp9_free(s->vp9);
      free(s->vp9);
      s->vp9 = NULL;
   }
   s->hw          = candidate;
   s->hw_backend  = backend;
   s->hw_want     = 1;
   s->hw_color    = color;
   s->hw_yuv_mask = s->hw_yuv = 0;
   if (t->matrix_coefficients == 1 || t->matrix_coefficients == 5
         || t->matrix_coefficients == 6)
   {
      s->hw_yuv_mask |= IMAGE_YUV_FLAG_BT709;
      if (t->matrix_coefficients == 1)
         s->hw_yuv |= IMAGE_YUV_FLAG_BT709;
   }
   if (t->colour_range)
   {
      s->hw_yuv_mask |= IMAGE_YUV_FLAG_FULL_RANGE;
      if (t->colour_range == 2)
         s->hw_yuv |= IMAGE_YUV_FLAG_FULL_RANGE;
   }
   rwebm_rewind(s->demux);
   s->pkt_idx = s->pending = s->waiting = 0;
   s->soft_pending = s->timing_active = s->timing_ready = s->lookahead_ready =
         0;
   s->seek_active = s->seek_ready = s->seek_key = s->seek_scan = 0;
   s->seek_index_pending = 0;
#ifdef HAVE_RAV1
   s->av1_hdr_active = 0;
#endif
   return 1;
#else
#ifdef HAVE_RAV1
   if (s && s->codec == RWEBM_CODEC_AV1)
      return s->av1 ? 1 : (enable ? rwebm_video_av1_open(s) : 0);
#endif
   (void)s;
   (void)enable;
   return 0;
#endif
}

int rwebm_video_stream_hw_frame(rwebm_video_stream_t *s, video_hw_frame_t *out)
{
   if (!s || !out || !s->hw_untaken || s->h26x_picture_pending) return 0;
   *out = s->hw_frame;
   s->hw_untaken = 0;
   memset(&s->hw_frame, 0, sizeof(s->hw_frame));
   return 1;
}

int rwebm_video_stream_waiting(const rwebm_video_stream_t *s)
{
   return s && s->waiting;
}

void *rwebm_video_stream_h264(rwebm_video_stream_t *s)
{
   return s ? s->h264 : NULL;
}

void *rwebm_video_stream_h265(rwebm_video_stream_t *s)
{
   return s ? s->h265 : NULL;
}

#ifdef HAVE_RVP9
static int rwebm_video_hw_step(
      rwebm_video_stream_t *s, int *duration_ms, int discard)
{
   int ret, duration;
   s->waiting = 0;
   if (s->hw_restart)
   {
      ret = s->hw_backend.restart(s->hw->ops.decoder);
      if (ret)
      {
         s->waiting = ret > 0;
         return ret > 0 ? 2 : -1;
      }
      s->hw_restart = 0;
   }
   if (s->pending && s->hw_skip != discard)
      return -1;
   if (!s->pending)
   {
      size_t offset;
      int i;
      if (s->hw_untaken)
         video_hw_frame_abandon(&s->hw_frame);
      s->hw_untaken = 0;
      memset(&s->hw_frame, 0, sizeof(s->hw_frame));
      for (;;)
      {
         size_t floor = rwebm_consumed(s->demux);
         if (s->seek_ready)
         {
            s->pending_packet = s->seek_packet;
            floor             = s->seek_floor;
            s->seek_ready     = 0;
            ret               = 1;
         }
         else
            ret = rwebm_video_read_packet(s, &s->pending_packet, &floor);
         if (ret == RWEBM_READ_AGAIN)
         {
            s->waiting = 1;
            return 2;
         }
         if (ret != 1)
         {
            if (ret < 0) return -1;
            if (s->hw_backend.drain)
            {
               ret = s->hw_backend.drain(s->hw->ops.decoder, discard != 0);
               if (ret > 0)
               {
                  s->waiting = 1;
                  return 2;
               }
               if (ret < 0) return -1;
            }
            s->completed = 1;
            return -1;
         }
         if (s->pending_packet.track != s->track)
            continue;
         s->pending_floor = (size_t)(s->pending_packet.data - s->buf);
         if (floor < s->pending_floor)
            s->pending_floor = floor;
         break;
      }
      s->super_count = rwebm_video_vp9_superframe(s->pending_packet.data,
            s->pending_packet.size, s->super_sizes, RWEBM_VIDEO_MAX_SUPER);
      s->last_show   = -1;
      for (i = 0, offset = 0; i < s->super_count; i++)
      {
         if (!s->super_sizes[i]
               || s->super_sizes[i] > s->pending_packet.size - offset)
            return -1;
         /* Profile 0/2: show-existing and show-frame are in the first byte. */
         if (s->pending_packet.data[offset] & 0x0a)
            s->last_show = i;
         offset += s->super_sizes[i];
      }
      s->pending_idx = s->pkt_idx++;
      s->hw_skip     = discard;
      s->super_index = s->submitted = 0;
      s->super_offset               = 0;
      s->pending                    = 1;
      s->timing_active = s->timing_ready = 0;
   }
   ret = rwebm_video_packet_duration(
         s, s->pending_idx, s->pending_packet.timestamp, &duration);
   if (ret == 2)
   {
      s->waiting = 1;
      return 2;
   }
   if (ret < 0)
      return -1;
   while (s->super_index < s->super_count)
   {
      if (!s->submitted)
      {
         ret = rvp9_hw_session_submit(s->hw,
               s->pending_packet.data + s->super_offset,
               s->super_sizes[s->super_index]);
         if (ret == 1)
         {
            s->waiting = 1;
            return 2;
         }
         if (ret < 0)
            return -1;
         s->submitted = 1;
      }
      if (s->hw->output_slot >= 0)
      {
         if (discard || s->super_index < s->last_show)
            rvp9_hw_session_discard_output(s->hw);
         else if (!s->hw_backend.drain)
         {
            ret = rvp9_hw_session_output(s->hw, &s->hw_frame);
            if (ret == 1)
            {
               s->waiting = 1;
               return 2;
            }
            if (ret < 0)
               return -1;
            s->hw_frame.yuv   = (s->hw_frame.yuv & ~s->hw_yuv_mask) | s->hw_yuv;
            s->hw_frame.color = s->hw_color;
            s->hw_untaken     = 1;
         }
      }
      s->super_offset += s->super_sizes[s->super_index++];
      s->submitted = 0;
   }
   if (s->hw_backend.drain)
   {
      int drained = 0;
      if (s->timing_eof)
      {
         drained = s->hw_backend.drain(s->hw->ops.decoder, discard != 0);
         if (drained < 0)
            return -1;
      }
      if (s->hw->output_slot >= 0)
      {
         ret = rvp9_hw_session_output(s->hw, &s->hw_frame);
         if (ret > 0)
         {
            s->waiting = 1;
            return 2;
         }
         if (ret < 0)
            return -1;
         s->hw_frame.yuv   = (s->hw_frame.yuv & ~s->hw_yuv_mask) | s->hw_yuv;
         s->hw_frame.color = s->hw_color;
         s->hw_untaken     = 1;
      }
      else if (drained > 0)
      {
         s->waiting = 1;
         return 2;
      }
   }
   s->pending = 0;
   if (duration_ms)
      *duration_ms = duration;
   if (s->last_show >= 0)
   {
      s->frame_time_ns   = s->pending_packet.timestamp;
      s->have_frame_time = 1;
   }
   return s->last_show >= 0 ? 1 : 0;
}
#endif

static uint32_t *rwebm_video_output(rwebm_video_stream_t *s)
{
   if (s->out)
      return s->out;
   if (!s->frame)
      s->frame = (uint32_t*)malloc((size_t)s->width * s->height
            * sizeof(uint32_t));
   return s->frame;
}

static void rwebm_video_h265_ready(rh265_output_timing_t *timing,
      int64_t timestamp, int64_t after)
{
   if (timestamp > after
         && (!timing->next_valid || timestamp < timing->next_timestamp))
   {
      timing->next_timestamp = timestamp;
      timing->next_valid = 1;
   }
}

static int rwebm_video_h265_probe(rwebm_video_stream_t *s,
      const rwebm_packet *packet, int64_t after)
{
   rh265_slice_info_t slice;
   int vcl = rwebm_video_h26x_vcl(s, packet, NULL, &slice);
   if (vcl <= 0) return vcl;
   return rh265_video_output_timing_advance(s->h265, &s->h265_timing,
         &slice, packet->timestamp, after);
}

static int rwebm_video_h265_next_time(rwebm_video_stream_t *s,
      int64_t timestamp)
{
   rh265_output_timing_t *timing = &s->h265_timing;
   unsigned walked;
   if (!s->timing_active)
   {
      if (rh265_video_output_timing(s->h265, timestamp, timing)) return -1;
      rwebm_cursor_copy(s->timing, s->demux);
      s->timing_active = 1;
      s->timing_eof = 0;
      if (!timing->next_valid && s->soft_pending && !s->h26x_consumed
            && rwebm_video_h265_probe(s, &s->soft_packet, timestamp) < 0)
         return -1;
   }
   for (walked = 0; !timing->next_valid && walked < 16; walked++)
   {
      rwebm_packet packet;
      int ret = rwebm_read_packet_track(s->timing, s->track, &packet);
      if (ret == RWEBM_READ_AGAIN || ret == RWEBM_READ_YIELD) return 2;
      if (ret < 0) return -1;
      if (!ret)
      {
         unsigned i;
         for (i = 0; i < timing->count; i++)
            rwebm_video_h265_ready(timing, timing->pending[i], timestamp);
         s->timing_eof = !timing->next_valid;
         break;
      }
      if (packet.track == s->track
            && rwebm_video_h265_probe(s, &packet, timestamp) < 0) return -1;
   }
   if (!timing->next_valid && !s->timing_eof) return 2;
   s->h26x_next_timestamp = timing->next_valid
      ? timing->next_timestamp : timestamp;
   s->timing_active = 0;
   return 1;
}

static int rwebm_video_h26x_next_time(rwebm_video_stream_t *s, int64_t timestamp)
{
   unsigned walked, limit = 1 + (s->h264
         ? rh264_video_reorder_depth(s->h264) : rh265_video_reorder_depth(s->h265));
   if (limit > RWEBM_VIDEO_MAX_REORDER) return -1;
   if (s->h26x_timeline_complete)
   {
      int lo = 0, hi = s->ts_count;
      while (lo < hi)
      {
         int mid = lo + (hi - lo) / 2;
         if (s->ts[mid] <= timestamp) lo = mid + 1;
         else hi = mid;
      }
      s->h26x_next_timestamp = lo < s->ts_count ? s->ts[lo] : timestamp;
      s->timing_eof = lo == s->ts_count;
      s->timing_active = 0;
      return 1;
   }
   if (s->h265) return rwebm_video_h265_next_time(s, timestamp);
   if (!s->timing_active)
   {
      unsigned i;
      rwebm_cursor_copy(s->timing, s->demux);
      s->timing_active = 1;
      s->timing_eof = 0;
      s->h26x_probe_count = 0;
      s->h26x_next_timestamp = timestamp;
      for (i = 0; i < s->h26x_count; i++)
         if (s->h26x_timestamp[i] > timestamp
               && (s->h26x_next_timestamp == timestamp
                  || s->h26x_timestamp[i] < s->h26x_next_timestamp))
            s->h26x_next_timestamp = s->h26x_timestamp[i];
      if (s->soft_pending && !s->h26x_consumed
            && rwebm_video_packet_shown(s, &s->soft_packet)
            && s->soft_packet.timestamp > timestamp
            && (s->h26x_next_timestamp == timestamp
               || s->soft_packet.timestamp < s->h26x_next_timestamp))
         s->h26x_next_timestamp = s->soft_packet.timestamp;
   }
   for (walked = 0; walked < 16; walked++)
   {
      rwebm_packet packet;
      int ret = rwebm_read_packet_track(s->timing, s->track, &packet);
      if (ret == RWEBM_READ_AGAIN || ret == RWEBM_READ_YIELD) return 2;
      if (ret < 0) return -1;
      if (!ret) { s->timing_eof = 1; break; }
      if (packet.track != s->track) continue;
      ret = rwebm_video_packet_shown(s, &packet);
      if (!ret) continue;
      if (packet.timestamp > timestamp
            && (s->h26x_next_timestamp == timestamp
               || packet.timestamp < s->h26x_next_timestamp))
         s->h26x_next_timestamp = packet.timestamp;
      if (++s->h26x_probe_count >= limit) break;
   }
   if (walked == 16) return 2;
   s->timing_active = 0;
   return 1;
}

static int rwebm_video_h26x_picture(rwebm_video_stream_t *s,
      const rwebm_packet *pkt, int discard, int render_only)
{
   const rwebm_track *t = rwebm_get_track(s->demux, s->track);
   const uint8_t *y, *u, *v;
   int ret, ys, uvs, w, h, cw, ch, depth, vcl = 0, shown = 0;
   int64_t timestamp = 0, span;
   unsigned i, first = 0;
   rwebm_blit_ctx_t c;
   if (render_only)
   {
      ret = 1;
      goto render;
   }
   if (s->h26x_picture_pending) goto timing;
   s->h26x_consumed = 0;
   if (pkt)
   {
      vcl = rwebm_video_h26x_vcl(s, pkt, &shown, NULL);
      if (vcl < 0) return -1;
   }
   if (s->hw_untaken)
   {
      video_hw_frame_abandon(&s->hw_frame);
      s->hw_untaken = 0;
   }
   if (shown && s->h26x_count == RWEBM_VIDEO_MAX_REORDER)
      return -1;
   ret = s->h264 ? (pkt ? rh264_video_decode(s->h264, pkt->data, pkt->size)
         : rh264_video_drain(s->h264))
      : (pkt ? rh265_video_decode_timed(s->h265, pkt->data, pkt->size,
               pkt->timestamp, &timestamp)
         : rh265_video_drain_timed(s->h265, &timestamp));
   if (ret == 2)
   {
      s->waiting = 1;
      return 2;
   }
   if (ret < 0) return pkt || ret != -1 ? -1 : -2;
   s->h26x_consumed = ret != 3;
   if (shown && ret != 3)
   {
      unsigned n = s->h26x_count++;
      s->h26x_timestamp[n] = pkt->timestamp;
      s->h26x_span[n] = pkt->duration_ns;
   }
   if (pkt && !ret) return 0;
   if (s->h264 ? rh264_video_is_hw(s->h264) : rh265_video_is_hw(s->h265))
   {
      video_hw_frame_t frame;
      if ((s->h264 ? rh264_video_hw_frame(s->h264, &frame)
            : rh265_video_hw_frame(s->h265, &frame)) != 0)
         return -1;
      if (frame.width < s->width || frame.height < s->height)
      {
         video_hw_frame_abandon(&frame);
         return -1;
      }
      frame.width = s->width;
      frame.height = s->height;
      if (s->hw_color.primaries && s->hw_color.primaries != 2)
         frame.color.primaries = s->hw_color.primaries;
      if (s->hw_color.transfer && s->hw_color.transfer != 2)
         frame.color.transfer = s->hw_color.transfer;
      if (s->hw_color.matrix && s->hw_color.matrix != 2)
         frame.color.matrix = s->hw_color.matrix;
      if (s->hw_color.range) frame.color.range = s->hw_color.range;
      if (t)
      {
         if (t->primaries && t->primaries != 2) frame.color.primaries = t->primaries;
         if (t->transfer_characteristics && t->transfer_characteristics != 2)
            frame.color.transfer = t->transfer_characteristics;
         if (t->matrix_coefficients && t->matrix_coefficients != 2)
            frame.color.matrix = t->matrix_coefficients;
         if (t->colour_range) frame.color.range = t->colour_range;
         if (t->mastering.present) frame.color.mastering = t->mastering;
         if (t->max_cll) frame.color.max_cll = t->max_cll;
         if (t->max_fall) frame.color.max_fall = t->max_fall;
      }
      if (frame.color.matrix == 1) frame.yuv |= IMAGE_YUV_FLAG_BT709;
      else if (frame.color.matrix == 5 || frame.color.matrix == 6)
         frame.yuv &= ~IMAGE_YUV_FLAG_BT709;
      if (frame.color.range == 2) frame.yuv |= IMAGE_YUV_FLAG_FULL_RANGE;
      else if (frame.color.range == 1) frame.yuv &= ~IMAGE_YUV_FLAG_FULL_RANGE;
      s->hw_frame = frame;
      s->hw_untaken = 1;
   }
   if (!s->h26x_count) return -1;
   if (s->h265)
   {
      for (first = 0; first < s->h26x_count; first++)
         if (s->h26x_timestamp[first] == timestamp) break;
      if (first == s->h26x_count) return -1;
   }
   else
      for (i = 1; i < s->h26x_count; i++)
         if (s->h26x_timestamp[i] < s->h26x_timestamp[first]) first = i;
   timestamp = s->h26x_timestamp[first];
   span = s->h26x_span[first];
   s->h26x_count--;
   s->h26x_timestamp[first] = s->h26x_timestamp[s->h26x_count];
   s->h26x_span[first] = s->h26x_span[s->h26x_count];
   if (s->h265)
      for (i = 0; i < s->h26x_count; )
      {
         if (s->h26x_timestamp[i] < timestamp)
         {
            s->h26x_count--;
            s->h26x_timestamp[i] = s->h26x_timestamp[s->h26x_count];
            s->h26x_span[i] = s->h26x_span[s->h26x_count];
         }
         else i++;
      }
   s->frame_time_ns = timestamp;
   s->have_frame_time = 1;
   s->h26x_picture_span = span;
   s->h26x_picture_pending = 1;
timing:
   timestamp = s->frame_time_ns;
   span = s->h26x_picture_span;
   ret = s->h26x_consumed ? 1 : 3;
   if (span <= 0)
   {
      int ready = rwebm_video_h26x_next_time(s, timestamp);
      if (ready < 0) return -1;
      if (ready == 2) { s->waiting = 1; return 2; }
      if (s->h26x_next_timestamp > timestamp)
      {
         uint64_t delta = (uint64_t)s->h26x_next_timestamp - (uint64_t)timestamp;
         if (delta > ((~(uint64_t)0) >> 1)) return -1;
         span = (int64_t)delta;
      }
      else if (s->timing_eof && (!t || !t->default_duration_ns))
      {
         int64_t end = rwebm_duration_ns(s->demux);
         uint64_t delta = (uint64_t)end - (uint64_t)timestamp;
         if (end > timestamp && delta <= ((~(uint64_t)0) >> 1))
            span = (int64_t)delta;
      }
   }
   if (span <= 0 && t && t->default_duration_ns
         && t->default_duration_ns <= ((~(uint64_t)0) >> 1))
   {
      uint64_t duration = t->default_duration_ns / 1000000;
      unsigned remainder = s->h26x_remainder
         + (unsigned)(t->default_duration_ns % 1000000);
      duration += remainder / 1000000;
      s->h26x_remainder = remainder % 1000000;
      s->h26x_duration = duration < 2147483647 ? (int)duration : 0;
   }
   else
   {
      if (span <= 0)
         for (i = 0; i < s->h26x_count; i++)
            if (s->h26x_timestamp[i] > timestamp)
            {
               uint64_t delta = (uint64_t)s->h26x_timestamp[i] - (uint64_t)timestamp;
               if (delta <= ((~(uint64_t)0) >> 1)
                     && (span <= 0 || delta < (uint64_t)span))
                  span = (int64_t)delta;
            }
      s->h26x_duration = span > 0 && span / 1000000 < 2147483647
         ? (int)(span / 1000000 + ((timestamp % 1000000 + span % 1000000)
                  / 1000000)) : 0;
   }
   s->frame_time_ns = timestamp;
   s->have_frame_time = 1;
   s->h26x_picture_pending = 0;
   if (s->hw_untaken && (discard || (s->seek_active && timestamp < s->seek_target)))
   {
      video_hw_frame_abandon(&s->hw_frame);
      s->hw_untaken = 0;
   }
   if (discard || s->seek_active)
      return ret == 3 ? 3 : 1;
render:
   if (s->h264 ? rh264_video_is_hw(s->h264) : rh265_video_is_hw(s->h265))
      return s->hw_untaken ? (ret == 3 ? 3 : 1) : -1;
   if (s->h264)
   {
      y = rh264_video_plane(s->h264, 0, &ys, &w, &h);
      u = rh264_video_plane(s->h264, 1, &uvs, &cw, &ch);
      v = rh264_video_plane(s->h264, 2, NULL, NULL, NULL);
      depth = rh264_video_bit_depth(s->h264);
   }
   else
   {
      y = rh265_video_plane(s->h265, 0, &ys, &w, &h);
      u = rh265_video_plane(s->h265, 1, &uvs, &cw, &ch);
      v = rh265_video_plane(s->h265, 2, NULL, NULL, NULL);
      depth = rh265_video_bit_depth(s->h265);
   }
   if (!y || !u || !v || w <= 0 || h <= 0
         || (cw != (w + 1) / 2 && cw != w)
         || (ch != (h + 1) / 2 && ch != h)
         || depth < 8 || depth > 14) return -1;
   memset(&c, 0, sizeof(c));
   if (!(c.dst = rwebm_video_output(s))) return -1;
   c.dst_stride = s->out && s->out_stride ? s->out_stride : s->width;
   c.w = (unsigned)w < s->width ? (unsigned)w : s->width;
   c.y = y; c.u = u; c.v = v; c.ys = ys; c.uvs = uvs;
   c.matrix = s->hw_color.matrix;
   c.transfer = s->hw_color.transfer;
   c.range = s->hw_color.range;
   s->is10 = s->is_fp16 = 0;
   if (cw != (w + 1) / 2 || ch != (h + 1) / 2
         || (depth != 8 && depth != 10) || (depth == 8 && c.range == 2))
   {
      c.kind = 4;
      c.chsh = cw < w ? 1 : 0;
      c.cvsh = ch < h ? 1 : 0;
      c.depth = depth;
      c.argb = s->emit_argb;
   }
   else if (depth == 10)
   {
      if (s->want_fp16 && s->out && (c.transfer == 16 || c.transfer == 18))
      {
         c.kind = 3;
         s->is_fp16 = 1;
      }
      else if (s->want10)
      {
         c.kind = 2;
         s->is10 = 1;
      }
      else
      {
         c.kind = 1;
         c.argb = !s->emit_argb;
      }
   }
   else
      c.argb = s->emit_argb;
   rwebm_video_blit_frame(s, &c,
         (unsigned)h < s->height ? (unsigned)h : s->height);
   return ret == 3 ? 3 : 1;
}

/* 1 displayed picture, 0 hidden packet, negative decode failure. */
static int rwebm_video_decode_packet(rwebm_video_stream_t *s,
      const rwebm_packet *pkt, int discard)
{
   uint32_t *dst;
   s->is_fp16    = 0;
   /* A decoder can be absent if the re-open in rewind hit OOM. */
   if (!s->vp8
#ifdef HAVE_RVP9
       && !s->vp9
#endif
      )
      return -1;
#ifdef HAVE_RVP9
   if (s->codec == RWEBM_CODEC_VP9)
   {
      size_t sizes[RWEBM_VIDEO_MAX_SUPER];
      const uint8_t *frame = pkt->data;
      int nf = rwebm_video_vp9_superframe(pkt->data, pkt->size,
            sizes, RWEBM_VIDEO_MAX_SUPER);
      int i, r, show, last_show = -1;
      for (i = 0; i < nf; i++)
      {
         r = rvp9_decode_frame(s->vp9, frame, sizes[i], &show);
         if (r < 0)
            return -1;
         if (show >= 0)
            last_show = show;
         frame += sizes[i];
      }
      if (last_show >= 0)
      {
         const rvp9_fb *fb = &s->vp9->fbs[last_show];
         unsigned w = (unsigned)fb->w < s->width  ? (unsigned)fb->w : s->width;
         unsigned h = (unsigned)fb->h < s->height ? (unsigned)fb->h : s->height;
         if (discard) return 1;
         if (!(dst = rwebm_video_output(s)))
            return -1;
         {
            const rwebm_track *ct = rwebm_get_track(s->demux, s->track);
            rwebm_blit_ctx_t c;
            c.dst        = dst;
            c.dst_stride = s->out && s->out_stride ? s->out_stride : s->width;
            c.w          = w;
            c.y          = fb->y;
            c.u          = fb->u;
            c.v          = fb->v;
            c.ys         = s->vp9->ys;
            c.uvs        = s->vp9->uvs;
            c.matrix     = ct ? ct->matrix_coefficients : 0;
            c.transfer   = ct ? ct->transfer_characteristics : 0;
            c.range      = ct ? ct->colour_range : 0;
            c.max_cll    = ct ? ct->max_cll : 0;
            if (s->vp9->hd.bit_depth == 10)
            {
               /* An HDR source, for a caller that takes linear light
                * into its own frame: no tone map. */
               if (     s->want_fp16 && s->out
                     && (c.transfer == 16 || c.transfer == 18))
               {
                  c.kind     = 3;
                  c.argb     = 0;
                  s->is_fp16 = 1;
               }
               else if (s->want10)
               {
                  /* Native 10-bit thumbnail: packed XRGB2101010,
                   * SDR-encoded at 10-bit precision (same colour as
                   * the 8-bit path). */
                  c.kind = 2;
                  c.argb = 0;
                  s->is10 = 1;
               }
               else
               {
                  c.kind = 1;
                  c.argb = s->emit_argb ? 0 : 1;
               }
            }
            else
            {
               c.kind = 0;
               c.argb = s->emit_argb;
            }
            rwebm_video_blit_frame(s, &c, h);
         }
         return 1;
      }
      return 0;
   }
#endif
   if (s->codec == RWEBM_CODEC_VP8)
   {
      const uint8_t *y, *u, *v;
      int ys, uvs, w, h, cw, ch;
      /* VP8 frame tag: bit 4 of byte 0 is show_frame. */
      int shown = (pkt->size > 0) && (pkt->data[0] & 0x10);
      if (rvp8_video_decode(s->vp8, pkt->data, pkt->size) != 0)
         return -1;
      if (!shown)
         return 0;
      if (discard) return 1;
      y = rvp8_video_plane(s->vp8, 0, &ys, &w, &h);
      u = rvp8_video_plane(s->vp8, 1, &uvs, &cw, &ch);
      v = rvp8_video_plane(s->vp8, 2, &uvs, &cw, &ch);
      if (!y || !u || !v)
         return -1;
      if (!(dst = rwebm_video_output(s)))
         return -1;
      if ((unsigned)w > s->width)
         w = (int)s->width;
      if ((unsigned)h > s->height)
         h = (int)s->height;
      {
         const rwebm_track *ct = rwebm_get_track(s->demux, s->track);
         rwebm_blit_ctx_t c;
         c.dst        = dst;
         c.dst_stride = s->out && s->out_stride ? s->out_stride : s->width;
         c.w          = (unsigned)w;
         c.y          = y;
         c.u          = u;
         c.v          = v;
         c.ys         = ys;
         c.uvs        = uvs;
         c.matrix     = ct ? ct->matrix_coefficients : 0;
         c.transfer   = 0;
         c.range      = 0;
         c.max_cll    = 0;
         c.argb       = s->emit_argb;
         c.kind       = 0;
         rwebm_video_blit_frame(s, &c, (unsigned)h);
      }
      return 1;
   }
   return -1;
}

/* 1 displayed picture (canvas or GPU lease), 0 hidden packet, 2 retry,
 * -1 end/error. Other tracks are skipped with header reads only. */
static int rwebm_video_stream_step(rwebm_video_stream_t *s,
      int *duration_ms, int discard)
{
   rwebm_packet pkt;
   s->waiting = 0;
   if (s->h26x_picture_pending)
   {
      int ret = rwebm_video_h26x_picture(s, NULL, discard, 0);
      if (duration_ms) *duration_ms = s->h26x_duration;
      return ret == 3 ? 1 : ret;
   }
   if (s->h26x_ready)
   {
      s->h26x_ready = 0;
      if (duration_ms) *duration_ms = s->h26x_duration;
      return discard ? 1 : rwebm_video_h26x_picture(s, NULL, 0, 1);
   }
#ifdef HAVE_RAV1
   if (s->codec == RWEBM_CODEC_AV1)
      return rwebm_video_av1_step(s, duration_ms, discard);
#endif
#ifdef HAVE_RVP9
   if (s->hw)
   {
      int ret = rwebm_video_hw_step(s, duration_ms, discard);
      if (ret < 0 && s->hw_untaken)
      {
         video_hw_frame_abandon(&s->hw_frame);
         s->hw_untaken = 0;
         memset(&s->hw_frame, 0, sizeof(s->hw_frame));
      }
      return ret;
   }
#endif

   for (;;)
   {
      int idx, r, duration = 0;
      size_t floor = rwebm_consumed(s->demux);
      if (s->soft_pending)
      {
         pkt = s->soft_packet;
         r = 1;
      }
      else if (s->seek_ready)
      {
         pkt = s->seek_packet;
         floor = s->seek_floor;
         s->seek_ready = 0;
         r = 1;
      }
      else
         r = rwebm_video_read_packet(s, &pkt, &floor);
      if (r == RWEBM_READ_AGAIN)
      {
         s->waiting = 1;
         return 2;   /* not yet resident; nothing consumed - retry
                        after rwebm_video_stream_set_avail */
      }
      if (r != 1)
      {
         if (!r && (s->h264 || s->h265))
         {
            r = rwebm_video_h26x_picture(s, NULL, discard, 0);
            if (r >= 0)
            {
               if (duration_ms) *duration_ms = s->h26x_duration;
               return r;
            }
            s->completed = r == -2;
            s->have_decode_time = 0;
            return -1;
         }
         s->completed = r == 0;
         s->have_decode_time = 0;
         break;
      }
      if (pkt.track != s->track)
         continue;
      if ((s->h264 || s->h265) && s->seek_active && !s->seek_key)
      {
         /* A lace's key flag describes all its pictures together. */
         if (!pkt.keyframe && rwebm_video_h26x_vcl(s, &pkt, NULL, NULL) != 2)
            return 0;
         s->seek_key = 1;
      }
      idx = s->pkt_idx;
      if (!s->soft_pending)
      {
         s->soft_packet = pkt;
         s->soft_floor = floor;
         s->soft_pending = 1;
      }
      if (s->h264 || s->h265)
      {
         r = rwebm_video_h26x_picture(s, &pkt, discard, 0);
         if (s->h26x_consumed)
         {
            s->soft_pending = 0;
            s->pkt_idx++;
         }
         if (r > 0 && r != 2)
         {
            s->decode_time_ns = s->frame_time_ns;
            s->have_decode_time = 1;
            if (duration_ms) *duration_ms = s->h26x_duration;
         }
         return r == 3 ? 1 : r;
      }
      r = rwebm_video_packet_duration(s, idx, pkt.timestamp, &duration);
      if (r == 2) { s->waiting = 1; return 2; }
      if (r < 0)
      {
         s->have_decode_time = 0;
         return -1;
      }
      s->soft_pending = s->timing_active = s->timing_ready = 0;
      s->pkt_idx++;
      r   = rwebm_video_decode_packet(s, &pkt, discard);
      if (r < 0)
      {
         s->have_decode_time = 0;
         return -1;      /* decode error: end the animation */
      }
      if (r > 0)
      {
         if (!s->have_decode_time || pkt.timestamp > s->decode_time_ns)
            s->decode_time_ns = pkt.timestamp;
         s->have_decode_time = 1;
      }
      if (r == 0)
         return 0;       /* non-shown frame                  */
      if (duration_ms)
         *duration_ms = duration;
      s->frame_time_ns = pkt.timestamp;
      s->have_frame_time = 1;
      return 1;
   }
   return -1;             /* end of one pass */
}

static int rwebm_video_stream_advance(rwebm_video_stream_t *s,
      int *duration_ms, int discard)
{
   int ret;
#ifdef HAVE_RAV1
   int64_t started = 0;
#endif
   if (!s)
      return -1;
   s->completed = 0;
#ifdef HAVE_RAV1
   if (s->codec == RWEBM_CODEC_AV1)
      started = cpu_features_get_time_usec();
#endif
   while ((ret = rwebm_video_stream_step(s, duration_ms, discard)) == 0)
   {
#ifdef HAVE_RAV1
      if (s->codec == RWEBM_CODEC_AV1
            && cpu_features_get_time_usec() - started >= 4000)
      {
         s->waiting = 1;
         return 2;
      }
#endif
   }
   return ret;
}

const uint32_t *rwebm_video_stream_next(rwebm_video_stream_t *s,
      int *duration_ms)
{
   if (rwebm_video_stream_advance(s, duration_ms, 0) != 1)
      return NULL;
   if ((s->h264 && rh264_video_is_hw(s->h264))
         || (s->h265 && rh265_video_is_hw(s->h265)))
      return &s->hw_marker;
#ifdef HAVE_RAV1
   if (s->codec == RWEBM_CODEC_AV1)
      return &s->hw_marker;
#endif
#ifdef HAVE_RVP9
   if (s->hw)
      return &s->hw_marker;
#endif
   return s->out ? s->out : s->frame;
}

int rwebm_video_stream_skip(rwebm_video_stream_t *s, int *duration_ms)
{
   int ret = rwebm_video_stream_advance(s, duration_ms, 1);
   return ret > 0 ? ret : 0;
}

void rwebm_video_stream_stop_hw(rwebm_video_stream_t *s)
{
   if (!s)
      return;
   s->hw_want = 0;
   rwebm_video_stream_rewind(s);
}

int rwebm_video_stream_completed(const rwebm_video_stream_t *s)
{
   return s && s->completed;
}

void rwebm_video_stream_rewind(rwebm_video_stream_t *s)
{
   if (!s)
      return;
   s->completed = 0;
   rwebm_rewind(s->demux);
   s->seek_active = s->seek_ready = s->seek_key = s->seek_scan = 0;
   s->seek_index_pending = 0;
#ifdef HAVE_RAV1
   s->av1_hdr_active = 0;
#endif
   s->soft_pending = s->timing_active = s->timing_ready = s->lookahead_ready = 0;
   s->pkt_idx = 0;
   s->have_frame_time = 0;
   s->have_decode_time = 0;
#ifdef HAVE_RVP9
   if (s->hw && s->hw_want && s->hw_backend.restart)
   {
      if (s->hw_untaken)
         video_hw_frame_abandon(&s->hw_frame);
      s->hw_untaken = s->pending = s->waiting = 0;
      memset(&s->hw_frame, 0, sizeof(s->hw_frame));
      rvp9_hw_session_restart(s->hw);
      s->hw_restart = 1;
      return;
   }
#endif
   /* The stream restarts at a key frame, so a fresh decoder (empty
    * reference chain, default probabilities) is the correct state. */
   rwebm_video_stream_close_decoder(s);
   rwebm_video_stream_open_decoder(s);
   s->waiting = 0;
   if (s->hw_want) rwebm_video_stream_set_hw(s, 1);
}

#ifdef HAVE_RAV1
static const rav1_hdr_metadata_t *rwebm_video_av1_hdr_at(
      const rwebm_video_stream_t *s, size_t offset)
{
   size_t lo = 0, hi = s->av1_hdr_count;
   while (lo < hi)
   {
      size_t mid = lo + (hi - lo) / 2;
      if (s->av1_hdr_points[mid].offset <= offset) lo = mid + 1;
      else hi = mid;
   }
   return lo ? &s->av1_hdr_points[lo - 1].hdr : &s->av1_hdr_seed;
}

static int rwebm_video_av1_hdr_point(rwebm_video_stream_t *s, size_t offset)
{
   rwebm_av1_hdr_point_t *points;
   size_t capacity;
   if (s->av1_hdr_count && !memcmp(&s->av1_hdr_work,
            &s->av1_hdr_points[s->av1_hdr_count - 1].hdr,
            sizeof(s->av1_hdr_work))) return 1;
   if (s->av1_hdr_count == s->av1_hdr_capacity)
   {
      capacity = s->av1_hdr_capacity ? s->av1_hdr_capacity * 2 : 8;
      if (capacity < s->av1_hdr_capacity
            || capacity > (size_t)-1 / sizeof(*points)) return 0;
      points = (rwebm_av1_hdr_point_t*)realloc(s->av1_hdr_points,
            capacity * sizeof(*points));
      if (!points) return 0;
      s->av1_hdr_points = points;
      s->av1_hdr_capacity = capacity;
   }
   s->av1_hdr_points[s->av1_hdr_count].offset = offset;
   s->av1_hdr_points[s->av1_hdr_count++].hdr = s->av1_hdr_work;
   return 1;
}

/* One header OBU per call, with the metadata cursor exposed to the feeder. */
static int rwebm_video_av1_scan_hdr(rwebm_video_stream_t *s)
{
   rav1_obu_t obu;
   int ret;
   if (!s->av1_hdr_cursor)
   {
      s->av1_hdr_cursor = rwebm_cursor_clone(s->demux);
      if (!s->av1_hdr_cursor) return -1;
      rwebm_rewind(s->av1_hdr_cursor);
      s->av1_hdr_work = s->av1_hdr_seed;
      if (!rwebm_video_av1_hdr_point(s, 0)) return -1;
      return 2;
   }
   if (s->av1_hdr_done >= s->av1_hdr_target) return 1;
   if (!s->av1_hdr_pending)
   {
      s->av1_hdr_floor = rwebm_consumed(s->av1_hdr_cursor);
      ret = rwebm_read_packet(s->av1_hdr_cursor, &s->av1_hdr_packet);
      if (ret == RWEBM_READ_AGAIN) { s->waiting = 1; return 0; }
      if (ret != 1) return -1;
      if (s->av1_hdr_packet.track != s->track)
      {
         s->av1_hdr_done = rwebm_consumed(s->av1_hdr_cursor);
         return 2;
      }
      s->av1_hdr_pending = 1;
      s->av1_hdr_offset = 0;
   }
   if ((size_t)(s->av1_hdr_packet.data - s->buf) >= s->av1_hdr_target)
   {
      s->av1_hdr_done = (size_t)(s->av1_hdr_packet.data - s->buf);
      return 1;
   }
   if ((size_t)(s->av1_hdr_packet.data - s->buf) > s->avail ||
         s->av1_hdr_packet.size > s->avail - (size_t)(s->av1_hdr_packet.data - s->buf))
   {
      s->waiting = 1;
      return 0;
   }
   ret = rav1_obu_next(&obu, s->av1_hdr_packet.data, s->av1_hdr_packet.size,
         &s->av1_hdr_offset, true);
   if (ret < 0) return -1;
   if (!ret)
   {
      size_t end = (size_t)(s->av1_hdr_packet.data - s->buf) + s->av1_hdr_packet.size;
      if (!rwebm_video_av1_hdr_point(s, end)) return -1;
      s->av1_hdr_done = end;
      s->av1_hdr_packets++;
      s->av1_hdr_pending = 0;
      return 2;
   }
   if (obu.temporal_id || obu.spatial_id) return -1;
   if (obu.type == 5 && rav1_hdr_metadata_obu(&s->av1_hdr_work, &obu) < 0)
      return -1;
   return 0;
}
#endif

/* Resumable seek: cue selection only changes metadata. Each
 * step reads at most 256 packet headers or decodes one video packet.
 * A file without visible Cues uses the same bounded walk from the start. */
static void rwebm_video_seek_select(rwebm_video_stream_t *s)
{
   int64_t landed;
   if (s->endpoint_known && s->have_last_shown
         && s->seek_target > s->last_shown_ts)
      s->seek_target = s->last_shown_ts;
   landed = rwebm_select_time_ns(s->demux, s->seek_target);
   if (landed < 0 || landed > s->seek_target) rwebm_rewind(s->demux);
   s->seek_scan = 0;
#ifdef HAVE_RAV1
   if (s->codec == RWEBM_CODEC_AV1)
   {
      s->av1_hdr_target = rwebm_consumed(s->demux);
      s->av1_hdr_active = s->av1_hdr_yield = 1;
   }
#endif
}

static void rwebm_video_seek_scan_begin(rwebm_video_stream_t *s)
{
   rwebm_cursor_copy(s->timing, s->demux);
   if (s->seek_index_verified && s->have_last_shown
         && rwebm_select_time_ns(s->demux, s->seek_target) >= 0)
   {
      rwebm_cursor_copy(s->timing, s->demux);
      s->seek_scan_from_cue = 1;
   }
   s->seek_scan = 1;
}

int64_t rwebm_video_stream_start_ns(const rwebm_video_stream_t *s)
{
   return s && s->have_last_shown ? s->first_shown_ts : 0;
}

int64_t rwebm_video_stream_frame_time_ns(const rwebm_video_stream_t *s)
{
   return s && s->have_frame_time ? s->frame_time_ns : -1;
}

int rwebm_video_stream_seek_movie_begin_ns(rwebm_video_stream_t *s, int64_t ns)
{
   if (!s) return 0;
   s->movie_seek_target_ns = ns < 0 ? 0 : ns;
   return rwebm_video_stream_seek_begin_ns(s, ns);
}

int rwebm_video_stream_seek_movie_step(rwebm_video_stream_t *s,
      int64_t *position_us)
{
   int result = rwebm_video_stream_seek_step(s, position_us);
   if (result == 1 && position_us && s->have_last_shown
         && (s->movie_seek_target_ns < s->first_shown_ts
            || (s->endpoint_known && s->movie_seek_target_ns > s->last_shown_ts)))
      *position_us = s->movie_seek_target_ns / 1000;
   return result;
}

int rwebm_video_stream_seekable(const rwebm_video_stream_t *s)
{
   if (!s) return 0;
   if (s->h264 || s->h265) return 1;
#ifdef HAVE_RAV1
   /* Format capability is known before deferred native admission. The
    * seek operation itself still requires an admitted decoder. */
   if (s->codec == RWEBM_CODEC_AV1)
      return 1;
#endif
#ifdef HAVE_RVP9
   if (s->hw || s->vp9) return 1;
#endif
   return s->vp8 != NULL;
}

int rwebm_video_stream_seek_begin_ns(rwebm_video_stream_t *s, int64_t ns)
{
   int reuse = 0;
   if (!rwebm_video_stream_seekable(s)) return 0;
   s->completed = 0;
   if (ns < 0) ns = 0;
   if (s->endpoint_known && s->have_last_shown && ns > s->last_shown_ts)
      ns = s->last_shown_ts;
   if (!s->h264 && !s->h265 && s->have_decode_time && ns > s->decode_time_ns
         && (s->endpoint_known || (s->have_last_shown && ns <= s->last_shown_ts))
         && rwebm_cue_time_ns(s->demux, ns) <= s->decode_time_ns
#ifdef HAVE_RVP9
         && !s->hw
#endif
#ifdef HAVE_RAV1
         && !s->av1
#endif
      )
      reuse = 1;
   if (!reuse)
      rwebm_video_stream_rewind(s);
   else
   {
      /* A held picture still precedes the demux cursor and must decode
       * before the next target, even when its output is discarded. */
      if (s->soft_pending || s->seek_ready)
      {
         s->lookahead_packet = s->soft_pending ? s->soft_packet : s->seek_packet;
         s->lookahead_floor = s->soft_pending ? s->soft_floor : s->seek_floor;
         s->lookahead_ready = 1;
      }
      s->soft_pending = s->timing_active = s->timing_ready = 0;
      s->seek_ready = s->seek_scan = s->waiting = 0;
      s->seek_key = 1;
   }
   if (!s->vp8
         && !s->h264 && !s->h265
#ifdef HAVE_RVP9
         && !s->vp9
         && !s->hw
#endif
#ifdef HAVE_RAV1
         && !s->av1
#endif
      ) return 0;
   s->seek_target = ns;
   s->seek_active = 1;
   s->seek_index_lo = rwebm_cues_offset(s->demux);
   s->seek_index_pending = s->seek_index_lo && s->len - s->seek_index_lo >= 12;
   s->seek_index_hi = s->seek_index_pending ? s->seek_index_lo + 12 : 0;
   s->seek_scan_from_cue = s->seek_scan_shown = 0;
   if (reuse) return 1;
   if (!s->endpoint_known && (!s->have_last_shown || ns > s->last_shown_ts))
   {
      /* Discover the actual endpoint incrementally, exposing the probe's
       * byte frontier to the same bounded feeder used for seek preroll. */
      rwebm_video_seek_scan_begin(s);
   }
   else rwebm_video_seek_select(s);
   return 1;
}

/* Only visibility bits are needed here; reference updates are decoded
 * normally before the target. A VP9 superframe can contain hidden frames
 * followed by a shown one, so inspect each coded frame's prefix. */
static int rwebm_video_packet_shown(rwebm_video_stream_t *s,
      const rwebm_packet *packet)
{
   if (!packet->size) return 0;
   if (s->codec == RWEBM_CODEC_H264 || s->codec == RWEBM_CODEC_H265)
   {
      int shown;
      return rwebm_video_h26x_vcl(s, packet, &shown, NULL) > 0 && shown;
   }
#ifdef HAVE_RAV1
   if (s->codec == RWEBM_CODEC_AV1)
   {
      rav1_obu_t obu;
      size_t offset = 0;
      if (!s->av1_scan_sequence.operating_point_count)
      {
         const rwebm_track *t = rwebm_get_track(s->demux, s->track);
         rav1_config_t config;
         if (t && rav1_config_parse(&config, t->codec_private,
                  t->codec_private_size))
         {
            rav1_config_sequence(&s->av1_scan_sequence, &config);
            if (!s->av1_hdr_initial_set &&
                !rav1_hdr_metadata_config(&s->av1_hdr_initial, &config))
               s->av1_hdr_initial_error = 1;
         }
      }
      while (rav1_obu_next(&obu, packet->data, packet->size, &offset, true) == 1)
      {
         if (obu.temporal_id || obu.spatial_id) return 0;
         if (obu.type == 1)
         {
            if (!rav1_sequence_parse(&s->av1_scan_sequence,
                     obu.payload, obu.payload_size)) return 0;
         }
         else if (obu.type == 5 && !s->av1_hdr_initial_set)
         {
            if (rav1_hdr_metadata_obu(&s->av1_hdr_initial, &obu) < 0)
            {
               s->av1_hdr_initial_error = 1;
               return 0;
            }
         }
         else if (obu.type == 3 || obu.type == 6)
         {
            rav1_frame_prefix_t frame;
            rav1_bits_t bits;
            bits.data = obu.payload;
            bits.size = obu.payload_size;
            bits.byte = 0;
            bits.bit = bits.failed = 0;
            if (!rav1_frame_prefix_parse(&frame, &bits, &s->av1_scan_sequence,
                     0, 0, obu.type == 6)) return 0;
            if (frame.show_existing || frame.show_frame)
            {
               s->av1_hdr_initial_set = 1;
               return 1;
            }
         }
      }
      return 0;
   }
#endif
   if (s->codec == RWEBM_CODEC_VP8) return (packet->data[0] & 16) != 0;
#ifdef HAVE_RVP9
   if (s->codec == RWEBM_CODEC_VP9)
   {
      size_t sizes[RWEBM_VIDEO_MAX_SUPER], offset = 0;
      int i, n = rwebm_video_vp9_superframe(packet->data, packet->size,
            sizes, RWEBM_VIDEO_MAX_SUPER);
      for (i = 0; i < n; i++)
      {
         unsigned prefix = packet->data[offset];
         unsigned profile = ((prefix >> 4) & 1) * 2 + ((prefix >> 5) & 1);
         unsigned shift = profile == 3 ? 1 : 0;
         if ((prefix & (8U >> shift)) || (prefix & (2U >> shift))) return 1;
         offset += sizes[i];
      }
   }
#endif
   return 0;
}

int rwebm_video_stream_seek_step(rwebm_video_stream_t *s,
      int64_t *position_us)
{
   int walked;
   if (!s) return -1;
   if (s->h26x_ready)
   {
      if (position_us) *position_us = s->frame_time_ns / 1000;
      return 1;
   }
   if (s->seek_ready)
   {
      if (position_us) *position_us = s->seek_packet.timestamp / 1000;
      return 1;
   }
   if (!s->seek_active) return -1;
   s->waiting = 0;
   if (s->seek_index_pending)
   {
      const rwebm_track *track = rwebm_get_track(s->demux, s->track);
      size_t needed = 0;
      int loaded;
      if (s->avail < s->seek_index_hi)
      {
         s->waiting = 1;
         return 0;
      }
      loaded = rwebm_load_cues(s->demux, s->buf + s->seek_index_lo,
            s->avail - s->seek_index_lo, track ? track->number : 0, &needed);
      if (!loaded)
      {
         s->seek_index_hi = s->seek_index_lo + needed;
         s->waiting = 1;
         return 0;
      }
      s->seek_index_pending = 0;
      if (loaded > 0)
      {
         s->seek_index_verified = 1;
         if (s->seek_scan)
            rwebm_video_seek_scan_begin(s);
         else
            rwebm_video_seek_select(s);
      }
      return 0;
   }
   if (s->seek_scan)
   {
      for (walked = 0; walked < 256; walked++)
      {
         rwebm_packet packet;
         int ret = rwebm_read_packet(s->timing, &packet);
         if (ret == RWEBM_READ_AGAIN) { s->waiting = 1; return 0; }
         if (ret < 0) goto failed;
         if (!ret)
         {
            if (s->seek_scan_from_cue && !s->seek_scan_shown)
            {
               rwebm_rewind(s->timing);
               s->seek_scan_from_cue = 0;
               return 0;
            }
            if (!s->have_last_shown) goto failed;
            s->endpoint_known = 1;
            rwebm_video_seek_select(s);
            return 0;
         }
         if (packet.track == s->track && rwebm_video_packet_shown(s, &packet))
            s->seek_scan_shown = 1;
         if (packet.track == s->track && rwebm_video_packet_shown(s, &packet)
               && (!s->have_last_shown || packet.timestamp > s->last_shown_ts))
         {
            if (!s->have_last_shown) s->first_shown_ts = packet.timestamp;
            s->last_shown_ts = packet.timestamp;
            s->have_last_shown = 1;
         }
         /* A shown picture at the target proves that no endpoint clamp is
          * needed. Leave endpoint discovery lazy instead of reading the
          * rest of a potentially multi-gigabyte movie before every jump. */
         if (s->have_last_shown && s->last_shown_ts >= s->seek_target)
         {
            rwebm_video_seek_select(s);
            return 0;
         }
      }
      return 0;
   }
   if (s->h264 || s->h265)
   {
      int ret = rwebm_video_stream_step(s, NULL, 0);
      if (ret < 0) goto failed;
      if (ret == 1 && s->frame_time_ns >= s->seek_target)
      {
         s->h26x_ready = 1;
         s->seek_active = 0;
         if (position_us) *position_us = s->frame_time_ns / 1000;
         return 1;
      }
      return 0;
   }
#ifdef HAVE_RAV1
   if (s->av1_hdr_active)
   {
      int64_t started = cpu_features_get_time_usec();
      unsigned budget = 32;
      if (s->av1_hdr_yield) { s->av1_hdr_yield = 0; return 0; }
      do
      {
         int ret = rwebm_video_av1_scan_hdr(s);
         if (ret < 0) goto failed;
         if (ret == 1)
         {
            s->av1->hdr = *rwebm_video_av1_hdr_at(s, s->av1_hdr_target);
            s->av1_hdr_active = 0;
            return 0;
         }
         if (ret == 2 || s->waiting) return 0;
         if (cpu_features_get_time_usec() - started >= 4000) break;
      } while (--budget);
      return 0;
   }
   if (s->codec == RWEBM_CODEC_AV1 && s->pending)
   {
      int64_t started = cpu_features_get_time_usec();
      unsigned budget = 32;
      do
      {
         int ret = rwebm_video_av1_step(s, NULL, 1);
         if (ret < 0) goto failed;
         if (ret == 2 || cpu_features_get_time_usec() - started >= 4000)
            return 0;
      } while (s->pending && --budget);
      if (s->pending) return 0;
   }
#endif
#ifdef HAVE_RVP9
   if (s->hw && s->pending)
   {
      if (rwebm_video_hw_step(s, NULL, 1) < 0) goto failed;
      return 0;
   }
#endif
   for (walked = 0; walked < 256; walked++)
   {
      rwebm_packet packet;
      size_t floor = rwebm_consumed(s->demux);
      int ret = rwebm_video_read_packet(s, &packet, &floor);
      if (ret == RWEBM_READ_AGAIN)
      {
         s->waiting = 1;
         return 0;
      }
      if (ret != 1) goto failed;
      if (packet.track != s->track) continue;
      if (!s->seek_key)
      {
         if (!packet.keyframe) continue;
         s->seek_key = 1;
      }
      if (packet.timestamp >= s->seek_target
            && rwebm_video_packet_shown(s, &packet))
      {
         int lo = 0, hi = s->ts_count;
         while (lo < hi)
         {
            int mid = lo + (hi - lo) / 2;
            if (s->ts[mid] < packet.timestamp) lo = mid + 1;
            else hi = mid;
         }
         s->pkt_idx = lo;
         s->seek_packet = packet;
         s->seek_floor = floor;
         s->seek_ready = 1;
         s->seek_active = 0;
         if (position_us) *position_us = packet.timestamp / 1000;
         return 1;
      }
#ifdef HAVE_RAV1
      if (s->codec == RWEBM_CODEC_AV1)
      {
         int lo = 0, hi = s->ts_count;
         while (lo < hi)
         {
            int mid = lo + (hi - lo) / 2;
            if (s->ts[mid] < packet.timestamp) lo = mid + 1;
            else hi = mid;
         }
         s->pending_packet = packet;
         s->pending_floor = floor;
         s->pending_idx = lo;
         s->pkt_idx = lo + 1;
         s->pending = 1;
         s->pending_started = 0;
         s->timing_active = s->timing_ready = 0;
         return 0;
      }
#endif
#ifdef HAVE_RVP9
      if (s->hw)
      {
         int lo = 0, hi = s->ts_count;
         while (lo < hi)
         {
            int mid = lo + (hi - lo) / 2;
            if (s->ts[mid] < packet.timestamp) lo = mid + 1;
            else hi = mid;
         }
         s->pkt_idx = lo;
         s->lookahead_packet = packet;
         s->lookahead_floor = floor;
         s->lookahead_ready = 1;
         if (rwebm_video_hw_step(s, NULL, 1) < 0) goto failed;
         return 0;
      }
#endif
      ret = rwebm_video_decode_packet(s, &packet, 1);
      if (ret < 0) goto failed;
      if (ret > 0)
      {
         if (!s->have_decode_time || packet.timestamp > s->decode_time_ns)
            s->decode_time_ns = packet.timestamp;
         s->have_decode_time = 1;
      }
      return 0;
   }
   return 0;
failed:
   s->have_decode_time = 0;
#ifdef HAVE_RAV1
   if (s->av1_hdr_active)
   {
      s->av1_hdr_offset = 0;
      s->av1_hdr_work = *rwebm_video_av1_hdr_at(s,
            s->av1_hdr_pending ? (size_t)(s->av1_hdr_packet.data - s->buf)
            : s->av1_hdr_done);
      s->av1_hdr_active = 0;
      if (s->av1) s->av1->error = -1;
   }
#endif
   s->seek_active = s->seek_scan = 0;
   return -1;
}

/* ------------------------------------------------------------------ */
/* Still image (first displayed frame)                                 */
/* ------------------------------------------------------------------ */
rwebm_video_t *rwebm_video_alloc(void)
{
   return (rwebm_video_t*)calloc(1, sizeof(rwebm_video_t));
}

void rwebm_video_free(rwebm_video_t *webm)
{
   if (!webm)
      return;
   if (webm->stream)
      rwebm_video_stream_close(webm->stream);
   free(webm->still_fp16);
   free(webm);
}

void rwebm_video_set_want_fp16(rwebm_video_t *webm, int want)
{
   if (webm)
      webm->want_fp16 = want ? 1 : 0;
}

bool rwebm_video_is_fp16(const rwebm_video_t *webm)
{
   return webm && webm->last_fp16;
}

rwebm_video_stream_t *rwebm_video_detach_stream(rwebm_video_t *webm)
{
   rwebm_video_stream_t *s;
   if (!webm || !webm->stream)
      return NULL;
   /* A stream mid-build in the sliced still decode is not complete
    * (pre-scan or decoder setup may be pending); never hand it out. */
   if (webm->still_stage != RWEBM_VIDEO_STILL_IDLE)
      return NULL;
   s            = webm->stream;
   webm->stream = NULL;
   /* The animation consumers upload plain 8-bit frames; the still may
    * have been decoded 10-bit, so drop the request before handing the
    * stream over (each frame re-blits, no stale pixels survive).  The
    * channel order likewise resets to the documented default; adopters
    * re-select per frame via rwebm_video_stream_set_argb. */
   s->want10    = 0;
   s->is10      = 0;
   s->emit_argb = 0;
   return s;
}

bool rwebm_video_set_buf_ptr(rwebm_video_t *webm, void *data, size_t len)
{
   if (!webm)
      return false;
   webm->buf = (const uint8_t*)data;
   webm->len = len;
   return true;
}

/* Ask the thumbnail decoder to emit packed XRGB2101010 for 10-bit HDR
 * sources (it silently keeps 8-bit output for 8-bit sources). */
void rwebm_video_set_want_10bit(rwebm_video_t *webm, int want)
{
   if (webm)
      webm->want10 = want ? 1 : 0;
}

void rwebm_video_set_avail(rwebm_video_t *webm, size_t avail)
{
   if (!webm)
      return;
   webm->partial = 1;
   if (avail > webm->len)
      avail = webm->len;
   /* An exact store: see rmp4_set_avail.  The bound is "readable
    * right now"; a windowed feeder lowers it after a loop's rewind,
    * and a raise-only mirror here would keep handing the stream its
    * stale high-water mark - reads of decommitted pages one lap in. */
   webm->avail = avail;
   if (webm->stream)
      rwebm_video_stream_set_avail(webm->stream, webm->avail);
}

/* True if the last rwebm_video_process_image() wrote packed XRGB2101010. */
bool rwebm_video_is_10bit(const rwebm_video_t *webm)
{
   return webm && webm->last_10bit;
}

/* Sliced still-image decode: each call performs one bounded unit of
 * work and returns IMAGE_PROCESS_NEXT until the first displayed frame
 * is ready, at which point the pixels are handed out and
 * IMAGE_PROCESS_END is returned - the contract rpng established, which
 * task_image drives against a per-display-frame time budget.  The
 * slices are: stream open (demuxer + track selection), pre-scan in
 * RWEBM_VIDEO_SCAN_SLICE-packet chunks, decoder setup, then one coded
 * packet per call (VP9 alt-refs and other non-shown frames each take
 * their own slice) until a picture is displayed.  Previously all of
 * this ran in a single process call, which stalled every sibling task
 * behind the decode (and the main loop, on builds without a threaded
 * task queue). */
int rwebm_video_process_image(rwebm_video_t *webm, void **buf,
      size_t len, unsigned *width, unsigned *height, bool supports_rgba)
{
   rwebm_video_stream_t *s;
   const uint32_t *frame;
   uint32_t *out;
   size_t n;
   int r;
   int duration_ms = 0;

   (void)len;

   if (!webm || !webm->buf || !buf)
      return IMAGE_PROCESS_ERROR;

   /* A partial reader raises webm->avail between calls; push it down
    * so blocked stages can make progress this call. */
   if (webm->stream && webm->partial)
      rwebm_video_stream_set_avail(webm->stream, webm->avail);

   switch (webm->still_stage)
   {
      case RWEBM_VIDEO_STILL_IDLE:
      {
         int    need_more = 0;
         size_t avail     = webm->partial ? webm->avail : webm->len;
         /* Defensive: a repeated process call re-opens from scratch. */
         if (webm->stream)
         {
            rwebm_video_stream_close(webm->stream);
            webm->stream = NULL;
         }
         if (!(webm->stream = rwebm_video_stream_open_begin(
               webm->buf, webm->len, avail, &need_more, 0)))
            /* The EBML header/Tracks are still arriving: wait for more
             * bytes rather than failing. */
            return need_more ? IMAGE_PROCESS_WAIT : IMAGE_PROCESS_ERROR;
         webm->still_stage = RWEBM_VIDEO_STILL_SCAN;
         return IMAGE_PROCESS_NEXT;
      }

      case RWEBM_VIDEO_STILL_SCAN:
      {
         int sr = rwebm_video_stream_scan_step(webm->stream,
               RWEBM_VIDEO_SCAN_SLICE);
         if (sr < 0) return IMAGE_PROCESS_ERROR;
         if (sr == 0)
            return IMAGE_PROCESS_NEXT;
         if (sr == 2)
         {
            /* The pre-scan hit the partial-read wall.  Timestamps live
             * in the block headers, so unlike MP4 the scan cannot run
             * ahead of the media bytes; rather than hold the still
             * hostage to the whole file, proceed once two frames are
             * known (enough to derive a first-frame duration) with a
             * table truncated at the wall - the per-frame duration
             * helper already degrades to its last-interval estimate
             * past the table's end.  Below two frames the first
             * displayed frame is not decodable anyway: wait. */
            if (webm->stream->num_frames < 2)
               return IMAGE_PROCESS_WAIT;
         }
         if (rwebm_video_stream_open_finish(webm->stream) != 0)
            goto fail;
         /* Request 10-bit output; the stream honours it only for
          * 10-bit sources.  For 8-bit output, have the blit emit the
          * caller's channel order directly (supports_rgba is sampled
          * once per transfer by task_image, so the value seen here
          * holds for the END slice's copy below). */
         webm->stream->want10    = webm->want10;
         webm->stream->emit_argb = supports_rgba ? 0 : 1;
         /* An HDR source for a caller that takes half floats: the
          * stream blits the still straight into the frame handed out,
          * at 8 bytes a pixel. Without the memory it is an ordinary
          * still. */
         free(webm->still_fp16);
         webm->still_fp16 = NULL;
         if (     webm->want_fp16
               && rwebm_video_stream_is_hdr(webm->stream)
               && (webm->still_fp16 = (uint16_t*)malloc(
                     (size_t)webm->stream->width
                     * webm->stream->height * 8)))
         {
            webm->stream->out       = (uint32_t*)webm->still_fp16;
            webm->stream->want_fp16 = 1;
         }
         webm->still_stage       = RWEBM_VIDEO_STILL_DECODE;
         return IMAGE_PROCESS_NEXT;
      }

      case RWEBM_VIDEO_STILL_DECODE:
         break;

      default:
         return IMAGE_PROCESS_ERROR;
   }

   s = webm->stream;
   if ((r = rwebm_video_stream_step(s, &duration_ms, 0)) == 0)
      return IMAGE_PROCESS_NEXT;   /* non-shown frame decoded */
   if (r == 2)
      /* The next block's bytes have not been read yet; nothing was
       * consumed - resume here once more of the file has arrived. */
      return IMAGE_PROCESS_WAIT;
   if (r < 0)
      goto fail;
   frame = s->frame;

   webm->last_10bit = s->is10;
   webm->last_fp16  = s->is_fp16;

   if (webm->still_fp16)
   {
      /* Decoded where it is handed out: half floats from a 10-bit HDR
       * frame, or the 32-bit words of any other, either way in place.
       * The stream lets go of it before it plays on as an animation. */
      out               = (uint32_t*)webm->still_fp16;
      webm->still_fp16  = NULL;
      s->out            = NULL;
      s->want_fp16      = 0;
   }
   else
   {
      n = (size_t)s->width * s->height;
      if (!(out = (uint32_t*)malloc(n * sizeof(uint32_t))))
         goto fail;

      /* The canvas is already in the caller's channel order: 10-bit
       * output is packed XRGB2101010, and the 8-bit blit emitted ARGB
       * or ABGR words per supports_rgba (set at the scan/decode
       * transition above), so the copy is verbatim in every case - the
       * per-pixel R/B swizzle this replaced was a full extra pass over
       * the frame. */
      memcpy(out, frame, n * sizeof(uint32_t));
   }

   if (width)
      *width  = s->width;
   if (height)
      *height = s->height;
   *buf = out;

   /* Keep the stream (positioned just past the first displayed frame)
    * so the caller can detach it and continue the video as an
    * animation without re-opening the file.  If nobody detaches it,
    * rwebm_video_free closes it. */
   webm->still_stage = RWEBM_VIDEO_STILL_IDLE;
   return IMAGE_PROCESS_END;

fail:
   rwebm_video_stream_close(webm->stream);
   webm->stream      = NULL;
   webm->still_stage = RWEBM_VIDEO_STILL_IDLE;
   free(webm->still_fp16);
   webm->still_fp16  = NULL;
   return IMAGE_PROCESS_ERROR;
}
