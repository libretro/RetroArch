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
#include "fifo_buffer.h"
#include "SDL_thread.h"

struct video_info
{
   AVCodecContext *codec;

   AVFrame *conv_frame;
   uint8_t *conv_frame_buf;
   int64_t frame_cnt;

   uint8_t *outbuf;
   size_t outbuf_size;

   AVFormatContext *format;

   struct SwsContext *sws_ctx;
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

   SDL_cond *cond;
   SDL_mutex *cond_lock;
   SDL_mutex *lock;
   fifo_buffer_t *audio_fifo;
   fifo_buffer_t *video_fifo;
   fifo_buffer_t *attr_fifo;
   SDL_Thread *thread;
   volatile bool alive;
   volatile bool can_sleep;
};

static bool init_audio(struct audio_info *audio, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder(CODEC_ID_FLAC);
   if (!codec)
      return false;

   audio->codec = avcodec_alloc_context();
   avcodec_get_context_defaults(audio->codec);

   audio->codec->sample_rate = param->samplerate;
   audio->codec->time_base = (AVRational) { 1, param->samplerate };
   audio->codec->channels = param->channels;
   audio->codec->sample_fmt = AV_SAMPLE_FMT_S16;
   if (avcodec_open(audio->codec, codec) != 0)
      return false;

   audio->buffer = av_malloc(audio->codec->frame_size * param->channels * sizeof(int16_t));
   if (!audio->buffer)
      return false;

   audio->outbuf_size = 200000;
   audio->outbuf = av_malloc(audio->outbuf_size);
   if (!audio->outbuf)
      return false;

   return true;
}

static bool init_video(struct video_info *video, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder(CODEC_ID_FFV1);
   if (!codec)
      return false;

   video->codec = avcodec_alloc_context();
   video->codec->width = param->out_width;
   video->codec->height = param->out_height;
   video->codec->time_base = (AVRational) {param->fps.den, param->fps.num};
   video->codec->pix_fmt = PIX_FMT_RGB32;
   video->codec->sample_aspect_ratio = av_d2q(param->aspect_ratio * param->out_height / param->out_width, 255);

   if (avcodec_open(video->codec, codec) != 0)
      return false;

   // Allocate a big buffer :p ffmpeg API doesn't seem to give us some clues how big this buffer should be.
   video->outbuf_size = 2000000;
   video->outbuf = av_malloc(video->outbuf_size);

   // Just to make sure we can handle the biggest frames. Seemed to crash with just 256 * 224.
   int size = avpicture_get_size(PIX_FMT_RGB32, param->fb_width, param->fb_height);
   video->conv_frame_buf = av_malloc(size);
   video->conv_frame = avcodec_alloc_frame();
   avpicture_fill((AVPicture*)video->conv_frame, video->conv_frame_buf, PIX_FMT_RGB32, param->fb_width, param->fb_height);

   return true;
}

static bool init_muxer(ffemu_t *handle)
{
   AVFormatContext *ctx = avformat_alloc_context();
   av_strlcpy(ctx->filename, handle->params.filename, sizeof(ctx->filename));
   ctx->oformat = av_guess_format(NULL, ctx->filename, NULL);
#ifdef AVIO_FLAG_WRITE
   if (avio_open(&ctx->pb, ctx->filename, AVIO_FLAG_WRITE) < 0)
#else
   if (url_fopen(&ctx->pb, ctx->filename, URL_WRONLY) < 0)
#endif
   {
      av_free(ctx);
      return false;
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
      return false;

   handle->muxer.ctx = ctx;
   return true;
}

#define MAX_FRAMES 64

static int SDLCALL ffemu_thread(void *data);

static bool init_thread(ffemu_t *handle)
{
   assert(handle->lock = SDL_CreateMutex());
   assert(handle->cond_lock = SDL_CreateMutex());
   assert(handle->cond = SDL_CreateCond());
   assert(handle->audio_fifo = fifo_new(32000 * sizeof(int16_t) * handle->params.channels * MAX_FRAMES / 60));
   assert(handle->attr_fifo = fifo_new(sizeof(struct ffemu_video_data) * MAX_FRAMES));
   assert(handle->video_fifo = fifo_new(handle->params.fb_width * handle->params.fb_height * sizeof(int16_t) * MAX_FRAMES));

   handle->alive = true;
   handle->can_sleep = true;
   assert(handle->thread = SDL_CreateThread(ffemu_thread, handle));

   return true;
}

static void deinit_thread(ffemu_t *handle)
{
   if (handle->thread)
   {
      SDL_mutexP(handle->cond_lock);
      handle->alive = false;
      handle->can_sleep = false;
      SDL_mutexV(handle->cond_lock);

      SDL_CondSignal(handle->cond);
      SDL_WaitThread(handle->thread, NULL);

      SDL_DestroyMutex(handle->lock);
      SDL_DestroyMutex(handle->cond_lock);
      SDL_DestroyCond(handle->cond);

      handle->thread = NULL;
   }
}

static void deinit_thread_buf(ffemu_t *handle)
{
   if (handle->audio_fifo)
   {
      fifo_free(handle->audio_fifo);
      handle->audio_fifo = NULL;
   }
   
   if (handle->attr_fifo)
   {
      fifo_free(handle->attr_fifo);
      handle->attr_fifo = NULL;
   }

   if (handle->video_fifo)
   {
      fifo_free(handle->video_fifo);
      handle->video_fifo = NULL;
   }
}

ffemu_t *ffemu_new(const struct ffemu_params *params)
{
   avcodec_init();
   av_register_all();

   ffemu_t *handle = calloc(1, sizeof(*handle));
   if (!handle)
      goto error;

   handle->params = *params;

   if (!init_video(&handle->video, &handle->params))
      goto error;

   if (!init_audio(&handle->audio, &handle->params))
      goto error;

   if (!init_muxer(handle))
      goto error;

   if (!init_thread(handle))
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
      deinit_thread(handle);
      deinit_thread_buf(handle);

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

      if (handle->video.sws_ctx)
         sws_freeContext(handle->video.sws_ctx);

      free(handle);
   }
}

int ffemu_push_video(ffemu_t *handle, const struct ffemu_video_data *data)
{
   for (;;)
   {
      SDL_mutexP(handle->lock);
      unsigned avail = fifo_write_avail(handle->attr_fifo);
      SDL_mutexV(handle->lock);

      if (!handle->alive)
         return -1;

      if (avail >= sizeof(*data))
         break;

      SDL_mutexP(handle->cond_lock);
      if (handle->can_sleep)
      {
         handle->can_sleep = false;
         SDL_CondWait(handle->cond, handle->cond_lock);
         handle->can_sleep = true;
      }
      else
         SDL_CondSignal(handle->cond);

      SDL_mutexV(handle->cond_lock);
   }

   SDL_mutexP(handle->lock);
   fifo_write(handle->attr_fifo, data, sizeof(*data));
   fifo_write(handle->video_fifo, data->data, data->pitch * data->height);
   SDL_mutexV(handle->lock);
   SDL_CondSignal(handle->cond);

   return 0;
}

int ffemu_push_audio(ffemu_t *handle, const struct ffemu_audio_data *data)
{
   for (;;)
   {
      SDL_mutexP(handle->lock);
      unsigned avail = fifo_write_avail(handle->audio_fifo);
      SDL_mutexV(handle->lock);

      if (!handle->alive)
         return -1;

      if (avail >= data->frames * handle->params.channels * sizeof(int16_t))
         break;

      SDL_mutexP(handle->cond_lock);
      if (handle->can_sleep)
      {
         handle->can_sleep = false;
         SDL_CondWait(handle->cond, handle->cond_lock);
         handle->can_sleep = true;
      }
      else
         SDL_CondSignal(handle->cond);

      SDL_mutexV(handle->cond_lock);
   }

   SDL_mutexP(handle->lock);
   fifo_write(handle->audio_fifo, data->data, data->frames * handle->params.channels * sizeof(int16_t));
   SDL_mutexV(handle->lock);
   SDL_CondSignal(handle->cond);

   return 0;
}

static int ffemu_push_video_thread(ffemu_t *handle, const struct ffemu_video_data *data)
{
   handle->video.sws_ctx = sws_getCachedContext(handle->video.sws_ctx, data->width, data->height, PIX_FMT_RGB555LE,
         handle->params.out_width, handle->params.out_height, PIX_FMT_RGB32, SWS_POINT,
         NULL, NULL, NULL);

   int linesize = data->pitch;

   sws_scale(handle->video.sws_ctx, (const uint8_t* const*)&data->data, &linesize, 0, handle->params.out_width, handle->video.conv_frame->data, handle->video.conv_frame->linesize);

   handle->video.conv_frame->pts = handle->video.frame_cnt;
   handle->video.conv_frame->display_picture_number = handle->video.frame_cnt;

   int outsize = avcodec_encode_video(handle->video.codec, handle->video.outbuf, handle->video.outbuf_size, handle->video.conv_frame);

   if (outsize < 0)
      return -1;

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

static int ffemu_push_audio_thread(ffemu_t *handle, const struct ffemu_audio_data *data)
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
            pkt.pts = av_rescale_q(handle->audio.codec->coded_frame->pts, handle->audio.codec->time_base, handle->muxer.astream->time_base);
         else
            pkt.pts = av_rescale_q(handle->audio.frame_cnt, handle->audio.codec->time_base, handle->muxer.astream->time_base);

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
   deinit_thread(handle);

   // Push out frames still stuck in queue.
   uint16_t *video_buf = av_malloc(handle->params.fb_width * handle->params.fb_height * sizeof(uint16_t));
   if (video_buf)
   {
      struct ffemu_video_data attr_buf;
      while (fifo_read_avail(handle->attr_fifo) >= sizeof(attr_buf))
      {
         fifo_read(handle->attr_fifo, &attr_buf, sizeof(attr_buf));
         fifo_read(handle->video_fifo, video_buf, attr_buf.height * attr_buf.pitch);
         attr_buf.data = video_buf;
         ffemu_push_video_thread(handle, &attr_buf);
      }
      av_free(video_buf);
   }

   size_t audio_buf_size = 128 * handle->params.channels * sizeof(int16_t);
   int16_t *audio_buf = av_malloc(audio_buf_size);
   if (audio_buf)
   {
      while (fifo_read_avail(handle->audio_fifo) >= audio_buf_size)
      {
         fifo_read(handle->audio_fifo, audio_buf, sizeof(audio_buf_size));

         struct ffemu_audio_data aud = {
            .frames = 128,
            .data = audio_buf
         };

         ffemu_push_audio_thread(handle, &aud);
      }
      av_free(audio_buf);
   }

   deinit_thread_buf(handle);

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

static int SDLCALL ffemu_thread(void *data)
{
   ffemu_t *ff = data;

   uint16_t *video_buf = av_malloc(ff->params.fb_width * ff->params.fb_height * sizeof(uint16_t));
   assert(video_buf);

   size_t audio_buf_size = 128 * ff->params.channels * sizeof(int16_t);
   int16_t *audio_buf = av_malloc(audio_buf_size);
   assert(audio_buf);

   struct ffemu_video_data attr_buf;

   while (ff->alive)
   {
      bool avail_video = false;
      bool avail_audio = false;

      SDL_mutexP(ff->lock);
      if (fifo_read_avail(ff->attr_fifo) >= sizeof(attr_buf))
         avail_video = true;

      if (fifo_read_avail(ff->audio_fifo) >= audio_buf_size)
         avail_audio = true;
      SDL_mutexV(ff->lock);

      if (!avail_video && !avail_audio)
      {
         SDL_mutexP(ff->cond_lock);
         if (ff->can_sleep)
         {
            ff->can_sleep = false;
            SDL_CondWait(ff->cond, ff->cond_lock);
            ff->can_sleep = true;
         }
         else
            SDL_CondSignal(ff->cond);

         SDL_mutexV(ff->cond_lock);
      }

      if (avail_video)
      {
         SDL_mutexP(ff->lock);
         fifo_read(ff->attr_fifo, &attr_buf, sizeof(attr_buf));
         fifo_read(ff->video_fifo, video_buf, attr_buf.height * attr_buf.pitch);
         SDL_mutexV(ff->lock);
         SDL_CondSignal(ff->cond);

         attr_buf.data = video_buf;
         ffemu_push_video_thread(ff, &attr_buf);
      }

      if (avail_audio)
      {
         SDL_mutexP(ff->lock);
         fifo_read(ff->audio_fifo, audio_buf, audio_buf_size);
         SDL_mutexV(ff->lock);
         SDL_CondSignal(ff->cond);

         struct ffemu_audio_data aud = {
            .frames = 128,
            .data = audio_buf
         };

         ffemu_push_audio_thread(ff, &aud);
      }
   }

   av_free(video_buf);
   av_free(audio_buf);

   return 0;
}
