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

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rwebm.h>
#include <formats/rvp8.h>
#ifdef HAVE_RVP9
#include <formats/rvp9.h>
#endif
#include <formats/rwebm_video.h>

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
#endif
   uint32_t    *frame;      /* width * height ABGR words              */
   int64_t     *ts;         /* pre-scanned packet timestamps (ns)     */
   int          ts_count;   /* entries stored in ts                   */
   int          num_frames; /* total video packets in the stream      */
   int          pkt_idx;    /* ordinal of the next video packet       */
   int          track;      /* index of the chosen video track        */
   enum rwebm_codec codec;
   unsigned     width;
   unsigned     height;
};

struct rwebm_video
{
   const uint8_t *buf;
   size_t         len;
};

/* ------------------------------------------------------------------ */
/* BT.601 limited-range I420 -> ABGR words (memory R,G,B,A on LE),     */
/* the packing the animated-WebP stream emits.                         */
/* ------------------------------------------------------------------ */
static INLINE uint32_t rwebm_video_yuv_px(int y, int u, int v)
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

static void rwebm_video_blit_i420(uint32_t *dst, unsigned dst_stride,
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
      unsigned i;
      for (i = 0; i < w; i++)
         dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1]);
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
      case RWEBM_CODEC_VP8:
         if (!(s->vp8 = rvp8_video_open()))
            return false;
         return true;
#ifdef HAVE_RVP9
      case RWEBM_CODEC_VP9:
         if (!(s->vp9 = (rvp9_dec*)calloc(1, sizeof(*s->vp9))))
            return false;
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
#ifdef HAVE_RVP9
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
rwebm_video_stream_t *rwebm_video_stream_open(const uint8_t *buf,
      size_t len)
{
   rwebm_video_stream_t *s;
   const rwebm_track *trk = NULL;
   rwebm_packet pkt;
   int i, num_tracks;

   if (!buf || !len)
      return NULL;

   if (!(s = (rwebm_video_stream_t*)calloc(1, sizeof(*s))))
      return NULL;

   if (!(s->demux = rwebm_open_memory(buf, len)))
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
    * frame durations come straight from the container. This walks block
    * headers only (no decode). */
   if (!(s->ts = (int64_t*)malloc(RWEBM_VIDEO_MAX_TS * sizeof(int64_t))))
      goto fail;
   while (rwebm_read_packet(s->demux, &pkt) == 1)
   {
      if (pkt.track != s->track)
         continue;
      if (s->num_frames < RWEBM_VIDEO_MAX_TS)
         s->ts[s->ts_count++] = pkt.timestamp;
      s->num_frames++;
   }
   if (s->num_frames < 1)
      goto fail;
   rwebm_rewind(s->demux);

   if (!rwebm_video_stream_open_decoder(s))
      goto fail;

   if (!(s->frame = (uint32_t*)malloc(
         (size_t)s->width * s->height * sizeof(uint32_t))))
      goto fail;

   return s;

fail:
   rwebm_video_stream_close(s);
   return NULL;
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

/* Display duration of packet 'idx', in ms, from the pre-scanned
 * timestamp table; 0 when unknown (caller applies its default). */
static int rwebm_video_duration_ms(const rwebm_video_stream_t *s, int idx)
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
static int rwebm_video_decode_packet(rwebm_video_stream_t *s,
      const rwebm_packet *pkt)
{
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
         rwebm_video_blit_i420(s->frame, s->width, w, h,
               fb->y, s->vp9->ys, fb->u, fb->v, s->vp9->uvs);
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
      y = rvp8_video_plane(s->vp8, 0, &ys, &w, &h);
      u = rvp8_video_plane(s->vp8, 1, &uvs, &cw, &ch);
      v = rvp8_video_plane(s->vp8, 2, &uvs, &cw, &ch);
      if (!y || !u || !v)
         return -1;
      if ((unsigned)w > s->width)
         w = (int)s->width;
      if ((unsigned)h > s->height)
         h = (int)s->height;
      rwebm_video_blit_i420(s->frame, s->width,
            (unsigned)w, (unsigned)h, y, ys, u, v, uvs);
      return 1;
   }
   return -1;
}

const uint32_t *rwebm_video_stream_next(rwebm_video_stream_t *s,
      int *duration_ms)
{
   rwebm_packet pkt;

   if (!s)
      return NULL;

   while (rwebm_read_packet(s->demux, &pkt) == 1)
   {
      int idx, r;
      if (pkt.track != s->track)
         continue;
      idx = s->pkt_idx++;
      r   = rwebm_video_decode_packet(s, &pkt);
      if (r < 0)
         return NULL;    /* decode error: end the animation */
      if (r == 0)
         continue;       /* non-shown frame: keep going      */
      if (duration_ms)
         *duration_ms = rwebm_video_duration_ms(s, idx);
      return s->frame;
   }
   return NULL;           /* end of one pass */
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
   free(webm);
}

bool rwebm_video_set_buf_ptr(rwebm_video_t *webm, void *data, size_t len)
{
   if (!webm)
      return false;
   webm->buf = (const uint8_t*)data;
   webm->len = len;
   return true;
}

int rwebm_video_process_image(rwebm_video_t *webm, void **buf,
      size_t len, unsigned *width, unsigned *height, bool supports_rgba)
{
   rwebm_video_stream_t *s;
   const uint32_t *frame;
   uint32_t *out;
   size_t i, n;
   int duration_ms = 0;

   (void)len;

   if (!webm || !webm->buf || !buf)
      return IMAGE_PROCESS_ERROR;

   if (!(s = rwebm_video_stream_open(webm->buf, webm->len)))
      return IMAGE_PROCESS_ERROR;

   if (!(frame = rwebm_video_stream_next(s, &duration_ms)))
   {
      rwebm_video_stream_close(s);
      return IMAGE_PROCESS_ERROR;
   }

   n = (size_t)s->width * s->height;
   if (!(out = (uint32_t*)malloc(n * sizeof(uint32_t))))
   {
      rwebm_video_stream_close(s);
      return IMAGE_PROCESS_ERROR;
   }

   if (supports_rgba)
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

   rwebm_video_stream_close(s);
   return IMAGE_PROCESS_END;
}
