/* webm_core -- built-in WebM/MP4 video player core.
 *
 * A dependency-free movie player built entirely on libretro-common.
 * WebM files decode through rwebm (demuxer), rvp9 and, when
 * available, rvp8; MP4 files (when rmp4 is built in) play through the
 * rmp4_video stream glue, which drives H.264 (rh264), VP9 and VP8
 * tracks with display-order reordering and colr-driven conversion.
 * Intended for builds without FFmpeg or MPV -- consoles and other
 * minimal targets -- where it acts as the built-in media player.
 *
 * Audio tracks (Opus via ropus, Vorbis via rvorbis, and for MP4 also
 * AAC via raac) play through the audio_transfer demuxed path when the
 * decoders are built in; emission is paced by the presented video
 * frame's timestamp, which keeps A/V in sync without a separate
 * clock.  Files without a supported audio track fall back to silence
 * to keep frame pacing driven by the audio callback.  VP9 superframes
 * and invisible (non-shown) VP8/VP9 frames are handled.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <formats/data_transfer.h>
#include <formats/rwebm.h>
#include <formats/rwebm_video.h>
#include <formats/rvp9.h>

#ifdef HAVE_RMP4
#include <formats/rmp4.h>
#include <formats/rmp4_video.h>
/* rh264 is built as part of the mp4 stack; with it present, Matroska
 * H.264 tracks (V_MPEG4/ISO/AVC) decode too. */
#define WEBM_HAVE_H264 1
#include <formats/rh264.h>
#endif

#if defined(HAVE_ROPUS) || defined(HAVE_RVORBIS) || defined(HAVE_RAAC)
#define WEBM_HAVE_AUDIO 1
#include <formats/audio.h>
#endif

#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#define WEBM_CORE_PREFIX(s) libretro_webm_##s
#else
#define WEBM_CORE_PREFIX(s) s
#endif

#ifdef HAVE_RWEBP
#include <formats/rvp8.h>
#endif

#define WEBM_AUDIO_RATE 48000

#ifdef WEBM_HAVE_AUDIO
/* Opus packet duration in 48 kHz frames from the TOC (RFC 6716 s3):
 * used to compute the exact decodable total so container end trimming
 * (DiscardPadding) can clamp emission. */
static int64_t webm_opus_pkt_frames(const uint8_t *d, size_t n)
{
   static const int16_t fs[32] = {
      480, 960, 1920, 2880, 480, 960, 1920, 2880,   /* SILK NB/MB      */
      480, 960, 1920, 2880,                         /* SILK WB         */
      480, 960, 480, 960,                           /* hybrid          */
      120, 240, 480, 960, 120, 240, 480, 960,       /* CELT NB/WB      */
      120, 240, 480, 960, 120, 240, 480, 960        /* CELT SWB/FB     */
   };
   int count;
   if (!n)
      return 0;
   switch (d[0] & 3)
   {
      case 0: count = 1; break;
      case 3: count = n >= 2 ? (d[1] & 0x3F) : 0; break;
      default: count = 2; break;
   }
   return (int64_t)fs[d[0] >> 3] * count;
}
#endif

static retro_log_printf_t WEBM_CORE_PREFIX(log_cb);
static retro_video_refresh_t WEBM_CORE_PREFIX(video_cb);
static retro_audio_sample_t WEBM_CORE_PREFIX(audio_cb);
static retro_audio_sample_batch_t WEBM_CORE_PREFIX(audio_batch_cb);
static retro_input_poll_t WEBM_CORE_PREFIX(input_poll_cb);
static retro_input_state_t WEBM_CORE_PREFIX(input_state_cb);
static retro_environment_t WEBM_CORE_PREFIX(environ_cb);

typedef struct
{
   uint8_t     *file_buf;
   int64_t      file_len;
   /* Progressive source: the file arrives through a data_transfer
    * while playback runs (MP4 today; the native WebM path fills it to
    * completion at load).  file_buf borrows the transfer's stable
    * buffer. */
   data_transfer_t *dt;
   int          fill_warned;         /* short-read logged once          */
   rwebm_t     *webm;
#ifdef HAVE_RMP4
   rmp4_video_stream_t *mp4vs;       /* video stream when playing MP4   */
#endif
   int          vtrack;              /* video track index               */
   enum rwebm_codec codec;
   rvp9_dec    *vp9;
#ifdef HAVE_RWEBP
   rvp8_video  *vp8;
#endif
#ifdef WEBM_HAVE_H264
   rh264_video *h264;
   int          wait_key;            /* prediction chain broken: hold  */
#endif
   uint32_t    *fb;                  /* XRGB8888 output                 */
   unsigned     width, height;
   double       fps;
   int          eof;
   int16_t     *silence;
   size_t       silence_frames;
#ifdef WEBM_HAVE_AUDIO
   void        *actx;                /* audio_transfer context          */
   enum audio_type_enum atype;
   int          atrack;              /* audio track index, -1 = none    */
   uint8_t     *apkts;               /* concatenated audio packets      */
   uint32_t    *asizes;
#ifdef HAVE_RMP4
   rmp4_t      *agather;             /* persistent audio-gather demuxer
                                        for the progressive MP4 path    */
   int          agtrack;
#endif
   size_t       apkts_used, apkts_cap;
   size_t       anpkts,     anpkts_cap;
   int64_t      atoc;                /* opus TOC frame sum so far       */
   int64_t      apreskip;
   int64_t      adiscard_ns;         /* webm DiscardPadding sum         */
   uint64_t     atrim;               /* AAC start trim (frames)         */
   unsigned     ach;                 /* decoded channels (1 or 2)       */
   unsigned     arate;
   int64_t      apos;                /* frames emitted so far           */
   int64_t      atotal;              /* emit clamp; <0 = no clamp       */
   int          aeof;
#endif
   rwebm_t     *wgather;             /* progressive native path: one
                                        persistent gather walking both
                                        tracks - video timestamps into
                                        vts, audio packets into the
                                        blob - as far as the fill has
                                        reached                         */
   int          nvts_cap;
   int          vts_unsorted;        /* out-of-order arrival this pump  */
   int64_t      vpts_ns;             /* pts of the last presented frame */
   int64_t      play_ns;             /* wall clock: one frame interval
                                        per retro_run.  Presentation is
                                        paced against it, so variable
                                        frame rate files repeat or drop
                                        frames to keep real time        */
   int64_t      frame_ns;            /* 1e9 / announced fps             */
   int64_t     *vts;                 /* video display timestamps, sorted
                                        (display order); pacing schedule
                                        for the demuxed path            */
   int          nvts;
   int64_t      vshown;              /* display slots presented so far;
                                        under H.264 reordering the pts
                                        clock runs ahead, so restoring a
                                        position is slot-based          */
   int64_t      dur_ns;              /* stream duration, 0 if unknown   */
   unsigned     last_input;          /* previous frame's buttons        */
   int          seeking;             /* suppress presentation           */
   int          pix10;               /* frontend accepted XRGB2101010   */
} webm_player_t;

#if defined(HAVE_RMP4) && defined(WEBM_HAVE_AUDIO)
static void webm_mp4_audio_pump(webm_player_t *p);
#endif
static void webm_native_pump(webm_player_t *p);
static int webm_ts_cmp(const void *a, const void *b);

static webm_player_t webm_player;

/* -------------------------------------------------------------------- */
/* Limited-range I420 -> XRGB8888.  Coefficients follow the Colour       */
/* element's MatrixCoefficients; untagged content defaults to BT.601     */
/* below 720 lines and BT.709 at or above (industry convention).         */
/* -------------------------------------------------------------------- */
static const int16_t webm_coef_601[4]  = { 409, 100, 208, 516 };
static const int16_t webm_coef_709[4]  = { 459,  55, 136, 541 };
static const int16_t webm_coef_2020[4] = { 431,  48, 167, 548 };

static const int16_t *webm_yuv_coefs(unsigned matrix, unsigned height)
{
   switch (matrix)
   {
      case 1:          return webm_coef_709;
      case 5: case 6:  return webm_coef_601;
      case 9: case 10: return webm_coef_2020;
      default:         return height >= 720 ? webm_coef_709 : webm_coef_601;
   }
}

static INLINE uint32_t webm_yuv_px(int y, int u, int v, const int16_t *k)
{
   int c = 298 * (y - 16) + 128;
   int r = (c + k[0] * (v - 128)) >> 8;
   int g = (c - k[1] * (u - 128) - k[2] * (v - 128)) >> 8;
   int b = (c + k[3] * (u - 128)) >> 8;
   if (r < 0) r = 0; else if (r > 255) r = 255;
   if (g < 0) g = 0; else if (g > 255) g = 255;
   if (b < 0) b = 0; else if (b > 255) b = 255;
   return 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8)
      | (uint32_t)b;
}

/* Widen an already-blitted XRGB8888 buffer to XRGB2101010 in place (each
 * channel 8 -> 10 bits via << 2). Used when the frontend was told the format
 * is XRGB2101010 but the decoded stream turned out to be 8-bit (VP8, or VP9
 * profile 0), so the presented buffer matches the announced format. */
static void webm_expand_8888_to_2101010(uint32_t *dst, unsigned stride,
      unsigned w, unsigned h)
{
   unsigned yy, xx;
   for (yy = 0; yy < h; yy++)
   {
      uint32_t *dr = dst + (size_t)yy * stride;
      for (xx = 0; xx < w; xx++)
      {
         uint32_t px = dr[xx];
         uint32_t r8 = (px >> 16) & 0xff;
         uint32_t g8 = (px >>  8) & 0xff;
         uint32_t b8 =  px        & 0xff;
         dr[xx] = ((r8 << 2) << 20)
                | ((g8 << 2) << 10)
                |  (b8 << 2)
                | 0xC0000000u;
      }
   }
}

static void webm_blit_yuv(uint32_t *dst, unsigned w, unsigned h,
      const uint8_t *y, int ys, const uint8_t *u, const uint8_t *v, int uvs,
      unsigned matrix, int cvsh)
{
   const int16_t *k = webm_yuv_coefs(matrix, h);
   unsigned i, j;
   for (j = 0; j < h; j++)
   {
      const uint8_t *yr = y + j * ys;
      const uint8_t *ur = u + (j >> cvsh) * uvs;
      const uint8_t *vr = v + (j >> cvsh) * uvs;
      uint32_t *dr      = dst + j * w;
      for (i = 0; i < w; i++)
         dr[i] = webm_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], k);
   }
}

/* 4:2:0: two luma rows share a chroma row. */
static void webm_blit_i420(uint32_t *dst, unsigned w, unsigned h,
      const uint8_t *y, int ys, const uint8_t *u, const uint8_t *v, int uvs,
      unsigned matrix)
{
   webm_blit_yuv(dst, w, h, y, ys, u, v, uvs, matrix, 1);
}

/* -------------------------------------------------------------------- */
/* VP9 superframe index (spec annex B): an encoded chunk may carry       */
/* several frames (invisible alt-refs plus one shown frame) followed by  */
/* an index whose marker byte 0b110xxxxx appears at both ends.           */
/* Returns the number of frames and their sizes; 1 frame = whole chunk.  */
/* -------------------------------------------------------------------- */
static int webm_vp9_superframe(const uint8_t *data, size_t size,
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

/* -------------------------------------------------------------------- */
/* Decode one demuxed packet; present the shown picture if any.          */
/* Returns 1 when a picture was presented, 0 otherwise, -1 on error.     */
/* -------------------------------------------------------------------- */
static int webm_present_vp9(webm_player_t *p, int show)
{
   const rvp9_fb *fb = &p->vp9->fbs[show];
   unsigned w = (unsigned)fb->w < p->width  ? (unsigned)fb->w : p->width;
   unsigned h = (unsigned)fb->h < p->height ? (unsigned)fb->h : p->height;
   if (p->seeking)   /* discarded during a seek: skip the conversion */
      return 1;
   if (p->vp9->hd.bit_depth == 10)
   {
      const rwebm_track *ct = rwebm_get_track(p->webm, p->vtrack);
      if (p->pix10)
         rwebm_video_blit_i420_10bit(p->fb, p->width, w, h,
            (const uint16_t*)fb->y, p->vp9->ys,
            (const uint16_t*)fb->u, (const uint16_t*)fb->v, p->vp9->uvs,
            ct ? ct->matrix_coefficients : 0,
            ct ? ct->transfer_characteristics : 0,
            ct ? ct->colour_range : 0,
            ct ? ct->max_cll : 0);
      else
         rwebm_video_blit_i420_hbd(p->fb, p->width, w, h,
            (const uint16_t*)fb->y, p->vp9->ys,
            (const uint16_t*)fb->u, (const uint16_t*)fb->v, p->vp9->uvs,
            ct ? ct->matrix_coefficients : 0,
            ct ? ct->transfer_characteristics : 0,
            ct ? ct->colour_range : 0,
            ct ? ct->max_cll : 0, 0);
   }
   else
      {
         const rwebm_track *ct = rwebm_get_track(p->webm, p->vtrack);
         webm_blit_i420(p->fb, w, h, fb->y, p->vp9->ys, fb->u, fb->v,
            p->vp9->uvs, ct ? ct->matrix_coefficients : 0);
         if (p->pix10)
            webm_expand_8888_to_2101010(p->fb, p->width, w, h);
      }
   WEBM_CORE_PREFIX(video_cb)(p->fb, w, h, p->width * sizeof(uint32_t));
   return 1;
}

static int webm_decode_packet(webm_player_t *p, const rwebm_packet *pkt)
{
   if (p->codec == RWEBM_CODEC_VP9)
   {
      size_t sizes[8];
      const uint8_t *frame = pkt->data;
      int nf     = webm_vp9_superframe(pkt->data, pkt->size, sizes, 8);
      int i, r, show, last_show = -1;
      for (i = 0; i < nf; i++)
      {
         r = rvp9_decode_frame(p->vp9, frame, sizes[i], &show);
         if (r < 0)
         {
            if (r == -15 && sizes[i] > 0)
            {
               /* Unsupported profile: name it instead of a bare error.
                * Frame byte 0: marker(2) profile_low profile_high ... */
               unsigned b0      = frame[0];
               unsigned profile = ((b0 >> 5) & 1) | (((b0 >> 4) & 1) << 1);
               const char *desc = (profile >= 2)
                  ? "10/12-bit; used by HDR streams"
                  : "8-bit 4:4:4/4:2:2";
               if (WEBM_CORE_PREFIX(log_cb))
                  WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
                     "[webm] VP9 profile %u (%s) is not supported; "
                     "only profile 0 (8-bit 4:2:0) can be decoded.\n",
                     profile, desc);
               if (WEBM_CORE_PREFIX(environ_cb))
               {
                  struct retro_message msg;
                  msg.msg    = (profile >= 2)
                     ? "10-bit/HDR VP9 (profile 2) is not supported"
                     : "This VP9 profile is not supported";
                  msg.frames = 240;
                  WEBM_CORE_PREFIX(environ_cb)(
                     RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
               }
            }
            else if (WEBM_CORE_PREFIX(log_cb))
               WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
                  "[webm] VP9 decode error %d.\n", r);
            return -1;
         }
         if (show >= 0)
            last_show = show;
         frame += sizes[i];
      }
      if (last_show >= 0)
         return webm_present_vp9(p, last_show);
      return 0;
   }
#ifdef HAVE_RWEBP
   if (p->codec == RWEBM_CODEC_VP8)
   {
      const uint8_t *y, *u, *v;
      int ys, uvs, w, h, cw, ch;
      /* VP8 frame tag: bit 4 of byte 0 is show_frame. */
      int shown = pkt->size > 0 && (pkt->data[0] & 0x10);
      if (rvp8_video_decode(p->vp8, pkt->data, pkt->size) != 0)
      {
         if (WEBM_CORE_PREFIX(log_cb))
            WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
               "[webm] VP8 decode error.\n");
         return -1;
      }
      if (!shown)
         return 0;
      y = rvp8_video_plane(p->vp8, 0, &ys, &w, &h);
      u = rvp8_video_plane(p->vp8, 1, &uvs, &cw, &ch);
      v = rvp8_video_plane(p->vp8, 2, &uvs, &cw, &ch);
      if (!y || !u || !v)
         return -1;
      if ((unsigned)w > p->width)  w = (int)p->width;
      if ((unsigned)h > p->height) h = (int)p->height;
      if (p->seeking)   /* discarded during a seek: skip conversion */
         return 1;
      {
         const rwebm_track *ct = rwebm_get_track(p->webm, p->vtrack);
         webm_blit_i420(p->fb, (unsigned)w, (unsigned)h, y, ys, u, v, uvs,
            ct ? ct->matrix_coefficients : 0);
         if (p->pix10)
            webm_expand_8888_to_2101010(p->fb, p->width,
               (unsigned)w, (unsigned)h);
      }
      WEBM_CORE_PREFIX(video_cb)(p->fb, (unsigned)w, (unsigned)h,
         p->width * sizeof(uint32_t));
      return 1;
   }
#endif
#ifdef WEBM_HAVE_H264
   if (p->codec == RWEBM_CODEC_H264)
   {
      const uint8_t *y, *u, *v;
      int ys, uvs, w, h, cw, ch, dec;
      /* Mirrors the mp4 glue: rh264 hands pictures out in display
       * order; if a frame in a prediction chain fails to decode the
       * following inter frames would drift, so hold on the last good
       * picture until the next key frame restarts the chain. */
      if (p->wait_key && !pkt->keyframe)
         return 0;
      dec = rh264_video_decode(p->h264, pkt->data, pkt->size);
      if (dec < 0)
      {
         if (pkt->keyframe)
         {
            if (WEBM_CORE_PREFIX(log_cb))
               WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
                  "[webm] H.264 key frame decode error.\n");
            return -1;
         }
         p->wait_key = 1;
         return 0;
      }
      p->wait_key = 0;
      if (dec == 0)     /* consumed; picture held for reordering */
         return 0;
      y = rh264_video_plane(p->h264, 0, &ys,  &w,  &h);
      u = rh264_video_plane(p->h264, 1, &uvs, &cw, &ch);
      v = rh264_video_plane(p->h264, 2, &uvs, &cw, &ch);
      if (!y || !u || !v)
         return -1;
      if ((unsigned)w > p->width)  w = (int)p->width;
      if ((unsigned)h > p->height) h = (int)p->height;
      if (p->seeking)   /* discarded during a seek: skip conversion */
         return 1;
      {
         const rwebm_track *ct = rwebm_get_track(p->webm, p->vtrack);
         webm_blit_yuv(p->fb, (unsigned)w, (unsigned)h, y, ys, u, v, uvs,
            ct ? ct->matrix_coefficients : 0,
               (ch < h) ? 1 : 0);
         if (p->pix10)
            webm_expand_8888_to_2101010(p->fb, p->width,
               (unsigned)w, (unsigned)h);
      }
      WEBM_CORE_PREFIX(video_cb)(p->fb, (unsigned)w, (unsigned)h,
         p->width * sizeof(uint32_t));
      return 1;
   }
#endif
   return -1;
}

#ifdef WEBM_HAVE_H264
/* Display reordering can leave the last few pictures queued inside the
 * H.264 decoder at end of stream; present one per call.  Returns 1
 * when a picture was presented. */
static int webm_drain_h264(webm_player_t *p)
{
   const uint8_t *y, *u, *v;
   int ys, uvs, w, h, cw, ch;
   if (!p->h264 || rh264_video_drain(p->h264) != 0)
      return 0;
   y = rh264_video_plane(p->h264, 0, &ys,  &w,  &h);
   u = rh264_video_plane(p->h264, 1, &uvs, &cw, &ch);
   v = rh264_video_plane(p->h264, 2, &uvs, &cw, &ch);
   if (!y || !u || !v)
      return 0;
   if ((unsigned)w > p->width)  w = (int)p->width;
   if ((unsigned)h > p->height) h = (int)p->height;
   if (p->seeking)
      return 1;
   {
      const rwebm_track *ct = rwebm_get_track(p->webm, p->vtrack);
      webm_blit_yuv(p->fb, (unsigned)w, (unsigned)h, y, ys, u, v, uvs,
         ct ? ct->matrix_coefficients : 0,
         (ch < h) ? 1 : 0);
      if (p->pix10)
         webm_expand_8888_to_2101010(p->fb, p->width,
            (unsigned)w, (unsigned)h);
   }
   WEBM_CORE_PREFIX(video_cb)(p->fb, (unsigned)w, (unsigned)h,
      p->width * sizeof(uint32_t));
   return 1;
}
#endif

static int webm_ts_cmp(const void *a, const void *b)
{
   int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
   return x < y ? -1 : x > y ? 1 : 0;
}

static void webm_free_player(webm_player_t *p)
{
   if (p->vp9)
   {
      rvp9_free(p->vp9);
      free(p->vp9);
   }
#ifdef HAVE_RWEBP
   if (p->vp8)
      rvp8_video_close(p->vp8);
#endif
#ifdef WEBM_HAVE_H264
   if (p->h264)
      rh264_video_close(p->h264);
#endif
   if (p->webm)
      rwebm_close(p->webm);
#ifdef HAVE_RMP4
   if (p->mp4vs)
      rmp4_video_stream_close(p->mp4vs);
#endif
#ifdef WEBM_HAVE_AUDIO
   if (p->actx)
      audio_transfer_free(p->actx, p->atype);
   free(p->apkts);
   free(p->asizes);
#if defined(HAVE_RMP4)
   if (p->agather)
      rmp4_close(p->agather);
#endif
#endif
   if (p->wgather)
      rwebm_close(p->wgather);
   free(p->fb);
   free(p->vts);
   free(p->silence);
   if (p->dt)
      data_transfer_free(p->dt);   /* owns file_buf; cancels in-flight */
   else
      free(p->file_buf);
   memset(p, 0, sizeof(*p));
}

/* -------------------------------------------------------------------- */
/* libretro entry points.                                                */
/* -------------------------------------------------------------------- */
void WEBM_CORE_PREFIX(retro_init)(void) { }

void WEBM_CORE_PREFIX(retro_deinit)(void) { }

unsigned WEBM_CORE_PREFIX(retro_api_version)(void)
{
   return RETRO_API_VERSION;
}

void WEBM_CORE_PREFIX(retro_set_controller_port_device)(unsigned port,
      unsigned device)
{
   (void)port;
   (void)device;
}

void WEBM_CORE_PREFIX(retro_get_system_info)(
      struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "webm";
   info->library_version  = "v1";
   info->need_fullpath    = true;
#ifdef HAVE_RMP4
   info->valid_extensions = "webm|mkv|mp4|m4v|mov";
#else
   info->valid_extensions = "webm|mkv";
#endif
}

void WEBM_CORE_PREFIX(retro_get_system_av_info)(
      struct retro_system_av_info *info)
{
   webm_player_t *p = &webm_player;
   memset(info, 0, sizeof(*info));
   info->timing.fps            = p->fps;
   info->timing.sample_rate    = WEBM_AUDIO_RATE;
#ifdef WEBM_HAVE_AUDIO
   if (webm_player.actx)
      info->timing.sample_rate = (double)webm_player.arate;
#endif
   info->geometry.base_width   = p->width;
   info->geometry.base_height  = p->height;
   info->geometry.max_width    = p->width;
   info->geometry.max_height   = p->height;
   info->geometry.aspect_ratio = (float)p->width / (float)p->height;
}

void WEBM_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   struct retro_log_callback log;
   WEBM_CORE_PREFIX(environ_cb) = cb;
   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      WEBM_CORE_PREFIX(log_cb) = log.log;
}

void WEBM_CORE_PREFIX(retro_set_video_refresh)(retro_video_refresh_t cb)
{
   WEBM_CORE_PREFIX(video_cb) = cb;
}

void WEBM_CORE_PREFIX(retro_set_audio_sample)(retro_audio_sample_t cb)
{
   WEBM_CORE_PREFIX(audio_cb) = cb;
}

void WEBM_CORE_PREFIX(retro_set_audio_sample_batch)(
      retro_audio_sample_batch_t cb)
{
   WEBM_CORE_PREFIX(audio_batch_cb) = cb;
}

void WEBM_CORE_PREFIX(retro_set_input_poll)(retro_input_poll_t cb)
{
   WEBM_CORE_PREFIX(input_poll_cb) = cb;
}

void WEBM_CORE_PREFIX(retro_set_input_state)(retro_input_state_t cb)
{
   WEBM_CORE_PREFIX(input_state_cb) = cb;
}

void WEBM_CORE_PREFIX(retro_reset)(void)
{
   webm_player_t *p = &webm_player;
   if (p->webm)
      rwebm_rewind(p->webm);
#ifdef HAVE_RMP4
   if (p->mp4vs)
      rmp4_video_stream_rewind(p->mp4vs);
#endif
   if (p->vp9)
   {
      rvp9_free(p->vp9);
      memset(p->vp9, 0, sizeof(*p->vp9));
   }
#ifdef HAVE_RWEBP
   if (p->vp8)
   {
      rvp8_video_close(p->vp8);
      p->vp8 = rvp8_video_open();
   }
#endif
#ifdef WEBM_HAVE_H264
   if (p->h264)
   {
      const rwebm_track *vt = rwebm_get_track(p->webm, p->vtrack);
      rh264_video_close(p->h264);
      p->h264 = rh264_video_open();
      if (p->h264 && vt && vt->codec_private_size)
         rh264_video_set_extradata(p->h264, vt->codec_private,
               vt->codec_private_size);
      p->wait_key = 0;
   }
#endif
#ifdef WEBM_HAVE_AUDIO
   if (p->actx)
      audio_transfer_seek(p->actx, p->atype, 0);
   p->apos    = 0;
   p->aeof    = 0;
#endif
   p->vpts_ns = 0;
   p->vshown  = 0;
   p->play_ns = p->nvts ? p->vts[0] : 0;
   p->eof = 0;
}

/* Seek both streams to about target_ns.  Video restarts at the
 * preceding key frame and decodes forward discarding pictures; audio
 * rewinds and decodes forward discarding samples (every codec here
 * decodes far faster than real time, so even a long skip is a
 * fraction of a second).  Returns the position actually reached. */
/* Seek: when to_slot is negative, target_ns selects the display slot
 * (the last one at or before it); otherwise to_slot is the display
 * slot itself and target_ns only clamps (save-state restore, where
 * the slot is exact but the pts clock may run ahead of it under
 * display reordering). */
static int64_t webm_seek_internal(webm_player_t *p, int64_t target_ns,
      int64_t to_slot)
{
   if (target_ns < 0)
      target_ns = 0;
   if (p->dur_ns > 0 && target_ns > p->dur_ns)
      target_ns = p->dur_ns;

#ifdef HAVE_RMP4
   if (p->mp4vs)
   {
      int64_t ms = rmp4_video_stream_seek_ms(p->mp4vs,
            target_ns / 1000000);
      p->vpts_ns = ms >= 0 ? ms * 1000000 : 0;
      p->play_ns = p->vpts_ns;
      p->eof     = 0;
      (void)to_slot;   /* the mp4 glue's timestamp table is exact */
   }
   else
#endif
   {
      /* Pass 1: the display slot at or before the target (packets
       * with a presentation time below it, invisible VP8 alt-refs
       * excluded), the slot and timestamp of the preceding key frame,
       * and the total shown count for end clamping.  Slot counting
       * rather than packet timestamps keeps this exact under H.264
       * display reordering, where decode order visits timestamps out
       * of sequence. */
      rwebm_packet pkt;
      int64_t kts = 0;
      int     tidx, kf_idx = 0, total = 0, below = 0;
      rwebm_rewind(p->webm);
      while (rwebm_read_packet(p->webm, &pkt) == 1)
      {
         if (pkt.track != p->vtrack)
            continue;
         if (p->codec == RWEBM_CODEC_VP8
               && (!pkt.size || !(pkt.data[0] & 0x10)))
            continue;          /* invisible alt-ref: no display slot   */
         /* counting below-target packets is order-independent, so it
          * gives the target's display slot even though this walk is
          * in decode order */
         if (pkt.timestamp < target_ns)
            below++;
         if (pkt.keyframe
               && (to_slot >= 0 ? (int64_t)total <= to_slot
                                : pkt.timestamp <= target_ns))
         {
            /* at an IDR, everything decoded earlier displays earlier,
             * so the decode position is also the display slot */
            kts    = pkt.timestamp;
            kf_idx = total;
         }
         total++;
      }
      tidx = to_slot >= 0 ? (int)to_slot : below;
      if (tidx < 0)
         tidx = 0;
      if (total == 0)
         return p->vpts_ns;
      if (tidx >= total)
         tidx = total - 1;
      /* Pass 2: fresh decoders; feed from that key frame on,
       * discarding pictures until the target slot's is presented. */
      rwebm_rewind(p->webm);
      if (p->vp9)
      {
         rvp9_free(p->vp9);
         memset(p->vp9, 0, sizeof(*p->vp9));
      }
#ifdef HAVE_RWEBP
      if (p->vp8)
      {
         rvp8_video_close(p->vp8);
         p->vp8 = rvp8_video_open();
      }
#endif
#ifdef WEBM_HAVE_H264
      if (p->h264)
      {
         const rwebm_track *vt = rwebm_get_track(p->webm, p->vtrack);
         rh264_video_close(p->h264);
         p->h264 = rh264_video_open();
         if (p->h264 && vt && vt->codec_private_size)
            rh264_video_set_extradata(p->h264, vt->codec_private,
                  vt->codec_private_size);
         p->wait_key = 0;
      }
#endif
      p->eof = 0;
      {
         int shown = kf_idx;
         p->seeking = shown < tidx;
         while (rwebm_read_packet(p->webm, &pkt) == 1)
         {
            int r;
            if (pkt.track != p->vtrack || pkt.timestamp < kts)
               continue;
            r = webm_decode_packet(p, &pkt);
            if (r < 0)
               break;
            if (r > 0)
            {
               if (!p->seeking)
               {
                  shown = tidx + 1;   /* target slot presented */
                  break;
               }
               shown++;
               p->seeking = shown < tidx;
            }
         }
#ifdef WEBM_HAVE_H264
         /* out of packets with pictures still queued for display
          * reordering: drain up to and including the target slot */
         while (shown <= tidx && p->codec == RWEBM_CODEC_H264)
         {
            p->seeking = shown < tidx;
            if (!webm_drain_h264(p))
               break;
            shown++;
         }
#endif
      }
      p->seeking = 0;
      /* anchor both clocks to the schedule's own timestamp for the
       * slot just presented (a caller's timestamp can run ahead of
       * the display position under reordering); the wall clock sits a
       * frame past it - where the run loop would be at this point in
       * linear playback */
      p->vpts_ns = p->vts && tidx < p->nvts ? p->vts[tidx] : target_ns;
      p->play_ns = p->vpts_ns + p->frame_ns;
      p->vshown  = (int64_t)tidx + 1;
   }

#ifdef WEBM_HAVE_AUDIO
   if (p->actx)
   {
      int64_t want = (int64_t)((double)p->vpts_ns * 1e-9
            * (double)p->arate);
      int16_t scratch[1024 * 2];
      audio_transfer_seek(p->actx, p->atype, 0);
      p->apos = 0;
      p->aeof = 0;
      if (p->atotal >= 0 && want > p->atotal)
         want = p->atotal;
      while (p->apos < want)
      {
         size_t chunk = (size_t)(want - p->apos);
         size_t got   = 0;
         int r;
         if (chunk > 1024)
            chunk = 1024;
         r = audio_transfer_read_s16(p->actx, p->atype, scratch,
               chunk, &got);
         p->apos += (int64_t)got;
         if (r != AUDIO_PROCESS_NEXT || !got)
         {
            p->aeof = 1;
            break;
         }
      }
   }
#endif
   return p->vpts_ns;
}

/* Edge-triggered transport controls: left/right seek 10 seconds,
 * L/R seek a minute. */
static int64_t webm_seek(webm_player_t *p, int64_t target_ns)
{
   return webm_seek_internal(p, target_ns, -1);
}

static void webm_check_input(webm_player_t *p)
{
   unsigned cur = 0;
   int64_t  delta = 0;
   if (WEBM_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_LEFT))
      cur |= 1;
   if (WEBM_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_RIGHT))
      cur |= 2;
   if (WEBM_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_L))
      cur |= 4;
   if (WEBM_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_R))
      cur |= 8;
   if ((cur & 1) && !(p->last_input & 1))
      delta = (int64_t)-10000 * 1000000;
   if ((cur & 2) && !(p->last_input & 2))
      delta = (int64_t)10000 * 1000000;
   if ((cur & 4) && !(p->last_input & 4))
      delta = (int64_t)-60000 * 1000000;
   if ((cur & 8) && !(p->last_input & 8))
      delta = (int64_t)60000 * 1000000;
   p->last_input = cur;
   if (delta)
   {
      int64_t pos = webm_seek(p, p->vpts_ns + delta);
      struct retro_message_ext msg;
      char text[64];
      unsigned ps = (unsigned)(pos / 1000000000);
      sprintf(text, "%u:%02u", ps / 60, ps % 60);
      if (p->dur_ns > 0)
      {
         unsigned ds = (unsigned)(p->dur_ns / 1000000000);
         sprintf(text, "%u:%02u / %u:%02u",
               ps / 60, ps % 60, ds / 60, ds % 60);
      }
      msg.msg      = text;
      msg.duration = 1500;
      msg.priority = 1;
      msg.level    = RETRO_LOG_INFO;
      msg.target   = RETRO_MESSAGE_TARGET_OSD;
      msg.type     = RETRO_MESSAGE_TYPE_STATUS;
      msg.progress = -1;
      WEBM_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE_EXT,
            &msg);
   }
}

void WEBM_CORE_PREFIX(retro_run)(void)
{
   webm_player_t *p = &webm_player;
   rwebm_packet pkt;

   WEBM_CORE_PREFIX(input_poll_cb)();
   webm_check_input(p);

#ifdef HAVE_RMP4
   if (p->mp4vs)
   {
      /* Progressive fill: a budget of file bytes per run - orders of
       * magnitude above any realtime bitrate, so starvation is a
       * startup transient at most - then the arrivals fan out to the
       * video stream and the audio gather. */
      if (p->dt && !data_transfer_complete(p->dt)
            && !data_transfer_failed(p->dt))
      {
         size_t avail = data_transfer_iterate(p->dt, 4 * 1024 * 1024);
         rmp4_video_stream_set_avail(p->mp4vs, avail);
#ifdef WEBM_HAVE_AUDIO
         if (p->agather)
         {
            rmp4_set_avail(p->agather, avail);
            webm_mp4_audio_pump(p);
         }
#endif
         if (data_transfer_failed(p->dt) && !p->fill_warned)
         {
            p->fill_warned = 1;
            if (WEBM_CORE_PREFIX(log_cb))
               WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_WARN,
                  "[webm] file read ended short; playing what "
                  "arrived.\n");
         }
      }
      /* Pacing: one frame interval of wall time per run, advancing
       * the stream while the next frame's timestamp (the running
       * duration sum) falls inside it, with half a frame of tolerance
       * for millisecond quantisation.  Constant-rate files advance
       * exactly one frame per run; variable-rate files hold the
       * picture through slow stretches and pass through several
       * frames in fast ones, converting only the last. */
      p->play_ns += p->frame_ns;
      if (!p->eof)
      {
         int have = 0;
         while (!p->eof
               && p->vpts_ns + p->frame_ns / 2 <= p->play_ns)
         {
            int dur_ms = 0;
            /* Pass-over frames are consumed without colour
             * conversion; only the frame actually presented is
             * rendered, below.  In fast variable-rate stretches this
             * saves a full-resolution blit per dropped frame. */
            int r = rmp4_video_stream_skip(p->mp4vs, &dur_ms);
            if (r == 2 && p->dt && !data_transfer_complete(p->dt)
                  && !data_transfer_failed(p->dt))
               /* the next sample has not arrived: hold the picture
                * (the wall clock keeps moving, so the catch-up next
                * run passes through what it missed) */
               break;
            if (r != 1)
            {
               p->eof = 1;
               break;
            }
            have        = 1;
            p->vpts_ns += (int64_t)dur_ms * 1000000;
            p->vshown++;
         }
         if (have)
         {
            /* The stream emits XRGB8888 words directly (selected at
             * open via rmp4_video_stream_set_argb), so the copy into
             * the core's framebuffer is verbatim. */
            const uint32_t *frame = rmp4_video_stream_render(p->mp4vs);
            if (frame)
               memcpy(p->fb, frame,
                     (size_t)p->width * p->height * sizeof(uint32_t));
         }
      }
      WEBM_CORE_PREFIX(video_cb)(p->fb, p->width, p->height,
            p->width * sizeof(uint32_t));
      if (p->eof)
      {
         int audio_done = 1;
#ifdef WEBM_HAVE_AUDIO
         if (p->actx && !p->aeof)
            audio_done = 0;
#endif
         if (audio_done)
            WEBM_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      }
      goto audio;
   }
#endif

   /* Progressive fill for the native path: budget, then fan the
    * arrivals to the playback demuxer and the gather. */
   if (p->dt && !data_transfer_complete(p->dt)
         && !data_transfer_failed(p->dt))
   {
      size_t avail = data_transfer_iterate(p->dt, 4 * 1024 * 1024);
      rwebm_set_avail(p->webm, avail);
      if (p->wgather)
      {
         rwebm_set_avail(p->wgather, avail);
         webm_native_pump(p);
      }
      if (data_transfer_failed(p->dt) && !p->fill_warned)
      {
         p->fill_warned = 1;
         if (WEBM_CORE_PREFIX(log_cb))
            WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_WARN,
               "[webm] file read ended short; playing what arrived.\n");
      }
   }

   if (!p->eof)
   {
      int presented = 0, shown_now = 0;
      int64_t target_slot = p->vshown - 1;
      /* Pacing: one frame interval of wall time per run; the last
       * display slot whose timestamp (with half a frame of tolerance
       * for container rounding) falls inside it is the one to show.
       * Constant-rate files advance exactly one slot per run;
       * variable-rate files repeat the current picture through slow
       * stretches and drop intermediates through fast ones. */
      p->play_ns += p->frame_ns;
      while (target_slot + 1 < p->nvts
            && p->vts[target_slot + 1] + p->frame_ns / 2 <= p->play_ns)
         target_slot++;
      while (p->vshown <= target_slot)
      {
         int r = rwebm_read_packet(p->webm, &pkt);
         if (r == 2 && p->dt && !data_transfer_complete(p->dt)
               && !data_transfer_failed(p->dt))
            /* the next block has not arrived: hold the picture; the
             * catch-up next run passes through what it missed */
            break;
         if (r != 1)
         {
#ifdef WEBM_HAVE_H264
            /* out of packets: hand out the pictures display reordering
             * left queued before ending the stream */
            if (p->codec == RWEBM_CODEC_H264)
            {
               p->seeking = p->vshown < target_slot;
               if (webm_drain_h264(p))
               {
                  if (!p->seeking)
                     shown_now = 1;
                  presented = 1;
                  p->vshown++;
                  continue;
               }
               p->seeking = 0;
            }
#endif
            p->eof = 1;
            break;
         }
         if (pkt.track != p->vtrack)
            continue;
         /* dropped intermediates skip colour conversion, like the
          * seek catch-up */
         p->seeking = p->vshown < target_slot;
         presented = webm_decode_packet(p, &pkt);
         if (presented < 0)
         {
            p->seeking = 0;
            p->eof = 1;
            break;
         }
         /* With H.264 display reordering the decoded packet's
          * timestamp can lag the displayed picture's; keep the clock
          * monotonic. */
         if (presented > 0)
         {
            if (!p->seeking)
               shown_now = 1;
            p->vshown++;
            if (pkt.timestamp > p->vpts_ns)
               p->vpts_ns = pkt.timestamp;
         }
      }
      p->seeking = 0;
      if (!shown_now)   /* nothing due this run: hold the picture */
         WEBM_CORE_PREFIX(video_cb)(p->fb, p->width, p->height,
            p->width * sizeof(uint32_t));
      /* every display slot shown and a further interval elapsed: the
       * stream is over (the packet reader no longer runs each frame,
       * so it cannot notice on its own) */
      if (p->nvts > 0 && p->vshown >= p->nvts && !shown_now
            && (!p->dt || data_transfer_complete(p->dt)
               || data_transfer_failed(p->dt)))
         p->eof = 1;
   }
   else
   {
      int audio_done = 1;
#ifdef WEBM_HAVE_AUDIO
      if (p->actx && !p->aeof)
         audio_done = 0;
#endif
      WEBM_CORE_PREFIX(video_cb)(p->fb, p->width, p->height,
         p->width * sizeof(uint32_t));
      if (audio_done)
         WEBM_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
   }

#ifdef HAVE_RMP4
audio:
#endif
#ifdef WEBM_HAVE_AUDIO
   if (p->actx && !p->aeof)
   {
      /* Emit decoded audio up to the presented frame's timestamp plus
       * one nominal frame of lead; at video EOF, drain about a frame
       * interval per run so the frontend's audio pacing continues to
       * throttle us through the tail. */
      int16_t buf[1024 * 2];
      int16_t st[1024 * 2];
      /* the wall clock, not the picture's timestamp: through a slow
       * stretch of a variable-rate file the picture holds while real
       * time - and the audience's audio - keeps moving */
      int64_t target = (int64_t)(((double)p->play_ns * 1e-9 + 1.0 / p->fps)
         * (double)p->arate);
      if (p->eof)
         target = p->apos + (int64_t)((double)p->arate / p->fps) + 1;
      if (p->atotal >= 0 && target > p->atotal)
         target = p->atotal;
      if (p->atotal >= 0 && p->apos >= p->atotal)
         p->aeof = 1;
      while (p->apos < target)
      {
         size_t want = (size_t)(target - p->apos);
         size_t got = 0;
         int r;
         if (want > 1024)
            want = 1024;
         r = audio_transfer_read_s16(p->actx, p->atype, buf, want, &got);
         if (got)
         {
            const int16_t *out = buf;
            if (p->ach == 1)
            {
               size_t i2;
               for (i2 = 0; i2 < got; i2++)
                  st[2 * i2] = st[2 * i2 + 1] = buf[i2];
               out = st;
            }
            WEBM_CORE_PREFIX(audio_batch_cb)(out, got);
            p->apos += (int64_t)got;
         }
         if (r != AUDIO_PROCESS_NEXT || !got)
         {
            p->aeof = 1;
            break;
         }
      }
   }
   else
#endif
   WEBM_CORE_PREFIX(audio_batch_cb)(p->silence, p->silence_frames);
}


/* Walk the native gather as far as the fill has reached, appending:
 * video display slots (show-frame filtered) into the pacing table,
 * audio packets into the growable blob.  Nothing is consumed at the
 * partial-read wall; the walk resumes exactly there next call.  The
 * pacing table stays sorted (a display-reordering codec can deliver
 * out of order; the sorted multiset is the display timeline), and the
 * audio decoder is rebased under the demuxed growth contract, its
 * exact playable length growing with the packets. */
static void webm_native_pump(webm_player_t *p)
{
   rwebm_packet pkt;
   int agrew = 0;
   if (!p->wgather)
      return;
   while (rwebm_read_packet(p->wgather, &pkt) == 1)
   {
#ifdef WEBM_HAVE_AUDIO
      if (p->atrack >= 0 && pkt.track == p->atrack)
      {
         if (p->apkts_used + pkt.size > p->apkts_cap)
         {
            size_t nc = p->apkts_cap ? p->apkts_cap * 2 : 256 * 1024;
            uint8_t *np;
            while (nc < p->apkts_used + pkt.size)
               nc *= 2;
            if (!(np = (uint8_t*)realloc(p->apkts, nc)))
               return;
            p->apkts     = np;
            p->apkts_cap = nc;
         }
         if (p->anpkts >= p->anpkts_cap)
         {
            size_t nc = p->anpkts_cap ? p->anpkts_cap * 2 : 4096;
            uint32_t *ns;
            if (!(ns = (uint32_t*)realloc(p->asizes,
                  nc * sizeof(uint32_t))))
               return;
            p->asizes     = ns;
            p->anpkts_cap = nc;
         }
         memcpy(p->apkts + p->apkts_used, pkt.data, pkt.size);
         p->asizes[p->anpkts++] = (uint32_t)pkt.size;
         p->apkts_used         += pkt.size;
         if (p->atype == AUDIO_TYPE_OPUS)
            p->atoc += webm_opus_pkt_frames(pkt.data, pkt.size);
         if (pkt.discard_padding > 0)
            p->adiscard_ns += pkt.discard_padding;
         agrew = 1;
         continue;
      }
#endif
      if (pkt.track != p->vtrack)
         continue;
      if (p->codec == RWEBM_CODEC_VP8
            && (!pkt.size || !(pkt.data[0] & 0x10)))
         continue;
      if (p->nvts >= p->nvts_cap)
      {
         int nc = p->nvts_cap ? p->nvts_cap * 2 : 1024;
         int64_t *nv;
         if (!(nv = (int64_t*)realloc(p->vts,
               (size_t)nc * sizeof(int64_t))))
            return;
         p->vts      = nv;
         p->nvts_cap = nc;
      }
      if (p->nvts > 0 && pkt.timestamp < p->vts[p->nvts - 1])
         p->vts_unsorted = 1;
      p->vts[p->nvts++] = pkt.timestamp;
   }
   if (p->vts_unsorted)
   {
      qsort(p->vts, (size_t)p->nvts, sizeof(int64_t), webm_ts_cmp);
      p->vts_unsorted = 0;
   }
#ifdef WEBM_HAVE_AUDIO
   if (agrew)
   {
      /* running exact playable length (binds only at the tail, long
       * after the fill has finished) */
      if (p->atype == AUDIO_TYPE_OPUS && p->atoc > 0)
      {
         p->atotal = p->atoc - p->apreskip
            - (p->adiscard_ns * 48000 + 500000000) / 1000000000;
         if (p->atotal < 0)
            p->atotal = 0;
      }
#ifdef HAVE_RAAC
      if (p->atype == AUDIO_TYPE_AAC)
      {
         p->atotal = (int64_t)p->anpkts * 1024 - (int64_t)p->atrim;
         if (p->atotal < 0)
            p->atotal = 0;
      }
#endif
      if (p->actx)
      {
         const rwebm_track *at = rwebm_get_track(p->wgather, p->atrack);
         audio_transfer_set_demuxed_ptr(p->actx, p->atype,
               at->codec_private, at->codec_private_size,
               p->apkts, p->apkts_used, p->asizes, p->anpkts);
      }
   }
#else
   (void)agrew;
#endif
}

#ifdef HAVE_RMP4
/* MP4 load path: video through the rmp4_video stream glue (H.264, VP9,
 * VP8, with display-order reordering and colr-driven conversion done
 * inside), audio through the audio_transfer demuxed arms (AAC, Opus,
 * Vorbis).  8-bit XRGB8888 output. */

#ifdef WEBM_HAVE_AUDIO
/* Append every audio packet the fill has reached to the growable
 * blob, then rebase the decoder onto the (possibly realloc-moved)
 * arrays under audio_transfer's demuxed growth contract.  The gather
 * demuxer stops at the partial-read wall (nothing consumed) and
 * resumes exactly there next call. */
static void webm_mp4_audio_pump(webm_player_t *p)
{
   rmp4_packet pkt;
   int grew = 0;
   if (!p->agather)
      return;
   while (rmp4_read_packet(p->agather, &pkt) == 1)
   {
      if (pkt.track != p->agtrack)
         continue;
      if (p->apkts_used + pkt.size > p->apkts_cap)
      {
         size_t nc = p->apkts_cap ? p->apkts_cap * 2 : 256 * 1024;
         uint8_t *np;
         while (nc < p->apkts_used + pkt.size)
            nc *= 2;
         if (!(np = (uint8_t*)realloc(p->apkts, nc)))
            return;
         p->apkts     = np;
         p->apkts_cap = nc;
      }
      if (p->anpkts >= p->anpkts_cap)
      {
         size_t nc = p->anpkts_cap ? p->anpkts_cap * 2 : 4096;
         uint32_t *ns;
         if (!(ns = (uint32_t*)realloc(p->asizes, nc * sizeof(uint32_t))))
            return;
         p->asizes     = ns;
         p->anpkts_cap = nc;
      }
      memcpy(p->apkts + p->apkts_used, pkt.data, pkt.size);
      p->asizes[p->anpkts++] = (uint32_t)pkt.size;
      p->apkts_used         += pkt.size;
#ifdef HAVE_ROPUS
      if (p->atype == AUDIO_TYPE_OPUS)
         p->atoc += webm_opus_pkt_frames(pkt.data, pkt.size);
#endif
      grew = 1;
   }
   if (grew && p->actx)
   {
      const rmp4_track *at = rmp4_get_track(p->agather, p->agtrack);
      audio_transfer_set_demuxed_ptr(p->actx, p->atype,
            at->codec_private, at->codec_private_size,
            p->apkts, p->apkts_used, p->asizes, p->anpkts);
#ifdef HAVE_ROPUS
      if (p->atype == AUDIO_TYPE_OPUS)
         p->atotal = p->atoc - p->apreskip;
#endif
   }
}
#endif

static bool webm_load_mp4(webm_player_t *p)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   unsigned w = 0, h = 0;
   int nframes = 0, loops = 0;
   int64_t dur_ns = 0;

   if (!WEBM_CORE_PREFIX(environ_cb)(
            RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (WEBM_CORE_PREFIX(log_cb))
         WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
            "[webm] XRGB8888 is not supported.\n");
      return false;
   }

   /* Progressive open: feed until the moov (and, for a faststart
    * file, almost nothing else) has arrived.  A trailing-moov file
    * degenerates to a full read here, as it must. */
   for (;;)
   {
      int need_more = 0;
      p->mp4vs = rmp4_video_stream_open_avail(p->file_buf,
            (size_t)p->file_len, data_transfer_avail(p->dt), &need_more);
      if (p->mp4vs)
         break;
      if (!need_more || data_transfer_failed(p->dt)
            || data_transfer_complete(p->dt))
      {
         if (WEBM_CORE_PREFIX(log_cb))
            WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
               "[webm] No supported (H.264/VP9/VP8) MP4 video track.\n");
         return false;
      }
      data_transfer_iterate(p->dt, 512 * 1024);
   }
   /* The video callback consumes XRGB8888 words; have the stream's
    * blit emit that order directly (the swap is baked at the blit's
    * interleave stage at no per-frame cost), so the per-frame copy
    * below needs no per-pixel swizzle. */
   rmp4_video_stream_set_argb(p->mp4vs, 1);
   rmp4_video_stream_get_info(p->mp4vs, &w, &h, &nframes, &loops);
   if (!w || !h)
      return false;
   p->width  = w;
   p->height = h;

   /* Audio and duration come from a second, persistent demuxer over
    * the same buffer: the audio-gather cursor.  It walks only as far
    * as the fill has reached, appending packets as they arrive
    * (webm_mp4_audio_pump), so playback starts without the old
    * whole-file packet walk. */
   {
      int need_more = 0;
      rmp4_t *m = rmp4_open_memory_avail(p->file_buf,
            (size_t)p->file_len, data_transfer_avail(p->dt), &need_more);
      /* the video open above required the moov, so this cannot fail
       * for want of bytes */
      if (m)
      {
         dur_ns = rmp4_duration_ns(m);
#ifdef WEBM_HAVE_AUDIO
         {
            const rmp4_track *at = NULL;
            int i, atrack = -1;
            for (i = 0; i < rmp4_num_tracks(m); i++)
            {
               const rmp4_track *t = rmp4_get_track(m, i);
               if (!t || t->type != RMP4_TRACK_AUDIO || atrack >= 0)
                  continue;
#ifdef HAVE_RAAC
               if (t->codec == RMP4_CODEC_AAC && t->codec_private_size)
               {
                  atrack   = i;
                  at       = t;
                  p->atype = AUDIO_TYPE_AAC;
               }
#endif
#ifdef HAVE_ROPUS
               if (t->codec == RMP4_CODEC_OPUS && t->codec_private_size
                     && atrack < 0)
               {
                  atrack   = i;
                  at       = t;
                  p->atype = AUDIO_TYPE_OPUS;
               }
#endif
#ifdef HAVE_RVORBIS
               if (t->codec == RMP4_CODEC_VORBIS && t->codec_private_size
                     && atrack < 0)
               {
                  atrack   = i;
                  at       = t;
                  p->atype = AUDIO_TYPE_VORBIS;
               }
#endif
            }
            if (atrack >= 0)
            {
               uint32_t tcount = 0;
               rmp4_track_pts(m, atrack, &tcount);
               p->agather = m;
               p->agtrack = atrack;
               m          = NULL;   /* ownership moved to the player */
#ifdef HAVE_ROPUS
               if (p->atype == AUDIO_TYPE_OPUS
                     && at->codec_private_size >= 19)
                  p->apreskip = at->codec_private[10]
                     | ((int64_t)at->codec_private[11] << 8);
#endif
               /* Exact playable length so emission stops with the
                * presentation timeline.  For AAC it comes straight
                * from the sample tables - no packet walk; for Opus it
                * accumulates from each appended packet's TOC. */
               p->atotal = -1;
               if (p->atype == AUDIO_TYPE_AAC)
                  p->atotal = (int64_t)tcount * 1024
                     - (int64_t)at->media_skip;
#ifdef HAVE_ROPUS
               if (p->atype == AUDIO_TYPE_OPUS)
                  p->atotal = 0;    /* grows with the pump */
#endif
               if (p->atotal < 0 && p->atype != AUDIO_TYPE_VORBIS)
                  p->atotal = 0;
               webm_mp4_audio_pump(p);   /* what has arrived so far */
               p->actx = audio_transfer_new(p->atype);
               if (p->actx
                     && audio_transfer_set_demuxed_ptr(p->actx,
                        p->atype, at->codec_private,
                        at->codec_private_size,
                        p->apkts, p->apkts_used, p->asizes, p->anpkts)
                     && (p->atype != AUDIO_TYPE_AAC
                        || audio_transfer_set_start_trim(p->actx,
                           p->atype, at->media_skip))
                     && audio_transfer_start(p->actx, p->atype)
                     && audio_transfer_info(p->actx, p->atype,
                        &p->ach, &p->arate, NULL)
                     && p->ach >= 1 && p->ach <= 2 && p->arate)
               {
                  if (WEBM_CORE_PREFIX(log_cb))
                     WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
                        "[webm] mp4 audio: %s, %u Hz, %u ch, "
                        "%u packets in the tables.\n",
                        p->atype == AUDIO_TYPE_AAC ? "AAC"
                        : p->atype == AUDIO_TYPE_OPUS ? "Opus"
                        : "Vorbis",
                        p->arate, p->ach, (unsigned)tcount);
               }
               else
               {
                  if (WEBM_CORE_PREFIX(log_cb))
                     WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_WARN,
                        "[webm] mp4 audio track unusable; "
                        "playing silent.\n");
                  if (p->actx)
                     audio_transfer_free(p->actx, p->atype);
                  p->actx = NULL;
                  free(p->apkts);
                  free(p->asizes);
                  p->apkts  = NULL;
                  p->asizes = NULL;
               }
            }
         }
#endif /* WEBM_HAVE_AUDIO */
      }
      if (m)
         rmp4_close(m);
   }

   p->dur_ns = dur_ns > 0 ? dur_ns : 0;
   /* the true mean rate comes from the timestamp span (presentation is
    * paced against the timestamps, so the announced rate must be their
    * mean or the pacing clock drifts); the duration-derived rate is
    * the fallback */
   {
      int64_t span_ms = rmp4_video_stream_span_ms(p->mp4vs);
      if (nframes > 1 && span_ms > 0)
         p->fps = (double)(nframes - 1) * 1000.0 / (double)span_ms;
      else if (nframes > 1 && dur_ns > 0)
         p->fps = (double)nframes * 1000000000.0 / (double)dur_ns;
      else
         p->fps = 30.0;
   }
   if (p->fps < 1.0 || p->fps > 240.0)
      p->fps = 30.0;

   p->fb = (uint32_t*)calloc((size_t)p->width * p->height,
      sizeof(uint32_t));
   p->silence_frames = (size_t)(WEBM_AUDIO_RATE / p->fps) + 1;
   p->frame_ns       = (int64_t)(1000000000.0 / p->fps + 0.5);
   p->silence = (int16_t*)calloc(p->silence_frames * 2, sizeof(int16_t));
   if (!p->fb || !p->silence)
      return false;

   if (WEBM_CORE_PREFIX(log_cb))
      WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
         "[webm] mp4 %ux%u, %.3f fps, %d frames.\n",
         p->width, p->height, p->fps, nframes);
   return true;
}
#endif /* HAVE_RMP4 */

bool WEBM_CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
   webm_player_t *p = &webm_player;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   enum retro_pixel_format fmt10 = RETRO_PIXEL_FORMAT_XRGB2101010;
   int want10 = 0;
   const rwebm_track *vt = NULL;
   int64_t len = 0;
   int i;
   int64_t dur_ns;

   if (!info || !info->path)
      return false;

   memset(p, 0, sizeof(*p));

   /* The file arrives through a data_transfer: a stable full-size
    * buffer filled incrementally.  MP4 plays progressively (the fill
    * continues during retro_run); the native WebM path fills to
    * completion below, load-time behaviour unchanged. */
   {
      size_t l = 0;
      if (!(p->dt = data_transfer_open(info->path)))
         goto error;
      p->file_buf = (uint8_t*)data_transfer_ptr(p->dt, &l);
      len         = (int64_t)l;
      if (!p->file_buf || len <= 0)
         goto error;
      /* enough for the container sniff */
      while (data_transfer_avail(p->dt) < 8
            && !data_transfer_complete(p->dt)
            && !data_transfer_failed(p->dt))
         data_transfer_iterate(p->dt, 64 * 1024);
      if (data_transfer_avail(p->dt) < 8)
         goto error;
   }
   p->file_len = len;

#ifdef HAVE_RMP4
   /* Container sniff: ISO-BMFF starts with a box whose type sits at
    * offset 4 ('ftyp' in practice, but any box means BMFF); WebM/MKV
    * starts with the EBML magic.  Only MP4 goes down the mp4 path;
    * everything else tries the EBML demuxer as before. */
   if (len >= 8
         && (   !memcmp(p->file_buf + 4, "ftyp", 4)
             || !memcmp(p->file_buf + 4, "moov", 4)
             || !memcmp(p->file_buf + 4, "styp", 4)))
   {
      if (webm_load_mp4(p))
         return true;
      goto error;
   }
#endif

   /* Prefer native 10-bit output, but only when the driver truly presents a
    * 10-bit surface end to end. SET_PIXEL_FORMAT(XRGB2101010) always
    * succeeds - the frontend silently narrows to 8-bit when the driver is not
    * 10-bit capable - so asking it alone would make us emit 10-bit that gets
    * thrown away: for 8-bit sources that is a pointless <<2 then >>2 round
    * trip, and for 10-bit sources the frontend truncates where our own 8-bit
    * path would round. GET_SCREEN_10BPC_CAPABLE reports the real capability,
    * so we only take the 10-bit path when it will survive; otherwise we go
    * straight to XRGB8888 and tone-map to 8 bits directly. Older frontends
    * that do not recognise the query leave want10 = 0 (safe: plain 8-bit). */
   {
      bool cap10 = false;
      if (WEBM_CORE_PREFIX(environ_cb)(
               RETRO_ENVIRONMENT_GET_SCREEN_10BPC_CAPABLE, &cap10)
            && cap10
            && WEBM_CORE_PREFIX(environ_cb)(
               RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt10))
         want10 = 1;
      else if (!WEBM_CORE_PREFIX(environ_cb)(
               RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      {
         if (WEBM_CORE_PREFIX(log_cb))
            WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
               "[webm] XRGB8888 is not supported.\n");
         goto error;
      }
   }
   p->pix10 = want10;

   /* Progressive open: the EBML header and Tracks sit at the front. */
   for (;;)
   {
      int need_more = 0;
      p->webm = rwebm_open_memory_avail(p->file_buf, (size_t)len,
            data_transfer_avail(p->dt), &need_more);
      if (p->webm)
         break;
      if (!need_more || data_transfer_failed(p->dt)
            || data_transfer_complete(p->dt))
         goto error;
      data_transfer_iterate(p->dt, 256 * 1024);
   }

   p->vtrack = -1;
   for (i = 0; i < rwebm_num_tracks(p->webm); i++)
   {
      const rwebm_track *t = rwebm_get_track(p->webm, i);
      if (t->type != RWEBM_TRACK_VIDEO)
         continue;
      if (t->codec == RWEBM_CODEC_VP9)
      {
         p->vtrack = i;
         vt = t;
         break;
      }
#ifdef WEBM_HAVE_H264
      if (t->codec == RWEBM_CODEC_H264 && t->codec_private_size
            && p->vtrack < 0)
      {
         p->vtrack = i;
         vt = t;
      }
#endif
#ifdef HAVE_RWEBP
      if (t->codec == RWEBM_CODEC_VP8 && p->vtrack < 0)
      {
         p->vtrack = i;
         vt = t;
      }
#endif
   }
#ifdef WEBM_HAVE_AUDIO
   p->atrack = -1;
   for (i = 0; i < rwebm_num_tracks(p->webm); i++)
   {
      const rwebm_track *t = rwebm_get_track(p->webm, i);
      if (t->type != RWEBM_TRACK_AUDIO || p->atrack >= 0)
         continue;
#ifdef HAVE_ROPUS
      if (t->codec == RWEBM_CODEC_OPUS && t->codec_private_size)
      {
         p->atrack = i;
         p->atype  = AUDIO_TYPE_OPUS;
      }
#endif
#ifdef HAVE_RVORBIS
      if (t->codec == RWEBM_CODEC_VORBIS && t->codec_private_size
            && p->atrack < 0)
      {
         p->atrack = i;
         p->atype  = AUDIO_TYPE_VORBIS;
      }
#endif
#ifdef HAVE_RAAC
      if (t->codec == RWEBM_CODEC_AAC && t->codec_private_size
            && p->atrack < 0)
      {
         p->atrack = i;
         p->atype  = AUDIO_TYPE_AAC;
      }
#endif
   }
#endif
   if (p->vtrack < 0 || !vt || !vt->width || !vt->height)
   {
      if (WEBM_CORE_PREFIX(log_cb))
         WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
            "[webm] No supported (H.264/VP8/VP9) video track found.\n");
      goto error;
   }
   p->codec  = vt->codec;
   p->width  = vt->width;
   p->height = vt->height;

   /* One persistent gather demuxer builds the pacing table and the
    * audio blob as far as the fill has reached; retro_run keeps
    * pumping it as the rest of the file arrives.  Feed here only far
    * enough for a frame-rate estimate (the announced rate is the
    * schedule's mean; a 512-slot prefix pins it for constant-rate
    * files and approximates genuine VFR no worse than a mean ever
    * did) and for the audio decoder's setup. */
   {
      int need_more = 0;
      p->wgather = rwebm_open_memory_avail(p->file_buf, (size_t)len,
            data_transfer_avail(p->dt), &need_more);
      if (!p->wgather)
         goto error;
#ifdef WEBM_HAVE_AUDIO
      if (p->atrack >= 0)
      {
         const rwebm_track *at0 = rwebm_get_track(p->webm, p->atrack);
         if (p->atype == AUDIO_TYPE_OPUS
               && at0->codec_private_size >= 19)
            p->apreskip = at0->codec_private[10]
               | ((int64_t)at0->codec_private[11] << 8);
#ifdef HAVE_RAAC
         if (p->atype == AUDIO_TYPE_AAC)
            p->atrim = (at0->codec_delay_ns
                  * (uint64_t)(at0->sample_rate ? at0->sample_rate
                     : 48000) + 500000000) / 1000000000;
#endif
         p->atotal = -1;
      }
#endif
      for (;;)
      {
         webm_native_pump(p);
         if (p->nvts >= 512
               || data_transfer_complete(p->dt)
               || data_transfer_failed(p->dt))
            break;
         data_transfer_iterate(p->dt, 256 * 1024);
         rwebm_set_avail(p->webm, data_transfer_avail(p->dt));
         rwebm_set_avail(p->wgather, data_transfer_avail(p->dt));
      }
#ifdef WEBM_HAVE_AUDIO
      if (p->atrack >= 0 && p->anpkts)
      {
         const rwebm_track *at = rwebm_get_track(p->webm, p->atrack);
         p->actx = audio_transfer_new(p->atype);
         if (p->actx
               && audio_transfer_set_demuxed_ptr(p->actx, p->atype,
                  at->codec_private, at->codec_private_size,
                  p->apkts, p->apkts_used, p->asizes, p->anpkts)
               && (p->atype != AUDIO_TYPE_AAC
                  || audio_transfer_set_start_trim(p->actx,
                     p->atype, p->atrim))
               && audio_transfer_start(p->actx, p->atype)
               && audio_transfer_info(p->actx, p->atype,
                  &p->ach, &p->arate, NULL)
               && p->ach >= 1 && p->ach <= 2 && p->arate)
         {
            if (WEBM_CORE_PREFIX(log_cb))
               WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
                  "[webm] audio: %s, %u Hz, %u ch.\n",
                  p->atype == AUDIO_TYPE_OPUS ? "Opus"
                  : p->atype == AUDIO_TYPE_AAC ? "AAC" : "Vorbis",
                  p->arate, p->ach);
         }
         else
         {
            if (WEBM_CORE_PREFIX(log_cb))
               WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_WARN,
                  "[webm] audio track unusable; playing silent.\n");
            if (p->actx)
               audio_transfer_free(p->actx, p->atype);
            p->actx = NULL;
            free(p->apkts);
            free(p->asizes);
            p->apkts  = NULL;
            p->asizes = NULL;
            p->atrack = -1;   /* the pump stops gathering it */
         }
      }
#endif
   }
   dur_ns = rwebm_duration_ns(p->webm);
   p->dur_ns = dur_ns > 0 ? dur_ns : 0;
   /* The frame rate comes from the video track's own span, not the
    * container duration (which includes the audio tail): presentation
    * is paced against the timestamp schedule, so the announced rate
    * must be the schedule's true mean or the pacing clock drifts. */
   /* Precedence: with the pacing table complete (the fill finished
    * during load - every pre-progressive load did), the table's own
    * span, exactly the historical derivation; otherwise the muxer's
    * declared DefaultDuration, exact and header-only; last, the
    * scanned prefix's mean. */
   if ((!p->dt || data_transfer_complete(p->dt))
         && p->nvts > 1 && p->vts[p->nvts - 1] > p->vts[0])
      p->fps = (double)(p->nvts - 1) * 1000000000.0
         / (double)(p->vts[p->nvts - 1] - p->vts[0]);
   else if (vt->default_duration_ns > 0)
      p->fps = 1000000000.0 / (double)vt->default_duration_ns;
   else if (p->nvts > 1 && p->vts[p->nvts - 1] > p->vts[0])
      p->fps = (double)(p->nvts - 1) * 1000000000.0
         / (double)(p->vts[p->nvts - 1] - p->vts[0]);
   else
      p->fps = 30.0;
   if (p->fps < 1.0 || p->fps > 240.0)
      p->fps = 30.0;

   if (p->codec == RWEBM_CODEC_VP9)
   {
      p->vp9 = (rvp9_dec*)calloc(1, sizeof(*p->vp9));
      if (!p->vp9)
         goto error;
   }
#ifdef WEBM_HAVE_H264
   else if (p->codec == RWEBM_CODEC_H264)
   {
      p->h264 = rh264_video_open();
      if (!p->h264)
         goto error;
      rh264_video_set_extradata(p->h264, vt->codec_private,
            vt->codec_private_size);
   }
#endif
#ifdef HAVE_RWEBP
   else
   {
      p->vp8 = rvp8_video_open();
      if (!p->vp8)
         goto error;
   }
#endif

   p->fb = (uint32_t*)calloc((size_t)p->width * p->height,
      sizeof(uint32_t));
   p->silence_frames = (size_t)(WEBM_AUDIO_RATE / p->fps) + 1;
   p->frame_ns       = (int64_t)(1000000000.0 / p->fps + 0.5);
   /* pace from the first frame's timestamp, wherever the container
    * starts its clock */
   p->play_ns        = p->nvts ? p->vts[0] : 0;
   p->silence = (int16_t*)calloc(p->silence_frames * 2, sizeof(int16_t));
   if (!p->fb || !p->silence)
      goto error;

   if (WEBM_CORE_PREFIX(log_cb))
      WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
         "[webm] %ux%u %s, %.3f fps, %d video packets.\n",
         p->width, p->height,
         p->codec == RWEBM_CODEC_VP9 ? "VP9"
         : p->codec == RWEBM_CODEC_H264 ? "H.264" : "VP8", p->fps, p->nvts);
   return true;

error:
   webm_free_player(p);
   return false;
}

bool WEBM_CORE_PREFIX(retro_load_game_special)(unsigned type,
      const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

void WEBM_CORE_PREFIX(retro_unload_game)(void)
{
   webm_free_player(&webm_player);
}

unsigned WEBM_CORE_PREFIX(retro_get_region)(void)
{
   return RETRO_REGION_NTSC;
}

void *WEBM_CORE_PREFIX(retro_get_memory_data)(unsigned id)
{
   (void)id;
   return NULL;
}

size_t WEBM_CORE_PREFIX(retro_get_memory_size)(unsigned id)
{
   (void)id;
   return 0;
}

/* The state of a movie is its position; serialising it makes the
 * frontend's save-state machinery a resume feature - saving on exit
 * and loading on start picks playback up where it left off, restored
 * through the same seek path the transport controls use. */
#define WEBM_STATE_SIZE 24

size_t WEBM_CORE_PREFIX(retro_serialize_size)(void)
{
   return WEBM_STATE_SIZE;
}

bool WEBM_CORE_PREFIX(retro_serialize)(void *data, size_t size)
{
   webm_player_t *p = &webm_player;
   uint8_t *d = (uint8_t*)data;
   uint64_t v = (uint64_t)p->vpts_ns;
   uint64_t s = (uint64_t)p->vshown;
   int i;
   if (size < WEBM_STATE_SIZE)
      return false;
   d[0] = 'W'; d[1] = 'P'; d[2] = 'O'; d[3] = 'S';
   d[4] = 2;   d[6] = 0;   d[7] = 0;
   /* Whether playback had already ended.  The clock and slot below
    * describe a position, and at the end of a movie that position is
    * the last frame - restoring from them alone resumes just before
    * the end instead of at it.  Older states leave this zero, which is
    * what they meant. */
   d[5] = (uint8_t)(p->eof ? 1 : 0);
   for (i = 0; i < 8; i++)
   {
      d[8 + i]  = (uint8_t)(v >> (8 * i));
      d[16 + i] = (uint8_t)(s >> (8 * i));
   }
   return true;
}

bool WEBM_CORE_PREFIX(retro_unserialize)(const void *data, size_t size)
{
   webm_player_t *p = &webm_player;
   const uint8_t *d = (const uint8_t*)data;
   uint64_t v = 0, s = 0;
   int i;
   if (size < 16 || !d)
      return false;
   if (d[0] != 'W' || d[1] != 'P' || d[2] != 'O' || d[3] != 'S')
      return false;
   for (i = 0; i < 8; i++)
      v |= (uint64_t)d[8 + i] << (8 * i);
   if (d[4] == 2 && size >= WEBM_STATE_SIZE)
   {
      for (i = 0; i < 8; i++)
         s |= (uint64_t)d[16 + i] << (8 * i);
      /* the slot is exact where the pts clock can run ahead of the
       * display position (H.264 reordering); the mp4 branch keeps
       * using the timestamp, whose own table is exact */
#ifdef HAVE_RMP4
      if (p->mp4vs)
         webm_seek(p, (int64_t)v);
      else
#endif
         webm_seek_internal(p, (int64_t)v,
               s > 0 ? (int64_t)s - 1 : 0);
      /* a state taken after the last frame restores to the end, with
       * that frame held, rather than to just before it */
      if (d[5])
         p->eof = 1;
      return true;
   }
   if (d[4] != 1)
      return false;
   webm_seek(p, (int64_t)v);
   return true;
}

void WEBM_CORE_PREFIX(retro_cheat_reset)(void) { }

void WEBM_CORE_PREFIX(retro_cheat_set)(unsigned idx, bool enabled,
      const char *code)
{
   (void)idx;
   (void)enabled;
   (void)code;
}
