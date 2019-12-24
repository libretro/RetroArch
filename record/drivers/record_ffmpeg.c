/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2017-2019 - Andrés Suárez
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <retro_assert.h>
#include <compat/msvc.h>
#include <compat/strl.h>

#include <boolean.h>
#include <queues/fifo_queue.h>
#include <rthreads/rthreads.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <file/config_file.h>
#include <audio/audio_resampler.h>
#include <string/stdstring.h>
#include <audio/conversion/float_to_s16.h>
#include <audio/conversion/s16_to_float.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef FFEMU_PERF
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
#include <libavutil/version.h>
#include <libavformat/avformat.h>
#ifdef HAVE_AV_CHANNEL_LAYOUT
#include <libavutil/channel_layout.h>
#endif
#include <libavutil/avconfig.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

#ifdef __cplusplus
}
#endif

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#ifndef AV_CODEC_FLAG_QSCALE
#define AV_CODEC_FLAG_QSCALE CODEC_FLAG_QSCALE
#endif

#ifndef AV_CODEC_FLAG_GLOBAL_HEADER
#define AV_CODEC_FLAG_GLOBAL_HEADER CODEC_FLAG_GLOBAL_HEADER
#endif

#ifndef AV_INPUT_BUFFER_MIN_SIZE
#define AV_INPUT_BUFFER_MIN_SIZE FF_MIN_BUFFER_SIZE
#endif

#ifndef PIX_FMT_RGB32
#define PIX_FMT_RGB32 AV_PIX_FMT_RGB32
#endif

#ifndef PIX_FMT_YUV444P
#define PIX_FMT_YUV444P AV_PIX_FMT_YUV444P
#endif

#ifndef PIX_FMT_YUV420P
#define PIX_FMT_YUV420P AV_PIX_FMT_YUV420P
#endif

#ifndef PIX_FMT_BGR24
#define PIX_FMT_BGR24 AV_PIX_FMT_BGR24
#endif

#ifndef PIX_FMT_RGB24
#define PIX_FMT_RGB24 AV_PIX_FMT_RGB24
#endif

#ifndef PIX_FMT_RGB8
#define PIX_FMT_RGB8 AV_PIX_FMT_RGB8
#endif

#ifndef PIX_FMT_RGB565
#define PIX_FMT_RGB565 AV_PIX_FMT_RGB565
#endif

#ifndef PIX_FMT_RGBA
#define PIX_FMT_RGBA AV_PIX_FMT_RGBA
#endif

#ifndef PIX_FMT_NONE
#define PIX_FMT_NONE AV_PIX_FMT_NONE
#endif

#ifndef PixelFormat
#define PixelFormat AVPixelFormat
#endif

#if LIBAVUTIL_VERSION_INT <= AV_VERSION_INT(52, 9, 0)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
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

   /* Output pixel format. */
   enum PixelFormat pix_fmt;
   /* Input pixel format. Only used by sws. */
   enum PixelFormat in_pix_fmt;

   unsigned frame_drop_ratio;
   unsigned frame_drop_count;

   /* Input pixel size. */
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

   /* Most lossy audio codecs only support certain sampling rates.
    * Could use libswresample, but it doesn't support floating point ratios.
    * Use either S16 or (planar) float for simplicity.
    */
   const retro_resampler_t *resampler;
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
   float scale_factor;

   bool audio_enable;
   /* Keep same naming conventions as libavcodec. */
   bool audio_qscale;
   int audio_global_quality;
   int audio_bit_rate;
   bool video_qscale;
   int video_global_quality;
   int video_bit_rate;

   AVDictionary *video_opts;
   AVDictionary *audio_opts;
};

typedef struct ffmpeg
{
   struct ff_video_info video;
   struct ff_audio_info audio;
   struct ff_muxer_info muxer;
   struct ff_config_param config;

   struct record_params params;

   scond_t *cond;
   slock_t *cond_lock;
   slock_t *lock;
   fifo_buffer_t *audio_fifo;
   fifo_buffer_t *video_fifo;
   fifo_buffer_t *attr_fifo;
   sthread_t *thread;

   volatile bool alive;
   volatile bool can_sleep;
} ffmpeg_t;

AVFormatContext *ctx;

static bool ffmpeg_codec_has_sample_format(enum AVSampleFormat fmt,
      const enum AVSampleFormat *fmts)
{
   unsigned i;

   for (i = 0; fmts[i] != AV_SAMPLE_FMT_NONE; i++)
      if (fmt == fmts[i])
         return true;
   return false;
}

static void ffmpeg_audio_resolve_format(struct ff_audio_info *audio,
      const AVCodec *codec)
{
   audio->codec->sample_fmt = AV_SAMPLE_FMT_NONE;

   if (ffmpeg_codec_has_sample_format(AV_SAMPLE_FMT_FLTP, codec->sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
      audio->use_float         = true;
      audio->is_planar         = true;
      RARCH_LOG("[FFmpeg]: Using sample format FLTP.\n");
   }
   else if (ffmpeg_codec_has_sample_format(AV_SAMPLE_FMT_FLT, codec->sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_FLT;
      audio->use_float         = true;
      audio->is_planar         = false;
      RARCH_LOG("[FFmpeg]: Using sample format FLT.\n");
   }
   else if (ffmpeg_codec_has_sample_format(AV_SAMPLE_FMT_S16P, codec->sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_S16P;
      audio->use_float         = false;
      audio->is_planar         = true;
      RARCH_LOG("[FFmpeg]: Using sample format S16P.\n");
   }
   else if (ffmpeg_codec_has_sample_format(AV_SAMPLE_FMT_S16, codec->sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_S16;
      audio->use_float         = false;
      audio->is_planar         = false;
      RARCH_LOG("[FFmpeg]: Using sample format S16.\n");
   }
   audio->sample_size = audio->use_float ? sizeof(float) : sizeof(int16_t);
}

static void ffmpeg_audio_resolve_sample_rate(ffmpeg_t *handle,
      const AVCodec *codec)
{
   struct ff_config_param *params  = &handle->config;
   struct record_params *param     = &handle->params;

   /* We'll have to force resampling to some supported sampling rate. */
   if (codec->supported_samplerates && !params->sample_rate)
   {
      unsigned i;
      int input_rate = (int)param->samplerate;

      /* Favor closest sampling rate, but always prefer ratio > 1.0. */
      int best_rate = codec->supported_samplerates[0];
      int best_diff = best_rate - input_rate;

      for (i = 1; codec->supported_samplerates[i]; i++)
      {
         bool better_rate;
         int diff = codec->supported_samplerates[i] - input_rate;

         if (best_diff < 0)
            better_rate = (diff > best_diff);
         else
            better_rate = ((diff >= 0) && (diff < best_diff));

         if (better_rate)
         {
            best_rate = codec->supported_samplerates[i];
            best_diff = diff;
         }
      }

      params->sample_rate = best_rate;
      RARCH_LOG("[FFmpeg]: Using output sampling rate: %d.\n", best_rate);
   }
}

static bool ffmpeg_init_audio(ffmpeg_t *handle)
{
   settings_t *settings            = config_get_ptr();
   struct ff_config_param *params  = &handle->config;
   struct ff_audio_info *audio     = &handle->audio;
   struct record_params *param     = &handle->params;
   AVCodec *codec                  = avcodec_find_encoder_by_name(
         *params->acodec ? params->acodec : "flac");
   if (!codec)
   {
      RARCH_ERR("[FFmpeg]: Cannot find acodec %s.\n",
            *params->acodec ? params->acodec : "flac");
      return false;
   }

   audio->encoder = codec;

   audio->codec = avcodec_alloc_context3(codec);

   audio->codec->codec_type     = AVMEDIA_TYPE_AUDIO;
   audio->codec->channels       = param->channels;
   audio->codec->channel_layout = (param->channels > 1)
      ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;

   ffmpeg_audio_resolve_format(audio, codec);
   ffmpeg_audio_resolve_sample_rate(handle, codec);

   if (params->sample_rate)
   {
      audio->ratio = (double)params->sample_rate / param->samplerate;
      audio->codec->sample_rate = params->sample_rate;
      audio->codec->time_base = av_d2q(1.0 / params->sample_rate, 1000000);

      retro_resampler_realloc(&audio->resampler_data,
            &audio->resampler,
            settings->arrays.audio_resampler,
            RESAMPLER_QUALITY_DONTCARE,
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
      audio->codec->flags |= AV_CODEC_FLAG_QSCALE;
      audio->codec->global_quality = params->audio_global_quality;
   }
   else if (params->audio_bit_rate)
      audio->codec->bit_rate = params->audio_bit_rate;

   /* Allow experimental codecs. */
   audio->codec->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

   if (handle->muxer.ctx->oformat->flags & AVFMT_GLOBALHEADER)
      audio->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

   if (avcodec_open2(audio->codec, codec, params->audio_opts ? &params->audio_opts : NULL) != 0)
      return false;

   if (!audio->codec->frame_size) /* If not set (PCM), just set something. */
      audio->codec->frame_size = 1024;

   audio->buffer = (uint8_t*)av_malloc(
         audio->codec->frame_size *
         audio->codec->channels *
         audio->sample_size);

#if 0
   RARCH_LOG("[FFmpeg]: Audio frame size: %d.\n", audio->codec->frame_size);
#endif

   if (!audio->buffer)
      return false;

   audio->outbuf_size = AV_INPUT_BUFFER_MIN_SIZE;
   audio->outbuf = (uint8_t*)av_malloc(audio->outbuf_size);
   if (!audio->outbuf)
      return false;

   return true;
}

static bool ffmpeg_init_video(ffmpeg_t *handle)
{
   size_t size;
   struct ff_config_param *params  = &handle->config;
   struct ff_video_info *video     = &handle->video;
   struct record_params *param     = &handle->params;
   AVCodec *codec = NULL;

   if (*params->vcodec)
      codec = avcodec_find_encoder_by_name(params->vcodec);
   else
   {
      /* By default, lossless video. */
      av_dict_set(&params->video_opts, "qp", "0", 0);
      codec = avcodec_find_encoder_by_name("libx264rgb");
   }

   if (!codec)
   {
      RARCH_ERR("[FFmpeg]: Cannot find vcodec %s.\n",
            *params->vcodec ? params->vcodec : "libx264rgb");
      return false;
   }

   video->encoder = codec;

   /* Don't use swscaler unless format is not something "in-house" scaler
    * supports.
    *
    * libswscale doesn't scale RGB -> RGB correctly (goes via YUV first),
    * and it's non-trivial to fix upstream as it's heavily geared towards YUV.
    * If we're dealing with strange formats or YUV, just use libswscale.
    */
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
   else /* Use BGR24 as default out format. */
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

   /* Useful to set scale_factor to 2 for chroma subsampled formats to
    * maintain full chroma resolution. (Or just use 4:4:4 or RGB ...)
    */
   param->out_width  = (float)param->out_width  * params->scale_factor;
   param->out_height = (float)param->out_height * params->scale_factor;

   video->codec->codec_type          = AVMEDIA_TYPE_VIDEO;
   video->codec->width               = param->out_width;
   video->codec->height              = param->out_height;
   video->codec->time_base           = av_d2q((double)
         params->frame_drop_ratio /param->fps, 1000000); /* Arbitrary big number. */
   video->codec->sample_aspect_ratio = av_d2q(
         param->aspect_ratio * param->out_height / param->out_width, 255);
   video->codec->pix_fmt             = video->pix_fmt;

   video->codec->thread_count = params->threads;

   if (params->video_qscale)
   {
      video->codec->flags |= AV_CODEC_FLAG_QSCALE;
      video->codec->global_quality = params->video_global_quality;
   }
   else if (params->video_bit_rate)
      video->codec->bit_rate = params->video_bit_rate;

   if (handle->muxer.ctx->oformat->flags & AVFMT_GLOBALHEADER)
      video->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

   if (avcodec_open2(video->codec, codec, params->video_opts ?
            &params->video_opts : NULL) != 0)
      return false;

   /* Allocate a big buffer. ffmpeg API doesn't seem to give us some
    * clues how big this buffer should be. */
   video->outbuf_size = 1 << 23;
   video->outbuf = (uint8_t*)av_malloc(video->outbuf_size);

   video->frame_drop_ratio = params->frame_drop_ratio;

   size = avpicture_get_size(video->pix_fmt, param->out_width,
         param->out_height);
   video->conv_frame_buf = (uint8_t*)av_malloc(size);
   video->conv_frame = av_frame_alloc();

   avpicture_fill((AVPicture*)video->conv_frame, video->conv_frame_buf,
         video->pix_fmt, param->out_width, param->out_height);

   video->conv_frame->width  = param->out_width;
   video->conv_frame->height = param->out_height;
   video->conv_frame->format = video->pix_fmt;

   return true;
}

static bool ffmpeg_init_config_common(struct ff_config_param *params, unsigned preset)
{
   settings_t *settings = config_get_ptr();

   switch (preset)
   {
      case RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY:
      case RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY:
         params->threads              = settings->uints.video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 75;
         params->out_pix_fmt          = PIX_FMT_YUV420P;

         strlcpy(params->vcodec, "libx264", sizeof(params->vcodec));
         strlcpy(params->acodec, "aac", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "preset", "ultrafast", 0);
         av_dict_set(&params->video_opts, "tune", "film", 0);
         av_dict_set(&params->video_opts, "crf", "35", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "75", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY:
      case RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY:
         params->threads              = settings->uints.video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 75;
         params->out_pix_fmt          = PIX_FMT_YUV420P;

         strlcpy(params->vcodec, "libx264", sizeof(params->vcodec));
         strlcpy(params->acodec, "aac", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "preset", "superfast", 0);
         av_dict_set(&params->video_opts, "tune", "film", 0);
         av_dict_set(&params->video_opts, "crf", "25", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "75", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY:
      case RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY:
         params->threads              = settings->uints.video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 100;
         params->out_pix_fmt          = PIX_FMT_YUV420P;

         strlcpy(params->vcodec, "libx264", sizeof(params->vcodec));
         strlcpy(params->acodec, "aac", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "preset", "superfast", 0);
         av_dict_set(&params->video_opts, "tune", "film", 0);
         av_dict_set(&params->video_opts, "crf", "15", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "100", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY:
         params->threads              = settings->uints.video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 80;
         params->out_pix_fmt          = PIX_FMT_BGR24;

         strlcpy(params->vcodec, "libx264rgb", sizeof(params->vcodec));
         strlcpy(params->acodec, "flac", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "qp", "0", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "100", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST:
         params->threads              = settings->uints.video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 50;
         params->out_pix_fmt          = PIX_FMT_YUV420P;

         strlcpy(params->vcodec, "libvpx", sizeof(params->vcodec));
         strlcpy(params->acodec, "libopus", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "deadline", "realtime", 0);
         av_dict_set(&params->video_opts, "crf", "14", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "50", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY:
         params->threads              = settings->uints.video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 75;
         params->out_pix_fmt          = PIX_FMT_YUV420P;

         strlcpy(params->vcodec, "libvpx", sizeof(params->vcodec));
         strlcpy(params->acodec, "libopus", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "deadline", "realtime", 0);
         av_dict_set(&params->video_opts, "crf", "4", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "75", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_GIF:
         params->threads              = settings->uints.video_record_threads;
         params->frame_drop_ratio     = 4;
         params->audio_enable         = false;
         params->audio_global_quality = 0;
         params->out_pix_fmt          = PIX_FMT_RGB8;

         strlcpy(params->vcodec, "gif", sizeof(params->vcodec));
         strlcpy(params->acodec, "", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "framerate", "30", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "0", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_APNG:
         params->threads              = settings->uints.video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = false;
         params->audio_global_quality = 0;
         params->out_pix_fmt          = PIX_FMT_RGB24;

         strlcpy(params->vcodec, "apng", sizeof(params->vcodec));
         strlcpy(params->acodec, "", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "pred", "avg", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "0", 0);
         break;
      case RECORD_CONFIG_TYPE_STREAMING_NETPLAY:
         params->threads              = settings->uints.video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 50;
         params->out_pix_fmt          = PIX_FMT_YUV420P;

         strlcpy(params->vcodec, "libx264", sizeof(params->vcodec));
         strlcpy(params->acodec, "aac", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "preset", "ultrafast", 0);
         av_dict_set(&params->video_opts, "tune", "zerolatency", 0);
         av_dict_set(&params->video_opts, "crf", "20", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "50", 0);

         /* TO-DO: detect if hwaccel is available and use it instead of the preset above
         strlcpy(params->vcodec, "h264_nvenc", sizeof(params->vcodec));
         strlcpy(params->acodec, "aac", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "preset", "llhp", 0);
         av_dict_set(&params->video_opts, "tune", "zerolatency", 0);
         av_dict_set(&params->video_opts, "zerolatency", "1", 0);
         av_dict_set(&params->video_opts, "-rc-lookahead", "0", 0);
         av_dict_set(&params->video_opts, "x264-params", "threads=0:intra-refresh=1:b-frames=0", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "100", 0);
         */

         break;
      default:
         break;
   }

   if (preset <= RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY)
   {
      if (!settings->bools.video_gpu_record)
         params->scale_factor = settings->uints.video_record_scale_factor > 0 ?
            settings->uints.video_record_scale_factor : 1;
      else
         params->scale_factor = 1;
      strlcpy(params->format, "matroska", sizeof(params->format));
   }
   else if (preset >= RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST && preset < RECORD_CONFIG_TYPE_RECORDING_GIF)
   {
      if (!settings->bools.video_gpu_record)
         params->scale_factor = settings->uints.video_record_scale_factor > 0 ?
            settings->uints.video_record_scale_factor : 1;
      else
         params->scale_factor = 1;
      strlcpy(params->format, "webm", sizeof(params->format));
   }
   else if (preset >= RECORD_CONFIG_TYPE_RECORDING_GIF && preset < RECORD_CONFIG_TYPE_RECORDING_APNG)
   {
      if (!settings->bools.video_gpu_record)
         params->scale_factor = settings->uints.video_record_scale_factor > 0 ?
            settings->uints.video_record_scale_factor : 1;
      else
         params->scale_factor = 1;
      strlcpy(params->format, "gif", sizeof(params->format));
   }
   else if (preset < RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY)
   {
      params->scale_factor = 1;
      strlcpy(params->format, "apng", sizeof(params->format));
   }
   else if (preset <= RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY)
   {
      if (!settings->bools.video_gpu_record)
         params->scale_factor = settings->uints.video_stream_scale_factor > 0 ?
            settings->uints.video_stream_scale_factor : 1;
      else
         params->scale_factor = 1;
      if (settings->uints.streaming_mode == STREAMING_MODE_YOUTUBE || settings->uints.streaming_mode == STREAMING_MODE_TWITCH)
         strlcpy(params->format, "flv", sizeof(params->format));
      else
         strlcpy(params->format, "mpegts", sizeof(params->format));
   }
   else if (preset == RECORD_CONFIG_TYPE_STREAMING_NETPLAY)
   {
      params->scale_factor = 1;
      strlcpy(params->format, "mpegts", sizeof(params->format));
   }

   return true;
}

/*
static bool ffmpeg_init_config_recording(struct ff_config_param *params)
{
   return true;
   params->threads              = 0;
   params->audio_global_quality = 100;

   strlcpy(params->vcodec, "libx264rgb", sizeof(params->vcodec));
   strlcpy(params->format, "matroska", sizeof(params->format));

   av_dict_set(&params->video_opts, "video_preset", "slow", 0);
   av_dict_set(&params->video_opts, "video_tune", "film", 0);
   av_dict_set(&params->video_opts, "video_crf", "10", 0);
   av_dict_set(&params->audio_opts, "audio_global_quality", "100", 0);

   return true;
}
*/

static bool ffmpeg_init_config(struct ff_config_param *params,
      const char *config)
{
   struct config_file_entry entry;
   char pix_fmt[64]         = {0};

   params->out_pix_fmt      = PIX_FMT_NONE;
   params->scale_factor     = 1;
   params->threads          = 1;
   params->frame_drop_ratio = 1;
   params->audio_enable     = true;

   if (!config)
      return true;

   RARCH_LOG("[FFmpeg] Loading FFmpeg config \"%s\".\n", config);

   if (!(params->conf = config_file_new_from_path_to_string(config)))
   {
      RARCH_ERR("[FFmpeg] Failed to load FFmpeg config \"%s\".\n", config);
      return false;
   }

   config_get_array(params->conf, "vcodec", params->vcodec,
         sizeof(params->vcodec));
   config_get_array(params->conf, "acodec", params->acodec,
         sizeof(params->acodec));
   config_get_array(params->conf, "format", params->format,
         sizeof(params->format));

   config_get_uint(params->conf, "threads", &params->threads);

   if (!config_get_uint(params->conf, "frame_drop_ratio",
            &params->frame_drop_ratio) || !params->frame_drop_ratio)
      params->frame_drop_ratio = 1;

   if (!config_get_bool(params->conf, "audio_enable", &params->audio_enable))
      params->audio_enable = true;

   config_get_uint(params->conf, "sample_rate", &params->sample_rate);
   config_get_float(params->conf, "scale_factor", &params->scale_factor);

   params->audio_qscale = config_get_int(params->conf, "audio_global_quality",
         &params->audio_global_quality);
   config_get_int(params->conf, "audio_bit_rate", &params->audio_bit_rate);
   params->video_qscale = config_get_int(params->conf, "video_global_quality",
         &params->video_global_quality);
   config_get_int(params->conf, "video_bit_rate", &params->video_bit_rate);

   if (config_get_array(params->conf, "pix_fmt", pix_fmt, sizeof(pix_fmt)))
   {
      params->out_pix_fmt = av_get_pix_fmt(pix_fmt);
      if (params->out_pix_fmt == PIX_FMT_NONE)
      {
         RARCH_ERR("[FFmpeg] Cannot find pix_fmt \"%s\".\n", pix_fmt);
         return false;
      }
   }

   if (!config_get_entry_list_head(params->conf, &entry))
      return true;

   do
   {
      if (strstr(entry.key, "video_") == entry.key)
      {
         const char *key = entry.key + STRLEN_CONST("video_");
         av_dict_set(&params->video_opts, key, entry.value, 0);
      }
      else if (strstr(entry.key, "audio_") == entry.key)
      {
         const char *key = entry.key + STRLEN_CONST("audio_");
         av_dict_set(&params->audio_opts, key, entry.value, 0);
      }
   } while (config_get_entry_list_next(&entry));

   return true;
}

static bool ffmpeg_init_muxer_pre(ffmpeg_t *handle)
{
   ctx = avformat_alloc_context();
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

static bool ffmpeg_init_muxer_post(ffmpeg_t *handle)
{
   AVStream *stream = avformat_new_stream(handle->muxer.ctx,
         handle->video.encoder);

   stream->codec = handle->video.codec;
   stream->time_base = stream->codec->time_base;
   handle->muxer.vstream = stream;
   handle->muxer.vstream->sample_aspect_ratio =
      handle->video.codec->sample_aspect_ratio;

   if (handle->config.audio_enable)
   {
      stream = avformat_new_stream(handle->muxer.ctx,
            handle->audio.encoder);
      stream->codec = handle->audio.codec;
      stream->time_base = stream->codec->time_base;
      handle->muxer.astream = stream;
   }

   av_dict_set(&handle->muxer.ctx->metadata, "title",
         "RetroArch Video Dump", 0);

   return avformat_write_header(handle->muxer.ctx, NULL) >= 0;
}

#define MAX_FRAMES 32

static void ffmpeg_thread(void *data);

static bool init_thread(ffmpeg_t *handle)
{
   handle->lock = slock_new();
   handle->cond_lock = slock_new();
   handle->cond = scond_new();
   handle->audio_fifo = fifo_new(32000 * sizeof(int16_t) *
         handle->params.channels * MAX_FRAMES / 60); /* Some arbitrary max size. */
   handle->attr_fifo = fifo_new(sizeof(struct record_video_data) * MAX_FRAMES);
   handle->video_fifo = fifo_new(handle->params.fb_width * handle->params.fb_height *
            handle->video.pix_size * MAX_FRAMES);

   handle->alive = true;
   handle->can_sleep = true;
   handle->thread = sthread_create(ffmpeg_thread, handle);

   retro_assert(handle->lock && handle->cond_lock &&
      handle->cond && handle->audio_fifo &&
      handle->attr_fifo && handle->video_fifo && handle->thread);

   return true;
}

static void deinit_thread(ffmpeg_t *handle)
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

static void deinit_thread_buf(ffmpeg_t *handle)
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

static void ffmpeg_free(void *data)
{
   ffmpeg_t *handle = (ffmpeg_t*)data;
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

   av_frame_free(&handle->video.conv_frame);
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

   if (handle->audio.resampler && handle->audio.resampler_data)
      handle->audio.resampler->free(handle->audio.resampler_data);
   handle->audio.resampler      = NULL;
   handle->audio.resampler_data = NULL;

   av_free(handle->audio.float_conv);
   av_free(handle->audio.resample_out);
   av_free(handle->audio.fixed_conv);
   av_free(handle->audio.planar_buf);

   free(handle);
}

static void *ffmpeg_new(const struct record_params *params)
{
   ffmpeg_t *handle = (ffmpeg_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   av_register_all();
   avformat_network_init();

   handle->params = *params;

   if (params->preset == RECORD_CONFIG_TYPE_RECORDING_CUSTOM || params->preset == RECORD_CONFIG_TYPE_STREAMING_CUSTOM)
   {
      RARCH_LOG("config: %s %s\n", &handle->config, params->config);
      if (!ffmpeg_init_config(&handle->config, params->config))
         goto error;
   }
   else
      ffmpeg_init_config_common(&handle->config, params->preset);

   if (!ffmpeg_init_muxer_pre(handle))
      goto error;

   if (!ffmpeg_init_video(handle))
      goto error;

   if (handle->config.audio_enable && !ffmpeg_init_audio(handle))
      goto error;

   if (!ffmpeg_init_muxer_post(handle))
      goto error;

   if (!init_thread(handle))
      goto error;

   return handle;

error:
   ffmpeg_free(handle);
   return NULL;
}

static bool ffmpeg_push_video(void *data,
      const struct record_video_data *vid)
{
   unsigned y;
   bool drop_frame;
   struct record_video_data attr_data;
   ffmpeg_t *handle = (ffmpeg_t*)data;
   int       offset = 0;

   if (!handle || !vid)
      return false;

   drop_frame = handle->video.frame_drop_count++ %
      handle->video.frame_drop_ratio;

   handle->video.frame_drop_count %= handle->video.frame_drop_ratio;

   if (drop_frame)
      return true;

   for (;;)
   {
      unsigned avail;
      slock_lock(handle->lock);
      avail = fifo_write_avail(handle->attr_fifo);
      slock_unlock(handle->lock);

      if (!handle->alive)
         return false;

      if (avail >= sizeof(*vid))
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

   /* Tightly pack our frame to conserve memory.
    * libretro tends to use a very large pitch.
    */
   attr_data = *vid;

   if (attr_data.is_dupe)
      attr_data.width = attr_data.height = attr_data.pitch = 0;
   else
      attr_data.pitch = attr_data.width * handle->video.pix_size;

   fifo_write(handle->attr_fifo, &attr_data, sizeof(attr_data));

   for (y = 0; y < attr_data.height; y++, offset += vid->pitch)
      fifo_write(handle->video_fifo,
            (const uint8_t*)vid->data + offset, attr_data.pitch);

   slock_unlock(handle->lock);
   scond_signal(handle->cond);

   return true;
}

static bool ffmpeg_push_audio(void *data,
      const struct record_audio_data *audio_data)
{
   ffmpeg_t *handle = (ffmpeg_t*)data;

   if (!handle || !audio_data)
      return false;

   if (!handle->config.audio_enable)
      return true;

   for (;;)
   {
      unsigned avail;
      slock_lock(handle->lock);
      avail = fifo_write_avail(handle->audio_fifo);
      slock_unlock(handle->lock);

      if (!handle->alive)
         return false;

      if (avail >= audio_data->frames * handle->params.channels
            * sizeof(int16_t))
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
   fifo_write(handle->audio_fifo, audio_data->data,
         audio_data->frames * handle->params.channels * sizeof(int16_t));
   slock_unlock(handle->lock);
   scond_signal(handle->cond);

   return true;
}

static bool encode_video(ffmpeg_t *handle, AVFrame *frame)
{
   AVPacket pkt;
   int ret;

   av_init_packet(&pkt);
   pkt.data = handle->video.outbuf;
   pkt.size = handle->video.outbuf_size;

   ret = avcodec_send_frame(handle->video.codec, frame);
   if (ret < 0)
   {
#ifdef __cplusplus
      RARCH_ERR("[FFmpeg]: Cannot send video frame. Error code: %d.\n", ret);
#else
      RARCH_ERR("[FFmpeg]: Cannot send video frame. Error code: %s.\n", av_err2str(ret));
#endif
      return false;
   }

   while (ret >= 0)
   {
      ret = avcodec_receive_packet(handle->video.codec, &pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
         break;
      else if (ret < 0)
      {
#ifdef __cplusplus
         RARCH_ERR("[FFmpeg]: Cannot receive video packet. Error code: %d.\n", ret);
#else
         RARCH_ERR("[FFmpeg]: Cannot receive video packet. Error code: %s.\n", av_err2str(ret));
#endif
         return false;
      }

      pkt.pts = av_rescale_q(pkt.pts, handle->video.codec->time_base,
         handle->muxer.vstream->time_base);

      pkt.dts = av_rescale_q(pkt.dts,
         handle->video.codec->time_base,
         handle->muxer.vstream->time_base);
      
      pkt.stream_index = handle->muxer.vstream->index;

      ret = av_interleaved_write_frame(handle->muxer.ctx, &pkt);
      if (ret < 0)
      {
#ifdef __cplusplus
         RARCH_ERR("[FFmpeg]: Cannot write video packet to output file. Error code: %d.\n", ret);
#else
         RARCH_ERR("[FFmpeg]: Cannot write video packet to output file. Error code: %s.\n", av_err2str(ret));
#endif
         return false;
      }
   }
   return true;
}

static void ffmpeg_scale_input(ffmpeg_t *handle,
      const struct record_video_data *vid)
{
   /* Attempt to preserve more information if we scale down. */
   bool shrunk = handle->params.out_width < vid->width
      || handle->params.out_height < vid->height;

   if (handle->video.use_sws)
   {
      int linesize = vid->pitch;

      handle->video.sws = sws_getCachedContext(handle->video.sws,
            vid->width, vid->height, handle->video.in_pix_fmt,
            handle->params.out_width, handle->params.out_height,
            handle->video.pix_fmt,
            shrunk ? SWS_BILINEAR : SWS_POINT, NULL, NULL, NULL);

      sws_scale(handle->video.sws, (const uint8_t* const*)&vid->data,
            &linesize, 0, vid->height, handle->video.conv_frame->data,
            handle->video.conv_frame->linesize);
   }
   else
   {
      video_frame_record_scale(
            &handle->video.scaler,
            handle->video.conv_frame->data[0],
            vid->data,
            handle->params.out_width,
            handle->params.out_height,
            handle->video.conv_frame->linesize[0],
            vid->width,
            vid->height,
            vid->pitch,
            shrunk);
   }
}

static bool ffmpeg_push_video_thread(ffmpeg_t *handle,
      const struct record_video_data *vid)
{
   if (!vid->is_dupe)
      ffmpeg_scale_input(handle, vid);

   handle->video.conv_frame->pts = handle->video.frame_cnt;

   if (!encode_video(handle, handle->video.conv_frame))
      return false;

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

static void planarize_audio(ffmpeg_t *handle)
{
   if (!handle->audio.is_planar)
      return;

   if (handle->audio.frames_in_buffer > handle->audio.planar_buf_frames)
   {
      handle->audio.planar_buf = av_realloc(handle->audio.planar_buf,
            handle->audio.frames_in_buffer * handle->params.channels *
            handle->audio.sample_size);
      if (!handle->audio.planar_buf)
         return;

      handle->audio.planar_buf_frames = handle->audio.frames_in_buffer;
   }

   if (handle->audio.use_float)
      planarize_float((float*)handle->audio.planar_buf,
            (const float*)handle->audio.buffer,
            handle->audio.frames_in_buffer);
   else
      planarize_s16((int16_t*)handle->audio.planar_buf,
            (const int16_t*)handle->audio.buffer,
            handle->audio.frames_in_buffer);
}

static bool encode_audio(ffmpeg_t *handle, bool dry)
{
   AVFrame *frame;
   AVPacket pkt;
   int samples_size;
   int ret;

   av_init_packet(&pkt);
   pkt.data = handle->audio.outbuf;
   pkt.size = handle->audio.outbuf_size;

   frame = av_frame_alloc();
   if (!frame)
      return false;

   frame->nb_samples     = handle->audio.frames_in_buffer;
   frame->format         = handle->audio.codec->sample_fmt;
   frame->channel_layout = handle->audio.codec->channel_layout;
   frame->pts            = handle->audio.frame_cnt;

   planarize_audio(handle);

   samples_size = av_samples_get_buffer_size(NULL,
         handle->audio.codec->channels,
         handle->audio.frames_in_buffer,
         handle->audio.codec->sample_fmt, 0);

   avcodec_fill_audio_frame(frame, handle->audio.codec->channels,
         handle->audio.codec->sample_fmt,
         handle->audio.is_planar ? (uint8_t*)handle->audio.planar_buf :
         handle->audio.buffer,
         samples_size, 0);

   ret = avcodec_send_frame(handle->audio.codec, dry ? NULL : frame);
   if (ret < 0)
   {
      av_frame_free(&frame);
#ifdef __cplusplus
      RARCH_ERR("[FFmpeg]: Cannot send audio frame. Return code: %d.\n", ret);
#else
      RARCH_ERR("[FFmpeg]: Cannot send audio frame. Return code: %s.\n", av_err2str(ret));
#endif
      return false;
   }

   while (ret >= 0) 
   {
      ret = avcodec_receive_packet(handle->audio.codec, &pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
         break;
      else if (ret < 0)
      {
         av_frame_free(&frame);
#ifdef __cplusplus
         RARCH_ERR("[FFmpeg]: Cannot receive audio packet. Return code: %d.\n", ret);
#else
         RARCH_ERR("[FFmpeg]: Cannot receive audio packet. Return code: %s.\n", av_err2str(ret));
#endif
         return false;
      }

      pkt.pts = av_rescale_q(pkt.pts,
         handle->audio.codec->time_base,
         handle->muxer.astream->time_base);

      pkt.dts = av_rescale_q(pkt.dts,
         handle->audio.codec->time_base,
         handle->muxer.astream->time_base);

      pkt.stream_index = handle->muxer.astream->index;

      ret = av_interleaved_write_frame(handle->muxer.ctx, &pkt);
      if (ret < 0)
      {
         av_frame_free(&frame);
#ifdef __cplusplus
         RARCH_ERR("[FFmpeg]: Cannot write video packet to output file. Error code: %d.\n", ret);
#else
         RARCH_ERR("[FFmpeg]: Cannot write video packet to output file. Error code: %s.\n", av_err2str(ret));
#endif
         return false;
      }
   }

   av_frame_free(&frame);
   return true;
}

static void ffmpeg_audio_resample(ffmpeg_t *handle,
      struct record_audio_data *aud)
{
   if (!handle->audio.use_float && !handle->audio.resampler)
      return;

   if (aud->frames > handle->audio.float_conv_frames)
   {
      handle->audio.float_conv = (float*)av_realloc(handle->audio.float_conv,
            aud->frames * handle->params.channels * sizeof(float));
      if (!handle->audio.float_conv)
         return;

      handle->audio.float_conv_frames = aud->frames;

      /* To make sure we don't accidentially overflow. */
      handle->audio.resample_out_frames = aud->frames * handle->audio.ratio + 16;

      handle->audio.resample_out = (float*)av_realloc(handle->audio.resample_out,
            handle->audio.resample_out_frames *
            handle->params.channels * sizeof(float));
      if (!handle->audio.resample_out)
         return;

      handle->audio.fixed_conv_frames = MAX(handle->audio.resample_out_frames,
            handle->audio.float_conv_frames);
      handle->audio.fixed_conv = (int16_t*)av_realloc(handle->audio.fixed_conv,
            handle->audio.fixed_conv_frames * handle->params.channels * sizeof(int16_t));
      if (!handle->audio.fixed_conv)
         return;
   }

   if (handle->audio.use_float || handle->audio.resampler)
   {
      convert_s16_to_float(handle->audio.float_conv,
            (const int16_t*)aud->data, aud->frames * handle->params.channels, 1.0);
      aud->data = handle->audio.float_conv;
   }

   if (handle->audio.resampler)
   {
      /* It's always two channels ... */
      struct resampler_data info = {0};

      info.data_in      = (const float*)aud->data;
      info.data_out     = handle->audio.resample_out;
      info.input_frames = aud->frames;
      info.ratio        = handle->audio.ratio;

      handle->audio.resampler->process(handle->audio.resampler_data, &info);

      aud->data         = handle->audio.resample_out;
      aud->frames       = info.output_frames;

      if (!handle->audio.use_float)
      {
         convert_float_to_s16(handle->audio.fixed_conv,
               handle->audio.resample_out,
               aud->frames * handle->params.channels);
         aud->data = handle->audio.fixed_conv;
      }
   }
}

static bool ffmpeg_push_audio_thread(ffmpeg_t *handle,
      struct record_audio_data *aud, bool require_block)
{
   size_t written_frames = 0;

   ffmpeg_audio_resample(handle, aud);

   while (written_frames < aud->frames)
   {
      size_t can_write    = handle->audio.codec->frame_size -
         handle->audio.frames_in_buffer;
      size_t write_left   = aud->frames - written_frames;
      size_t write_frames = write_left > can_write ? can_write : write_left;
      size_t write_size   = write_frames *
         handle->params.channels * handle->audio.sample_size;

      size_t bytes_in_buffer = handle->audio.frames_in_buffer *
         handle->params.channels * handle->audio.sample_size;
      size_t written_bytes   = written_frames *
         handle->params.channels * handle->audio.sample_size;

      memcpy(handle->audio.buffer + bytes_in_buffer,
            (const uint8_t*)aud->data + written_bytes,
            write_size);

      written_frames                 += write_frames;
      handle->audio.frames_in_buffer += write_frames;

      if ((handle->audio.frames_in_buffer
               < (size_t)handle->audio.codec->frame_size) && require_block)
         break;

      if (!encode_audio(handle, false))
         return false;

      handle->audio.frame_cnt       += handle->audio.frames_in_buffer;
      handle->audio.frames_in_buffer = 0;
   }
   return true;
}

static void ffmpeg_flush_audio(ffmpeg_t *handle, void *audio_buf,
      size_t audio_buf_size)
{
   size_t avail = fifo_read_avail(handle->audio_fifo);

   if (avail)
   {
      struct record_audio_data aud = {0};

      fifo_read(handle->audio_fifo, audio_buf, avail);

      aud.frames = avail / (sizeof(int16_t) * handle->params.channels);
      aud.data = audio_buf;

      ffmpeg_push_audio_thread(handle, &aud, false);
   }

   encode_audio(handle, true);
   }

static void ffmpeg_flush_video(ffmpeg_t *handle)
{
   encode_video(handle, NULL);

}

static void ffmpeg_flush_buffers(ffmpeg_t *handle)
{
   bool did_work;
   void *video_buf = av_malloc(2 * handle->params.fb_width *
         handle->params.fb_height * handle->video.pix_size);
   size_t audio_buf_size = handle->config.audio_enable ?
      (handle->audio.codec->frame_size *
       handle->params.channels * sizeof(int16_t)) : 0;
   void *audio_buf = NULL;

   if (audio_buf_size)
      audio_buf = av_malloc(audio_buf_size);
   /* Try pushing data in an interleaving pattern to
    * ease the work of the muxer a bit. */

   do
   {
      struct record_video_data attr_buf;

      did_work = false;

      if (handle->config.audio_enable)
      {
         if (fifo_read_avail(handle->audio_fifo) >= audio_buf_size)
         {
            struct record_audio_data aud = {0};

            fifo_read(handle->audio_fifo, audio_buf, audio_buf_size);
            aud.frames = handle->audio.codec->frame_size;
            aud.data = audio_buf;
            ffmpeg_push_audio_thread(handle, &aud, true);

            did_work = true;
         }
      }

      if (fifo_read_avail(handle->attr_fifo) >= sizeof(attr_buf))
      {
         fifo_read(handle->attr_fifo, &attr_buf, sizeof(attr_buf));
         fifo_read(handle->video_fifo, video_buf,
               attr_buf.height * attr_buf.pitch);
         attr_buf.data = video_buf;
         ffmpeg_push_video_thread(handle, &attr_buf);

         did_work = true;
      }
   } while (did_work);

   /* Flush out last audio. */
   if (handle->config.audio_enable)
      ffmpeg_flush_audio(handle, audio_buf, audio_buf_size);

   /* Flush out last video. */
   ffmpeg_flush_video(handle);

   av_free(video_buf);
   av_free(audio_buf);
}

static bool ffmpeg_finalize(void *data)
{
   ffmpeg_t *handle = (ffmpeg_t*)data;
   if (!handle)
      return false;

   deinit_thread(handle);

   /* Flush out data still in buffers (internal, and FFmpeg internal). */
   ffmpeg_flush_buffers(handle);

   deinit_thread_buf(handle);

   /* Write final data. */
   av_write_trailer(handle->muxer.ctx);

   avio_close(ctx->pb);

   return true;
}

static void ffmpeg_thread(void *data)
{
   size_t audio_buf_size;
   void *audio_buf = NULL;
   ffmpeg_t *ff    = (ffmpeg_t*)data;
   /* For some reason, FFmpeg has a tendency to crash
    * if we don't overallocate a bit. */
   void *video_buf = av_malloc(2 * ff->params.fb_width *
         ff->params.fb_height * ff->video.pix_size);

   retro_assert(video_buf);

   audio_buf_size = ff->config.audio_enable ?
      (ff->audio.codec->frame_size * ff->params.channels * sizeof(int16_t)) : 0;
   audio_buf      = audio_buf_size ? av_malloc(audio_buf_size) : NULL;

   while (ff->alive)
   {
      struct record_video_data attr_buf;

      bool avail_video = false;
      bool avail_audio = false;

      slock_lock(ff->lock);
      if (fifo_read_avail(ff->attr_fifo) >= sizeof(attr_buf))
         avail_video = true;

      if (ff->config.audio_enable)
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

      if (avail_video && video_buf)
      {
         slock_lock(ff->lock);
         fifo_read(ff->attr_fifo, &attr_buf, sizeof(attr_buf));
         fifo_read(ff->video_fifo, video_buf,
               attr_buf.height * attr_buf.pitch);
         slock_unlock(ff->lock);
         scond_signal(ff->cond);

         attr_buf.data = video_buf;
         ffmpeg_push_video_thread(ff, &attr_buf);
      }

      if (avail_audio && audio_buf)
      {
         struct record_audio_data aud = {0};

         slock_lock(ff->lock);
         fifo_read(ff->audio_fifo, audio_buf, audio_buf_size);
         slock_unlock(ff->lock);
         scond_signal(ff->cond);

         aud.frames = ff->audio.codec->frame_size;
         aud.data = audio_buf;

         ffmpeg_push_audio_thread(ff, &aud, true);
      }
   }

   av_free(video_buf);
   av_free(audio_buf);
}

const record_driver_t record_ffmpeg = {
   ffmpeg_new,
   ffmpeg_free,
   ffmpeg_push_video,
   ffmpeg_push_audio,
   ffmpeg_finalize,
   "ffmpeg",
};
