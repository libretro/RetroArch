#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avconfig.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ffemu.h"
#include "fifo_buffer.h"
#include "thread.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

struct video_info
{
   AVCodecContext *codec;
   AVCodec *encoder;

   AVFrame *conv_frame;
   uint8_t *conv_frame_buf;
   int64_t frame_cnt;

   uint8_t *outbuf;
   size_t outbuf_size;

   int fmt;
   int pix_fmt;
   size_t pix_size;

   AVFormatContext *format;

   struct SwsContext *sws_ctx;
} video;

struct audio_info
{
   AVCodecContext *codec;
   AVCodec *encoder;

   int16_t *buffer;
   size_t frames_in_buffer;

   int64_t frame_cnt;

   void *outbuf;
   size_t outbuf_size;
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

   scond_t *cond;
   slock_t *cond_lock;
   slock_t *lock;
   fifo_buffer_t *audio_fifo;
   fifo_buffer_t *video_fifo;
   fifo_buffer_t *attr_fifo;
   sthread_t *thread;

   volatile bool alive;
   volatile bool can_sleep;
};

static bool init_audio(struct audio_info *audio, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder(CODEC_ID_FLAC);
   if (!codec)
      return false;

   audio->encoder = codec;

   // FFmpeg just loves to deprecate stuff :)
#ifdef HAVE_FFMPEG_ALLOC_CONTEXT3
   audio->codec = avcodec_alloc_context3(codec);
#else
   audio->codec = avcodec_alloc_context();
   avcodec_get_context_defaults(audio->codec);
#endif

   audio->codec->sample_rate = param->samplerate;
   audio->codec->time_base = av_d2q(1.0 / param->samplerate, 1000000);
   audio->codec->channels = param->channels;
   audio->codec->sample_fmt = AV_SAMPLE_FMT_S16;

#ifdef HAVE_FFMPEG_AVCODEC_OPEN2
   if (avcodec_open2(audio->codec, codec, NULL) != 0)
#else
   if (avcodec_open(audio->codec, codec) != 0)
#endif
   {
      return false;
   }

   audio->buffer = av_malloc(audio->codec->frame_size * param->channels * sizeof(int16_t));
   if (!audio->buffer)
      return false;

   audio->outbuf_size = 2000000;
   audio->outbuf = av_malloc(audio->outbuf_size);
   if (!audio->outbuf)
      return false;

   return true;
}

static bool init_video(struct video_info *video, const struct ffemu_params *param)
{
#ifdef HAVE_X264RGB
   AVCodec *codec = avcodec_find_encoder(CODEC_ID_H264);
#else
   AVCodec *codec = avcodec_find_encoder(CODEC_ID_FFV1);
#endif
   if (!codec)
      return false;

   video->encoder = codec;

#if AV_HAVE_BIGENDIAN
   video->fmt = PIX_FMT_RGB555BE;
#else
   video->fmt = PIX_FMT_RGB555LE;
#endif
   video->pix_size = sizeof(uint16_t);
   if (param->rgb32)
   {
      video->fmt = PIX_FMT_RGB32;
      video->pix_size = sizeof(uint32_t);
   }

#ifdef HAVE_X264RGB
   video->pix_fmt = PIX_FMT_BGR24;
#else
   video->pix_fmt = PIX_FMT_RGB32;
#endif

#ifdef HAVE_FFMPEG_ALLOC_CONTEXT3
   video->codec = avcodec_alloc_context3(codec);
#else
   video->codec = avcodec_alloc_context();
   avcodec_get_context_defaults(video->codec);
#endif

   video->codec->width = param->out_width;
   video->codec->height = param->out_height;
   video->codec->time_base = av_d2q(1.0 / param->fps, 1000000); // Arbitrary big number.
   video->codec->sample_aspect_ratio = av_d2q(param->aspect_ratio * param->out_height / param->out_width, 255);
   video->codec->pix_fmt = video->pix_fmt;

#ifdef HAVE_FFMPEG_AVCODEC_OPEN2
   AVDictionary *opts = NULL;
#endif

#ifdef HAVE_X264RGB
   video->codec->thread_count = 3;
   av_dict_set(&opts, "qp", "0", 0);
#else
   video->codec->thread_count = 2;
#endif

#ifdef HAVE_FFMPEG_AVCODEC_OPEN2
   if (avcodec_open2(video->codec, codec, &opts) != 0)
#else
   if (avcodec_open(video->codec, codec) != 0)
#endif
   {
      return false;
   }

#ifdef HAVE_FFMPEG_AVCODEC_OPEN2
   if (opts)
      av_dict_free(&opts);
#endif

   // Allocate a big buffer :p ffmpeg API doesn't seem to give us some clues how big this buffer should be.
   video->outbuf_size = 1 << 23;
   video->outbuf = av_malloc(video->outbuf_size);

   size_t size = avpicture_get_size(video->pix_fmt, param->out_width, param->out_height);
   video->conv_frame_buf = av_malloc(size);
   video->conv_frame = avcodec_alloc_frame();
   avpicture_fill((AVPicture*)video->conv_frame, video->conv_frame_buf, video->pix_fmt, param->out_width, param->out_height);

   return true;
}

static bool init_muxer(ffemu_t *handle)
{
   AVFormatContext *ctx = avformat_alloc_context();
   av_strlcpy(ctx->filename, handle->params.filename, sizeof(ctx->filename));
   ctx->oformat = av_guess_format(NULL, ctx->filename, NULL);

   // FFmpeg sure likes to make things difficult.
#if defined(AVIO_FLAG_WRITE)
#define FFMPEG_FLAG_RW AVIO_FLAG_WRITE
#elif defined(AVIO_WRONLY)
#define FFMPEG_FLAG_RW AVIO_WRONLY
#elif defined(URL_WRONLY)
#define FFMPEG_FLAG_RW URL_WRONLY
#else
#define FFMPEG_FLAG_RW 2 // Seems to be consistent, but you never know.
#endif

#ifdef HAVE_FFMPEG_AVIO_OPEN
   if (avio_open(&ctx->pb, ctx->filename, FFMPEG_FLAG_RW) < 0)
#else
   if (url_fopen(&ctx->pb, ctx->filename, FFMPEG_FLAG_RW) < 0)
#endif
   {
      av_free(ctx);
      return false;
   }

#ifdef HAVE_FFMPEG_AVFORMAT_NEW_STREAM
   AVStream *stream = avformat_new_stream(ctx, handle->video.encoder);
#else
   unsigned stream_cnt = 0;
   AVStream *stream = av_new_stream(ctx, stream_cnt++);
#endif
   stream->codec = handle->video.codec;

   if (ctx->oformat->flags & AVFMT_GLOBALHEADER)
      handle->video.codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
   handle->muxer.vstream = stream;
   handle->muxer.vstream->sample_aspect_ratio = handle->video.codec->sample_aspect_ratio;

#ifdef HAVE_FFMPEG_AVFORMAT_NEW_STREAM
   stream = avformat_new_stream(ctx, handle->audio.encoder);
#else
   stream = av_new_stream(ctx, stream_cnt++);
#endif
   stream->codec = handle->audio.codec;

   if (ctx->oformat->flags & AVFMT_GLOBALHEADER)
      handle->audio.codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
   handle->muxer.astream = stream;

#ifdef HAVE_X264RGB // Avoids a warning at end about non-monotonically increasing DTS values. It seems to be harmless to disable this.
   ctx->oformat->flags |= AVFMT_TS_NONSTRICT;
#endif

   av_dict_set(&ctx->metadata, "title", "SSNES video dump", 0); 

#ifdef HAVE_FFMPEG_AVFORMAT_WRITE_HEADER
   if (avformat_write_header(ctx, NULL) < 0)
#else
   if (av_write_header(ctx) != 0)
#endif
   {
      return false;
   }

   handle->muxer.ctx = ctx;
   return true;
}

#define MAX_FRAMES 32

static void ffemu_thread(void *data);

static bool init_thread(ffemu_t *handle)
{
   assert(handle->lock = slock_new());
   assert(handle->cond_lock = slock_new());
   assert(handle->cond = scond_new());
   assert(handle->audio_fifo = fifo_new(32000 * sizeof(int16_t) * handle->params.channels * MAX_FRAMES / 60));
   assert(handle->attr_fifo = fifo_new(sizeof(struct ffemu_video_data) * MAX_FRAMES));
   assert(handle->video_fifo = fifo_new(handle->params.fb_width * handle->params.fb_height *
            handle->video.pix_size * MAX_FRAMES));

   handle->alive = true;
   handle->can_sleep = true;
   assert(handle->thread = sthread_create(ffemu_thread, handle));

   return true;
}

static void deinit_thread(ffemu_t *handle)
{
   if (handle->thread)
   {
      slock_lock(handle->cond_lock);
      handle->alive = false;
      handle->can_sleep = false;
      slock_unlock(handle->cond_lock);

      scond_signal(handle->cond);
      sthread_join(handle->thread);

      slock_free(handle->lock);
      slock_free(handle->cond_lock);
      scond_free(handle->cond);

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

bool ffemu_push_video(ffemu_t *handle, const struct ffemu_video_data *data)
{
   for (;;)
   {
      slock_lock(handle->lock);
      unsigned avail = fifo_write_avail(handle->attr_fifo);
      slock_unlock(handle->lock);

      if (!handle->alive)
         return false;

      if (avail >= sizeof(*data))
         break;

      slock_lock(handle->cond_lock);
      if (handle->can_sleep)
      {
         handle->can_sleep = false;
         scond_wait(handle->cond, handle->cond_lock);
         handle->can_sleep = true;
      }
      else
         scond_signal(handle->cond);

      slock_unlock(handle->cond_lock);
   }

   slock_lock(handle->lock);

   // Tightly pack our frame to conserve memory. libsnes tends to use a very large pitch.
   struct ffemu_video_data attr_data = *data;
   attr_data.pitch = attr_data.width * handle->video.pix_size;

   fifo_write(handle->attr_fifo, &attr_data, sizeof(attr_data));

   unsigned offset = 0;
   for (unsigned y = 0; y < data->height; y++, offset += data->pitch)
      fifo_write(handle->video_fifo, (const uint8_t*)data->data + offset, attr_data.pitch);

   slock_unlock(handle->lock);
   scond_signal(handle->cond);

   return true;
}

bool ffemu_push_audio(ffemu_t *handle, const struct ffemu_audio_data *data)
{
   for (;;)
   {
      slock_lock(handle->lock);
      unsigned avail = fifo_write_avail(handle->audio_fifo);
      slock_unlock(handle->lock);

      if (!handle->alive)
         return false;

      if (avail >= data->frames * handle->params.channels * sizeof(int16_t))
         break;

      slock_lock(handle->cond_lock);
      if (handle->can_sleep)
      {
         handle->can_sleep = false;
         scond_wait(handle->cond, handle->cond_lock);
         handle->can_sleep = true;
      }
      else
         scond_signal(handle->cond);

      slock_unlock(handle->cond_lock);
   }

   slock_lock(handle->lock);
   fifo_write(handle->audio_fifo, data->data, data->frames * handle->params.channels * sizeof(int16_t));
   slock_unlock(handle->lock);
   scond_signal(handle->cond);

   return true;
}

static bool ffemu_push_video_thread(ffemu_t *handle, const struct ffemu_video_data *data)
{
   handle->video.sws_ctx = sws_getCachedContext(handle->video.sws_ctx, data->width, data->height, handle->video.fmt,
         handle->params.out_width, handle->params.out_height, handle->video.pix_fmt, SWS_POINT,
         NULL, NULL, NULL);

   int linesize = data->pitch;

   sws_scale(handle->video.sws_ctx, (const uint8_t* const*)&data->data, &linesize, 0,
         data->height, handle->video.conv_frame->data, handle->video.conv_frame->linesize);

   handle->video.conv_frame->pts = handle->video.frame_cnt;

   int outsize = avcodec_encode_video(handle->video.codec, handle->video.outbuf,
         handle->video.outbuf_size, handle->video.conv_frame);

   if (outsize < 0)
      return false;

   AVPacket pkt;
   av_init_packet(&pkt);
   pkt.stream_index = handle->muxer.vstream->index;
   pkt.data = handle->video.outbuf;
   pkt.size = outsize;

   pkt.pts = av_rescale_q(handle->video.codec->coded_frame->pts, handle->video.codec->time_base,
         handle->muxer.vstream->time_base);

   if (handle->video.codec->coded_frame->key_frame)
      pkt.flags |= AV_PKT_FLAG_KEY;

   if (pkt.size > 0)
   {
      if (av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
         return false;
   }

   handle->video.frame_cnt++;

   return true;
}

static bool ffemu_push_audio_thread(ffemu_t *handle, const struct ffemu_audio_data *data)
{
   size_t written_frames = 0;
   while (written_frames < data->frames)
   {
      size_t can_write = handle->audio.codec->frame_size - handle->audio.frames_in_buffer;
      size_t write_frames = data->frames - written_frames > can_write ? can_write : data->frames - written_frames;

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
            return false;

         pkt.size = out_size;

         pkt.pts = av_rescale_q(handle->audio.codec->coded_frame->pts, handle->audio.codec->time_base, handle->muxer.astream->time_base);

         pkt.flags |= AV_PKT_FLAG_KEY;
         handle->audio.frames_in_buffer = 0;
         handle->audio.frame_cnt += handle->audio.codec->frame_size;

         if (pkt.size > 0)
         {
            if (av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
               return false;
         }
      }
   }
   return true;
}

bool ffemu_finalize(ffemu_t *handle)
{
   deinit_thread(handle);

   // Push out audio still in queue.
   size_t audio_buf_size = 512 * handle->params.channels * sizeof(int16_t);
   int16_t *audio_buf = av_malloc(audio_buf_size);
   if (audio_buf)
   {
      while (fifo_read_avail(handle->audio_fifo) >= audio_buf_size)
      {
         fifo_read(handle->audio_fifo, audio_buf, sizeof(audio_buf_size));

         struct ffemu_audio_data aud = {
            .frames = 512,
            .data = audio_buf
         };

         ffemu_push_audio_thread(handle, &aud);
      }

      size_t avail = fifo_read_avail(handle->audio_fifo);
      fifo_read(handle->audio_fifo, audio_buf, avail);
      struct ffemu_audio_data aud = {
         .frames = avail / (sizeof(int16_t) * handle->params.channels),
         .data = audio_buf
      };

      ffemu_push_audio_thread(handle, &aud);

      av_free(audio_buf);
   }

   // Push out frames still stuck in queue.
   void *video_buf = av_malloc(2 * handle->params.fb_width * handle->params.fb_height * handle->video.pix_size);
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

   deinit_thread_buf(handle);

   // Push out delayed frames. (MPEG codecs)
   AVPacket pkt;
   av_init_packet(&pkt);
   pkt.stream_index = handle->muxer.vstream->index;
   pkt.data = handle->video.outbuf;

   int out_size = 0;
   for (;;)
   {
      out_size = avcodec_encode_video(handle->video.codec, handle->video.outbuf, handle->video.outbuf_size, NULL);
      if (out_size <= 0)
         break;

      pkt.pts = av_rescale_q(handle->video.codec->coded_frame->pts, handle->video.codec->time_base, handle->muxer.vstream->time_base);

      if (handle->video.codec->coded_frame->key_frame)
         pkt.flags |= AV_PKT_FLAG_KEY;

      pkt.size = out_size;
      int err = av_interleaved_write_frame(handle->muxer.ctx, &pkt);
      if (err < 0)
         break;
   }

   // Write final data.
   av_write_trailer(handle->muxer.ctx);

   return true;
}

static void ffemu_thread(void *data)
{
   ffemu_t *ff = data;

   // For some reason, FFmpeg has a tendency to crash if we don't overallocate a bit. :s
   void *video_buf = av_malloc(2 * ff->params.fb_width * ff->params.fb_height * ff->video.pix_size);
   assert(video_buf);

   size_t audio_buf_size = 512 * ff->params.channels * sizeof(int16_t);
   int16_t *audio_buf = av_malloc(audio_buf_size);
   assert(audio_buf);

   while (ff->alive)
   {
      struct ffemu_video_data attr_buf;

      bool avail_video = false;
      bool avail_audio = false;

      slock_lock(ff->lock);
      if (fifo_read_avail(ff->attr_fifo) >= sizeof(attr_buf))
         avail_video = true;

      if (fifo_read_avail(ff->audio_fifo) >= audio_buf_size)
         avail_audio = true;
      slock_unlock(ff->lock);

      if (!avail_video && !avail_audio)
      {
         slock_lock(ff->cond_lock);
         if (ff->can_sleep)
         {
            ff->can_sleep = false;
            scond_wait(ff->cond, ff->cond_lock);
            ff->can_sleep = true;
         }
         else
            scond_signal(ff->cond);

         slock_unlock(ff->cond_lock);
      }

      if (avail_video)
      {
         slock_lock(ff->lock);
         fifo_read(ff->attr_fifo, &attr_buf, sizeof(attr_buf));
         fifo_read(ff->video_fifo, video_buf, attr_buf.height * attr_buf.pitch);
         slock_unlock(ff->lock);
         scond_signal(ff->cond);

         attr_buf.data = video_buf;
         ffemu_push_video_thread(ff, &attr_buf);
      }

      if (avail_audio)
      {
         slock_lock(ff->lock);
         fifo_read(ff->audio_fifo, audio_buf, audio_buf_size);
         slock_unlock(ff->lock);
         scond_signal(ff->cond);

         struct ffemu_audio_data aud = {
            .frames = 512,
            .data = audio_buf
         };

         ffemu_push_audio_thread(ff, &aud);
      }
   }

   av_free(video_buf);
   av_free(audio_buf);
}

