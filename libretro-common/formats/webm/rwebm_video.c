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
#ifdef HAVE_RVP9
#include <formats/rvp9_hw.h>
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
 * pathological stream cannot balloon it. Frames past the cap reuse the
 * last stored delta. */
#define RWEBM_VIDEO_MAX_TS       8192

/* A VP9 superframe carries at most 8 sub-frames. */
#define RWEBM_VIDEO_MAX_SUPER    8

struct rwebm_video_stream
{
   rwebm_t     *demux;
   rvp8_video  *vp8;
#ifdef HAVE_RVP9
   rvp9_dec    *vp9;
   rvp9_hw_session_t *hw;
   rvp9_hw_backend_t hw_backend;
   size_t super_sizes[RWEBM_VIDEO_MAX_SUPER], super_offset;
   int super_count, super_index, last_show, submitted;
#endif
#ifdef HAVE_RAV1
   rav1_hw_stream_t *av1;
#endif
#if defined(HAVE_RVP9) || defined(HAVE_RAV1)
   uint32_t hw_marker;
   video_hw_frame_t hw_frame;
   const uint8_t *buf;
   size_t len, avail, pending_floor;
   rwebm_packet pending_packet;
   int pending, pending_idx;
   int hw_want, hw_untaken, hw_skip;
   unsigned hw_yuv_mask, hw_yuv;
   video_color_metadata_t hw_color;
#endif
   int waiting;
   uint32_t    *frame;      /* width * height ABGR words              */
   uint32_t    *out;        /* caller's frame to blit into instead of
                               'frame'; NULL for the stream's own    */
   void        *blit_pool;  /* tpool_t the blit's row bands run on   */
   unsigned     out_stride; /* caller row stride in pixels, 0: width */
   unsigned     blit_bands; /* how many; <= 1 blits on this thread   */
   int64_t     *ts;         /* pre-scanned packet timestamps (ns)     */
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
static INLINE uint32_t rwebm_video_yuv_px(int y, int u, int v,
      const int16_t *k, int argb)
{
   int c = 298 * (y - 16);
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
static bool rwebm_video_stream_open_decoder(rwebm_video_stream_t *s)
{
   switch (s->codec)
   {
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
   if (s->vp8)
   {
      rvp8_video_close(s->vp8);
      s->vp8 = NULL;
   }
#if defined(HAVE_RVP9) || defined(HAVE_RAV1)
   if (s->hw_untaken) video_hw_frame_abandon(&s->hw_frame);
   s->hw_untaken = 0;
   memset(&s->hw_frame, 0, sizeof(s->hw_frame));
   s->pending = s->waiting = 0;
#endif
#ifdef HAVE_RAV1
   if (s->av1)
   {
      rav1_hw_stream_close(s->av1);
      free(s->av1);
      s->av1 = NULL;
   }
#endif
#ifdef HAVE_RVP9
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
#if defined(HAVE_RVP9) || defined(HAVE_RAV1)
   s->buf = buf; s->len = len; s->avail = avail > len ? len : avail;
#endif

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

   return s;

fail:
   rwebm_video_stream_close(s);
   return NULL;
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
      int r;
      if (max_packets > 0 && walked >= max_packets)
         return 0;
      r = rwebm_read_packet(s->demux, &pkt);
      if (r == RWEBM_READ_AGAIN)
         return 2;   /* blocked at the partial-read wall; resumable */
      if (r != 1)
         break;
      walked++;
      if (pkt.track != s->track)
         continue;
      s->ts[s->ts_count++] = pkt.timestamp;
      s->num_frames++;
   }
   return 1;
}

static int rwebm_video_stream_open_finish(rwebm_video_stream_t *s)
{
   if (s->num_frames < 1)
      return -1;
   rwebm_rewind(s->demux);

   if (!rwebm_video_stream_open_decoder(s))
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
   rwebm_video_stream_scan_step(s, 0);
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
   if (s->demux)
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
   unsigned matrix, transfer, range, max_cll;
   int argb;
   int kind;                /* 0: 8-bit, 1: hbd to 8-bit, 2: 10-bit,
                               3: HDR to scRGB half floats, whose
                               rows are dst_stride 8-byte pixels */
} rwebm_blit_ctx_t;

static void rwebm_video_blit_rows(void *arg, unsigned row0, unsigned rows)
{
   const rwebm_blit_ctx_t *c = (const rwebm_blit_ctx_t*)arg;
   uint32_t *dst             = c->dst + (size_t)row0 * c->dst_stride;
   switch (c->kind)
   {
      case 0:
         rwebm_video_blit_i420(dst, c->dst_stride, c->w, rows,
               c->y + (size_t)row0 * c->ys, c->ys,
               c->u + (size_t)(row0 >> 1) * c->uvs,
               c->v + (size_t)(row0 >> 1) * c->uvs, c->uvs,
               c->matrix, c->argb);
         break;
      case 1:
         rwebm_video_blit_i420_hbd(dst, c->dst_stride, c->w, rows,
               (const uint16_t*)c->y + (size_t)row0 * c->ys, c->ys,
               (const uint16_t*)c->u + (size_t)(row0 >> 1) * c->uvs,
               (const uint16_t*)c->v + (size_t)(row0 >> 1) * c->uvs, c->uvs,
               c->matrix, c->transfer, c->range, c->max_cll, c->argb);
         break;
      case 2:
         rwebm_video_blit_i420_10bit(dst, c->dst_stride, c->w, rows,
               (const uint16_t*)c->y + (size_t)row0 * c->ys, c->ys,
               (const uint16_t*)c->u + (size_t)(row0 >> 1) * c->uvs,
               (const uint16_t*)c->v + (size_t)(row0 >> 1) * c->uvs, c->uvs,
               c->matrix, c->transfer, c->range, c->max_cll);
         break;
      default:
         rwebm_video_blit_i420_fp16(
               (uint16_t*)c->dst + (size_t)row0 * c->dst_stride * 4,
               c->dst_stride, c->w, rows,
               (const uint16_t*)c->y + (size_t)row0 * c->ys, c->ys,
               (const uint16_t*)c->u + (size_t)(row0 >> 1) * c->uvs,
               (const uint16_t*)c->v + (size_t)(row0 >> 1) * c->uvs, c->uvs,
               c->matrix, c->transfer, c->range);
         break;
   }
}

/* Run the frame's blit, in bands on the stream's pool when it has
 * one. An untagged 8-bit matrix is decided by the frame's height here
 * (rwebm_video_coefs); the HDR blits take theirs from the transfer
 * and lazily build their tables, so those are warmed with a
 * zero-row call before any band can race the build. */
static void rwebm_video_blit_frame(rwebm_video_stream_t *s,
      rwebm_blit_ctx_t *c, unsigned h)
{
   if (c->kind == 0 && !(c->matrix == 1 || c->matrix == 5
            || c->matrix == 6 || c->matrix == 9 || c->matrix == 10))
      c->matrix = h >= 720 ? 1 : 5;
   else if (c->kind != 0 && s->blit_pool && s->blit_bands > 1)
      rwebm_video_blit_rows(c, 0, 0);
   image_blit_bands(s->blit_pool, s->blit_bands, h, 2,
         rwebm_video_blit_rows, c);
}

void rwebm_video_stream_set_avail(rwebm_video_stream_t *s, size_t avail)
{
   if (s)
   {
      rwebm_set_avail(s->demux, avail);
#if defined(HAVE_RVP9) || defined(HAVE_RAV1)
      s->avail = avail > s->len ? s->len : avail;
#endif
   }
}

size_t rwebm_video_stream_media_floor(rwebm_video_stream_t *s)
{
   return s ? rwebm_media_floor(s->demux) : 0;
}

size_t rwebm_video_stream_consumed(rwebm_video_stream_t *s)
{
   if (!s) return 0;
#if defined(HAVE_RVP9) || defined(HAVE_RAV1)
   if (s->pending) return s->pending_floor;
#endif
   return rwebm_consumed(s->demux);
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
   int skip;

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
   skip = s->ts_count;
   while (rwebm_read_packet(d, &pkt) == 1)
   {
      if (pkt.track != s->track)
         continue;
      if (skip > 0)
      {
         skip--;
         continue;
      }
      s->ts[s->ts_count++] = pkt.timestamp;
      s->num_frames++;
      if (s->num_frames >= RWEBM_VIDEO_MAX_TS)
         break;
   }
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

#ifdef HAVE_RAV1
static bool rwebm_video_av1_sequence(rwebm_video_stream_t *s,
      rav1_config_t *config)
{
   rwebm_t *probe;
   rwebm_packet packet;
   unsigned walked;
   bool found = false;
   if (config->sequence.data)
      return !config->sequence.temporal_id && !config->sequence.spatial_id;
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
   return found;
}

static int rwebm_video_av1_open(rwebm_video_stream_t *s)
{
   const rwebm_track *t = rwebm_get_track(s->demux, s->track);
   rav1_config_t config;
   rav1_sequence_t *seq;
   unsigned matrix, transfer;
   if (!t || !rav1_config_parse(&config, t->codec_private,
            t->codec_private_size) || !rwebm_video_av1_sequence(s, &config))
      return 0;
   s->av1 = (rav1_hw_stream_t*)calloc(1, sizeof(*s->av1));
   if (!s->av1)
      return 0;
   rav1_hw_stream_init(s->av1, NULL);
   seq = &s->av1->sequence;
   if (!rav1_config_sequence(seq, &config) || seq->max_width != s->width
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
   if ((matrix != 0 && matrix != 1 && matrix != 5 && matrix != 6)
         || transfer == 16 || transfer == 18
         || !rav1_hw_stream_open_sequence(s->av1))
      goto fail;
   s->hw_color.primaries = seq->color_primaries != 2
      ? seq->color_primaries : t->primaries;
   s->hw_color.transfer = transfer;
   s->hw_color.matrix = matrix;
   s->hw_color.range = seq->color_range ? 2 : 1;
   s->hw_color.mastering = t->mastering;
   s->hw_color.max_cll = t->max_cll;
   s->hw_color.max_fall = t->max_fall;
   s->hw_yuv_mask = IMAGE_YUV_FLAG_BT709 | IMAGE_YUV_FLAG_FULL_RANGE;
   s->hw_yuv = seq->color_range ? IMAGE_YUV_FLAG_FULL_RANGE : 0;
   if (matrix == 1 || (!matrix && s->height >= 720))
      s->hw_yuv |= IMAGE_YUV_FLAG_BT709;
   s->hw_want = 1;
   return 1;
fail:
   rav1_hw_stream_close(s->av1);
   free(s->av1);
   s->av1 = NULL;
   return 0;
}

static int rwebm_video_av1_step(rwebm_video_stream_t *s,
      int *duration_ms, int discard)
{
   video_hw_frame_t frame;
   int ret;
   if (!s->av1)
      return -1;
   if (!s->pending)
   {
      unsigned walked;
      for (walked = 0; walked < RWEBM_VIDEO_SCAN_SLICE; walked++)
      {
         size_t floor = rwebm_consumed(s->demux);
         ret = rwebm_read_packet(s->demux, &s->pending_packet);
         if (ret == RWEBM_READ_AGAIN)
         {
            s->waiting = 1;
            return 2;
         }
         if (ret != 1)
            return -1;
         if (s->pending_packet.track != s->track)
            continue;
         s->pending_floor = (size_t)(s->pending_packet.data - s->buf);
         if (floor < s->pending_floor)
            s->pending_floor = floor;
         s->pending_idx = s->pkt_idx++;
         s->pending = 1;
         if (rav1_hw_stream_begin(s->av1, s->pending_packet.data,
                  s->pending_packet.size))
            return -1;
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
   ret = rav1_hw_stream_step(s->av1, &frame, discard != 0);
   if (ret == RAV1_HW_STREAM_BUSY)
   {
      s->waiting = 1;
      return 2;
   }
   if (ret == RAV1_HW_STREAM_END)
      s->pending = 0;
   if (ret == RAV1_HW_STREAM_PROGRESS || ret == RAV1_HW_STREAM_END)
      return 0;
   if (ret != RAV1_HW_STREAM_FRAME && ret != RAV1_HW_STREAM_DISCARDED)
      return -1;
   if (s->hw_untaken)
      video_hw_frame_abandon(&s->hw_frame);
   s->hw_untaken = ret == RAV1_HW_STREAM_FRAME;
   if (s->hw_untaken)
   {
      frame.yuv = (frame.yuv & ~s->hw_yuv_mask) | s->hw_yuv;
      frame.color = s->hw_color;
      s->hw_frame = frame;
   }
   else
      memset(&s->hw_frame, 0, sizeof(s->hw_frame));
   if (duration_ms)
      *duration_ms = rwebm_video_duration_ms(s, s->pending_idx);
   return 1;
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
   size_t sizes[RWEBM_VIDEO_MAX_SUPER];
   unsigned walked;
   int found = 0;
#ifdef HAVE_RAV1
   if (s && s->codec == RWEBM_CODEC_AV1)
      return s->av1 ? 1 : (enable ? rwebm_video_av1_open(s) : 0);
#endif
   if (!s || s->codec != RWEBM_CODEC_VP9) return 0;
   if (!enable || s->hw) return s->hw != NULL;
   t = rwebm_get_track(s->demux, s->track);
   if (!t || rwebm_video_stream_is_hdr(s) || t->colour_range > 2
         || !(t->matrix_coefficients == 0 || t->matrix_coefficients == 2
            || t->matrix_coefficients == 1 || t->matrix_coefficients == 5
            || t->matrix_coefficients == 6)) return 0;
   probe = rwebm_open_memory_avail(s->buf, s->len, s->avail, NULL);
   if (!probe) return 0;
   candidate = (rvp9_hw_session_t*)calloc(1, sizeof(*candidate));
   if (!candidate) { rwebm_close(probe); return 0; }
   for (walked = 0; walked < RWEBM_VIDEO_MAX_TS; walked++)
   {
      if (rwebm_read_packet(probe, &packet) != 1) break;
      if (packet.track != s->track) continue;
      rwebm_video_vp9_superframe(packet.data, packet.size, sizes, RWEBM_VIDEO_MAX_SUPER);
      found = !rvp9_parse_frame_header(packet.data, sizes[0], NULL, NULL,
            &candidate->pending.header);
      break;
   }
   rwebm_close(probe);
   if (!found || candidate->pending.header.frame_type
         || candidate->pending.header.w != (int)s->width
         || candidate->pending.header.h != (int)s->height
         || !rvp9_hw_backend_open(&candidate->pending.header, 10, &ops, &backend))
   { free(candidate); return 0; }
   if (!rvp9_hw_session_init(candidate, &ops, 10))
   { backend.close(&ops); free(candidate); return 0; }
   if (s->vp9) { rvp9_free(s->vp9); free(s->vp9); s->vp9 = NULL; }
   s->hw = candidate; s->hw_backend = backend; s->hw_want = 1;
   s->hw_color.primaries = t->primaries;
   s->hw_color.transfer = t->transfer_characteristics;
   s->hw_color.matrix = t->matrix_coefficients;
   s->hw_color.range = t->colour_range;
   s->hw_color.mastering = t->mastering;
   s->hw_color.max_cll = t->max_cll;
   s->hw_color.max_fall = t->max_fall;
   s->hw_yuv_mask = s->hw_yuv = 0;
   if (t->matrix_coefficients == 1 || t->matrix_coefficients == 5 || t->matrix_coefficients == 6)
   {
      s->hw_yuv_mask |= IMAGE_YUV_FLAG_BT709;
      if (t->matrix_coefficients == 1) s->hw_yuv |= IMAGE_YUV_FLAG_BT709;
   }
   if (t->colour_range)
   {
      s->hw_yuv_mask |= IMAGE_YUV_FLAG_FULL_RANGE;
      if (t->colour_range == 2) s->hw_yuv |= IMAGE_YUV_FLAG_FULL_RANGE;
   }
   rwebm_rewind(s->demux);
   s->pkt_idx = s->pending = s->waiting = 0;
   return 1;
#else
#ifdef HAVE_RAV1
   if (s && s->codec == RWEBM_CODEC_AV1)
      return s->av1 ? 1 : (enable ? rwebm_video_av1_open(s) : 0);
#endif
   (void)s; (void)enable;
   return 0;
#endif
}

int rwebm_video_stream_hw_frame(rwebm_video_stream_t *s, video_hw_frame_t *out)
{
#if defined(HAVE_RVP9) || defined(HAVE_RAV1)
   if (!s || !out || !s->hw_untaken) return 0;
   *out = s->hw_frame;
   s->hw_untaken = 0;
   memset(&s->hw_frame, 0, sizeof(s->hw_frame));
   return 1;
#else
   (void)s; (void)out;
   return 0;
#endif
}

int rwebm_video_stream_waiting(const rwebm_video_stream_t *s)
{
   return s && s->waiting;
}

#ifdef HAVE_RVP9
static int rwebm_video_hw_step(rwebm_video_stream_t *s, int *duration_ms, int discard)
{
   int ret;
   s->waiting = 0;
   if (s->pending && s->hw_skip != discard) return -1;
   if (!s->pending)
   {
      size_t offset;
      int i;
      if (s->hw_untaken) video_hw_frame_abandon(&s->hw_frame);
      s->hw_untaken = 0;
      memset(&s->hw_frame, 0, sizeof(s->hw_frame));
      for (;;)
      {
         size_t floor = rwebm_consumed(s->demux);
         ret = rwebm_read_packet(s->demux, &s->pending_packet);
         if (ret == RWEBM_READ_AGAIN) { s->waiting = 1; return 2; }
         if (ret != 1) return -1;
         if (s->pending_packet.track != s->track) continue;
         s->pending_floor = (size_t)(s->pending_packet.data - s->buf);
         if (floor < s->pending_floor) s->pending_floor = floor;
         break;
      }
      s->super_count = rwebm_video_vp9_superframe(s->pending_packet.data,
            s->pending_packet.size, s->super_sizes, RWEBM_VIDEO_MAX_SUPER);
      s->last_show = -1;
      for (i = 0, offset = 0; i < s->super_count; i++)
      {
         if (!s->super_sizes[i] || s->super_sizes[i] > s->pending_packet.size - offset)
            return -1;
         /* Profile 0/2: show-existing and show-frame are in the first byte. */
         if (s->pending_packet.data[offset] & 0x0a) s->last_show = i;
         offset += s->super_sizes[i];
      }
      s->pending_idx = s->pkt_idx++;
      s->hw_skip = discard;
      s->super_index = s->submitted = 0; s->super_offset = 0;
      s->pending = 1;
   }
   while (s->super_index < s->super_count)
   {
      if (!s->submitted)
      {
         ret = rvp9_hw_session_submit(s->hw, s->pending_packet.data + s->super_offset,
               s->super_sizes[s->super_index]);
         if (ret == 1) { s->waiting = 1; return 2; }
         if (ret < 0) return -1;
         s->submitted = 1;
      }
      if (s->hw->output_slot >= 0)
      {
         if (discard || s->super_index != s->last_show)
            rvp9_hw_session_discard_output(s->hw);
         else
         {
            ret = rvp9_hw_session_output(s->hw, &s->hw_frame);
            if (ret == 1) { s->waiting = 1; return 2; }
            if (ret < 0) return -1;
            s->hw_frame.yuv = (s->hw_frame.yuv & ~s->hw_yuv_mask) | s->hw_yuv;
            s->hw_frame.color = s->hw_color;
            s->hw_untaken = 1;
         }
      }
      s->super_offset += s->super_sizes[s->super_index++];
      s->submitted = 0;
   }
   s->pending = 0;
   if (duration_ms) *duration_ms = rwebm_video_duration_ms(s, s->pending_idx);
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
      int idx, r;
      r = rwebm_read_packet(s->demux, &pkt);
      if (r == RWEBM_READ_AGAIN)
      {
         s->waiting = 1;
         return 2;   /* not yet resident; nothing consumed - retry
                        after rwebm_video_stream_set_avail */
      }
      if (r != 1)
         break;
      if (pkt.track != s->track)
         continue;
      idx = s->pkt_idx++;
      r   = rwebm_video_decode_packet(s, &pkt, discard);
      if (r < 0)
         return -1;      /* decode error: end the animation */
      if (r == 0)
         return 0;       /* non-shown frame                  */
      if (duration_ms)
         *duration_ms = rwebm_video_duration_ms(s, idx);
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

void rwebm_video_stream_rewind(rwebm_video_stream_t *s)
{
   if (!s)
      return;
   rwebm_rewind(s->demux);
   s->pkt_idx = 0;
   /* The stream restarts at a key frame, so a fresh decoder (empty
    * reference chain, default probabilities) is the correct state. */
   rwebm_video_stream_close_decoder(s);
   rwebm_video_stream_open_decoder(s);
   s->waiting = 0;
#if defined(HAVE_RVP9) || defined(HAVE_RAV1)
   if (s->hw_want) rwebm_video_stream_set_hw(s, 1);
#endif
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
