/* webm_core -- built-in WebM video player core.
 *
 * A dependency-free movie player for WebM files, built entirely on
 * libretro-common: rwebm (demuxer), rvp9 (VP9 decoder) and, when
 * available, rvp8 (VP8 decoder).  Intended for builds without FFmpeg
 * or MPV -- consoles and other minimal targets -- where it acts as the
 * built-in media player fallback for .webm content.
 *
 * Audio tracks (Opus via ropus, Vorbis via rvorbis) play through the
 * audio_transfer demuxed path when the decoders are built in; emission
 * is paced by the presented video frame's timestamp, which keeps A/V
 * in sync without a separate clock.  Files without a supported audio
 * track fall back to silence to keep frame pacing driven by the audio
 * callback.  VP9 superframes and invisible (non-shown) VP8/VP9 frames
 * are handled.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <formats/rwebm.h>
#include <formats/rvp9.h>

#if defined(HAVE_ROPUS) || defined(HAVE_RVORBIS)
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
   rwebm_t     *webm;
   int          vtrack;              /* video track index               */
   enum rwebm_codec codec;
   rvp9_dec    *vp9;
#ifdef HAVE_RWEBP
   rvp8_video  *vp8;
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
   unsigned     ach;                 /* decoded channels (1 or 2)       */
   unsigned     arate;
   int64_t      apos;                /* frames emitted so far           */
   int64_t      atotal;              /* emit clamp; <0 = no clamp       */
   int          aeof;
   int64_t      vpts_ns;             /* pts of the last presented frame */
#endif
} webm_player_t;

static webm_player_t webm_player;

/* -------------------------------------------------------------------- */
/* BT.601 limited-range I420 -> XRGB8888.                                */
/* -------------------------------------------------------------------- */
static INLINE uint32_t webm_yuv_px(int y, int u, int v)
{
   int c = 298 * (y - 16) + 128;
   int r = (c + 409 * (v - 128)) >> 8;
   int g = (c - 100 * (u - 128) - 208 * (v - 128)) >> 8;
   int b = (c + 516 * (u - 128)) >> 8;
   if (r < 0) r = 0; else if (r > 255) r = 255;
   if (g < 0) g = 0; else if (g > 255) g = 255;
   if (b < 0) b = 0; else if (b > 255) b = 255;
   return 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8)
      | (uint32_t)b;
}

static void webm_blit_i420(uint32_t *dst, unsigned w, unsigned h,
      const uint8_t *y, int ys, const uint8_t *u, const uint8_t *v, int uvs)
{
   unsigned i, j;
   for (j = 0; j < h; j++)
   {
      const uint8_t *yr = y + j * ys;
      const uint8_t *ur = u + (j >> 1) * uvs;
      const uint8_t *vr = v + (j >> 1) * uvs;
      uint32_t *dr      = dst + j * w;
      for (i = 0; i < w; i++)
         dr[i] = webm_yuv_px(yr[i], ur[i >> 1], vr[i >> 1]);
   }
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
   webm_blit_i420(p->fb, w, h, fb->y, p->vp9->ys, fb->u, fb->v,
      p->vp9->uvs);
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
      webm_blit_i420(p->fb, (unsigned)w, (unsigned)h, y, ys, u, v, uvs);
      WEBM_CORE_PREFIX(video_cb)(p->fb, (unsigned)w, (unsigned)h,
         p->width * sizeof(uint32_t));
      return 1;
   }
#endif
   return -1;
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
   if (p->webm)
      rwebm_close(p->webm);
#ifdef WEBM_HAVE_AUDIO
   if (p->actx)
      audio_transfer_free(p->actx, p->atype);
   free(p->apkts);
   free(p->asizes);
#endif
   free(p->fb);
   free(p->silence);
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
   info->valid_extensions = "webm|mkv";
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
#ifdef WEBM_HAVE_AUDIO
   if (p->actx)
      audio_transfer_seek(p->actx, p->atype, 0);
   p->apos    = 0;
   p->aeof    = 0;
   p->vpts_ns = 0;
#endif
   p->eof = 0;
}

void WEBM_CORE_PREFIX(retro_run)(void)
{
   webm_player_t *p = &webm_player;
   rwebm_packet pkt;

   WEBM_CORE_PREFIX(input_poll_cb)();

   if (!p->eof)
   {
      int presented = 0;
      while (!presented)
      {
         int r = rwebm_read_packet(p->webm, &pkt);
         if (r != 1)
         {
            p->eof = 1;
            break;
         }
         if (pkt.track != p->vtrack)
            continue;
         presented = webm_decode_packet(p, &pkt);
         if (presented < 0)
         {
            p->eof = 1;
            break;
         }
#ifdef WEBM_HAVE_AUDIO
         if (presented > 0 && pkt.timestamp > 0)
            p->vpts_ns = pkt.timestamp;
#endif
      }
      if (!presented)
         WEBM_CORE_PREFIX(video_cb)(p->fb, p->width, p->height,
            p->width * sizeof(uint32_t));
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

#ifdef WEBM_HAVE_AUDIO
   if (p->actx && !p->aeof)
   {
      /* Emit decoded audio up to the presented frame's timestamp plus
       * one nominal frame of lead; at video EOF, drain about a frame
       * interval per run so the frontend's audio pacing continues to
       * throttle us through the tail. */
      int16_t buf[1024 * 2];
      int16_t st[1024 * 2];
      int64_t target = (int64_t)(((double)p->vpts_ns * 1e-9 + 1.0 / p->fps)
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

bool WEBM_CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
   webm_player_t *p = &webm_player;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   const rwebm_track *vt = NULL;
   int64_t len = 0;
   int i, nvpkts;
   int64_t dur_ns;

   if (!info || !info->path)
      return false;
   if (!WEBM_CORE_PREFIX(environ_cb)(
         RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (WEBM_CORE_PREFIX(log_cb))
         WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
            "[webm] XRGB8888 is not supported.\n");
      return false;
   }

   memset(p, 0, sizeof(*p));

   if (!filestream_read_file(info->path, (void**)&p->file_buf, &len)
         || len <= 0)
      goto error;
   p->file_len = len;

   p->webm = rwebm_open_memory(p->file_buf, (size_t)len);
   if (!p->webm)
      goto error;

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
   }
#endif
   if (p->vtrack < 0 || !vt || !vt->width || !vt->height)
   {
      if (WEBM_CORE_PREFIX(log_cb))
         WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
            "[webm] No supported (VP8/VP9) video track found.\n");
      goto error;
   }
   p->codec  = vt->codec;
   p->width  = vt->width;
   p->height = vt->height;

   /* Frame rate: shown video packets over stream duration; the scan is
    * cheap since packets alias the file buffer.  VP8 invisible (alt-ref)
    * frames are muxed as separate blocks and must not inflate the count;
    * bit 4 of the frame tag is show_frame.  VP9 hides its invisible
    * frames inside superframes, so each packet shows at most once. */
   nvpkts = 0;
   {
      rwebm_packet pkt;
#ifdef WEBM_HAVE_AUDIO
      size_t napkts = 0, abytes = 0;
#endif
      while (rwebm_read_packet(p->webm, &pkt) == 1)
      {
#ifdef WEBM_HAVE_AUDIO
         if (pkt.track == p->atrack)
         {
            napkts++;
            abytes += pkt.size;
            continue;
         }
#endif
         if (pkt.track != p->vtrack)
            continue;
         if (p->codec == RWEBM_CODEC_VP8
               && (!pkt.size || !(pkt.data[0] & 0x10)))
            continue;
         nvpkts++;
      }
      rwebm_rewind(p->webm);
#ifdef WEBM_HAVE_AUDIO
      /* Second pass: copy the audio packets into one contiguous blob
       * for the audio_transfer demuxed contract (the packets alias the
       * file buffer but are interleaved with video, so they are not
       * contiguous in place). */
      if (p->atrack >= 0 && napkts)
      {
         size_t k = 0, off = 0;
         p->apkts  = (uint8_t*)malloc(abytes ? abytes : 1);
         p->asizes = (uint32_t*)malloc(napkts * sizeof(uint32_t));
         if (p->apkts && p->asizes)
         {
            int64_t toc_frames = 0, discard_ns = 0;
            while (rwebm_read_packet(p->webm, &pkt) == 1)
            {
               if (pkt.track != p->atrack)
                  continue;
               memcpy(p->apkts + off, pkt.data, pkt.size);
               p->asizes[k++] = (uint32_t)pkt.size;
               off += pkt.size;
               if (p->atype == AUDIO_TYPE_OPUS)
                  toc_frames += webm_opus_pkt_frames(pkt.data, pkt.size);
               if (pkt.discard_padding > 0)
                  discard_ns += pkt.discard_padding;
            }
            rwebm_rewind(p->webm);
            /* Exact playable length: decoded total minus the pre-skip
             * (dropped inside the decoder arm) minus container end
             * trimming.  Only computable for Opus, whose TOC encodes
             * packet durations; Vorbis emission is left unclamped. */
            p->atotal = -1;
            if (p->atype == AUDIO_TYPE_OPUS && toc_frames > 0)
            {
               const rwebm_track *at0 =
                  rwebm_get_track(p->webm, p->atrack);
               int64_t preskip = 0;
               if (at0->codec_private_size >= 19)
                  preskip = at0->codec_private[10]
                     | ((int64_t)at0->codec_private[11] << 8);
               p->atotal = toc_frames - preskip
                  - (discard_ns * 48000 + 500000000) / 1000000000;
               if (p->atotal < 0)
                  p->atotal = 0;
            }
            {
               const rwebm_track *at = rwebm_get_track(p->webm, p->atrack);
               p->actx = audio_transfer_new(p->atype);
               if (p->actx
                     && audio_transfer_set_demuxed_ptr(p->actx, p->atype,
                        at->codec_private, at->codec_private_size,
                        p->apkts, off, p->asizes, k)
                     && audio_transfer_start(p->actx, p->atype)
                     && audio_transfer_info(p->actx, p->atype,
                        &p->ach, &p->arate, NULL)
                     && p->ach >= 1 && p->ach <= 2 && p->arate)
               {
                  if (WEBM_CORE_PREFIX(log_cb))
                     WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
                        "[webm] audio: %s, %u Hz, %u ch, %u packets.\n",
                        p->atype == AUDIO_TYPE_OPUS ? "Opus" : "Vorbis",
                        p->arate, p->ach, (unsigned)k);
               }
               else
               {
                  if (WEBM_CORE_PREFIX(log_cb))
                     WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_WARN,
                        "[webm] audio track unusable; playing silent.\n");
                  if (p->actx)
                     audio_transfer_free(p->actx, p->atype);
                  p->actx = NULL;
               }
            }
         }
         if (!p->actx)
         {
            free(p->apkts);
            free(p->asizes);
            p->apkts  = NULL;
            p->asizes = NULL;
         }
      }
#endif
   }
   dur_ns = rwebm_duration_ns(p->webm);
   if (nvpkts > 1 && dur_ns > 0)
      p->fps = (double)nvpkts * 1000000000.0 / (double)dur_ns;
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
   p->silence = (int16_t*)calloc(p->silence_frames * 2, sizeof(int16_t));
   if (!p->fb || !p->silence)
      goto error;

   if (WEBM_CORE_PREFIX(log_cb))
      WEBM_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
         "[webm] %ux%u %s, %.3f fps, %d video packets.\n",
         p->width, p->height,
         p->codec == RWEBM_CODEC_VP9 ? "VP9" : "VP8", p->fps, nvpkts);
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

size_t WEBM_CORE_PREFIX(retro_serialize_size)(void)
{
   return 0;
}

bool WEBM_CORE_PREFIX(retro_serialize)(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool WEBM_CORE_PREFIX(retro_unserialize)(const void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void WEBM_CORE_PREFIX(retro_cheat_reset)(void) { }

void WEBM_CORE_PREFIX(retro_cheat_set)(unsigned idx, bool enabled,
      const char *code)
{
   (void)idx;
   (void)enabled;
   (void)code;
}
