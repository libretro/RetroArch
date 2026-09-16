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
 * fails the pixel check. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../../record/record_driver.h"
#include "../../../record/drivers/record_ffmpeg.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

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

int main(void)
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
   params.out_width  = params.fb_width  = W;
   params.out_height = params.fb_height = H;
   params.channels                   = 2;
   params.video_record_scale_factor  = 1;
   params.video_stream_scale_factor  = 1;
   params.video_record_threads       = 1;
   params.aspect_ratio               = (float)W / H;
   params.preset                     = RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY;
   params.pix_fmt                    = FFEMU_PIX_ARGB8888;

   remove(OUT);
   rec = record_ffmpeg.init(&params);
   if (!rec)
   {
      fprintf(stderr, "init failed (is libx264rgb available?)\n");
      return 1;
   }

   memset(pcm, 0, sizeof(pcm));
   for (n = 0; n < FRAMES; n++)
   {
      paint(fb, n);
      vid.data    = fb;
      vid.width   = W;
      vid.height  = H;
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
            if (!check_frame(frm, got))
               return 1;
            got++;
            av_frame_unref(frm);
         }
         if (flushing)
            break;
      }
      if (got != FRAMES)
      {
         fprintf(stderr, "decoded %u frames, pushed %u\n", got, FRAMES);
         return 1;
      }
      avcodec_free_context(&dec);
      avformat_close_input(&fmt);
      av_packet_free(&pkt);
      av_frame_free(&frm);
   }
   remove(OUT);
   printf("[pass] %u frames through record_ffmpeg, all in order and pixel-exact\n", FRAMES);
   printf("ALL OK\n");
   return 0;
}
