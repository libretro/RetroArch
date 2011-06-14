#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ffemu.h"

struct video_info
{
   AVCodecContext *codec;

   AVFrame *conv_frame;
   uint8_t *conv_frame_buf;
   int64_t frame_cnt;

   uint8_t *outbuf;
   size_t outbuf_size;

   AVFormatContext *format;
} video;

struct audio_info
{
   AVCodecContext *codec;

   int16_t *buffer;
   size_t frames_in_buffer;

   int64_t frame_cnt;

   void *outbuf;
   size_t outbuf_size;

   int pix_fmt;
} audio;

struct muxer_info
{
   AVFormatContext *ctx;
   AVStream *astream;
   AVStream *vstream;
};

struct ffemu
{
   struct video_info video;
   struct audio_info audio;
   struct muxer_info muxer;
   
   struct ffemu_params params;
};

static int init_audio(struct audio_info *audio, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder(CODEC_ID_FLAC);
   if (!codec)
      return -1;

   audio->codec = avcodec_alloc_context();
   avcodec_get_context_defaults(audio->codec);

   audio->codec->sample_rate = param->samplerate;
   audio->codec->time_base = (AVRational) { 1, param->samplerate };
   audio->codec->channels = param->channels;
   audio->codec->sample_fmt = AV_SAMPLE_FMT_S16;
   if (avcodec_open(audio->codec, codec) != 0)
      return -1;

   audio->buffer = av_malloc(audio->codec->frame_size * param->channels * sizeof(int16_t));
   if (!audio->buffer)
      return -1;

   audio->outbuf_size = 200000;
   audio->outbuf = av_malloc(audio->outbuf_size);
   if (!audio->outbuf)
      return -1;

   return 0;
}

static int init_video(struct video_info *video, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder(CODEC_ID_HUFFYUV);
   if (!codec)
      return -1;

   video->codec = avcodec_alloc_context();
   video->codec->width = param->out_width;
   video->codec->height = param->out_height;
   video->codec->time_base = (AVRational) {param->fps.den, param->fps.num};
   video->codec->pix_fmt = PIX_FMT_RGB32;
   video->codec->sample_aspect_ratio = av_d2q(param->aspect_ratio * param->out_height / param->out_width, 255);

   if (avcodec_open(video->codec, codec) != 0)
      return -1;

   // Allocate a big buffer :p ffmpeg API doesn't seem to give us some clues how big this buffer should be.
   video->outbuf_size = 5000000;
   video->outbuf = av_malloc(video->outbuf_size);

   // Just to make sure we can handle the biggest frames. Seemed to crash with just 256 * 224.
   int size = avpicture_get_size(PIX_FMT_RGB32, 512, 448);
   video->conv_frame_buf = av_malloc(size);
   video->conv_frame = avcodec_alloc_frame();
   avpicture_fill((AVPicture*)video->conv_frame, video->conv_frame_buf, PIX_FMT_RGB32, 512, 448);

   return 0;
}

static int init_muxer(ffemu_t *handle)
{
   AVFormatContext *ctx = avformat_alloc_context();
   av_strlcpy(ctx->filename, handle->params.filename, sizeof(ctx->filename));
   ctx->oformat = av_guess_format(NULL, ctx->filename, NULL);
   if (avio_open(&ctx->pb, ctx->filename, URL_WRONLY) < 0)
   {
      av_free(ctx);
      return -1;
   }

   int stream_cnt = 0;
   AVStream *stream = av_new_stream(ctx, stream_cnt++);
   stream->codec = handle->video.codec;

   if (ctx->oformat->flags & AVFMT_GLOBALHEADER)
      handle->video.codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
   handle->muxer.vstream = stream;
   handle->muxer.vstream->sample_aspect_ratio = handle->video.codec->sample_aspect_ratio;

   stream = av_new_stream(ctx, stream_cnt++);
   stream->codec = handle->audio.codec;

   if (ctx->oformat->flags & AVFMT_GLOBALHEADER)
      handle->audio.codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
   handle->muxer.astream = stream;

   if (av_write_header(ctx) < 0)
      return -1;

   handle->muxer.ctx = ctx;
   return 0;
}

ffemu_t *ffemu_new(const struct ffemu_params *params)
{
   avcodec_init();
   av_register_all();

   ffemu_t *handle = calloc(1, sizeof(*handle));
   if (!handle)
      goto error;

   handle->params = *params;

   if (init_video(&handle->video, &handle->params) < 0)
      goto error;

   if (init_audio(&handle->audio, &handle->params) < 0)
      goto error;

   if (init_muxer(handle) < 0)
      goto error;

   return handle;

error:
   ffemu_free(handle);
   return NULL;
}

void ffemu_free(ffemu_t *handle)
{
   if (handle)
   {
      if (handle->audio.codec)
      {
         avcodec_close(handle->audio.codec);
         av_free(handle->audio.codec);
      }

      if (handle->audio.buffer)
         av_free(handle->audio.buffer);

      if (handle->video.codec)
      {
         avcodec_close(handle->video.codec);
         av_free(handle->video.codec);
      }

      if (handle->video.conv_frame)
         av_free(handle->video.conv_frame);

      if (handle->video.conv_frame_buf)
         av_free(handle->video.conv_frame_buf);

      free(handle);
   }
}

// Need to make this thread based, but hey.
int ffemu_push_video(ffemu_t *handle, const struct ffemu_video_data *data)
{
   // This is deprecated, can't find a proper replacement... :(
   struct SwsContext *conv_ctx = sws_getContext(data->width, data->height, PIX_FMT_RGB555LE,
         handle->params.out_width, handle->params.out_height, PIX_FMT_RGB32, SWS_POINT,
         NULL, NULL, NULL);

   int linesize = data->pitch;

   sws_scale(conv_ctx, (const uint8_t* const*)&data->data, &linesize, 0, handle->params.out_width, handle->video.conv_frame->data, handle->video.conv_frame->linesize);

   handle->video.conv_frame->pts = handle->video.frame_cnt;
   handle->video.conv_frame->display_picture_number = handle->video.frame_cnt;

   int outsize = avcodec_encode_video(handle->video.codec, handle->video.outbuf, handle->video.outbuf_size, handle->video.conv_frame);

   if (outsize < 0)
      return -1;

   sws_freeContext(conv_ctx);

   AVPacket pkt;
   av_init_packet(&pkt);
   pkt.stream_index = handle->muxer.vstream->index;
   pkt.data = handle->video.outbuf;
   pkt.size = outsize;

   pkt.pts = av_rescale_q(handle->video.codec->coded_frame->pts, handle->video.codec->time_base, handle->muxer.vstream->time_base);

   if (handle->video.codec->coded_frame->key_frame)
      pkt.flags |= AV_PKT_FLAG_KEY;

   if (pkt.size > 0)
   {
      if (av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
         return -1;
   }

   handle->video.frame_cnt++;

   return 0;
}

int ffemu_push_audio(ffemu_t *handle, const struct ffemu_audio_data *data)
{

   size_t written_frames = 0;
   while (written_frames < data->frames)
   {
      size_t can_write = handle->audio.codec->frame_size - handle->audio.frames_in_buffer;
      size_t write_frames = data->frames > can_write ? can_write : data->frames;

      memcpy(handle->audio.buffer + handle->audio.frames_in_buffer * handle->params.channels,
            data->data + written_frames * handle->params.channels, 
            write_frames * handle->params.channels * sizeof(int16_t));

      written_frames += write_frames;
      handle->audio.frames_in_buffer += write_frames;

      if (handle->audio.frames_in_buffer == (size_t)handle->audio.codec->frame_size)
      {
         AVPacket pkt;
         av_init_packet(&pkt);
         pkt.data = handle->audio.outbuf;
         pkt.stream_index = handle->muxer.astream->index;

         int out_size = avcodec_encode_audio(handle->audio.codec, handle->audio.outbuf, handle->audio.outbuf_size, handle->audio.buffer);
         if (out_size < 0)
            return -1;

         pkt.size = out_size;
         if (handle->audio.codec->coded_frame && handle->audio.codec->coded_frame->pts != AV_NOPTS_VALUE)
         {
            pkt.pts = av_rescale_q(handle->audio.codec->coded_frame->pts, handle->audio.codec->time_base, handle->muxer.astream->time_base);
         }
         else
         {
            pkt.pts = av_rescale_q(handle->audio.frame_cnt, handle->audio.codec->time_base, handle->muxer.astream->time_base);
         }

         pkt.flags |= AV_PKT_FLAG_KEY;
         handle->audio.frames_in_buffer = 0;
         handle->audio.frame_cnt += handle->audio.codec->frame_size;

         if (pkt.size > 0)
         {
            if (av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
               return -1;
         }
      }
   }
   return 0;
}

int ffemu_finalize(ffemu_t *handle)
{
   // Push out delayed frames. (MPEG codecs)
   AVPacket pkt;
   av_init_packet(&pkt);
   pkt.stream_index = handle->muxer.vstream->index;
   pkt.data = handle->video.outbuf;

   int out_size = 0;
   do
   {
      out_size = avcodec_encode_video(handle->video.codec, handle->video.outbuf, handle->video.outbuf_size, NULL);
      pkt.pts = av_rescale_q(handle->video.codec->coded_frame->pts, handle->video.codec->time_base, handle->muxer.vstream->time_base);

      if (handle->video.codec->coded_frame->key_frame)
         pkt.flags |= AV_PKT_FLAG_KEY;

      pkt.size = out_size;

      if (pkt.size > 0)
      {
         int err = av_interleaved_write_frame(handle->muxer.ctx, &pkt);
         if (err < 0)
            break;
      }

   } while (out_size > 0);

   // Write final data.
   av_write_trailer(handle->muxer.ctx);

   return 0;
}

