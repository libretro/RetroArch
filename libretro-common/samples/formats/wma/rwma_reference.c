/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwma_reference.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include <float.h>
#include <formats/rwma.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static float reference[8][262144];
int main(int argc, char **argv)
{
   rwma_t *d = NULL;
   rwma_config cfg;
   rwma_frame view;
   AVFormatContext *format = NULL;
   AVCodecContext *c;
   const AVCodec *codec;
   AVFrame *f = av_frame_alloc();
   AVPacket *pkt = av_packet_alloc();
   unsigned frames = 0, samples = 0, i, ch, packets = 0;
   double max_error = 0, squared = 0, energy = 0;
   int stream, ret;
   if (argc != 2 || avformat_open_input(&format, argv[1], NULL, NULL) < 0)
      return 1;
   if (avformat_find_stream_info(format, NULL) < 0)
      return 2;
   stream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
   if (stream < 0)
      return 3;
   codec = avcodec_find_decoder(format->streams[stream]->codecpar->codec_id);
   if (!codec)
      return 4;
   c = avcodec_alloc_context3(codec);
   if (avcodec_parameters_to_context(c, format->streams[stream]->codecpar) < 0)
      return 5;
   c->flags |= AV_CODEC_FLAG_BITEXACT;
   c->flags2 |= AV_CODEC_FLAG2_SKIP_MANUAL;
   if (avcodec_open2(c, codec, NULL) < 0)
      return 6;
   memset(&cfg, 0, sizeof(cfg));
   cfg.codec = c->codec_id == AV_CODEC_ID_WMAPRO  ? RWMA_PRO
               : c->codec_id == AV_CODEC_ID_WMAV1 ? RWMA_V1
                                                  : RWMA_V2;
   cfg.channels = c->ch_layout.nb_channels;
   cfg.sample_rate = c->sample_rate;
   cfg.block_align = c->block_align;
   cfg.bit_rate = (unsigned)c->bit_rate;
   cfg.extra = c->extradata;
   cfg.extra_size = c->extradata_size;
   printf("codec %u channels %u rate %u align %u bitrate %u extra %u\n", cfg.codec,
          cfg.channels, cfg.sample_rate, cfg.block_align, cfg.bit_rate,
          (unsigned)cfg.extra_size);
   ret = rwma_create(&d, &cfg);
   if (ret)
   {
      fprintf(stderr, "create %d\n", ret);
      return 7;
   }
   for (;;)
   {
      const uint8_t *data;
      size_t left;
      unsigned out_at = 0, guard = 0, reference_count = 0;
      ret = av_read_frame(format, pkt);
      if (ret < 0)
      {
         if (ret != AVERROR_EOF)
            return 17;
         break;
      }
      if (pkt->stream_index != stream)
      {
         av_packet_unref(pkt);
         continue;
      }
      packets++;
      ret = avcodec_send_packet(c, pkt);
      if (ret < 0)
         return 8;
      while ((ret = avcodec_receive_frame(c, f)) >= 0)
      {
         if (reference_count + (unsigned)f->nb_samples > 262144)
            return 21;
         for (ch = 0; ch < cfg.channels; ch++)
            memcpy(reference[ch] + reference_count, f->extended_data[ch],
                   f->nb_samples * sizeof(float));
         reference_count += f->nb_samples;
         av_frame_unref(f);
      }
      if (ret < 0 && ret != AVERROR(EAGAIN))
      {
         fprintf(stderr, "ref %d\n", ret);
         return 9;
      }
      data = pkt->data;
      left = pkt->size;
      while (left)
      {
         size_t used = 0;
         int r = rwma_decode_packet(d, data, left, &used, &view);
         if (r || used > left || ++guard > 100)
         {
            fprintf(stderr, "packet %u native %d left %u guard %u\n", packets, r,
                    (unsigned)left, guard);
            return 10;
         }
         if (!reference_count && view.samples)
         {
            fprintf(stderr, "unexpected native samples\n");
            return 11;
         }
         if (view.samples && out_at + view.samples > reference_count)
         {
            fprintf(stderr, "too many native samples\n");
            return 12;
         }
         for (ch = 0; ch < view.channels; ch++)
            for (i = 0; i < view.samples; i++)
            {
               double a = view.planes[ch][i], b = reference[ch][out_at + i],
                      e = fabs(a - b);
               if (!(a <= FLT_MAX && a >= -FLT_MAX))
                  return 13;
               if (e > max_error)
                  max_error = e;
               energy += b * b;
               squared += (a - b) * (a - b);
               samples++;
               if (e > 0.00003 + fabs(b) * 0.0001)
               {
                  fprintf(stderr, "packet %u ch%u sample%u native %.9g ref %.9g\n",
                          packets, ch, out_at + i, a, b);
                  return 14;
               }
            }
         out_at += view.samples;
         if (view.samples)
            frames++;
         data += used;
         left -= used;
      }
      if (out_at != reference_count)
      {
         fprintf(stderr, "sample count %u vs %d\n", out_at, (int)reference_count);
         return 15;
      }
      av_packet_unref(pkt);
      av_frame_unref(f);
   }
   printf("PASS %u packets %u frames %u samples max error %.9g relative RMS %.9g\n",
          packets, frames, samples, max_error, sqrt(squared / energy));
   rwma_free(d);
   avcodec_free_context(&c);
   av_frame_free(&f);
   av_packet_free(&pkt);
   avformat_close_input(&format);
   return frames ? 0 : 16;
}
