/* ffmpeg_pipeline_test: the record_ffmpeg driver end to end, without
 * RetroArch.  Opens the driver on the lossless preset, pushes a run
 * of frames whose pixels encode the frame number, plus audio, from
 * this thread while the driver's encoder thread drains, finalizes,
 * and then decodes the file back with libavcodec and checks that
 * every frame came out in order with the pixels it was given.
 *
 * What it exercises: the three queues between the pushing thread and
 * the encoder thread - attr, video, audio - under real contention
 * (frames are pushed as fast as they are taken), the blocking wait
 * when the queues are full, and the drain after the encoder thread
 * is joined.  A frame out of order, torn, or from the wrong slot
 * fails the pixel check.
 *
 * A second pass runs in drop mode (allow_frame_drop, the smallest
 * video queue): frames the encoder can't take in time are dropped,
 * so it checks the survivors come out in order, pixel-exact, at the
 * timestamp of the frame they were pushed as, that frames were in
 * fact dropped, and that the frontend was told nothing while it
 * recorded: no log line and no on-screen message from the pushes.
 * On Linux, drop mode also has the encoder's own threads run at a
 * lower priority than the thread that opened the recording.
 *
 * A geometry lane checks the output size and display aspect of a raw
 * recording of a frame the display turns a quarter.
 *
 * An open-only lane checks two choices made at init: a video queue
 * deeper than the ring's byte ceiling is shortened to fit under it,
 * and the frontend's Debug log level asks libav* for no more than
 * AV_LOG_VERBOSE, below which encoders log every frame. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>

#include "../../../record/record_driver.h"
#include "../../../record/drivers/record_ffmpeg.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#if defined(__linux__)
#include <dirent.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

extern unsigned stub_warn_calls;
extern unsigned stub_msg_calls;
extern char     stub_last_log[512];
extern bool     stub_verbose;
extern unsigned stub_log_level;

#include <libavutil/log.h>

#define W        160
#define H        120
#define FRAMES   240
#define FPS      60.0
#define RATE     48000.0
#define OUT      "ffmpeg_pipeline_test.mkv"

/* Frame n: a solid colour derived from n in the top half, the pattern
 * inverted in the bottom half, so a frame's number can be read back
 * from any pixel and a torn frame (rows from two frames) is caught. */
static void paint(uint32_t *fb, unsigned n)
{
   unsigned x, y;
   uint32_t c = 0xff000000u | ((n * 37u) & 0xffu) << 16
              | ((n * 91u) & 0xffu) << 8 | ((n * 13u) & 0xffu);
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
         fb[y * W + x] = (y < H / 2) ? c : (c ^ 0x00ffffffu);
}

static int check_frame(const AVFrame *f, unsigned n)
{
   uint32_t want = 0xff000000u | ((n * 37u) & 0xffu) << 16
                 | ((n * 91u) & 0xffu) << 8 | ((n * 13u) & 0xffu);
   unsigned x, y;
   for (y = 0; y < (unsigned)f->height; y++)
   {
      uint32_t exp = (y < H / 2) ? want : (want ^ 0x00ffffffu);
      for (x = 0; x < (unsigned)f->width; x++)
      {
         uint32_t px;
         if (f->format == AV_PIX_FMT_GBRP)
         {
            /* libx264rgb decodes to planar G, B, R. */
            uint8_t g = f->data[0][y * f->linesize[0] + x];
            uint8_t b = f->data[1][y * f->linesize[1] + x];
            uint8_t r = f->data[2][y * f->linesize[2] + x];
            px = 0xff000000u | (r << 16) | (g << 8) | b;
         }
         else if (f->format == AV_PIX_FMT_BGR24)
         {
            const uint8_t *p = f->data[0] + y * f->linesize[0] + x * 3;
            px = 0xff000000u | (p[2] << 16) | (p[1] << 8) | p[0];
         }
         else
         {
            fprintf(stderr, "unexpected pix_fmt %d\n", f->format);
            return 0;
         }
         if (px != exp)
         {
            fprintf(stderr, "frame %u row %u col %u: got %06x want %06x\n",
                  n, y, x, px & 0xffffff, exp & 0xffffff);
            return 0;
         }
      }
   }
   return 1;
}

/* Which frame this is, from its top-left pixel. */
static int frame_number(const AVFrame *f)
{
   unsigned n;
   for (n = 0; n < FRAMES; n++)
   {
      uint32_t want = ((n * 37u) & 0xffu) << 16
                    | ((n * 91u) & 0xffu) << 8 | ((n * 13u) & 0xffu);
      uint32_t px;
      if (f->format == AV_PIX_FMT_GBRP)
         px = (f->data[2][0] << 16) | (f->data[0][0] << 8) | f->data[1][0];
      else
         px = (f->data[0][2] << 16) | (f->data[0][1] << 8) | f->data[0][0];
      if (px == want)
         return (int)n;
   }
   return -1;
}

#if defined(__linux__)
/* Threads of this process running at a higher nice value than the
 * calling thread. */
static unsigned threads_lowered(void)
{
   struct dirent *ent;
   unsigned n = 0;
   int me     = getpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid));
   DIR *dir   = opendir("/proc/self/task");
   if (!dir)
      return 0;
   while ((ent = readdir(dir)))
   {
      long tid = strtol(ent->d_name, NULL, 10);
      int  nice;
      if (tid <= 0)
         continue;
      errno = 0;
      nice  = getpriority(PRIO_PROCESS, (id_t)tid);
      if (!errno && nice > me)
         n++;
   }
   closedir(dir);
   return n;
}
#endif

static int run(bool drop)
{
   struct record_params params;
   struct record_video_data vid;
   struct record_audio_data aud;
   uint32_t *fb = (uint32_t*)malloc(W * H * sizeof(uint32_t));
   int16_t   pcm[800 * 2];
   void     *rec;
   unsigned  n;

   memset(&params, 0, sizeof(params));
   params.fps                        = FPS;
   params.samplerate                 = RATE;
   params.filename                   = OUT;
   params.audio_resampler            = "sinc";
   params.out_dims                   = VIDEO_SCALE_PACK(W, H);
   params.fb_dims                    = VIDEO_SCALE_PACK(W, H);
   params.channels                   = 2;
   params.video_record_scale_factor  = 1;
   params.video_stream_scale_factor  = 1;
   /* Drop mode with encoder threads of its own, to see them lowered */
   params.video_record_threads       = drop ? 2 : 1;
   params.aspect_ratio               = (float)W / H;
   params.preset                     = RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY;
   params.pix_fmt                    = FFEMU_PIX_ARGB8888;
   params.allow_frame_drop           = drop;
   params.video_fifo_frames          = drop ? 8 : 32;

   remove(OUT);
   rec = record_ffmpeg.init(&params);
   if (!rec)
   {
      fprintf(stderr, "init failed (is libx264rgb available?)\n");
      return 1;
   }

#if defined(__linux__)
   if (drop && !threads_lowered())
   {
      fprintf(stderr, "drop mode: no encoder thread runs lowered\n");
      return 1;
   }
#endif

   stub_warn_calls = 0;
   stub_msg_calls  = 0;
   memset(pcm, 0, sizeof(pcm));
   for (n = 0; n < FRAMES; n++)
   {
      paint(fb, n);
      vid.data    = fb;
      vid.dims    = VIDEO_SCALE_PACK(W, H);
      vid.pitch   = W * sizeof(uint32_t);
      vid.is_dupe = false;
      if (!record_ffmpeg.push_video(rec, &vid))
      {
         fprintf(stderr, "push_video %u failed\n", n);
         return 1;
      }
      aud.data   = pcm;
      aud.frames = 800;
      if (!record_ffmpeg.push_audio(rec, &aud))
      {
         fprintf(stderr, "push_audio %u failed\n", n);
         return 1;
      }
   }
   if (drop && (stub_warn_calls || stub_msg_calls))
   {
      fprintf(stderr, "drop mode: %u warnings and %u on-screen messages"
            " while recording\n", stub_warn_calls, stub_msg_calls);
      return 1;
   }
   record_ffmpeg.finalize(rec);
   record_ffmpeg.free(rec);
   free(fb);

   /* Decode it back. */
   {
      AVFormatContext *fmt = NULL;
      AVCodecContext  *dec = NULL;
      const AVCodec   *codec;
      AVPacket        *pkt = av_packet_alloc();
      AVFrame         *frm = av_frame_alloc();
      int vstream = -1, i;
      int last = -1;
      unsigned got = 0;

      if (avformat_open_input(&fmt, OUT, NULL, NULL) < 0
            || avformat_find_stream_info(fmt, NULL) < 0)
      {
         fprintf(stderr, "cannot open %s\n", OUT);
         return 1;
      }
      for (i = 0; i < (int)fmt->nb_streams; i++)
         if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            vstream = i;
      if (vstream < 0)
      {
         fprintf(stderr, "no video stream\n");
         return 1;
      }
      codec = avcodec_find_decoder(fmt->streams[vstream]->codecpar->codec_id);
      dec   = avcodec_alloc_context3(codec);
      avcodec_parameters_to_context(dec, fmt->streams[vstream]->codecpar);
      if (avcodec_open2(dec, codec, NULL) < 0)
      {
         fprintf(stderr, "cannot open decoder\n");
         return 1;
      }
      while (av_read_frame(fmt, pkt) >= 0 || (avcodec_send_packet(dec, NULL), 1))
      {
         int flushing = pkt->size == 0 && pkt->data == NULL;
         if (!flushing)
         {
            if (pkt->stream_index != vstream)
            {
               av_packet_unref(pkt);
               continue;
            }
            avcodec_send_packet(dec, pkt);
            av_packet_unref(pkt);
         }
         while (avcodec_receive_frame(dec, frm) == 0)
         {
            int n = drop ? frame_number(frm) : (int)got;
            if (drop)
            {
               /* In order, and at the pushed frame's own time. */
               double t = frm->best_effort_timestamp
                  * av_q2d(fmt->streams[vstream]->time_base);
               if (n <= last)
               {
                  fprintf(stderr, "frame %d after %d\n", n, last);
                  return 1;
               }
               if (t < n / FPS - 0.002 || t > n / FPS + 0.002)
               {
                  fprintf(stderr, "frame %d at %.4f s, want %.4f s\n",
                        n, t, n / FPS);
                  return 1;
               }
               last = n;
            }
            if (n < 0 || !check_frame(frm, (unsigned)n))
               return 1;
            got++;
            av_frame_unref(frm);
         }
         if (flushing)
            break;
      }
      if (drop ? (got == 0 || got >= FRAMES) : got != FRAMES)
      {
         fprintf(stderr, "decoded %u frames, pushed %u\n", got, FRAMES);
         return 1;
      }
      avcodec_free_context(&dec);
      avformat_close_input(&fmt);
      av_packet_free(&pkt);
      av_frame_free(&frm);
      if (drop)
         printf("[pass] drop mode: %u of %u frames kept, in order, "
               "pixel-exact, on time\n", got, FRAMES);
      else
         printf("[pass] %u frames through record_ffmpeg, all in order "
               "and pixel-exact\n", FRAMES);
   }
   remove(OUT);
   return 0;
}

/* A raw recording of frames the display turns a quarter: the output
 * is turned, and its aspect is the display's, which is already the
 * turned picture's - not that inverted again. */
static int geometry(void)
{
   unsigned dims;
   float aspect;

   /* 320x240 core frame, shown turned on a 3:4 display */
   dims = VIDEO_SCALE_PACK(320, 240);
   record_raw_geometry(&dims, &aspect, 1, 0.75f, false);
   if (dims != VIDEO_SCALE_PACK(240, 320) || aspect != 0.75f)
   {
      fprintf(stderr, "geometry: turned frame recorded %ux%u at %.3f,"
            " want 240x320 at 0.750\n", VIDEO_SCALE_W(dims),
            VIDEO_SCALE_H(dims), aspect);
      return 1;
   }
   /* No display aspect: the turned size decides */
   dims = VIDEO_SCALE_PACK(320, 240);
   record_raw_geometry(&dims, &aspect, 3, 0.0f, false);
   if (dims != VIDEO_SCALE_PACK(240, 320) || aspect != 0.75f)
   {
      fprintf(stderr, "geometry: turned size gives %ux%u at %.3f\n",
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), aspect);
      return 1;
   }
   /* --size is kept as given, and its own shape is its aspect */
   dims = VIDEO_SCALE_PACK(640, 480);
   record_raw_geometry(&dims, &aspect, 1, 0.0f, true);
   if (dims != VIDEO_SCALE_PACK(640, 480) || aspect < 1.333f
         || aspect > 1.334f)
   {
      fprintf(stderr, "geometry: --size became %ux%u at %.3f\n",
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), aspect);
      return 1;
   }
   /* Unturned: as it was */
   dims = VIDEO_SCALE_PACK(320, 240);
   record_raw_geometry(&dims, &aspect, 2, 1.25f, false);
   if (dims != VIDEO_SCALE_PACK(320, 240) || aspect != 1.25f)
   {
      fprintf(stderr, "geometry: half turn gives %ux%u at %.3f\n",
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), aspect);
      return 1;
   }
   printf("[pass] raw recording geometry under rotation\n");
   return 0;
}

/* Opens and frees the driver: the choices made at init */
static int open_only(void)
{
   struct record_params params;
   void    *rec;
   unsigned frames = 0;
   double   mib    = 0.0;
   /* 16 MiB frames: 128 of them is twice the ceiling */
   const unsigned fw = 2048, fh = 2048;

   memset(&params, 0, sizeof(params));
   params.fps                        = FPS;
   params.samplerate                 = RATE;
   params.filename                   = OUT;
   params.audio_resampler            = "sinc";
   params.out_dims                   = VIDEO_SCALE_PACK(W, H);
   params.fb_dims                    = VIDEO_SCALE_PACK(fw, fh);
   params.channels                   = 2;
   params.video_record_scale_factor  = 1;
   params.video_stream_scale_factor  = 1;
   params.video_record_threads       = 1;
   params.aspect_ratio               = (float)W / H;
   params.preset                     = RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY;
   params.pix_fmt                    = FFEMU_PIX_ARGB8888;
   params.allow_frame_drop           = true;
   params.video_fifo_frames          = 128;

   stub_verbose   = true;
   stub_log_level = 0;
   remove(OUT);
   if (!(rec = record_ffmpeg.init(&params)))
   {
      fprintf(stderr, "open-only: init failed\n");
      return 1;
   }
   if (av_log_get_level() > AV_LOG_VERBOSE)
   {
      fprintf(stderr, "open-only: Debug logging asks libav* for level %d,"
            " past AV_LOG_VERBOSE\n", av_log_get_level());
      return 1;
   }
   if (     sscanf(stub_last_log, "[FFmpeg] Video queue: %u frames (%lf MiB)",
               &frames, &mib) != 2
         || mib > (sizeof(void*) > 4 ? 1024.0 : 256.0))
   {
      fprintf(stderr, "open-only: video queue %u frames, %.1f MiB, over"
            " the ceiling (%s)\n", frames, mib, stub_last_log);
      return 1;
   }
   record_ffmpeg.free(rec);
   stub_verbose   = false;
   stub_log_level = 1;
   remove(OUT);
   printf("[pass] open-only: a 128-frame queue of 16 MiB frames is %u"
         " frames (%.0f MiB); Debug logging stops at AV_LOG_VERBOSE\n",
         frames, mib);
   return 0;
}

int main(void)
{
   if (geometry() || open_only() || run(false) || run(true))
      return 1;
   printf("ALL OK\n");
   return 0;
}
