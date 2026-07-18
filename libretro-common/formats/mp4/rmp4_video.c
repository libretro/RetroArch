/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (rmp4_video.c).
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

/* MP4 video-to-image glue: rmp4 demuxer + rvp8/rvp9 decoders exposed
 * through the still-image and streaming-animation contracts that
 * image_transfer.c dispatches on (see rmp4_video.h).  The structure
 * mirrors rwebm_video.c with the demuxer swapped. */

#include <stdlib.h>
#include <string.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rmp4.h>
#include <formats/rvp8.h>
#ifdef HAVE_RVP9
#include <formats/rvp9.h>
/* The 10-bit / HDR I420->RGB blits are shared (image_hdr_blit.c) but still
 * declared in rwebm_video.h; the implementation is demuxer-independent. */
#include <formats/rwebm_video.h>
#endif
#include <formats/rh264.h>
#include <formats/rmp4_video.h>

/* Per-packet timestamps are pre-scanned at open so every frame's display
 * duration is known without lookahead decoding; cap the table so a
 * pathological stream cannot balloon it. Frames past the cap reuse the
 * last stored delta. */
#define RMP4_VIDEO_MAX_TS       8192

/* A VP9 superframe carries at most 8 sub-frames. */
#define RMP4_VIDEO_MAX_SUPER    8

struct rmp4_video_stream
{
   rmp4_t      *demux;
   rvp8_video  *vp8;
#ifdef HAVE_RVP9
   rvp9_dec    *vp9;
#endif
   rh264_video *h264;
   uint32_t    *frame;      /* width * height ABGR words              */
   int64_t     *ts;         /* pre-scanned packet timestamps (ns)     */
   int          ts_count;   /* entries stored in ts                   */
   int          num_frames; /* total video packets in the stream      */
   int          pkt_idx;    /* ordinal of the next video packet       */
   int          track;      /* index of the chosen video track        */
   enum rmp4_codec codec;
   unsigned     width;
   unsigned     height;
   int          want10;     /* caller requested 10-bit output         */
   int          is10;       /* last decoded frame written as 10-bit    */
};

struct rmp4_video
{
   const uint8_t *buf;
   size_t         len;
   int            want10;     /* caller requested 10-bit thumbnail output */
   int            last_10bit; /* last processed frame was XRGB2101010 */
};

/* ------------------------------------------------------------------ */
/* BT.601 limited-range I420 -> ABGR words (memory R,G,B,A on LE),     */
/* the packing the animated-WebP stream emits.                         */
/* ------------------------------------------------------------------ */
static INLINE uint32_t rmp4_video_yuv_px(int y, int u, int v)
{
   int c = 298 * (y - 16);
   int d = u - 128;
   int e = v - 128;
   int r = (c + 409 * e + 128) >> 8;
   int g = (c - 100 * d - 208 * e + 128) >> 8;
   int b = (c + 516 * d + 128) >> 8;
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
static void rmp4_video_yuv_row_sse2(uint32_t *dr,
      const uint8_t *yr, const uint8_t *ur, const uint8_t *vr, unsigned w)
{
   const __m128i k16   = _mm_set1_epi16(16);
   const __m128i k128  = _mm_set1_epi16(128);
   const __m128i zero  = _mm_setzero_si128();
   const __m128i ones  = _mm_set1_epi16(1);
   const __m128i a255  = _mm_set1_epi8((char)0xFF);
   /* Packs two int16 coefficients into the int32 lane pmaddwd expects,
    * without shifting a negative value (all arithmetic unsigned). */
#define RMP4_PAIR16(hi, lo) \
   ((int32_t)(((uint32_t)(uint16_t)(int16_t)(hi) << 16) \
            |  (uint32_t)(uint16_t)(int16_t)(lo)))
   const __m128i c_r   = _mm_set1_epi32(RMP4_PAIR16( 409, 298));
   const __m128i c_g1  = _mm_set1_epi32(RMP4_PAIR16(-100, 298));
   const __m128i c_g2  = _mm_set1_epi32(RMP4_PAIR16( 128, -208));
   const __m128i c_b   = _mm_set1_epi32(RMP4_PAIR16( 516, 298));
#undef RMP4_PAIR16
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

      /* Interleave to memory order R,G,B,A (ABGR words) */
      rg  = _mm_unpacklo_epi8(r8, g8);
      ba  = _mm_unpacklo_epi8(b8, a255);
      _mm_storeu_si128((__m128i*)(dr + i),
            _mm_unpacklo_epi16(rg, ba));
      _mm_storeu_si128((__m128i*)(dr + i + 4),
            _mm_unpackhi_epi16(rg, ba));
   }
   for (; i < w; i++)
      dr[i] = rmp4_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1]);
}
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
/* NEON translation of the SSE2 kernel above: identical integer
 * arithmetic (widening multiply-accumulate into i32, arithmetic shift,
 * saturating narrows for the clamp), so results are byte-identical to
 * the scalar path. */
static void rmp4_video_yuv_row_neon(uint32_t *dr,
      const uint8_t *yr, const uint8_t *ur, const uint8_t *vr, unsigned w)
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

      r_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(e),   409), 8);
      r_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(e),  409), 8);
      g_lo = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_lo,
               vget_low_s16(d),  100), vget_low_s16(e),  208), 8);
      g_hi = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_hi,
               vget_high_s16(d), 100), vget_high_s16(e), 208), 8);
      b_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(d),   516), 8);
      b_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(d),  516), 8);

      /* Saturating narrows implement the 0..255 clamp */
      r16 = vcombine_s16(vqmovn_s32(r_lo), vqmovn_s32(r_hi));
      g16 = vcombine_s16(vqmovn_s32(g_lo), vqmovn_s32(g_hi));
      b16 = vcombine_s16(vqmovn_s32(b_lo), vqmovn_s32(b_hi));

      out.val[0] = vqmovun_s16(r16);
      out.val[1] = vqmovun_s16(g16);
      out.val[2] = vqmovun_s16(b16);
      out.val[3] = vdup_n_u8(0xFF);
      vst4_u8((uint8_t*)(dr + i), out);
   }
   for (; i < w; i++)
      dr[i] = rmp4_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1]);
}
#endif

static void rmp4_video_blit_i420(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h,
      const uint8_t *y, int ys,
      const uint8_t *u, const uint8_t *v, int uvs)
{
   unsigned j;
   for (j = 0; j < h; j++)
   {
      const uint8_t *yr = y + (size_t)j * ys;
      const uint8_t *ur = u + (size_t)(j >> 1) * uvs;
      const uint8_t *vr = v + (size_t)(j >> 1) * uvs;
      uint32_t      *dr = dst + (size_t)j * dst_stride;
#if defined(__SSE2__)
      rmp4_video_yuv_row_sse2(dr, yr, ur, vr, w);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
      rmp4_video_yuv_row_neon(dr, yr, ur, vr, w);
#else
      {
         unsigned i;
         for (i = 0; i < w; i++)
            dr[i] = rmp4_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1]);
      }
#endif
   }
}

/* ------------------------------------------------------------------ */
/* VP9 superframe index (parsed from the trailing marker byte).        */
/* Returns the number of sub-frames and their sizes; 1 = whole chunk.  */
/* ------------------------------------------------------------------ */
#ifdef HAVE_RVP9
static int rmp4_video_vp9_superframe(const uint8_t *data, size_t size,
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
static bool rmp4_video_stream_open_decoder(rmp4_video_stream_t *s)
{
   switch (s->codec)
   {
      case RMP4_CODEC_VP8:
         if (!(s->vp8 = rvp8_video_open()))
            return false;
         return true;
#ifdef HAVE_RVP9
      case RMP4_CODEC_VP9:
         if (!(s->vp9 = (rvp9_dec*)calloc(1, sizeof(*s->vp9))))
            return false;
         return true;
#endif
      case RMP4_CODEC_H264:
      {
         const rmp4_track *t = rmp4_get_track(s->demux, s->track);
         if (!(s->h264 = rh264_video_open()))
            return false;
         if (t && t->codec_private && t->codec_private_size)
            rh264_video_set_extradata(s->h264, t->codec_private,
                  t->codec_private_size);
         return true;
      }
      default:
         break;
   }
   return false;
}

static void rmp4_video_stream_close_decoder(rmp4_video_stream_t *s)
{
   if (s->vp8)
   {
      rvp8_video_close(s->vp8);
      s->vp8 = NULL;
   }
#ifdef HAVE_RVP9
   if (s->vp9)
   {
      rvp9_free(s->vp9);
      free(s->vp9);
      s->vp9 = NULL;
   }
#endif
   if (s->h264)
   {
      rh264_video_close(s->h264);
      s->h264 = NULL;
   }
}

/* ------------------------------------------------------------------ */
/* Streaming animation                                                 */
/* ------------------------------------------------------------------ */
rmp4_video_stream_t *rmp4_video_stream_open(const uint8_t *buf,
      size_t len)
{
   rmp4_video_stream_t *s;
   const rmp4_track *trk = NULL;
   rmp4_packet pkt;
   int i, num_tracks;

   if (!buf || !len)
      return NULL;

   if (!(s = (rmp4_video_stream_t*)calloc(1, sizeof(*s))))
      return NULL;

   if (!(s->demux = rmp4_open_memory(buf, len)))
      goto fail;

   /* Pick the first video track whose codec we can decode. */
   s->track   = -1;
   num_tracks = rmp4_num_tracks(s->demux);
   for (i = 0; i < num_tracks; i++)
   {
      const rmp4_track *t = rmp4_get_track(s->demux, i);
      if (!t || t->type != RMP4_TRACK_VIDEO)
         continue;
      if (t->width < 1 || t->height < 1)
         continue;
      if (t->codec != RMP4_CODEC_VP8
#ifdef HAVE_RVP9
          && t->codec != RMP4_CODEC_VP9
#endif
          && t->codec != RMP4_CODEC_H264
         )
         continue;
      s->track  = i;
      s->codec  = t->codec;
      trk       = t;
      break;
   }
   if (s->track < 0)
      goto fail;

   s->width  = trk->width;
   s->height = trk->height;

   /* Pre-scan: count the track's packets and record their timestamps so
    * frame durations come straight from the container. This walks the
    * sample table only (no decode). */
   if (!(s->ts = (int64_t*)malloc(RMP4_VIDEO_MAX_TS * sizeof(int64_t))))
      goto fail;
   while (rmp4_read_packet(s->demux, &pkt) == 1)
   {
      if (pkt.track != s->track)
         continue;
      if (s->num_frames < RMP4_VIDEO_MAX_TS)
         s->ts[s->ts_count++] = pkt.timestamp;
      s->num_frames++;
   }
   if (s->num_frames < 1)
      goto fail;
   rmp4_rewind(s->demux);

   if (!rmp4_video_stream_open_decoder(s))
      goto fail;

   if (!(s->frame = (uint32_t*)malloc(
         (size_t)s->width * s->height * sizeof(uint32_t))))
      goto fail;

   return s;

fail:
   rmp4_video_stream_close(s);
   return NULL;
}

void rmp4_video_stream_close(rmp4_video_stream_t *s)
{
   if (!s)
      return;
   rmp4_video_stream_close_decoder(s);
   if (s->demux)
      rmp4_close(s->demux);
   free(s->ts);
   free(s->frame);
   free(s);
}

void rmp4_video_stream_get_info(const rmp4_video_stream_t *s,
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

/* Display duration of packet 'idx', in ms, from the pre-scanned
 * timestamp table; 0 when unknown (caller applies its default). */
static int rmp4_video_duration_ms(const rmp4_video_stream_t *s, int idx)
{
   int64_t delta_ns = 0;
   if (idx + 1 < s->ts_count)
      delta_ns = s->ts[idx + 1] - s->ts[idx];
   else if (s->ts_count >= 2)
      delta_ns = s->ts[s->ts_count - 1] - s->ts[s->ts_count - 2];
   if (delta_ns <= 0)
      return 0;
   return (int)(delta_ns / 1000000);
}

/* Decode one demuxed packet into s->frame. Returns 1 when a picture was
 * produced, 0 when the packet decoded but is not displayed, -1 on a
 * decode error. */
static int rmp4_video_decode_packet(rmp4_video_stream_t *s,
      const rmp4_packet *pkt)
{
   /* A decoder can be absent if the re-open in rewind hit OOM. */
   if (!s->vp8
#ifdef HAVE_RVP9
       && !s->vp9
#endif
       && !s->h264
      )
      return -1;
#ifdef HAVE_RVP9
   if (s->codec == RMP4_CODEC_VP9)
   {
      size_t sizes[RMP4_VIDEO_MAX_SUPER];
      const uint8_t *frame = pkt->data;
      int nf = rmp4_video_vp9_superframe(pkt->data, pkt->size,
            sizes, RMP4_VIDEO_MAX_SUPER);
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
         if (s->vp9->hd.bit_depth == 10)
         {
            /* This demuxer does not parse the MP4 'colr' box, so colour
             * metadata is unavailable; pass 0 and let the shared blit pick
             * HD-appropriate defaults (BT.2020-ncl for PQ, BT.709/601 by
             * resolution), matching untagged webm content. Without this
             * branch the 10-bit (uint16) planes were handed to the 8-bit
             * blit and mis-decoded. */
            if (s->want10)
            {
               rwebm_video_blit_i420_10bit(s->frame, s->width, w, h,
                     (const uint16_t*)fb->y, s->vp9->ys,
                     (const uint16_t*)fb->u, (const uint16_t*)fb->v,
                     s->vp9->uvs, 0, 0, 0, 0);
               s->is10 = 1;
            }
            else
               rwebm_video_blit_i420_hbd(s->frame, s->width, w, h,
                     (const uint16_t*)fb->y, s->vp9->ys,
                     (const uint16_t*)fb->u, (const uint16_t*)fb->v,
                     s->vp9->uvs, 0, 0, 0, 0, 1);
         }
         else
            rmp4_video_blit_i420(s->frame, s->width, w, h,
                  fb->y, s->vp9->ys, fb->u, fb->v, s->vp9->uvs);
         return 1;
      }
      return 0;
   }
#endif
   if (s->codec == RMP4_CODEC_VP8)
   {
      const uint8_t *y, *u, *v;
      int ys, uvs, w, h, cw, ch;
      /* VP8 frame tag: bit 4 of byte 0 is show_frame. */
      int shown = (pkt->size > 0) && (pkt->data[0] & 0x10);
      if (rvp8_video_decode(s->vp8, pkt->data, pkt->size) != 0)
         return -1;
      if (!shown)
         return 0;
      y = rvp8_video_plane(s->vp8, 0, &ys, &w, &h);
      u = rvp8_video_plane(s->vp8, 1, &uvs, &cw, &ch);
      v = rvp8_video_plane(s->vp8, 2, &uvs, &cw, &ch);
      if (!y || !u || !v)
         return -1;
      if ((unsigned)w > s->width)
         w = (int)s->width;
      if ((unsigned)h > s->height)
         h = (int)s->height;
      rmp4_video_blit_i420(s->frame, s->width,
            (unsigned)w, (unsigned)h, y, ys, u, v, uvs);
      return 1;
   }
   if (s->codec == RMP4_CODEC_H264)
   {
      const uint8_t *y, *u, *v;
      int ys, uvs, w, h, cw, ch;
      /* rh264 reconstructs IDR key frames and CAVLC-coded P frames, the
       * latter predicted from the previously decoded picture. Anything it
       * still cannot handle (B frames, CABAC-coded P frames) is skipped
       * rather than aborting the stream, so such clips keep animating across
       * their key frames as before. A key frame that fails to decode is a
       * real error. */
      {
         int dec = rh264_video_decode(s->h264, pkt->data, pkt->size);
         if (dec < 0)
            return pkt->keyframe ? -1 : 0;
         if (dec == 0)   /* consumed; picture held for display reordering */
            return 0;
      }
      y = rh264_video_plane(s->h264, 0, &ys,  &w,  &h);
      u = rh264_video_plane(s->h264, 1, &uvs, &cw, &ch);
      v = rh264_video_plane(s->h264, 2, &uvs, &cw, &ch);
      if (!y || !u || !v)
         return -1;
      if ((unsigned)w > s->width)
         w = (int)s->width;
      if ((unsigned)h > s->height)
         h = (int)s->height;
      rmp4_video_blit_i420(s->frame, s->width,
            (unsigned)w, (unsigned)h, y, ys, u, v, uvs);
      return 1;
   }
   return -1;
}

const uint32_t *rmp4_video_stream_next(rmp4_video_stream_t *s,
      int *duration_ms)
{
   rmp4_packet pkt;

   if (!s)
      return NULL;

   while (rmp4_read_packet(s->demux, &pkt) == 1)
   {
      int idx, r;
      if (pkt.track != s->track)
         continue;
      idx = s->pkt_idx++;
      r   = rmp4_video_decode_packet(s, &pkt);
      if (r < 0)
         return NULL;    /* decode error: end the animation */
      if (r == 0)
         continue;       /* non-shown frame: keep going      */
      if (duration_ms)
         *duration_ms = rmp4_video_duration_ms(s, idx);
      return s->frame;
   }
   return NULL;           /* end of one pass */
}

void rmp4_video_stream_rewind(rmp4_video_stream_t *s)
{
   if (!s)
      return;
   rmp4_rewind(s->demux);
   s->pkt_idx = 0;
   /* The stream restarts at a key frame, so a fresh decoder (empty
    * reference chain, default probabilities) is the correct state. */
   rmp4_video_stream_close_decoder(s);
   rmp4_video_stream_open_decoder(s);
}

/* ------------------------------------------------------------------ */
/* Still image (first displayed frame)                                 */
/* ------------------------------------------------------------------ */
rmp4_video_t *rmp4_video_alloc(void)
{
   return (rmp4_video_t*)calloc(1, sizeof(rmp4_video_t));
}

void rmp4_video_free(rmp4_video_t *mp4)
{
   free(mp4);
}

bool rmp4_video_set_buf_ptr(rmp4_video_t *mp4, void *data, size_t len)
{
   if (!mp4)
      return false;
   mp4->buf = (const uint8_t*)data;
   mp4->len = len;
   return true;
}

/* Ask the thumbnail decoder to emit packed XRGB2101010 for 10-bit HDR
 * sources (it keeps 8-bit output for 8-bit sources). */
void rmp4_video_set_want_10bit(rmp4_video_t *mp4, int want)
{
   if (mp4)
      mp4->want10 = want ? 1 : 0;
}

/* True if the last rmp4_video_process_image() wrote packed XRGB2101010. */
bool rmp4_video_is_10bit(const rmp4_video_t *mp4)
{
   return mp4 && mp4->last_10bit;
}

int rmp4_video_process_image(rmp4_video_t *mp4, void **buf,
      size_t len, unsigned *width, unsigned *height, bool supports_rgba)
{
   rmp4_video_stream_t *s;
   const uint32_t *frame;
   uint32_t *out;
   size_t i, n;
   int duration_ms = 0;

   (void)len;

   if (!mp4 || !mp4->buf || !buf)
      return IMAGE_PROCESS_ERROR;

   if (!(s = rmp4_video_stream_open(mp4->buf, mp4->len)))
      return IMAGE_PROCESS_ERROR;

   /* Request 10-bit output; the stream honours it only for 10-bit sources. */
   s->want10 = mp4->want10;

   if (!(frame = rmp4_video_stream_next(s, &duration_ms)))
   {
      rmp4_video_stream_close(s);
      return IMAGE_PROCESS_ERROR;
   }

   mp4->last_10bit = s->is10;

   n = (size_t)s->width * s->height;
   if (!(out = (uint32_t*)malloc(n * sizeof(uint32_t))))
   {
      rmp4_video_stream_close(s);
      return IMAGE_PROCESS_ERROR;
   }

   if (s->is10)
      /* Packed XRGB2101010 already in the frontend's channel order; copy
       * verbatim (no 8-bit R/B swizzle). */
      memcpy(out, frame, n * sizeof(uint32_t));
   else if (supports_rgba)
      memcpy(out, frame, n * sizeof(uint32_t));
   else
   {
      /* ABGR words -> ARGB words (swap R and B channels). */
      for (i = 0; i < n; i++)
      {
         uint32_t px = frame[i];
         out[i]      = (px & 0xFF00FF00u)
                     | ((px & 0xFFu) << 16)
                     | ((px >> 16) & 0xFFu);
      }
   }

   if (width)
      *width  = s->width;
   if (height)
      *height = s->height;
   *buf = out;

   rmp4_video_stream_close(s);
   return IMAGE_PROCESS_END;
}
