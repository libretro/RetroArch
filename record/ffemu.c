/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include <libavutil/pixdesc.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
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
#include "../conf/config_file.h"
#include "../audio/utils.h"
#include "../audio/resampler.h"
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

   // Output pixel format.
   enum PixelFormat pix_fmt;
   // Input pixel format. Only used by sws.
   enum PixelFormat in_pix_fmt;

   unsigned frame_drop_ratio;
   unsigned frame_drop_count;

   // Input pixel size.
   size_t pix_size;

   AVFormatContext *format;

   struct scaler_ctx scaler;
   struct SwsContext *sws;
   bool use_sws;
};

struct ff_audio_info
{
   AVCodecContext *codec;
   AVCodec *encoder;

   uint8_t *buffer;
   size_t frames_in_buffer;

   int64_t frame_cnt;

   uint8_t *outbuf;
   size_t outbuf_size;

   // Most lossy audio codecs only support certain sampling rates.
   // Could use libswresample, but it doesn't support floating point ratios. :(
   // Use either S16 or (planar) float for simplicity.
   const rarch_resampler_t *resampler;
   void *resampler_data;

   bool use_float;
   bool is_planar;
   unsigned sample_size;

   float *float_conv;
   size_t float_conv_frames;

   float *resample_out;
   size_t resample_out_frames;

   int16_t *fixed_conv;
   size_t fixed_conv_frames;

   void *planar_buf;
   size_t planar_buf_frames;

   double ratio;
};

struct ff_muxer_info
{
   AVFormatContext *ctx;
   AVStream *astream;
   AVStream *vstream;
};

struct ff_config_param
{
   config_file_t *conf;
   char vcodec[64];
   char acodec[64];
   char format[64];
   enum PixelFormat out_pix_fmt;
   unsigned threads;
   unsigned frame_drop_ratio;
   unsigned sample_rate;
   unsigned scale_factor;

   // Keep same naming conventions as libavcodec.
   bool audio_qscale;
   int audio_global_quality;
   int audio_bit_rate;

   AVDictionary *video_opts;
   AVDictionary *audio_opts;
};

struct ffemu
{
   struct ff_video_info video;
   struct ff_audio_info audio;
   struct ff_muxer_info muxer;
   struct ff_config_param config;
   
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

static bool ffemu_codec_has_sample_format(enum AVSampleFormat fmt, const enum AVSampleFormat *fmts)
{
   unsigned i;
   for (i = 0; fmts[i] != AV_SAMPLE_FMT_NONE; i++)
      if (fmt == fmts[i])
         return true;
   return false;
}

static void ffemu_audio_resolve_format(struct ff_audio_info *audio, const AVCodec *codec)
{
   audio->codec->sample_fmt = AV_SAMPLE_FMT_NONE;

   if (ffemu_codec_has_sample_format(AV_SAMPLE_FMT_FLTP, codec->sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
      audio->use_float         = true;
      audio->is_planar         = true;
      RARCH_LOG("[FFmpeg]: Using sample format FLTP.\n");
   }
   else if (ffemu_codec_has_sample_format(AV_SAMPLE_FMT_FLT, codec->sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_FLT;
      audio->use_float         = true;
      audio->is_planar         = false;
      RARCH_LOG("[FFmpeg]: Using sample format FLT.\n");
   }
   else if (ffemu_codec_has_sample_format(AV_SAMPLE_FMT_S16P, codec->sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_S16P;
      audio->use_float         = false;
      audio->is_planar         = true;
      RARCH_LOG("[FFmpeg]: Using sample format S16P.\n");
   }
   else if (ffemu_codec_has_sample_format(AV_SAMPLE_FMT_S16, codec->sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_S16;
      audio->use_float         = false;
      audio->is_planar         = false;
      RARCH_LOG("[FFmpeg]: Using sample format S16.\n");
   }
   audio->sample_size = audio->use_float ? sizeof(float) : sizeof(int16_t);
}

static void ffemu_audio_resolve_sample_rate(ffemu_t *handle, const AVCodec *codec)
{
   unsigned i;
   struct ff_config_param *params = &handle->config;
   struct ffemu_params *param     = &handle->params;

   // We'll have to force resampling to some supported sampling rate.
   if (codec->supported_samplerates && !params->sample_rate)
   {
      int input_rate = (int)param->samplerate;

      // Favor closest sampling rate, but always prefer ratio > 1.0.
      int best_rate = codec->supported_samplerates[0];
      int best_diff = best_rate - input_rate;

      for (i = 1; codec->supported_samplerates[i]; i++)
      {
         int diff = codec->supported_samplerates[i] - input_rate;

         bool better_rate;
         if (best_diff < 0)
            better_rate = diff > best_diff;
         else
            better_rate = diff >= 0 && diff < best_diff;

         if (better_rate)
         {
            best_rate = codec->supported_samplerates[i];
            best_diff = diff;
         }
      }

      params->sample_rate = best_rate;
      RARCH_LOG("[FFmpeg]: Using output sampling rate: %u.\n", best_rate);
   }
}

static bool ffemu_init_audio(ffemu_t *handle)
{
   struct ff_config_param *params = &handle->config;
   struct ff_audio_info *audio    = &handle->audio;
   struct ffemu_params *param     = &handle->params;

   AVCodec *codec = avcodec_find_encoder_by_name(*params->acodec ? params->acodec : "flac");
   if (!codec)
   {
      RARCH_ERR("[FFmpeg]: Cannot find acodec %s.\n", *params->acodec ? params->acodec : "flac");
      return false;
   }

   audio->encoder = codec;

   audio->codec = avcodec_alloc_context3(codec);

   audio->codec->codec_type     = AVMEDIA_TYPE_AUDIO;
   audio->codec->channels       = param->channels;
   audio->codec->channel_layout = param->channels > 1 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;

   ffemu_audio_resolve_format(audio, codec);
   ffemu_audio_resolve_sample_rate(handle, codec);

   if (params->sample_rate)
   {
      audio->ratio = (double)params->sample_rate / param->samplerate;
      audio->codec->sample_rate = params->sample_rate;
      audio->codec->time_base = av_d2q(1.0 / params->sample_rate, 1000000);

      rarch_resampler_realloc(&audio->resampler_data,
            &audio->resampler,
            *g_settings.audio.resampler ? g_settings.audio.resampler : NULL,
            audio->ratio);
   }
   else
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_S16;
      audio->codec->sample_rate = (int)roundf(param->samplerate);
      audio->codec->time_base = av_d2q(1.0 / param->samplerate, 1000000);
   }

   if (params->audio_qscale)
   {
      audio->codec->flags |= CODEC_FLAG_QSCALE;
      audio->codec->global_quality = params->audio_global_quality;
   }
   else if (params->audio_bit_rate)
      audio->codec->bit_rate = params->audio_bit_rate;

   // Allow experimental codecs.
   audio->codec->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

   if (handle->muxer.ctx->oformat->flags & AVFMT_GLOBALHEADER)
      audio->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

   if (avcodec_open2(audio->codec, codec, params->audio_opts ? &params->audio_opts : NULL) != 0)
      return false;

   if (!audio->codec->frame_size) // If not set (PCM), just set something.
      audio->codec->frame_size = 1024;

   audio->buffer = (uint8_t*)av_malloc(
         audio->codec->frame_size *
         audio->codec->channels *
         audio->sample_size);

   //RARCH_LOG("[FFmpeg]: Audio frame size: %d.\n", audio->codec->frame_size);

   if (!audio->buffer)
      return false;

   audio->outbuf_size = FF_MIN_BUFFER_SIZE;
   audio->outbuf = (uint8_t*)av_malloc(audio->outbuf_size);
   if (!audio->outbuf)
      return false;

   return true;
}

static bool ffemu_init_video(ffemu_t *handle)
{
   struct ff_config_param *params = &handle->config;
   struct ff_video_info *video    = &handle->video;
   struct ffemu_params *param     = &handle->params;

   AVCodec *codec = NULL;

   if (*params->vcodec)
      codec = avcodec_find_encoder_by_name(params->vcodec);
   else
   {
      // By default, lossless video.
      av_dict_set(&params->video_opts, "qp", "0", 0);
      codec = avcodec_find_encoder_by_name("libx264rgb");
   }

   if (!codec)
   {
      RARCH_ERR("[FFmpeg]: Cannot find vcodec %s.\n", *params->vcodec ? params->vcodec : "libx264rgb");
      return false;
   }

   video->encoder = codec;

   // Don't use swscaler unless format is not something "in-house" scaler supports.
   // libswscale doesn't scale RGB -> RGB correctly (goes via YUV first), and it's non-trivial to fix
   // upstream as it's heavily geared towards YUV.
   // If we're dealing with strange formats or YUV, just use libswscale.
   if (params->out_pix_fmt != PIX_FMT_NONE)
   {
      video->pix_fmt = params->out_pix_fmt;
      if (video->pix_fmt != PIX_FMT_BGR24 && video->pix_fmt != PIX_FMT_RGB32)
         video->use_sws = true;

      switch (video->pix_fmt)
      {
         case PIX_FMT_BGR24:
            video->scaler.out_fmt = SCALER_FMT_BGR24;
            break;

         case PIX_FMT_RGB32:
            video->scaler.out_fmt = SCALER_FMT_ARGB8888;
            break;

         default:
            break;
      }
   }
   else // Use BGR24 as default out format.
   {
      video->pix_fmt        = PIX_FMT_BGR24;
      video->scaler.out_fmt = SCALER_FMT_BGR24;
   }

   switch (param->pix_fmt)
   {
      case FFEMU_PIX_RGB565:
         video->scaler.in_fmt = SCALER_FMT_RGB565;
         video->in_pix_fmt    = PIX_FMT_RGB565;
         video->pix_size      = 2;
         break;

      case FFEMU_PIX_BGR24:
         video->scaler.in_fmt = SCALER_FMT_BGR24;
         video->in_pix_fmt    = PIX_FMT_BGR24;
         video->pix_size      = 3;
         break;

      case FFEMU_PIX_ARGB8888:
         video->scaler.in_fmt = SCALER_FMT_ARGB8888;
         video->in_pix_fmt    = PIX_FMT_RGB32;
         video->pix_size      = 4;
         break;

      default:
         return false;
   }

   video->codec = avcodec_alloc_context3(codec);

   // Useful to set scale_factor to 2 for chroma subsampled formats to maintain full chroma resolution.
   // (Or just use 4:4:4 or RGB ...)
   param->out_width  *= params->scale_factor;
   param->out_height *= params->scale_factor;

   video->codec->codec_type          = AVMEDIA_TYPE_VIDEO;
   video->codec->width               = param->out_width;
   video->codec->height              = param->out_height;
   video->codec->time_base           = av_d2q((double)params->frame_drop_ratio / param->fps, 1000000); // Arbitrary big number.
   video->codec->sample_aspect_ratio = av_d2q(param->aspect_ratio * param->out_height / param->out_width, 255);
   video->codec->pix_fmt             = video->pix_fmt;

   video->codec->thread_count = params->threads;

   if (handle->muxer.ctx->oformat->flags & AVFMT_GLOBALHEADER)
      video->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

   if (avcodec_open2(video->codec, codec, params->video_opts ? &params->video_opts : NULL) != 0)
      return false;

   // Allocate a big buffer :p ffmpeg API doesn't seem to give us some clues how big this buffer should be.
   video->outbuf_size = 1 << 23;
   video->outbuf = (uint8_t*)av_malloc(video->outbuf_size);

   video->frame_drop_ratio = params->frame_drop_ratio;

   size_t size = avpicture_get_size(video->pix_fmt, param->out_width, param->out_height);
   video->conv_frame_buf = (uint8_t*)av_malloc(size);
   video->conv_frame = avcodec_alloc_frame();
   avpicture_fill((AVPicture*)video->conv_frame, video->conv_frame_buf, video->pix_fmt,
         param->out_width, param->out_height);

   return true;
}

static bool ffemu_init_config(struct ff_config_param *params, const char *config)
{
   params->out_pix_fmt = PIX_FMT_NONE;
   params->scale_factor = 1;
   params->threads = 1;
   params->frame_drop_ratio = 1;

   if (!config)
      return true;

   params->conf = config_file_new(config);
   if (!params->conf)
   {
      RARCH_ERR("Failed to load FFmpeg config \"%s\".\n", config);
      return false;
   }

   config_get_array(params->conf, "vcodec", params->vcodec, sizeof(params->vcodec));
   config_get_array(params->conf, "acodec", params->acodec, sizeof(params->acodec));
   config_get_array(params->conf, "format", params->format, sizeof(params->format));

   config_get_uint(params->conf, "threads", &params->threads);

   if (!config_get_uint(params->conf, "frame_drop_ratio", &params->frame_drop_ratio)
         || !params->frame_drop_ratio)
      params->frame_drop_ratio = 1;

   config_get_uint(params->conf, "sample_rate", &params->sample_rate);
   config_get_uint(params->conf, "scale_factor", &params->scale_factor);

   params->audio_qscale = config_get_int(params->conf, "audio_global_quality", &params->audio_global_quality);
   config_get_int(params->conf, "audio_bit_rate", &params->audio_bit_rate);

   char pix_fmt[64] = {0};
   if (config_get_array(params->conf, "pix_fmt", pix_fmt, sizeof(pix_fmt)))
   {
      params->out_pix_fmt = av_get_pix_fmt(pix_fmt);
      if (params->out_pix_fmt == PIX_FMT_NONE)
      {
         RARCH_ERR("Cannot find pix_fmt \"%s\".\n", pix_fmt);
         return false;
      }
   }

   struct config_file_entry entry;
   if (!config_get_entry_list_head(params->conf, &entry))
      return true;

   do
   {
      if (strstr(entry.key, "video_") == entry.key)
      {
         const char *key = entry.key + strlen("video_");
         av_dict_set(&params->video_opts, key, entry.value, 0);
      }
      else if (strstr(entry.key, "audio_") == entry.key)
      {
         const char *key = entry.key + strlen("audio_");
         av_dict_set(&params->audio_opts, key, entry.value, 0);
      }
   } while (config_get_entry_list_next(&entry));

   return true;
}

static bool ffemu_init_muxer_pre(ffemu_t *handle)
{
   AVFormatContext *ctx = avformat_alloc_context();
   av_strlcpy(ctx->filename, handle->params.filename, sizeof(ctx->filename));

   if (*handle->config.format)
      ctx->oformat = av_guess_format(handle->config.format, NULL, NULL); 
   else
      ctx->oformat = av_guess_format(NULL, ctx->filename, NULL);

   if (!ctx->oformat)
      return false;

   if (avio_open(&ctx->pb, ctx->filename, AVIO_FLAG_WRITE) < 0)
   {
      av_free(ctx);
      return false;
   }

   handle->muxer.ctx = ctx;
   return true;
}

static bool ffemu_init_muxer_post(ffemu_t *handle)
{
   AVStream *stream = avformat_new_stream(handle->muxer.ctx, handle->video.encoder);
   stream->codec = handle->video.codec;
   handle->muxer.vstream = stream;
   handle->muxer.vstream->sample_aspect_ratio = handle->video.codec->sample_aspect_ratio;

   stream = avformat_new_stream(handle->muxer.ctx, handle->audio.encoder);
   stream->codec = handle->audio.codec;
   handle->muxer.astream = stream;

   av_dict_set(&handle->muxer.ctx->metadata, "title", "RetroArch video dump", 0); 

   return avformat_write_header(handle->muxer.ctx, NULL) >= 0;
}

#define MAX_FRAMES 32

static void ffemu_thread(void *data);

static bool init_thread(ffemu_t *handle)
{
   handle->lock = slock_new();
   handle->cond_lock = slock_new();
   handle->cond = scond_new();
   handle->audio_fifo = fifo_new(32000 * sizeof(int16_t) * handle->params.channels * MAX_FRAMES / 60); // Some arbitrary max size.
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
   if (!handle->thread)
      return;

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
   avformat_network_init();

   ffemu_t *handle = (ffemu_t*)calloc(1, sizeof(*handle));
   if (!handle)
      goto error;

   handle->params = *params;

   if (!ffemu_init_config(&handle->config, params->config))
      goto error;

   if (!ffemu_init_muxer_pre(handle))
      goto error;

   if (!ffemu_init_video(handle))
      goto error;

   if (!ffemu_init_audio(handle))
      goto error;

   if (!ffemu_init_muxer_post(handle))
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
   if (!handle)
      return;

   deinit_thread(handle);
   deinit_thread_buf(handle);

   if (handle->audio.codec)
   {
      avcodec_close(handle->audio.codec);
      av_free(handle->audio.codec);
   }

   av_free(handle->audio.buffer);

   if (handle->video.codec)
   {
      avcodec_close(handle->video.codec);
      av_free(handle->video.codec);
   }

   av_free(handle->video.conv_frame);
   av_free(handle->video.conv_frame_buf);

   scaler_ctx_gen_reset(&handle->video.scaler);

   if (handle->video.sws)
      sws_freeContext(handle->video.sws);

   if (handle->config.conf)
      config_file_free(handle->config.conf);
   if (handle->config.video_opts)
      av_dict_free(&handle->config.video_opts);
   if (handle->config.audio_opts)
      av_dict_free(&handle->config.audio_opts);

   rarch_resampler_freep(&handle->audio.resampler,
         &handle->audio.resampler_data);

   av_free(handle->audio.float_conv);
   av_free(handle->audio.resample_out);
   av_free(handle->audio.fixed_conv);
   av_free(handle->audio.planar_buf);

   free(handle);
}

bool ffemu_push_video(ffemu_t *handle, const struct ffemu_video_data *data)
{
   unsigned y;
   bool drop_frame = handle->video.frame_drop_count++ % handle->video.frame_drop_ratio;
   handle->video.frame_drop_count %= handle->video.frame_drop_ratio;
   if (drop_frame)
      return true;

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
   for (y = 0; y < attr_data.height; y++, offset += data->pitch)
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

   pkt->stream_index = handle->muxer.vstream->index;
   return true;
}

static void ffemu_scale_input(ffemu_t *handle, const struct ffemu_video_data *data)
{
   // Attempt to preserve more information if we scale down.
   bool shrunk = handle->params.out_width < data->width || handle->params.out_height < data->height;

   if (handle->video.use_sws)
   {
      handle->video.sws = sws_getCachedContext(handle->video.sws, data->width, data->height, handle->video.in_pix_fmt,
            handle->params.out_width, handle->params.out_height, handle->video.pix_fmt,
            shrunk ? SWS_BILINEAR : SWS_POINT, NULL, NULL, NULL);

      int linesize = data->pitch;
      sws_scale(handle->video.sws, (const uint8_t* const*)&data->data, &linesize, 0,
            data->height, handle->video.conv_frame->data, handle->video.conv_frame->linesize);
   }
   else
   {
      if ((int)data->width != handle->video.scaler.in_width || (int)data->height != handle->video.scaler.in_height)
      {
         handle->video.scaler.in_width  = data->width;
         handle->video.scaler.in_height = data->height;
         handle->video.scaler.in_stride = data->pitch;

         handle->video.scaler.scaler_type = shrunk ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT;

         handle->video.scaler.out_width  = handle->params.out_width;
         handle->video.scaler.out_height = handle->params.out_height;
         handle->video.scaler.out_stride = handle->video.conv_frame->linesize[0];

         scaler_ctx_gen_filter(&handle->video.scaler);
      }

      scaler_ctx_scale(&handle->video.scaler, handle->video.conv_frame->data[0], data->data);
   }
}

static bool ffemu_push_video_thread(ffemu_t *handle, const struct ffemu_video_data *data)
{
   if (!data->is_dupe)
      ffemu_scale_input(handle, data);

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

static void planarize_float(float *out, const float *in, size_t frames)
{
   size_t i;
   for (i = 0; i < frames; i++)
   {
      out[i] = in[2 * i + 0];
      out[i + frames] = in[2 * i + 1];
   }
}

static void planarize_s16(int16_t *out, const int16_t *in, size_t frames)
{
   size_t i;
   for (i = 0; i < frames; i++)
   {
      out[i] = in[2 * i + 0];
      out[i + frames] = in[2 * i + 1];
   }
}

static void planarize_audio(ffemu_t *handle)
{
   if (!handle->audio.is_planar)
      return;

   if (handle->audio.frames_in_buffer > handle->audio.planar_buf_frames)
   {
      handle->audio.planar_buf = av_realloc(handle->audio.planar_buf, 
            handle->audio.frames_in_buffer * handle->params.channels * handle->audio.sample_size);
      if (!handle->audio.planar_buf)
         return;

      handle->audio.planar_buf_frames = handle->audio.frames_in_buffer;
   }

   if (handle->audio.use_float)
      planarize_float((float*)handle->audio.planar_buf,
            (const float*)handle->audio.buffer, handle->audio.frames_in_buffer);
   else
      planarize_s16((int16_t*)handle->audio.planar_buf,
            (const int16_t*)handle->audio.buffer, handle->audio.frames_in_buffer);
}

static bool encode_audio(ffemu_t *handle, AVPacket *pkt, bool dry)
{
   av_init_packet(pkt);
   pkt->data = handle->audio.outbuf;
   pkt->size = handle->audio.outbuf_size;

   AVFrame *frame = avcodec_alloc_frame();
   if (!frame)
      return false;

   frame->nb_samples     = handle->audio.frames_in_buffer;
   frame->format         = handle->audio.codec->sample_fmt;
   frame->channel_layout = handle->audio.codec->channel_layout;
   frame->pts            = handle->audio.frame_cnt;

   planarize_audio(handle);

   int samples_size = av_samples_get_buffer_size(NULL, handle->audio.codec->channels,
         handle->audio.frames_in_buffer,
         handle->audio.codec->sample_fmt, 0);

   avcodec_fill_audio_frame(frame, handle->audio.codec->channels,
         handle->audio.codec->sample_fmt,
         handle->audio.is_planar ? (uint8_t*)handle->audio.planar_buf : handle->audio.buffer,
         samples_size, 0);

   int got_packet = 0;
   if (avcodec_encode_audio2(handle->audio.codec,
            pkt, dry ? NULL : frame, &got_packet) < 0)
   {
      avcodec_free_frame(&frame);
      return false;
   }

   if (!got_packet)
   {
      pkt->size = 0;
      pkt->pts = AV_NOPTS_VALUE;
      pkt->dts = AV_NOPTS_VALUE;
      avcodec_free_frame(&frame);
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

   avcodec_free_frame(&frame);

   pkt->stream_index = handle->muxer.astream->index;
   return true;
}

static void ffemu_audio_resample(ffemu_t *handle, struct ffemu_audio_data *data)
{
   if (!handle->audio.use_float && !handle->audio.resampler)
      return;

   if (data->frames > handle->audio.float_conv_frames)
   {
      handle->audio.float_conv = (float*)av_realloc(handle->audio.float_conv,
            data->frames * handle->params.channels * sizeof(float));
      if (!handle->audio.float_conv)
         return;

      handle->audio.float_conv_frames = data->frames;

      // To make sure we don't accidentially overflow.
      handle->audio.resample_out_frames = data->frames * handle->audio.ratio + 16;

      handle->audio.resample_out = (float*)av_realloc(handle->audio.resample_out,
            handle->audio.resample_out_frames * handle->params.channels * sizeof(float));
      if (!handle->audio.resample_out)
         return;

      handle->audio.fixed_conv_frames = max(handle->audio.resample_out_frames, handle->audio.float_conv_frames);
      handle->audio.fixed_conv = (int16_t*)av_realloc(handle->audio.fixed_conv,
            handle->audio.fixed_conv_frames * handle->params.channels * sizeof(int16_t));
      if (!handle->audio.fixed_conv)
         return;
   }

   if (handle->audio.use_float || handle->audio.resampler)
   {
      audio_convert_s16_to_float(handle->audio.float_conv,
            (const int16_t*)data->data, data->frames * handle->params.channels, 1.0);
      data->data = handle->audio.float_conv;
   }

   if (handle->audio.resampler)
   {
      // It's always two channels ...
      struct resampler_data info = {0};
      info.data_in      = (const float*)data->data;
      info.data_out     = handle->audio.resample_out;
      info.input_frames = data->frames;
      info.ratio        = handle->audio.ratio;

      rarch_resampler_process(handle->audio.resampler, handle->audio.resampler_data, &info);
      data->data   = handle->audio.resample_out;
      data->frames = info.output_frames;

      if (!handle->audio.use_float)
      {
         audio_convert_float_to_s16(handle->audio.fixed_conv, handle->audio.resample_out,
               data->frames * handle->params.channels);
         data->data = handle->audio.fixed_conv;
      }
   }
}

static bool ffemu_push_audio_thread(ffemu_t *handle, struct ffemu_audio_data *data, bool require_block)
{
   ffemu_audio_resample(handle, data);

   size_t written_frames = 0;
   while (written_frames < data->frames)
   {
      size_t can_write    = handle->audio.codec->frame_size - handle->audio.frames_in_buffer;
      size_t write_left   = data->frames - written_frames;
      size_t write_frames = write_left > can_write ? can_write : write_left;
      size_t write_size   = write_frames * handle->params.channels * handle->audio.sample_size;

      size_t bytes_in_buffer = handle->audio.frames_in_buffer * handle->params.channels * handle->audio.sample_size;
      size_t written_bytes   = written_frames * handle->params.channels * handle->audio.sample_size;

      memcpy(handle->audio.buffer + bytes_in_buffer,
            (const uint8_t*)data->data + written_bytes,
            write_size);

      written_frames                 += write_frames;
      handle->audio.frames_in_buffer += write_frames;

      if ((handle->audio.frames_in_buffer < (size_t)handle->audio.codec->frame_size) && require_block)
         break;

      AVPacket pkt;
      if (!encode_audio(handle, &pkt, false))
         return false;

      handle->audio.frame_cnt       += handle->audio.frames_in_buffer;
      handle->audio.frames_in_buffer = 0;

      if (pkt.size)
      {
         if (av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
            return false;
      }
   }

   return true;
}

static void ffemu_flush_audio(ffemu_t *handle, void *audio_buf, size_t audio_buf_size)
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
   size_t audio_buf_size = handle->audio.codec->frame_size * handle->params.channels * sizeof(int16_t);
   void *audio_buf = av_malloc(audio_buf_size);

   // Try pushing data in an interleaving pattern to ease the work of the muxer a bit.
   bool did_work;
   do
   {
      did_work = false;

      if (fifo_read_avail(handle->audio_fifo) >= audio_buf_size)
      {
         fifo_read(handle->audio_fifo, audio_buf, audio_buf_size);

         struct ffemu_audio_data aud = {0};
         aud.frames = handle->audio.codec->frame_size;
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

   size_t audio_buf_size = ff->audio.codec->frame_size * ff->params.channels * sizeof(int16_t);
   void *audio_buf = av_malloc(audio_buf_size);

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
         aud.frames = ff->audio.codec->frame_size;
         aud.data = audio_buf;

         ffemu_push_audio_thread(ff, &aud, true);
      }
   }

   av_free(video_buf);
   av_free(audio_buf);
}

