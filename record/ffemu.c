/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../msvc/msvc_compat.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libavutil/avconfig.h>
#ifdef __cplusplus
}
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../boolean.h"
#include "../fifo_buffer.h"
#include "../thread.h"
#include "../general.h"
#include "../gfx/scaler/scaler.h"
#include "ffemu.h"
#include <assert.h>

#ifdef FFEMU_PERF
#include <time.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

struct ff_video_info
{
   AVCodecContext *codec;
   AVCodec *encoder;

   AVFrame *conv_frame;
   uint8_t *conv_frame_buf;
   int64_t frame_cnt;

   uint8_t *outbuf;
   size_t outbuf_size;

   enum PixelFormat pix_fmt;
   size_t pix_size;

   AVFormatContext *format;

   struct scaler_ctx scaler;
};

struct ff_audio_info
{
   AVCodecContext *codec;
   AVCodec *encoder;

   int16_t *buffer;
   size_t frames_in_buffer;

   int64_t frame_cnt;

   uint8_t *outbuf;
   size_t outbuf_size;
};

struct ff_muxer_info
{
   AVFormatContext *ctx;
   AVStream *astream;
   AVStream *vstream;
};

struct ffemu
{
   struct ff_video_info video;
   struct ff_audio_info audio;
   struct ff_muxer_info muxer;
   
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

static bool ffemu_init_audio(struct ff_audio_info *audio, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder_by_name("flac");
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

   audio->codec->sample_rate = (int)roundf(param->samplerate);
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

   audio->buffer = (int16_t*)av_malloc(
         audio->codec->frame_size *
         audio->codec->channels *
         sizeof(int16_t));

   if (!audio->buffer)
      return false;

   audio->outbuf_size = FF_MIN_BUFFER_SIZE;
   audio->outbuf = (uint8_t*)av_malloc(audio->outbuf_size);
   if (!audio->outbuf)
      return false;

   return true;
}

static bool ffemu_init_video(struct ff_video_info *video, const struct ffemu_params *param)
{
   AVCodec *codec = NULL;
   if (g_settings.video.h264_record)
   {
      codec = avcodec_find_encoder_by_name("libx264rgb");
      // Older versions of FFmpeg have RGB encoding in libx264.
      if (!codec)
         codec = avcodec_find_encoder_by_name("libx264");
   }
   else
      codec = avcodec_find_encoder_by_name("ffv1");

   if (!codec)
      return false;

   video->encoder = codec;

   switch (param->pix_fmt)
   {
      case FFEMU_PIX_RGB565:
         video->scaler.in_fmt = SCALER_FMT_RGB565;
         video->pix_size = 2;
         break;

      case FFEMU_PIX_BGR24:
         video->scaler.in_fmt = SCALER_FMT_BGR24;
         video->pix_size = 3;
         break;

      case FFEMU_PIX_ARGB8888:
         video->scaler.in_fmt = SCALER_FMT_ARGB8888;
         video->pix_size = 4;
         break;

      default:
         return false;
   }

   if (g_settings.video.h264_record)
   {
      video->pix_fmt = PIX_FMT_BGR24;
      video->scaler.out_fmt = SCALER_FMT_BGR24;
   }
   else
   {
      video->pix_fmt = PIX_FMT_RGB32;
      video->scaler.out_fmt = SCALER_FMT_ARGB8888;
   }

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

   if (g_settings.video.h264_record)
   {
      video->codec->thread_count = 3;
      av_dict_set(&opts, "qp", "0", 0);
   }
   else
      video->codec->thread_count = 2;

#ifdef HAVE_FFMPEG_AVCODEC_OPEN2
   if (avcodec_open2(video->codec, codec, &opts) != 0)
#else
   if (avcodec_open(video->codec, codec) != 0)
#endif
      return false;

#ifdef HAVE_FFMPEG_AVCODEC_OPEN2
   if (opts)
      av_dict_free(&opts);
#endif

   // Allocate a big buffer :p ffmpeg API doesn't seem to give us some clues how big this buffer should be.
   video->outbuf_size = 1 << 23;
   video->outbuf = (uint8_t*)av_malloc(video->outbuf_size);

   size_t size = avpicture_get_size(video->pix_fmt, param->out_width, param->out_height);
   video->conv_frame_buf = (uint8_t*)av_malloc(size);
   video->conv_frame = avcodec_alloc_frame();
   avpicture_fill((AVPicture*)video->conv_frame, video->conv_frame_buf, video->pix_fmt, param->out_width, param->out_height);

   return true;
}

static bool ffemu_init_muxer(ffemu_t *handle)
{
   AVFormatContext *ctx = avformat_alloc_context();
   av_strlcpy(ctx->filename, handle->params.filename, sizeof(ctx->filename));
   ctx->oformat = av_guess_format(NULL, ctx->filename, NULL);

   if (!ctx->oformat)
      return false;

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

#ifdef AVFMT_TS_NONSTRICT
   // Avoids a warning at end about non-monotonically increasing DTS values.
   // It seems to be harmless to disable this.
   if (g_settings.video.h264_record)
      ctx->oformat->flags |= AVFMT_TS_NONSTRICT;
#endif

   av_dict_set(&ctx->metadata, "title", "RetroArch video dump", 0); 

#ifdef HAVE_FFMPEG_AVFORMAT_WRITE_HEADER
   if (avformat_write_header(ctx, NULL) < 0)
#else
   if (av_write_header(ctx) != 0)
#endif
      return false;

   handle->muxer.ctx = ctx;
   return true;
}

#define MAX_FRAMES 32

static void ffemu_thread(void *data);

static bool init_thread(ffemu_t *handle)
{
   handle->lock = slock_new();
   handle->cond_lock = slock_new();
   handle->cond = scond_new();
   handle->audio_fifo = fifo_new(32000 * sizeof(int16_t) * handle->params.channels * MAX_FRAMES / 60);
   handle->attr_fifo = fifo_new(sizeof(struct ffemu_video_data) * MAX_FRAMES);
   handle->video_fifo = fifo_new(handle->params.fb_width * handle->params.fb_height *
            handle->video.pix_size * MAX_FRAMES);

   handle->alive = true;
   handle->can_sleep = true;
   handle->thread = sthread_create(ffemu_thread, handle);

   assert(handle->lock && handle->cond_lock &&
      handle->cond && handle->audio_fifo &&
      handle->attr_fifo && handle->video_fifo && handle->thread);

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

   ffemu_t *handle = (ffemu_t*)calloc(1, sizeof(*handle));
   if (!handle)
      goto error;

   handle->params = *params;

   if (!ffemu_init_video(&handle->video, &handle->params))
      goto error;

   if (!ffemu_init_audio(&handle->audio, &handle->params))
      goto error;

   if (!ffemu_init_muxer(handle))
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

      scaler_ctx_gen_reset(&handle->video.scaler);

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

   // Tightly pack our frame to conserve memory. libretro tends to use a very large pitch.
   struct ffemu_video_data attr_data = *data;

   if (attr_data.is_dupe)
      attr_data.width = attr_data.height = attr_data.pitch = 0;
   else
      attr_data.pitch = attr_data.width * handle->video.pix_size;

   fifo_write(handle->attr_fifo, &attr_data, sizeof(attr_data));

   int offset = 0;
   for (unsigned y = 0; y < attr_data.height; y++, offset += data->pitch)
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

static bool encode_video(ffemu_t *handle, AVPacket *pkt, AVFrame *frame)
{
   av_init_packet(pkt);
   pkt->data = handle->video.outbuf;
   pkt->size = handle->video.outbuf_size;

#ifdef HAVE_FFMPEG_AVCODEC_ENCODE_VIDEO2
   int got_packet = 0;
   if (avcodec_encode_video2(handle->video.codec, pkt, frame, &got_packet) < 0)
      return false;

   if (!got_packet)
   {
      pkt->size = 0;
      pkt->pts = AV_NOPTS_VALUE;
      pkt->dts = AV_NOPTS_VALUE;
      return true;
   }

   if (pkt->pts != (int64_t)AV_NOPTS_VALUE)
   {
      pkt->pts = av_rescale_q(pkt->pts, handle->video.codec->time_base,
            handle->muxer.vstream->time_base);
   }

   if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
   {
      pkt->dts = av_rescale_q(pkt->dts, handle->video.codec->time_base,
            handle->muxer.vstream->time_base);
   }

#else
   int outsize = avcodec_encode_video(handle->video.codec, handle->video.outbuf,
         handle->video.outbuf_size, frame);

   if (outsize < 0)
      return false;

   pkt->size = outsize;

   if (handle->video.codec->coded_frame->pts != (int64_t)AV_NOPTS_VALUE)
   {
      pkt->pts = av_rescale_q(handle->video.codec->coded_frame->pts, handle->video.codec->time_base,
            handle->muxer.vstream->time_base);
   }
   else
      pkt->pts = AV_NOPTS_VALUE;

   if (handle->video.codec->coded_frame->key_frame)
      pkt->flags |= AV_PKT_FLAG_KEY;
#endif

   pkt->stream_index = handle->muxer.vstream->index;
   return true;
}

static bool ffemu_push_video_thread(ffemu_t *handle, const struct ffemu_video_data *data)
{
   if (!data->is_dupe)
   {
      if ((int)data->width != handle->video.scaler.in_width || (int)data->height != handle->video.scaler.in_height)
      {
         handle->video.scaler.in_width  = data->width;
         handle->video.scaler.in_height = data->height;
         handle->video.scaler.in_stride = data->pitch;

         // Attempt to preserve more information if we scale down.
         bool shrunk = handle->params.out_width < data->width || handle->params.out_height < data->height;
         handle->video.scaler.scaler_type = shrunk ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT;

         handle->video.scaler.out_width  = handle->params.out_width;
         handle->video.scaler.out_height = handle->params.out_height;
         handle->video.scaler.out_stride = handle->video.conv_frame->linesize[0];

         scaler_ctx_gen_filter(&handle->video.scaler);
      }

      scaler_ctx_scale(&handle->video.scaler, handle->video.conv_frame->data[0], data->data);
   }

   handle->video.conv_frame->pts = handle->video.frame_cnt;

   AVPacket pkt;
   if (!encode_video(handle, &pkt, handle->video.conv_frame))
      return false;

   if (pkt.size)
   {
      if (av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
         return false;
   }

   handle->video.frame_cnt++;
   return true;
}

static bool encode_audio(ffemu_t *handle, AVPacket *pkt, bool dry)
{
   av_init_packet(pkt);
   pkt->data = handle->audio.outbuf;
   pkt->size = handle->audio.outbuf_size;

#ifdef HAVE_FFMPEG_AVCODEC_ENCODE_AUDIO2
   AVFrame frame;
   avcodec_get_frame_defaults(&frame);

   frame.nb_samples = handle->audio.frames_in_buffer;
   frame.pts = handle->audio.frame_cnt;

   int samples_size = frame.nb_samples *
      handle->audio.codec->channels *
      sizeof(int16_t);

   avcodec_fill_audio_frame(&frame, handle->audio.codec->channels,
         handle->audio.codec->sample_fmt, (const uint8_t*)handle->audio.buffer,
         samples_size, 1);

   int got_packet = 0;
   if (avcodec_encode_audio2(handle->audio.codec,
            pkt, dry ? NULL : &frame, &got_packet) < 0)
      return false;

   if (!got_packet)
   {
      pkt->size = 0;
      pkt->pts = AV_NOPTS_VALUE;
      pkt->dts = AV_NOPTS_VALUE;
      return true;
   }

   if (pkt->pts != (int64_t)AV_NOPTS_VALUE)
   {
      pkt->pts = av_rescale_q(pkt->pts,
            handle->audio.codec->time_base,
            handle->muxer.astream->time_base);
   }

   if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
   {
      pkt->dts = av_rescale_q(pkt->dts,
            handle->audio.codec->time_base,
            handle->muxer.astream->time_base);
   }

#else
   if (dry)
      return false;

   memset(handle->audio.buffer + handle->audio.frames_in_buffer * handle->audio.codec->channels, 0,
         (handle->audio.codec->frame_size - handle->audio.frames_in_buffer) *
         handle->audio.codec->channels * sizeof(int16_t));

   int out_size = avcodec_encode_audio(handle->audio.codec,
         handle->audio.outbuf, handle->audio.outbuf_size, handle->audio.buffer);

   if (out_size < 0)
      return false;

   if (out_size == 0)
   {
      pkt->size = 0;
      return true;
   }

   pkt->size = out_size;

   if (handle->audio.codec->coded_frame->pts != (int64_t)AV_NOPTS_VALUE)
   {
      pkt->pts = av_rescale_q(handle->audio.codec->coded_frame->pts,
            handle->audio.codec->time_base,
            handle->muxer.astream->time_base);
   }
   else
      pkt->pts = AV_NOPTS_VALUE;

   if (handle->audio.codec->coded_frame->key_frame)
      pkt->flags |= AV_PKT_FLAG_KEY;
#endif

   pkt->stream_index = handle->muxer.astream->index;
   return true;
}

static bool ffemu_push_audio_thread(ffemu_t *handle, const struct ffemu_audio_data *data, bool require_block)
{
   size_t written_frames = 0;
   while (written_frames < data->frames)
   {
      size_t can_write = handle->audio.codec->frame_size - handle->audio.frames_in_buffer;
      size_t write_left = data->frames - written_frames;
      size_t write_frames = write_left > can_write ? can_write : write_left;
      size_t write_size = write_frames * handle->params.channels * sizeof(int16_t);

      size_t samples_in_buffer = handle->audio.frames_in_buffer * handle->params.channels;
      size_t written_samples = written_frames * handle->params.channels;

      memcpy(handle->audio.buffer + samples_in_buffer,
            data->data + written_samples,
            write_size);

      written_frames += write_frames;
      handle->audio.frames_in_buffer += write_frames;

      if ((handle->audio.frames_in_buffer < (size_t)handle->audio.codec->frame_size) && require_block)
         continue;

      AVPacket pkt;
      if (!encode_audio(handle, &pkt, false))
         return false;

      handle->audio.frames_in_buffer = 0;
      handle->audio.frame_cnt += handle->audio.codec->frame_size;

      if (pkt.size)
      {
         if (av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
            return false;
      }
   }

   return true;
}

static void ffemu_flush_audio(ffemu_t *handle, int16_t *audio_buf, size_t audio_buf_size)
{
   size_t avail = fifo_read_avail(handle->audio_fifo);
   if (avail)
   {
      fifo_read(handle->audio_fifo, audio_buf, avail);

      struct ffemu_audio_data aud = {0};
      aud.frames = avail / (sizeof(int16_t) * handle->params.channels);
      aud.data = audio_buf;

      ffemu_push_audio_thread(handle, &aud, false);
   }

   for (;;)
   {
      AVPacket pkt;
      if (!encode_audio(handle, &pkt, true) || !pkt.size ||
            av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
         break;
   }
}

static void ffemu_flush_video(ffemu_t *handle)
{
   for (;;)
   {
      AVPacket pkt;
      if (!encode_video(handle, &pkt, NULL) || !pkt.size ||
            av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
         break;
   }
}

static void ffemu_flush_buffers(ffemu_t *handle)
{
   void *video_buf = av_malloc(2 * handle->params.fb_width * handle->params.fb_height * handle->video.pix_size);
   size_t audio_buf_size = 512 * handle->params.channels * sizeof(int16_t);
   int16_t *audio_buf = (int16_t*)av_malloc(audio_buf_size);

   // Try pushing data in an interleaving pattern to ease the work of the muxer a bit.
   bool did_work;
   do
   {
      did_work = false;

      if (fifo_read_avail(handle->audio_fifo) >= audio_buf_size)
      {
         fifo_read(handle->audio_fifo, audio_buf, audio_buf_size);

         struct ffemu_audio_data aud = {0};
         aud.frames = 512;
         aud.data = audio_buf;

         ffemu_push_audio_thread(handle, &aud, true);
         did_work = true;
      }

      struct ffemu_video_data attr_buf;
      if (fifo_read_avail(handle->attr_fifo) >= sizeof(attr_buf))
      {
         fifo_read(handle->attr_fifo, &attr_buf, sizeof(attr_buf));
         fifo_read(handle->video_fifo, video_buf, attr_buf.height * attr_buf.pitch);
         attr_buf.data = video_buf;
         ffemu_push_video_thread(handle, &attr_buf);

         did_work = true;
      }
   } while (did_work);

   // Flush out last audio.
   ffemu_flush_audio(handle, audio_buf, audio_buf_size);

   // Flush out last video.
   ffemu_flush_video(handle);

   av_free(video_buf);
   av_free(audio_buf);
}

bool ffemu_finalize(ffemu_t *handle)
{
   deinit_thread(handle);

   // Flush out data still in buffers (internal, and FFmpeg internal).
   ffemu_flush_buffers(handle);

   deinit_thread_buf(handle);

   // Write final data.
   av_write_trailer(handle->muxer.ctx);

   return true;
}

static void ffemu_thread(void *data)
{
   ffemu_t *ff = (ffemu_t*)data;

   // For some reason, FFmpeg has a tendency to crash if we don't overallocate a bit. :s
   void *video_buf = av_malloc(2 * ff->params.fb_width * ff->params.fb_height * ff->video.pix_size);
   assert(video_buf);

   size_t audio_buf_size = 512 * ff->params.channels * sizeof(int16_t);
   int16_t *audio_buf = (int16_t*)av_malloc(audio_buf_size);
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

         struct ffemu_audio_data aud = {0};
         aud.frames = 512;
         aud.data = audio_buf;

         ffemu_push_audio_thread(ff, &aud, true);
      }
   }

   av_free(video_buf);
   av_free(audio_buf);
}

