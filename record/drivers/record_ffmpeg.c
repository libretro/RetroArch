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

#include <compat/msvc.h>
#include <compat/strl.h>

#include <boolean.h>
#include <retro_spsc.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <retro_atomic.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <file/config_file.h>
#include <audio/audio_resampler.h>
#include <audio/sinc_resampler_int16.h>
#include <string/stdstring.h>
#include <audio/conversion/float_to_s16.h>
#include <audio/conversion/s16_to_float.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef FFEMU_PERF
#include <time.h>
#endif

#if defined(__linux__)
#include <dirent.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#define FFMPEG_HAVE_THREAD_NICE 1
#endif

#include "record_ffmpeg.h"

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

#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../verbosity.h"

#ifndef FFMPEG3
#define FFMPEG3 ((LIBAVUTIL_VERSION_INT < AV_VERSION_INT(56, 6, 100)) || \
      (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100)))
#endif

#ifndef FFMPEG8
#define FFMPEG8 (LIBAVCODEC_VERSION_MAJOR >= 62)
#endif

/* avcodec_get_supported_config() was added in lavc 61.13.100 (FFmpeg 7.1)
 * and the AVCodec.sample_fmts / AVCodec.supported_samplerates arrays it
 * replaces were deprecated at the same time, then removed entirely in
 * lavc 63 (FFmpeg 9). Use the new API as soon as it is available so a
 * single codepath covers FFmpeg 7.1 through 9+, and keep the old struct
 * members for FFmpeg 7.0 and older. */
#define HAVE_AVCODEC_GET_SUPPORTED_CONFIG \
      (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100))

#ifndef AV_INPUT_BUFFER_MIN_SIZE
#define AV_INPUT_BUFFER_MIN_SIZE 16384
#endif

#define HAVE_CH_LAYOUT (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100))

struct ff_video_info
{
   AVCodecContext *codec;
   const AVCodec *encoder;

   AVFrame *conv_frame;
   uint8_t *conv_frame_buf;
   int64_t frame_cnt;

   /* Output pixel format. */
   enum AVPixelFormat pix_fmt;
   /* Input pixel format. Only used by sws. */
   enum AVPixelFormat in_pix_fmt;

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
   const AVCodec *encoder;

   uint8_t *buffer;
   size_t frames_in_buffer;

   int64_t frame_cnt;

   /* The AVFrame handed to the encoder for every audio block. It only
    * ever wraps buffer or planar_buf, which the handle owns, so it is
    * allocated once here rather than with a sample buffer of its own
    * on every block. */
   AVFrame *frame;

   /* Most lossy audio codecs only support certain sampling rates.
    * Could use libswresample, but it doesn't support floating point ratios.
    * Use either S16 or (planar) float for simplicity.
    */
   const retro_resampler_t *resampler;
   void *resampler_data;
   /* The float resampler is stereo: a wider frame goes through one
    * instance per pair at the same ratio on the same count, which
    * produces the same number of frames for every pair. resampler_data
    * is the first pair's; the others follow here. */
   void *resampler_pair[3];
   float *pair_in, *pair_out;
   size_t pair_frames;

   /* When the encoder consumes s16 and a resample is required, use the
    * integer sinc resampler so the game signal never detours through
    * float (s16 -> resample -> s16 directly). */
   void *resampler_int16;

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
   enum AVPixelFormat out_pix_fmt;
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
   /* Rate control / GOP for bitrate-constrained outputs (RTMP).
    * 0 (or <0 for max_b_frames) leaves the encoder default. */
   int video_max_rate;
   int video_buf_size;
   float video_gop_seconds;
   int video_max_b_frames;
   /* Bits per pixel per frame used to derive a bitrate for hardware
    * encoders when the preset is CRF-based (hardware has no CRF). */
   float video_hw_bpp;

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

   AVPacket *pkt;

   /* The three queues to the encoder thread.  Single producer (the
    * main thread in ffmpeg_push_video / ffmpeg_push_audio), single
    * consumer (ffmpeg_thread), so lock-free retro_spsc rings: the
    * push of a frame - up to MAX_FRAMES frames of fb_dims in
    * video_fifo - and the encoder's read of one no
    * longer exclude each other, where before both copied the whole
    * frame under one lock.
    * After deinit_thread() has joined the encoder, the main thread
    * drains them alone (ffmpeg_flush_buffers). */
   retro_spsc_t audio_fifo;
   retro_spsc_t video_fifo;
   retro_spsc_t attr_fifo;
   bool fifos_init;
   sthread_t *thread;

   /* Set by init_thread(), cleared by deinit_thread(); the encoder
    * thread's loop condition and the push paths' bail-out.  Was a
    * volatile bool, which TSan flagged against the clear. */
   retro_atomic_int_t alive;
   /* The sleeps, neither with a lock: the encoder waits on data, which
    * a push notifies after writing, and a push that finds no room
    * waits on space, which the encoder notifies after each read.
    * deinit_thread() notifies both once alive is clear. */
   retro_eventcount_t data;
   retro_eventcount_t space;
   bool data_init;
   bool space_init;

   /* Drop-on-full accounting. Written by the producer only; the
    * totals are read after the encoder thread has been joined. */
   uint64_t video_frames_in;
   uint64_t video_frames_dropped;
   uint64_t video_frames_logged;
   /* Frames dropped since the last queued one; rides along with the
    * next frame so the encoder leaves a pts gap for them. */
   unsigned video_pts_pending;
   unsigned video_fifo_frames;
   bool     allow_frame_drop;

   /* Audio bytes dropped on a full queue and still owed to it as
    * silence, so the sample-count timeline never loses time. */
   size_t   audio_silence_pending;
   uint64_t audio_bytes_dropped;

   /* Encoder thread: pts of the last keyframe forced for
    * video_gop_seconds. */
   int64_t  video_last_key_pts;
} ffmpeg_t;

/* attr_fifo record: the frame plus the drops right before it. */
struct ff_video_attr
{
   struct record_video_data vid;
   unsigned pts_skip;
};

/* How long a push waits for room before looking at alive again. The
 * encoder notifies space on every read and deinit_thread() on the way
 * out, so this only bounds a wake that never comes. */
#define FFMPEG_PUSH_WAIT_US 100000

AVFormatContext *ctx;

/* Returns the encoder's list of supported sample formats, terminated by
 * AV_SAMPLE_FMT_NONE, or NULL if the encoder does not restrict sample
 * formats (or the list could not be queried). */
static const enum AVSampleFormat *ffmpeg_codec_sample_formats(
      const AVCodec *codec)
{
#if HAVE_AVCODEC_GET_SUPPORTED_CONFIG
   const void *fmts = NULL;
   if (avcodec_get_supported_config(NULL, codec,
         AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &fmts, NULL) < 0)
      return NULL;
   return (const enum AVSampleFormat*)fmts;
#else
   return codec->sample_fmts;
#endif
}

/* Returns the encoder's list of supported sample rates, terminated by 0,
 * or NULL if the encoder does not restrict sample rates (or the list
 * could not be queried). */
static const int *ffmpeg_codec_supported_samplerates(const AVCodec *codec)
{
#if HAVE_AVCODEC_GET_SUPPORTED_CONFIG
   const void *rates = NULL;
   if (avcodec_get_supported_config(NULL, codec,
         AV_CODEC_CONFIG_SAMPLE_RATE, 0, &rates, NULL) < 0)
      return NULL;
   return (const int*)rates;
#else
   return codec->supported_samplerates;
#endif
}

static bool ffmpeg_codec_has_sample_format(enum AVSampleFormat fmt,
      const enum AVSampleFormat *fmts)
{
   unsigned i;

   /* A NULL list means the encoder does not restrict sample formats. */
   if (!fmts)
      return true;

   for (i = 0; fmts[i] != AV_SAMPLE_FMT_NONE; i++)
      if (fmt == fmts[i])
         return true;
   return false;
}

static void ffmpeg_audio_resolve_format(struct ff_audio_info *audio,
      const AVCodec *codec)
{
   const enum AVSampleFormat *sample_fmts = ffmpeg_codec_sample_formats(codec);

   audio->codec->sample_fmt = AV_SAMPLE_FMT_NONE;

   if (ffmpeg_codec_has_sample_format(AV_SAMPLE_FMT_FLTP, sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
      audio->use_float         = true;
      audio->is_planar         = true;
      RARCH_LOG("[FFmpeg] Using sample format FLTP.\n");
   }
   else if (ffmpeg_codec_has_sample_format(AV_SAMPLE_FMT_FLT, sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_FLT;
      audio->use_float         = true;
      audio->is_planar         = false;
      RARCH_LOG("[FFmpeg] Using sample format FLT.\n");
   }
   else if (ffmpeg_codec_has_sample_format(AV_SAMPLE_FMT_S16P, sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_S16P;
      audio->use_float         = false;
      audio->is_planar         = true;
      RARCH_LOG("[FFmpeg] Using sample format S16P.\n");
   }
   else if (ffmpeg_codec_has_sample_format(AV_SAMPLE_FMT_S16, sample_fmts))
   {
      audio->codec->sample_fmt = AV_SAMPLE_FMT_S16;
      audio->use_float         = false;
      audio->is_planar         = false;
      RARCH_LOG("[FFmpeg] Using sample format S16.\n");
   }
   audio->sample_size = audio->use_float ? sizeof(float) : sizeof(int16_t);
}

static void ffmpeg_audio_resolve_sample_rate(ffmpeg_t *handle,
      const AVCodec *codec)
{
   struct ff_config_param *params  = &handle->config;
   struct record_params *param     = &handle->params;
   const int *supported_samplerates = ffmpeg_codec_supported_samplerates(codec);

   /* We'll have to force resampling to some supported sampling rate.
    * A NULL list means the encoder accepts any sample rate, in which
    * case the input rate is kept as-is. */
   if (supported_samplerates && !params->sample_rate)
   {
      unsigned i;
      int input_rate = (int)param->samplerate;

      /* Favor closest sampling rate, but always prefer ratio > 1.0. */
      int best_rate  = supported_samplerates[0];
      int best_diff  = best_rate - input_rate;

      for (i = 1; supported_samplerates[i]; i++)
      {
         bool better_rate = false;
         int diff         = supported_samplerates[i] - input_rate;

         if (best_diff < 0)
            better_rate   = (diff > best_diff);
         else
            better_rate   = ((diff >= 0) && (diff < best_diff));

         if (better_rate)
         {
            best_rate = supported_samplerates[i];
            best_diff = diff;
         }
      }

      params->sample_rate = best_rate;
      RARCH_LOG("[FFmpeg] Using output sampling rate: %d.\n", best_rate);
   }
}

static bool ffmpeg_init_audio(ffmpeg_t *handle, const char *audio_resampler)
{
   int nb_channels;
   struct ff_config_param *params  = &handle->config;
   struct ff_audio_info *audio     = &handle->audio;
   struct record_params *param     = &handle->params;
   const AVCodec *codec            = avcodec_find_encoder_by_name(
         *params->acodec ? params->acodec : "flac");
   if (!codec)
   {
      RARCH_ERR("[FFmpeg] Cannot find acodec %s.\n",
            *params->acodec ? params->acodec : "flac");
      return false;
   }

   audio->encoder               = codec;

   audio->codec                 = avcodec_alloc_context3(codec);

   audio->codec->codec_type     = AVMEDIA_TYPE_AUDIO;
   /* The default layout for the count: mono, stereo, quad, 5.1 (the
    * pair at the back), 7.1 - FL FR FC LFE BL BR SL SR, the order the
    * frames arrive in. */
#if HAVE_CH_LAYOUT
   av_channel_layout_default(&audio->codec->ch_layout, (int)param->channels);
#else
   audio->codec->channels       = param->channels;
   audio->codec->channel_layout = av_get_default_channel_layout((int)param->channels);
#endif

   ffmpeg_audio_resolve_format(audio, codec);
   ffmpeg_audio_resolve_sample_rate(handle, codec);

   if (params->sample_rate)
   {
      audio->ratio              = (double)params->sample_rate
         / param->samplerate;
      audio->codec->sample_rate = params->sample_rate;
      audio->codec->time_base   = av_d2q(1.0 / params->sample_rate, 1000000);

      /* For an s16 encoder, resample directly in the integer domain and
       * skip the s16<->float round-trip on the game signal.  For a float
       * encoder, keep the float resampler (its output is needed as float
       * anyway). */
      /* The integer sinc resampler is interleaved-stereo only, so it is
       * used only for 2-channel s16 output; mono and float keep the
       * float path. */
      if (!audio->use_float && param->channels == 2)
      {
         /* Pass the raw ratio as bandwidth_mod, matching the main audio
          * path: <1.0 (downsampling) lowers the cutoff for anti-aliasing,
          * >=1.0 (upsampling) leaves it unchanged. */
         audio->resampler_int16 = sinc_resampler_int16_init(
               audio->ratio, SINC_INT16_QUALITY_NORMAL);
      }

      if (!audio->resampler_int16)
      {
         unsigned i, npairs = (param->channels + 1) / 2;
         retro_resampler_realloc(
               &audio->resampler_data,
               &audio->resampler,
               audio_resampler,
               RESAMPLER_QUALITY_DONTCARE,
               audio->ratio);
         for (i = 1; i < npairs && i < 4; i++)
         {
            const retro_resampler_t *drv = NULL;
            retro_resampler_realloc(&audio->resampler_pair[i - 1], &drv,
                  audio_resampler, RESAMPLER_QUALITY_DONTCARE, audio->ratio);
            if (!audio->resampler_pair[i - 1])
            {
               RARCH_ERR("[FFmpeg] Cannot create the resampler for channel pair %u.\n", i);
               return false;
            }
         }
      }
   }
   else
   {
      audio->codec->sample_fmt  = AV_SAMPLE_FMT_S16;
      audio->codec->sample_rate = (int)roundf(param->samplerate);
      audio->codec->time_base   = av_d2q(1.0 / param->samplerate, 1000000);
   }

   if (params->audio_qscale)
   {
      audio->codec->flags             |= AV_CODEC_FLAG_QSCALE;
      audio->codec->global_quality     = params->audio_global_quality;
   }
   else if (params->audio_bit_rate)
      audio->codec->bit_rate           = params->audio_bit_rate;

   /* Allow experimental codecs. */
   audio->codec->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

   if (handle->muxer.ctx->oformat->flags & AVFMT_GLOBALHEADER)
      audio->codec->flags             |= AV_CODEC_FLAG_GLOBAL_HEADER;

   if (avcodec_open2(audio->codec, codec,
            params->audio_opts ? &params->audio_opts : NULL) != 0)
      return false;

   /* If not set (PCM), just set something. */
   if (!audio->codec->frame_size)
      audio->codec->frame_size = 1024;

#if HAVE_CH_LAYOUT
   nb_channels = audio->codec->ch_layout.nb_channels;
#else
   nb_channels = audio->codec->channels;
#endif

   audio->buffer = (uint8_t*)av_malloc(
         audio->codec->frame_size *
         nb_channels *
         audio->sample_size);

   if (!audio->buffer)
      return false;

   if (!(audio->frame = av_frame_alloc()))
      return false;
   audio->frame->format = audio->codec->sample_fmt;
#if HAVE_CH_LAYOUT
   av_channel_layout_copy(&audio->frame->ch_layout, &audio->codec->ch_layout);
#else
   audio->frame->channel_layout = audio->codec->channel_layout;
#endif

   return true;
}

/* Returns the encoder's supported pixel formats, terminated by
 * AV_PIX_FMT_NONE, or NULL if unrestricted / unknown. */
static const enum AVPixelFormat *ffmpeg_codec_pix_fmts(const AVCodec *codec)
{
#if HAVE_AVCODEC_GET_SUPPORTED_CONFIG
   const void *fmts = NULL;
   if (avcodec_get_supported_config(NULL, codec,
         AV_CODEC_CONFIG_PIX_FORMAT, 0, &fmts, NULL) < 0)
      return NULL;
   return (const enum AVPixelFormat*)fmts;
#else
   return codec->pix_fmts;
#endif
}

/* H.264 encoders that take frames from system memory, tried in order.
 * VAAPI is left out: it needs a hw_frames_ctx upload path. */
static const char *const ffmpeg_hw_h264_encoders[] = {
   "h264_nvv4l2",       /* Tegra (L4T FFmpeg) */
   "h264_nvmpi",        /* Tegra/Jetson (jetson-ffmpeg) */
   "h264_v4l2m2m",      /* V4L2 mem2mem: RPi, Exynos, ... */
   "h264_rkmpp",        /* Rockchip */
   "h264_nvenc",        /* NVIDIA desktop */
   "h264_amf",          /* AMD */
   "h264_qsv",          /* Intel */
   "h264_videotoolbox", /* Apple */
   "h264_mediacodec",   /* Android */
   NULL
};

/* Pick YUV420P, else NV12, from what the encoder accepts. */
static enum AVPixelFormat ffmpeg_hw_pick_pix_fmt(const AVCodec *codec)
{
   unsigned i;
   bool has_nv12                    = false;
   const enum AVPixelFormat *fmts   = ffmpeg_codec_pix_fmts(codec);

   if (!fmts)
      return AV_PIX_FMT_YUV420P;

   for (i = 0; fmts[i] != AV_PIX_FMT_NONE; i++)
   {
      if (fmts[i] == AV_PIX_FMT_YUV420P)
         return AV_PIX_FMT_YUV420P;
      if (fmts[i] == AV_PIX_FMT_NV12)
         has_nv12 = true;
   }
   return has_nv12 ? AV_PIX_FMT_NV12 : AV_PIX_FMT_NONE;
}

/* Allocates and opens handle->video.codec; frees it on failure. */
static bool ffmpeg_open_video_codec(ffmpeg_t *handle,
      const AVCodec *codec, enum AVPixelFormat pix_fmt,
      unsigned out_w, unsigned out_h, bool hw, AVDictionary **opts)
{
   struct ff_config_param *params  = &handle->config;
   struct ff_video_info *video     = &handle->video;
   struct record_params *param     = &handle->params;
   AVCodecContext *c;

   /* Ensure even dimensions for chroma-subsampled pixel formats.
    * Odd dimensions cause encoder init failure with e.g. libx264.
    * Round up (pad) rather than down so no source pixels are lost. */
   if (     pix_fmt == AV_PIX_FMT_YUV420P
         || pix_fmt == AV_PIX_FMT_YUV422P
         || pix_fmt == AV_PIX_FMT_NV12)
   {
      out_w = (out_w + 1) & ~1;
      out_h = (out_h + 1) & ~1;
   }

   if (!(c = avcodec_alloc_context3(codec)))
      return false;

   c->codec_type          = AVMEDIA_TYPE_VIDEO;
   c->width               = out_w;
   c->height              = out_h;
   c->time_base           = av_d2q((double)
         params->frame_drop_ratio /param->fps, 1000000); /* Arbitrary big number. */
   c->framerate           = av_inv_q(c->time_base);
   c->sample_aspect_ratio = av_d2q(
         param->aspect_ratio * out_h / out_w, 255);
   c->pix_fmt             = pix_fmt;
   c->thread_count        = params->threads;

   if (params->video_qscale)
   {
      c->flags          |= AV_CODEC_FLAG_QSCALE;
      c->global_quality  = params->video_global_quality;
   }
   else if (params->video_bit_rate)
      c->bit_rate = params->video_bit_rate;
   else if (hw)
   {
      /* No CRF in hardware; derive a bitrate from the preset. */
      float bpp   = params->video_hw_bpp > 0.0f
         ? params->video_hw_bpp : 0.1f;
      double rate = (double)bpp * out_w * out_h
         * param->fps / params->frame_drop_ratio;
      if (rate < 500000.0)
         rate = 500000.0;
      c->bit_rate = (int64_t)rate;
   }

   if (params->video_max_rate)
      c->rc_max_rate    = params->video_max_rate;
   if (params->video_buf_size)
      c->rc_buffer_size = params->video_buf_size;
   if (params->video_max_b_frames >= 0)
      c->max_b_frames   = params->video_max_b_frames;
   if (params->video_gop_seconds > 0.0f)
   {
      int gop = (int)(params->video_gop_seconds * param->fps
            / params->frame_drop_ratio + 0.5f);
      if (gop < 1)
         gop = 1;
      c->gop_size = gop;
   }

   if (handle->muxer.ctx->oformat->flags & AVFMT_GLOBALHEADER)
      c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

   if (avcodec_open2(c, codec, opts) != 0)
   {
      avcodec_free_context(&c);
      return false;
   }

   video->codec    = c;
   video->encoder  = codec;
   param->out_dims = VIDEO_SCALE_PACK(out_w, out_h);
   return true;
}

/* Try the hardware H.264 encoders this FFmpeg build has, in order. */
static bool ffmpeg_open_hw_h264(ffmpeg_t *handle,
      unsigned out_w, unsigned out_h, enum AVPixelFormat *out_fmt)
{
   unsigned i;
   struct ff_config_param *params = &handle->config;

   for (i = 0; ffmpeg_hw_h264_encoders[i]; i++)
   {
      AVDictionary *opts   = NULL;
      enum AVPixelFormat fmt;
      const AVCodec *codec = avcodec_find_encoder_by_name(
            ffmpeg_hw_h264_encoders[i]);

      if (!codec)
         continue;
      if ((fmt = ffmpeg_hw_pick_pix_fmt(codec)) == AV_PIX_FMT_NONE)
      {
         RARCH_LOG("[FFmpeg] %s: no YUV420P/NV12 input, skipping.\n",
               codec->name);
         continue;
      }

      /* x264's private options mean nothing to these. Ask for CBR where
       * there's an "rc" knob; an encoder that rejects it fails to open
       * and the next one is tried. */
      if (params->video_max_rate)
         av_dict_set(&opts, "rc", "cbr", 0);

      if (ffmpeg_open_video_codec(handle, codec, fmt, out_w, out_h,
               true, &opts))
      {
         av_dict_free(&opts);
         *out_fmt = fmt;
         RARCH_LOG("[FFmpeg] Using hardware encoder %s (%s).\n",
               codec->name, av_get_pix_fmt_name(fmt));
         return true;
      }
      av_dict_free(&opts);
      RARCH_WARN("[FFmpeg] Hardware encoder %s failed to open.\n",
            codec->name);
   }
   return false;
}

static bool ffmpeg_init_video(ffmpeg_t *handle)
{
   size_t size;
   AVFrame *frame;
   struct ff_config_param *params  = &handle->config;
   struct ff_video_info *video     = &handle->video;
   struct record_params *param     = &handle->params;
   const AVCodec *codec            = NULL;
   enum AVPixelFormat pix_fmt      = AV_PIX_FMT_BGR24;
   bool opened                     = false;
   unsigned out_w, out_h;

   switch (param->pix_fmt)
   {
      case FFEMU_PIX_RGB565:
         video->scaler.in_fmt = SCALER_FMT_RGB565;
         video->in_pix_fmt    = AV_PIX_FMT_RGB565;
         video->pix_size      = 2;
         break;

      case FFEMU_PIX_BGR24:
         video->scaler.in_fmt = SCALER_FMT_BGR24;
         video->in_pix_fmt    = AV_PIX_FMT_BGR24;
         video->pix_size      = 3;
         break;

      case FFEMU_PIX_ARGB8888:
         video->scaler.in_fmt = SCALER_FMT_ARGB8888;
         video->in_pix_fmt    = AV_PIX_FMT_RGB32;
         video->pix_size      = 4;
         break;

      default:
         return false;
   }

   /* Useful to set scale_factor to 2 for chroma subsampled formats to
    * maintain full chroma resolution. (Or just use 4:4:4 or RGB ...)
    */
   out_w = (float)VIDEO_SCALE_W(param->out_dims) * params->scale_factor;
   out_h = (float)VIDEO_SCALE_H(param->out_dims) * params->scale_factor;

   /* Hardware H.264 replaces libx264 for the built-in presets only;
    * an explicit vcodec in a custom config is left alone. */
   if (     param->hw_encoder
         && !params->conf
         && string_is_equal(params->vcodec, "libx264"))
   {
      opened = ffmpeg_open_hw_h264(handle, out_w, out_h, &pix_fmt);
      if (!opened)
         RARCH_WARN("[FFmpeg] No usable hardware H.264 encoder,"
               " falling back to libx264.\n");
   }

   if (!opened)
   {
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
         RARCH_ERR("[FFmpeg] Cannot find vcodec %s.\n",
               *params->vcodec ? params->vcodec : "libx264rgb");
         return false;
      }

      if (params->out_pix_fmt != AV_PIX_FMT_NONE)
         pix_fmt = params->out_pix_fmt;

      if (!ffmpeg_open_video_codec(handle, codec, pix_fmt, out_w, out_h,
               false, params->video_opts ? &params->video_opts : NULL))
         return false;
   }

   out_w = VIDEO_SCALE_W(param->out_dims);
   out_h = VIDEO_SCALE_H(param->out_dims);

   /* Don't use swscaler unless format is not something "in-house" scaler
    * supports.
    *
    * libswscale doesn't scale RGB -> RGB correctly (goes via YUV first),
    * and it's non-trivial to fix upstream as it's heavily geared towards YUV.
    * If we're dealing with strange formats or YUV, just use libswscale.
    */
   video->pix_fmt = pix_fmt;
   switch (pix_fmt)
   {
      case AV_PIX_FMT_BGR24:
         video->scaler.out_fmt = SCALER_FMT_BGR24;
         break;
      case AV_PIX_FMT_RGB32:
         video->scaler.out_fmt = SCALER_FMT_ARGB8888;
         break;
      default:
         video->use_sws = true;
         break;
   }

   video->frame_drop_ratio = params->frame_drop_ratio;

   size = av_image_get_buffer_size(video->pix_fmt, out_w, out_h, 1);
   video->conv_frame_buf   = (uint8_t*)av_malloc(size);
   /* NULL-check conv_frame_buf: the memset on the next line
    * NULL-derefs on OOM, as does av_image_fill_arrays below
    * which writes into frame->data pointers based on
    * conv_frame_buf. */
   if (!video->conv_frame_buf)
      return false;
   /* Zero the buffer so padding pixels (from rounding odd dimensions
    * up to even) are black rather than uninitialized. */
   memset(video->conv_frame_buf, 0, size);
   video->conv_frame       = av_frame_alloc();

   frame = video->conv_frame;
   av_image_fill_arrays(frame->data, frame->linesize, video->conv_frame_buf,
         video->pix_fmt, out_w, out_h, 1);

   video->conv_frame->width  = out_w;
   video->conv_frame->height = out_h;
   video->conv_frame->format = video->pix_fmt;

   return true;
}

/* RTMP ingest (Twitch, YouTube, Facebook, Kick) wants H.264 + AAC-LC in
 * FLV, CBR video, a fixed 2 s keyframe interval and 44.1/48 kHz audio.
 * Twitch caps non-partner video at 6000 kbps and audio at 160 kbps. */
static void ffmpeg_config_rtmp(struct ff_config_param *params,
      unsigned preset)
{
   int kbps;

   switch (preset)
   {
      case RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY:
         kbps = 2500;
         break;
      case RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY:
         kbps = 4500;
         break;
      default:
         kbps = 6000;
         break;
   }

   params->video_qscale       = false;
   params->video_bit_rate     = kbps * 1000;
   params->video_max_rate     = kbps * 1000;
   params->video_buf_size     = kbps * 1000;
   params->video_gop_seconds  = 2.0f;
   params->video_max_b_frames = 0;

   /* CBR instead of CRF. nal-hrd=cbr makes x264 pad to a true CBR
    * stream, which is what the ingest servers expect. */
   av_dict_set(&params->video_opts, "crf", NULL, 0);
   av_dict_set(&params->video_opts, "nal-hrd", "cbr", 0);
   /* No extra keyframes on scene cuts: ingest wants a fixed cadence.
    * (keyint_min can't do this; x264 clamps it to keyint/2+1.) */
   av_dict_set(&params->video_opts, "x264-params", "scenecut=0", 0);

   params->audio_qscale       = false;
   params->audio_bit_rate     = 160000;
   params->sample_rate        = 48000;
   av_dict_set(&params->audio_opts, "audio_global_quality", NULL, 0);
}

static bool ffmpeg_init_config_common(struct ff_config_param *params,
      unsigned preset,
      bool video_gpu_record,
      unsigned video_record_scale_factor,
      unsigned video_stream_scale_factor,
      unsigned streaming_mode,
      unsigned video_record_threads)
{
   params->video_max_b_frames = -1;

   switch (preset)
   {
      case RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY:
      case RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY:
         params->threads              = video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 75;
         params->out_pix_fmt          = AV_PIX_FMT_YUV420P;

         strlcpy_lit(params->vcodec, "libx264", sizeof(params->vcodec));
         strlcpy_lit(params->acodec, "aac", sizeof(params->acodec));

         params->video_hw_bpp         = 0.05f;
         av_dict_set(&params->video_opts, "preset", "ultrafast", 0);
         av_dict_set(&params->video_opts, "tune", "film", 0);
         av_dict_set(&params->video_opts, "crf", "35", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "75", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY:
      case RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY:
         params->threads              = video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 75;
         params->out_pix_fmt          = AV_PIX_FMT_YUV420P;

         strlcpy_lit(params->vcodec, "libx264", sizeof(params->vcodec));
         strlcpy_lit(params->acodec, "aac", sizeof(params->acodec));

         params->video_hw_bpp         = 0.1f;
         av_dict_set(&params->video_opts, "preset", "superfast", 0);
         av_dict_set(&params->video_opts, "tune", "film", 0);
         av_dict_set(&params->video_opts, "crf", "25", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "75", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY:
      case RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY:
         params->threads              = video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 100;
         params->out_pix_fmt          = AV_PIX_FMT_YUV420P;

         strlcpy_lit(params->vcodec, "libx264", sizeof(params->vcodec));
         strlcpy_lit(params->acodec, "aac", sizeof(params->acodec));

         params->video_hw_bpp         = 0.2f;
         av_dict_set(&params->video_opts, "preset", "superfast", 0);
         av_dict_set(&params->video_opts, "tune", "film", 0);
         av_dict_set(&params->video_opts, "crf", "15", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "100", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY:
         params->threads              = video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 80;
         params->out_pix_fmt          = AV_PIX_FMT_BGR24;

         strlcpy_lit(params->vcodec, "libx264rgb", sizeof(params->vcodec));
         strlcpy_lit(params->acodec, "flac", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "qp", "0", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "100", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST:
         params->threads              = video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 50;
         params->out_pix_fmt          = AV_PIX_FMT_YUV420P;

         strlcpy_lit(params->vcodec, "libvpx", sizeof(params->vcodec));
         strlcpy_lit(params->acodec, "libopus", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "deadline", "realtime", 0);
         av_dict_set(&params->video_opts, "crf", "14", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "50", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY:
         params->threads              = video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 75;
         params->out_pix_fmt          = AV_PIX_FMT_YUV420P;

         strlcpy_lit(params->vcodec, "libvpx", sizeof(params->vcodec));
         strlcpy_lit(params->acodec, "libopus", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "deadline", "realtime", 0);
         av_dict_set(&params->video_opts, "crf", "4", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "75", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_GIF:
         params->threads              = video_record_threads;
         params->frame_drop_ratio     = 4;
         params->audio_enable         = false;
         params->audio_global_quality = 0;
         params->out_pix_fmt          = AV_PIX_FMT_RGB8;

         strlcpy_lit(params->vcodec, "gif", sizeof(params->vcodec));
         strlcpy_lit(params->acodec, "", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "framerate", "30", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "0", 0);
         break;
      case RECORD_CONFIG_TYPE_RECORDING_APNG:
         params->threads              = video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = false;
         params->audio_global_quality = 0;
         params->out_pix_fmt          = AV_PIX_FMT_RGB24;

         strlcpy_lit(params->vcodec, "apng", sizeof(params->vcodec));
         strlcpy_lit(params->acodec, "", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "pred", "avg", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "0", 0);
         break;
      case RECORD_CONFIG_TYPE_STREAMING_NETPLAY:
         params->threads              = video_record_threads;
         params->frame_drop_ratio     = 1;
         params->audio_enable         = true;
         params->audio_global_quality = 50;
         params->out_pix_fmt          = AV_PIX_FMT_YUV420P;

         strlcpy_lit(params->vcodec, "libx264", sizeof(params->vcodec));
         strlcpy_lit(params->acodec, "aac", sizeof(params->acodec));

         av_dict_set(&params->video_opts, "preset", "ultrafast", 0);
         av_dict_set(&params->video_opts, "tune", "zerolatency", 0);
         av_dict_set(&params->video_opts, "crf", "20", 0);
         av_dict_set(&params->audio_opts, "audio_global_quality", "50", 0);

         /* TO-DO: detect if hwaccel is available and use it instead of the preset above
            strlcpy_lit(params->vcodec, "h264_nvenc", sizeof(params->vcodec));
            strlcpy_lit(params->acodec, "aac", sizeof(params->acodec));

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
      if (!video_gpu_record)
         params->scale_factor = (video_record_scale_factor > 0) ?
            video_record_scale_factor : 1;
      else
         params->scale_factor = 1;
      strlcpy_lit(params->format, "matroska", sizeof(params->format));
   }
   else if (preset >= RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST && preset < RECORD_CONFIG_TYPE_RECORDING_GIF)
   {
      if (!video_gpu_record)
         params->scale_factor = (video_record_scale_factor > 0) ?
            video_record_scale_factor : 1;
      else
         params->scale_factor = 1;
      strlcpy_lit(params->format, "webm", sizeof(params->format));
   }
   else if (preset >= RECORD_CONFIG_TYPE_RECORDING_GIF && preset < RECORD_CONFIG_TYPE_RECORDING_APNG)
   {
      if (!video_gpu_record)
         params->scale_factor = (video_record_scale_factor > 0) ?
            video_record_scale_factor : 1;
      else
         params->scale_factor = 1;
      strlcpy_lit(params->format, "gif", sizeof(params->format));
   }
   else if (preset < RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY)
   {
      params->scale_factor = 1;
      strlcpy_lit(params->format, "apng", sizeof(params->format));
   }
   else if (preset <= RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY)
   {
      if (!video_gpu_record)
         params->scale_factor = (video_stream_scale_factor > 0) ?
            video_stream_scale_factor : 1;
      else
         params->scale_factor = 1;
      if (     streaming_mode == STREAMING_MODE_YOUTUBE
            || streaming_mode == STREAMING_MODE_TWITCH
            || streaming_mode == STREAMING_MODE_FACEBOOK
            || streaming_mode == STREAMING_MODE_KICK)
      {
         strlcpy_lit(params->format, "flv", sizeof(params->format));
         ffmpeg_config_rtmp(params, preset);
      }
      else
         strlcpy_lit(params->format, "mpegts", sizeof(params->format));
   }
   else if (preset == RECORD_CONFIG_TYPE_STREAMING_NETPLAY)
   {
      params->scale_factor = 1;
      strlcpy_lit(params->format, "mpegts", sizeof(params->format));
   }

   return true;
}

static bool ffmpeg_init_config(struct ff_config_param *params,
      const char *config)
{
   struct config_file_entry entry;
   char pix_fmt[64]         = {0};

   params->out_pix_fmt      = AV_PIX_FMT_NONE;
   params->scale_factor     = 1;
   params->threads          = 1;
   params->frame_drop_ratio = 1;
   params->audio_enable     = true;
   params->video_max_b_frames = -1;

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
      if (params->out_pix_fmt == AV_PIX_FMT_NONE)
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
#if !FFMPEG3
   size_t _len;
#endif
   ctx                    = avformat_alloc_context();
   handle->muxer.ctx      = ctx;
   /* NULL-check ctx: ffmpeg_free cleans up handle->muxer.ctx via
    * avformat_free_context which NULL-tolerates (see ffmpeg
    * docs).  ctx->* writes below NULL-deref on OOM. */
   if (!ctx)
      return false;
#if !FFMPEG3
   _len                   = MIN(strlen(handle->params.filename) + 1, PATH_MAX_LENGTH);
   ctx->url               = (char*)av_malloc(_len);
   /* NULL-check ctx->url: strlcpy on the next line NULL-derefs
    * on OOM.  ctx->url is freed by avformat_free_context as
    * part of the muxer teardown in ffmpeg_free. */
   if (!ctx->url)
      return false;
   strlcpy(ctx->url, handle->params.filename, _len);
#else
   strlcpy(ctx->filename, handle->params.filename, sizeof(ctx->filename));
#endif

   if (*handle->config.format)
      ctx->oformat = av_guess_format(handle->config.format, NULL, NULL);
   else
#if !FFMPEG3
      ctx->oformat = av_guess_format(NULL, ctx->url, NULL);
#else
   ctx->oformat = av_guess_format(NULL, ctx->filename, NULL);
#endif

   if (!ctx->oformat)
      return false;

#if !FFMPEG3
   if (avio_open(&ctx->pb, ctx->url, AVIO_FLAG_WRITE) < 0)
#else
   if (avio_open(&ctx->pb, ctx->filename, AVIO_FLAG_WRITE) < 0)
#endif
      return false;

   return true;
}

static bool ffmpeg_init_muxer_post(ffmpeg_t *handle)
{
   AVStream *stream = avformat_new_stream(handle->muxer.ctx,
         handle->video.encoder);

   avcodec_parameters_from_context(stream->codecpar, handle->video.codec);
   stream->time_base                          = handle->video.codec->time_base;
   handle->muxer.vstream                      = stream;
   handle->muxer.vstream->sample_aspect_ratio =
      handle->video.codec->sample_aspect_ratio;

   if (handle->config.audio_enable)
   {
      stream = avformat_new_stream(handle->muxer.ctx,
            handle->audio.encoder);
      avcodec_parameters_from_context(stream->codecpar, handle->audio.codec);
      stream->time_base     = handle->audio.codec->time_base;
      handle->muxer.astream = stream;
   }

   av_dict_set(&handle->muxer.ctx->metadata, "title",
         "RetroArch Video Dump", 0);

   return avformat_write_header(handle->muxer.ctx, NULL) >= 0;
}

/* Audio queue depth in seconds of input audio. */
#define AUDIO_FIFO_SECONDS         2
/* Video queue depth, from video_record_fifo_frames. */
#define DEFAULT_VIDEO_FIFO_FRAMES 32
#define MIN_VIDEO_FIFO_FRAMES      8
#define MAX_VIDEO_FIFO_FRAMES    128
/* One warning per this many newly dropped frames. */
#define DROP_LOG_INTERVAL         64

static void ffmpeg_thread(void *data);

/* Encoder threads run this much nicer than the core, in drop mode
 * only: with blocking on, a starved encoder just stalls the game. */
#define ENCODER_NICE_DELTA 10
#define MAX_TRACKED_TIDS   512

#ifdef FFMPEG_HAVE_THREAD_NICE
struct ff_tid_set
{
   pid_t tids[MAX_TRACKED_TIDS];
   unsigned count;
};

static void ffmpeg_tids_snapshot(struct ff_tid_set *set)
{
   struct dirent *ent;
   DIR *dir   = opendir("/proc/self/task");

   set->count = 0;
   if (!dir)
      return;
   while ((ent = readdir(dir)) && set->count < MAX_TRACKED_TIDS)
   {
      long tid = strtol(ent->d_name, NULL, 10);
      if (tid > 0)
         set->tids[set->count++] = (pid_t)tid;
   }
   closedir(dir);
}

static void ffmpeg_renice_tid(pid_t tid, int delta)
{
   int cur;

   errno = 0;
   cur   = getpriority(PRIO_PROCESS, (id_t)tid);
   if (cur == -1 && errno)
      return;
   /* Raising nice never needs privileges. */
   setpriority(PRIO_PROCESS, (id_t)tid, cur + delta);
}

/* Renice threads an encoder spawned during avcodec_open2(). They
 * inherit the main thread's priority, and lowering that around the
 * open can't be undone without CAP_SYS_NICE. */
static unsigned ffmpeg_renice_new_threads(const struct ff_tid_set *before)
{
   unsigned i, j, n = 0;
   struct ff_tid_set *after = (struct ff_tid_set*)malloc(sizeof(*after));

   if (!after)
      return 0;
   ffmpeg_tids_snapshot(after);
   for (i = 0; i < after->count; i++)
   {
      bool seen = false;
      for (j = 0; j < before->count; j++)
      {
         if (after->tids[i] == before->tids[j])
         {
            seen = true;
            break;
         }
      }
      if (!seen)
      {
         ffmpeg_renice_tid(after->tids[i], ENCODER_NICE_DELTA);
         n++;
      }
   }
   free(after);
   return n;
}
#endif

static bool init_thread(ffmpeg_t *handle)
{
   unsigned vf = handle->video_fifo_frames;
   double rate = handle->params.samplerate > 0.0
         ? handle->params.samplerate : 48000.0;

   if (vf < MIN_VIDEO_FIFO_FRAMES || vf > MAX_VIDEO_FIFO_FRAMES)
      vf = DEFAULT_VIDEO_FIFO_FRAMES;
   handle->video_fifo_frames = vf;

   handle->data_init  = retro_eventcount_init(&handle->data);
   handle->space_init = retro_eventcount_init(&handle->space);
   if (!handle->data_init || !handle->space_init)
   {
      if (handle->data_init)
         retro_eventcount_free(&handle->data);
      if (handle->space_init)
         retro_eventcount_free(&handle->space);
      handle->data_init = handle->space_init = false;
      return false;
   }
   /* fifo_new() was never checked; a ring that fails to init fails
    * the recorder now.  (retro_spsc rounds each capacity up to a
    * power of two, so they hold at least what the fifos did; the
    * room checks in the push paths bound both attr and video, so a
    * larger attr ring cannot admit a frame the video ring lacks
    * room for.) */
   handle->fifos_init =
         retro_spsc_init(&handle->audio_fifo, (size_t)(rate * AUDIO_FIFO_SECONDS)
               * handle->params.channels * sizeof(int16_t))
      && retro_spsc_init(&handle->attr_fifo, sizeof(struct ff_video_attr) * vf)
      && retro_spsc_init(&handle->video_fifo,
               (size_t)VIDEO_SCALE_AREA(handle->params.fb_dims) *
               handle->video.pix_size * vf);
   if (!handle->fifos_init)
   {
      RARCH_ERR("[FFmpeg] Failed to allocate recording queues"
            " (%u video frames).\n", vf);
      retro_spsc_free(&handle->audio_fifo);
      retro_spsc_free(&handle->attr_fifo);
      retro_spsc_free(&handle->video_fifo);
      retro_eventcount_free(&handle->data);
      retro_eventcount_free(&handle->space);
      handle->data_init = handle->space_init = false;
      return false;
   }

   retro_atomic_store_release_int(&handle->alive, 1);
   handle->thread    = sthread_create(ffmpeg_thread, handle);
   if (!handle->thread)
   {
      RARCH_ERR("[FFmpeg] Failed to create encoder thread.\n");
      retro_atomic_store_release_int(&handle->alive, 0);
      retro_eventcount_free(&handle->data);
      retro_eventcount_free(&handle->space);
      handle->data_init = handle->space_init = false;
      return false;
   }

   RARCH_LOG("[FFmpeg] Video queue: %u frames (%.1f MiB), drop-on-full: %s.\n",
         vf, ((double)VIDEO_SCALE_AREA(handle->params.fb_dims)
            * handle->video.pix_size * vf) / (1024.0 * 1024.0),
         handle->allow_frame_drop ? "yes" : "no");

   return true;
}

static void deinit_thread(ffmpeg_t *handle)
{
   if (!handle->thread)
      return;

   retro_atomic_store_release_int(&handle->alive, 0);
   retro_eventcount_notify(&handle->data);
   retro_eventcount_notify(&handle->space);
   sthread_join(handle->thread);

   retro_eventcount_free(&handle->data);
   retro_eventcount_free(&handle->space);
   handle->data_init = handle->space_init = false;

   handle->thread = NULL;
}

static void deinit_thread_buf(ffmpeg_t *handle)
{
   if (!handle->fifos_init)
      return;
   retro_spsc_free(&handle->audio_fifo);
   retro_spsc_free(&handle->attr_fifo);
   retro_spsc_free(&handle->video_fifo);
   handle->fifos_init = false;
}

static void ffmpeg_free(void *data)
{
   ffmpeg_t *handle = (ffmpeg_t*)data;
   if (!handle)
      return;

   deinit_thread(handle);
   deinit_thread_buf(handle);

   if (handle->video_frames_in > 0)
      RARCH_LOG("[FFmpeg] Recording ended: %llu video frames in,"
            " %llu dropped (%.2f%%).\n",
            (unsigned long long)handle->video_frames_in,
            (unsigned long long)handle->video_frames_dropped,
            100.0 * (double)handle->video_frames_dropped
                  / (double)handle->video_frames_in);
   if (handle->audio_bytes_dropped && handle->params.samplerate > 0.0)
      RARCH_LOG("[FFmpeg] Recording ended: %.2f s of audio replaced"
            " with silence.\n",
            (double)handle->audio_bytes_dropped
            / (handle->params.samplerate
               * handle->params.channels * sizeof(int16_t)));

   if (handle->audio.codec)
   {
#if FFMPEG8
      avcodec_free_context(&handle->audio.codec);
#else
      avcodec_close(handle->audio.codec);
#endif
      av_free(handle->audio.codec);
   }

   av_free(handle->audio.buffer);
   av_frame_free(&handle->audio.frame);

   if (handle->video.codec)
   {
#if FFMPEG8
      avcodec_free_context(&handle->video.codec);
#else
      avcodec_close(handle->video.codec);
#endif
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
   if (handle->audio.resampler)
   {
      unsigned i;
      for (i = 0; i < 3; i++)
         if (handle->audio.resampler_pair[i])
            handle->audio.resampler->free(handle->audio.resampler_pair[i]);
   }
   memset(handle->audio.resampler_pair, 0, sizeof(handle->audio.resampler_pair));
   av_freep(&handle->audio.pair_in);
   av_freep(&handle->audio.pair_out);
   handle->audio.pair_frames    = 0;
   handle->audio.resampler      = NULL;
   handle->audio.resampler_data = NULL;

   if (handle->audio.resampler_int16)
      sinc_resampler_int16_free(handle->audio.resampler_int16);
   handle->audio.resampler_int16 = NULL;

   av_free(handle->audio.float_conv);
   av_free(handle->audio.resample_out);
   av_free(handle->audio.fixed_conv);
   av_free(handle->audio.planar_buf);
   /* The muxer context owns its streams, their codec parameters, its
    * metadata and its url; releasing just the struct left the rest
    * behind on every session. */
   avformat_free_context(handle->muxer.ctx);
   av_packet_free(&handle->pkt);

   free(handle);

#if FFMPEG3
   avformat_network_deinit();
#endif
}

static void *ffmpeg_new(const struct record_params *params)
{
   ffmpeg_t *handle     = (ffmpeg_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

#if FFMPEG3
   av_register_all();
   avformat_network_init();
#endif

   handle->params            = *params;
   handle->pkt               = av_packet_alloc();
   handle->allow_frame_drop  = params->allow_frame_drop;
   handle->video_fifo_frames = params->video_fifo_frames;

   switch (params->preset)
   {
      case RECORD_CONFIG_TYPE_RECORDING_CUSTOM:
      case RECORD_CONFIG_TYPE_STREAMING_CUSTOM:
         if (!ffmpeg_init_config(
                  &handle->config,
                  params->config))
            goto error;
         break;
      default:
         ffmpeg_init_config_common(
               &handle->config,
               params->preset,
               params->video_gpu_record,
               params->video_record_scale_factor,
               params->video_stream_scale_factor,
               params->streaming_mode,
               params->video_record_threads
               );
         break;
   }

   if (!ffmpeg_init_muxer_pre(handle))
      goto error;

   {
      bool ok;
#ifdef FFMPEG_HAVE_THREAD_NICE
      struct ff_tid_set *before = NULL;
      if (handle->allow_frame_drop
            && (before = (struct ff_tid_set*)malloc(sizeof(*before))))
         ffmpeg_tids_snapshot(before);
#endif
      ok = ffmpeg_init_video(handle)
         && (  !handle->config.audio_enable
             || ffmpeg_init_audio(handle, params->audio_resampler));
#ifdef FFMPEG_HAVE_THREAD_NICE
      if (before)
      {
         if (ok)
         {
            unsigned n = ffmpeg_renice_new_threads(before);
            if (n)
               RARCH_LOG("[FFmpeg] Lowered priority of %u encoder"
                     " thread(s).\n", n);
         }
         free(before);
      }
#endif
      if (!ok)
         goto error;
   }

   if (!ffmpeg_init_muxer_post(handle))
      goto error;

   if (!init_thread(handle))
      goto error;

   return handle;

error:
   ffmpeg_free(handle);
   return NULL;
}

static bool ffmpeg_video_fifo_has_room(ffmpeg_t *handle, size_t video_bytes)
{
   return retro_spsc_write_avail(&handle->attr_fifo)
            >= sizeof(struct ff_video_attr)
       && retro_spsc_write_avail(&handle->video_fifo) >= video_bytes;
}

static void ffmpeg_report_drops(uint64_t dropped)
{
   char msg[128];
   size_t _len = snprintf(msg, sizeof(msg),
         "Recording: %llu frames dropped (encoder too slow)",
         (unsigned long long)dropped);
   RARCH_WARN("[FFmpeg] %s.\n", msg);
   runloop_msg_queue_push(msg, _len, 1, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
}

static bool ffmpeg_push_video(void *data,
      const struct record_video_data *vid)
{
   unsigned y;
   unsigned rows;
   size_t video_bytes;
   struct ff_video_attr attr;
   bool drop_frame  = false;
   ffmpeg_t *handle = (ffmpeg_t*)data;
   int       offset = 0;

   if (!handle || !vid)
      return false;

   drop_frame       = handle->video.frame_drop_count++ %
      handle->video.frame_drop_ratio;

   handle->video.frame_drop_count %= handle->video.frame_drop_ratio;

   if (drop_frame)
      return true;

   if (!retro_atomic_load_acquire_int(&handle->alive))
      return false;

   /* Tightly pack our frame to conserve memory.
    * libretro tends to use a very large pitch.
    */
   attr.vid      = *vid;
   attr.pts_skip = 0;

   if (attr.vid.is_dupe)
   {
      attr.vid.dims  = 0;
      attr.vid.pitch = 0;
   }
   else
      attr.vid.pitch = (int)(VIDEO_SCALE_W(attr.vid.dims)
            * handle->video.pix_size);

   rows        = VIDEO_SCALE_H(attr.vid.dims);
   video_bytes = (size_t)rows * attr.vid.pitch;

   handle->video_frames_in++;

   /* Drop mode: a full queue costs this frame, never the frontend. */
   if (     handle->allow_frame_drop
         && !ffmpeg_video_fifo_has_room(handle, video_bytes))
   {
      handle->video_frames_dropped++;
      handle->video_pts_pending++;
      if (handle->video_frames_dropped - handle->video_frames_logged
            >= DROP_LOG_INTERVAL)
      {
         handle->video_frames_logged = handle->video_frames_dropped;
         ffmpeg_report_drops(handle->video_frames_dropped);
      }
      return true;
   }

   for (;;)
   {
      int key;
      if (!retro_atomic_load_acquire_int(&handle->alive))
         return false;

      if (ffmpeg_video_fifo_has_room(handle, video_bytes))
         break;

      key = retro_eventcount_prepare_wait(&handle->space);
      if (     !retro_atomic_load_acquire_int(&handle->alive)
            || ffmpeg_video_fifo_has_room(handle, video_bytes))
         retro_eventcount_cancel_wait(&handle->space);
      else
         retro_eventcount_commit_wait_timeout(&handle->space, key,
               FFMPEG_PUSH_WAIT_US);
   }

   attr.pts_skip             = handle->video_pts_pending;
   handle->video_pts_pending = 0;

   /* Frame first, attr last: the encoder takes the attr as the
    * signal that a whole frame is behind it, and the ring's
    * release/acquire on each write orders the rows before it. */
   for (y = 0; y < rows; y++, offset += vid->pitch)
      retro_spsc_write(&handle->video_fifo,
            (const uint8_t*)vid->data + offset, attr.vid.pitch);

   retro_spsc_write(&handle->attr_fifo, &attr, sizeof(attr));
   retro_eventcount_notify(&handle->data);

   return true;
}

/* Writes as much owed silence as fits, in whole frames. */
static void ffmpeg_audio_write_silence(ffmpeg_t *handle)
{
   static const int16_t zeros[1024] = {0};
   size_t avail = retro_spsc_write_avail(&handle->audio_fifo);
   size_t todo  = handle->audio_silence_pending;
   size_t frame = handle->params.channels * sizeof(int16_t);

   if (todo > avail)
      todo = avail - (avail % frame);

   handle->audio_silence_pending -= todo;
   while (todo)
   {
      size_t chunk = todo < sizeof(zeros) ? todo : sizeof(zeros);
      retro_spsc_write(&handle->audio_fifo, zeros, chunk);
      todo -= chunk;
   }
}

static bool ffmpeg_push_audio(void *data,
      const struct record_audio_data *audio_data)
{
   ffmpeg_t *handle = (ffmpeg_t*)data;
   size_t need;

   if (!handle || !audio_data)
      return false;

   if (!handle->config.audio_enable)
      return true;

   need = audio_data->frames * handle->params.channels * sizeof(int16_t);

   if (handle->allow_frame_drop)
   {
      if (!retro_atomic_load_acquire_int(&handle->alive))
         return false;
      /* A chunk that doesn't fit is owed as silence, not waited for. */
      if (handle->audio_silence_pending)
         ffmpeg_audio_write_silence(handle);
      if (     !handle->audio_silence_pending
            && retro_spsc_write_avail(&handle->audio_fifo) >= need)
         retro_spsc_write(&handle->audio_fifo, audio_data->data, need);
      else
      {
         handle->audio_silence_pending += need;
         handle->audio_bytes_dropped   += need;
      }
      retro_eventcount_notify(&handle->data);
      return true;
   }

   for (;;)
   {
      int key;
      if (!retro_atomic_load_acquire_int(&handle->alive))
         return false;

      if (retro_spsc_write_avail(&handle->audio_fifo) >= need)
         break;

      key = retro_eventcount_prepare_wait(&handle->space);
      if (     !retro_atomic_load_acquire_int(&handle->alive)
            || retro_spsc_write_avail(&handle->audio_fifo) >= need)
         retro_eventcount_cancel_wait(&handle->space);
      else
         retro_eventcount_commit_wait_timeout(&handle->space, key,
               FFMPEG_PUSH_WAIT_US);
   }

   retro_spsc_write(&handle->audio_fifo, audio_data->data, need);
   retro_eventcount_notify(&handle->data);

   return true;
}

static bool encode_video(ffmpeg_t *handle, AVFrame *frame)
{
   int ret;
   /* avcodec_receive_packet() supplies the packet's own buffer; nothing
    * of ours is preset on it. */
   AVPacket *pkt = handle->pkt;

   ret = avcodec_send_frame(handle->video.codec, frame);
   if (ret < 0)
   {
      char msg[AV_ERROR_MAX_STRING_SIZE];
      av_make_error_string(msg, AV_ERROR_MAX_STRING_SIZE, ret);
      RARCH_ERR("[FFmpeg] Cannot send video frame. Error code: %s.\n", msg);
      return false;
   }

   while (ret >= 0)
   {
      ret = avcodec_receive_packet(handle->video.codec, pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
         break;
      else if (ret < 0)
      {
         char msg[AV_ERROR_MAX_STRING_SIZE];
         av_make_error_string(msg, AV_ERROR_MAX_STRING_SIZE, ret);
         RARCH_ERR("[FFmpeg] Cannot receive video packet. Error code: %s.\n", msg);
         return false;
      }

      pkt->pts = av_rescale_q(pkt->pts,
            handle->video.codec->time_base,
            handle->muxer.vstream->time_base);

      pkt->dts = av_rescale_q(pkt->dts,
            handle->video.codec->time_base,
            handle->muxer.vstream->time_base);

      pkt->stream_index = handle->muxer.vstream->index;

      ret = av_interleaved_write_frame(handle->muxer.ctx, pkt);
      if (ret < 0)
      {
         char msg[AV_ERROR_MAX_STRING_SIZE];
         av_make_error_string(msg, AV_ERROR_MAX_STRING_SIZE, ret);
         RARCH_ERR("[FFmpeg] Cannot write video packet to output file. Error code: %s.\n", msg);
         return false;
      }

      av_packet_unref(pkt);
   }
   return true;
}

static void ffmpeg_scale_input(ffmpeg_t *handle,
      const struct record_video_data *vid)
{
   unsigned src_w = VIDEO_SCALE_W(vid->dims);
   unsigned src_h = VIDEO_SCALE_H(vid->dims);
   /* When output was padded to even dimensions, clamp the scaling
    * destination to the source size. */
   unsigned out_w = VIDEO_SCALE_W(handle->params.out_dims);
   unsigned out_h = VIDEO_SCALE_H(handle->params.out_dims);
   unsigned dst_w = (src_w < out_w) ? src_w : out_w;
   unsigned dst_h = (src_h < out_h) ? src_h : out_h;

   /* Attempt to preserve more information if we scale down. */
   bool shrunk = dst_w < src_w || dst_h < src_h;

   if (handle->video.use_sws)
   {
      int linesize      = vid->pitch;

      handle->video.sws = sws_getCachedContext(handle->video.sws,
            src_w, src_h, handle->video.in_pix_fmt,
            dst_w, dst_h,
            handle->video.pix_fmt,
            shrunk ? SWS_BILINEAR : SWS_POINT, NULL, NULL, NULL);

      sws_scale(handle->video.sws, (const uint8_t* const*)&vid->data,
            &linesize, 0, src_h, handle->video.conv_frame->data,
            handle->video.conv_frame->linesize);
   }
   else
      video_frame_record_scale(
            &handle->video.scaler,
            handle->video.conv_frame->data[0],
            vid->data,
            dst_w,
            dst_h,
            handle->video.conv_frame->linesize[0],
            src_w,
            src_h,
            vid->pitch,
            shrunk);
}

static bool ffmpeg_push_video_thread(ffmpeg_t *handle,
      const struct record_video_data *vid, unsigned pts_skip)
{
   if (!vid->is_dupe)
      ffmpeg_scale_input(handle, vid);

   /* Leave a gap for dropped frames so A/V stays aligned. */
   handle->video.frame_cnt      += pts_skip;

   handle->video.conv_frame->pts = handle->video.frame_cnt;

   /* gop_size counts frames, so pts gaps (drops, cores presenting
    * every other tick) stretch it in time; force keyframes on time. */
   handle->video.conv_frame->pict_type = AV_PICTURE_TYPE_NONE;
   if (handle->config.video_gop_seconds > 0.0f)
   {
      int64_t ticks = (int64_t)(handle->config.video_gop_seconds
            * handle->params.fps / handle->video.frame_drop_ratio + 0.5);
      if (ticks < 1)
         ticks = 1;
      if (handle->video.frame_cnt - handle->video_last_key_pts >= ticks)
      {
         handle->video.conv_frame->pict_type = AV_PICTURE_TYPE_I;
         handle->video_last_key_pts          = handle->video.frame_cnt;
      }
   }

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
      /* realloc-to-tmp: av_realloc follows C realloc contract and
       * leaves the old buffer intact on failure.  Pre-patch self-
       * assigned planar_buf to the return, which on OOM would
       * leak the old buffer AND leave planar_buf_frames (and
       * subsequently the 'frames_in_buffer > planar_buf_frames'
       * guard above) claiming the old size.  On next call with
       * the same or smaller frame count the guard would skip the
       * realloc branch and pass a now-NULL planar_buf to
       * planarize_float/s16 below, NULL-derefing. */
      void *new_buf = av_realloc(handle->audio.planar_buf,
            handle->audio.frames_in_buffer * handle->params.channels *
            handle->audio.sample_size);
      if (!new_buf)
         return;
      handle->audio.planar_buf = new_buf;

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
   int ret;
   AVFrame *frame = handle->audio.frame;
   int samples_size;
   AVPacket *pkt = handle->pkt;

   if (!dry)
   {
#if HAVE_CH_LAYOUT
      int nb_channels = handle->audio.codec->ch_layout.nb_channels;
#else
      int nb_channels = handle->audio.codec->channels;
#endif
      frame->nb_samples     = (int)handle->audio.frames_in_buffer;
      frame->pts            = handle->audio.frame_cnt;

      planarize_audio(handle);

      samples_size          = av_samples_get_buffer_size(
            NULL,
            nb_channels,
            (int)handle->audio.frames_in_buffer,
            handle->audio.codec->sample_fmt, 0);

      /* The frame wraps the handle's own sample buffer; the encoder
       * copies out of a frame that does not own its data. */
      avcodec_fill_audio_frame(frame,
            nb_channels,
            handle->audio.codec->sample_fmt,
            handle->audio.is_planar
            ? (uint8_t*)handle->audio.planar_buf :
            handle->audio.buffer,
            samples_size, 0);
   }

   ret = avcodec_send_frame(handle->audio.codec, dry ? NULL : frame);
   if (ret < 0)
   {
      char msg[AV_ERROR_MAX_STRING_SIZE];

      av_make_error_string(msg, AV_ERROR_MAX_STRING_SIZE, ret);
      RARCH_ERR("[FFmpeg] Cannot send audio frame. Return code: %s.\n", msg);
      return false;
   }

   while (ret >= 0)
   {
      ret = avcodec_receive_packet(handle->audio.codec, pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
         break;
      else if (ret < 0)
      {
         char msg[AV_ERROR_MAX_STRING_SIZE];

         av_make_error_string(msg, AV_ERROR_MAX_STRING_SIZE, ret);
         RARCH_ERR("[FFmpeg] Cannot receive audio packet. Return code: %s.\n", msg);
         return false;
      }

      pkt->pts = av_rescale_q(pkt->pts,
            handle->audio.codec->time_base,
            handle->muxer.astream->time_base);

      pkt->dts = av_rescale_q(pkt->dts,
            handle->audio.codec->time_base,
            handle->muxer.astream->time_base);

      pkt->stream_index = handle->muxer.astream->index;

      ret = av_interleaved_write_frame(handle->muxer.ctx, pkt);
      if (ret < 0)
      {
         char msg[AV_ERROR_MAX_STRING_SIZE];

         av_make_error_string(msg, AV_ERROR_MAX_STRING_SIZE, ret);
         RARCH_ERR("[FFmpeg] Cannot write video packet to output file. Error code: %s.\n", msg);
         return false;
      }

      av_packet_unref(pkt);
   }

   return true;
}

static void ffmpeg_audio_resample(ffmpeg_t *handle,
      struct record_audio_data *aud)
{
   /* Integer path: s16 encoder with an integer resampler.  Resample
    * s16 -> s16 with no float detour.  The output buffer is fixed_conv
    * (reused from the float path's final-stage buffer). */
   if (handle->audio.resampler_int16)
   {
      struct resampler_data_int16 info;
      size_t needed_out = (size_t)(aud->frames * handle->audio.ratio) + 16;

      if (needed_out > handle->audio.fixed_conv_frames)
      {
         int16_t *new_fixed = (int16_t*)av_realloc(handle->audio.fixed_conv,
               needed_out * handle->params.channels * sizeof(int16_t));
         if (!new_fixed)
            return;
         handle->audio.fixed_conv        = new_fixed;
         handle->audio.fixed_conv_frames = needed_out;
      }

      info.data_in       = (const int16_t*)aud->data;
      info.data_out      = handle->audio.fixed_conv;
      info.input_frames  = aud->frames;
      info.output_frames = 0;
      info.ratio         = handle->audio.ratio;

      sinc_resampler_int16_process(handle->audio.resampler_int16, &info);

      aud->data   = handle->audio.fixed_conv;
      aud->frames = info.output_frames;
      return;
   }

   if (!handle->audio.use_float && !handle->audio.resampler)
      return;

   if (aud->frames > handle->audio.float_conv_frames)
   {
      /* Three stacked realloc-assign-self patterns pre-patch, all
       * leaked on OOM and all left the associated *_frames
       * counter out of sync with the actual allocation.  Worst
       * case: the second or third av_realloc fails after the
       * first (or first two) succeeded, float_conv_frames was
       * already bumped to the new size, and the next call would
       * skip this whole 'if (aud->frames > ..._frames)' block
       * and pass a NULL buffer to the downstream convert/
       * resample calls.
       *
       * Fix: realloc into tmp locals, commit each pointer only
       * after its own alloc succeeds, and defer the
       * float_conv_frames / resample_out_frames /
       * fixed_conv_frames counter bumps until the corresponding
       * allocation has succeeded. */
      float   *new_float_conv;
      float   *new_resample_out;
      int16_t *new_fixed_conv;
      size_t   new_resample_out_frames;
      size_t   new_fixed_conv_frames;

      new_float_conv = (float*)av_realloc(handle->audio.float_conv,
            aud->frames * handle->params.channels * sizeof(float));
      if (!new_float_conv)
         return;
      handle->audio.float_conv        = new_float_conv;
      handle->audio.float_conv_frames = aud->frames;

      /* To make sure we don't accidentally overflow. */
      new_resample_out_frames = aud->frames * handle->audio.ratio + 16;
      new_resample_out        = (float*)
         av_realloc(handle->audio.resample_out,
               new_resample_out_frames *
               handle->params.channels * sizeof(float));
      if (!new_resample_out)
         return;
      handle->audio.resample_out        = new_resample_out;
      handle->audio.resample_out_frames = new_resample_out_frames;

      new_fixed_conv_frames = MAX(
            handle->audio.resample_out_frames,
            handle->audio.float_conv_frames);
      new_fixed_conv        = (int16_t*)av_realloc(
            handle->audio.fixed_conv,
            new_fixed_conv_frames *
            handle->params.channels * sizeof(int16_t));
      if (!new_fixed_conv)
         return;
      handle->audio.fixed_conv        = new_fixed_conv;
      handle->audio.fixed_conv_frames = new_fixed_conv_frames;
   }

   if (handle->audio.use_float || handle->audio.resampler)
   {
      convert_s16_to_float(handle->audio.float_conv,
            (const int16_t*)aud->data, aud->frames
            * handle->params.channels, 1.0);
      aud->data = handle->audio.float_conv;
   }

   if (handle->audio.resampler)
   {
      struct resampler_data info = {0};
      const unsigned ch = handle->params.channels;

      if (ch <= 2)
      {
         info.data_in      = (const float*)aud->data;
         info.data_out     = handle->audio.resample_out;
         info.input_frames = aud->frames;
         info.ratio        = handle->audio.ratio;
         handle->audio.resampler->process(handle->audio.resampler_data, &info);
      }
      else
      {
         /* a wider frame: each pair through its own instance, the
          * last channel of an odd count paired with itself */
         const float *in = (const float*)aud->data;
         unsigned npairs = (ch + 1) / 2, i, c0, c1;
         size_t f, out_frames = 0;
         size_t need_out = (size_t)(aud->frames * handle->audio.ratio) + 16;
         if (aud->frames > handle->audio.pair_frames)
         {
            float *ni = (float*)av_realloc(handle->audio.pair_in, aud->frames * 2 * sizeof(float));
            float *no = (float*)av_realloc(handle->audio.pair_out, need_out * 2 * sizeof(float));
            if (!ni || !no)
               return;
            handle->audio.pair_in     = ni;
            handle->audio.pair_out    = no;
            handle->audio.pair_frames = aud->frames;
         }
         for (i = 0; i < npairs; i++)
         {
            c0 = 2 * i;
            c1 = (2 * i + 1 < ch) ? 2 * i + 1 : 2 * i;
            for (f = 0; f < aud->frames; f++)
            {
               handle->audio.pair_in[2 * f]     = in[f * ch + c0];
               handle->audio.pair_in[2 * f + 1] = in[f * ch + c1];
            }
            memset(&info, 0, sizeof(info));
            info.data_in      = handle->audio.pair_in;
            info.data_out     = handle->audio.pair_out;
            info.input_frames = aud->frames;
            info.ratio        = handle->audio.ratio;
            handle->audio.resampler->process(
                  i == 0 ? handle->audio.resampler_data : handle->audio.resampler_pair[i - 1], &info);
            if (info.output_frames > handle->audio.resample_out_frames)
               info.output_frames = handle->audio.resample_out_frames;
            for (f = 0; f < info.output_frames; f++)
            {
               handle->audio.resample_out[f * ch + c0] = handle->audio.pair_out[2 * f];
               if (c1 != c0)
                  handle->audio.resample_out[f * ch + c1] = handle->audio.pair_out[2 * f + 1];
            }
            out_frames = info.output_frames;
         }
         info.output_frames = out_frames;
      }

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
      size_t can_write       = handle->audio.codec->frame_size -
         handle->audio.frames_in_buffer;
      size_t write_left      = aud->frames - written_frames;
      size_t write_frames    = write_left > can_write ? can_write : write_left;
      size_t write_size      = write_frames *
         handle->params.channels * handle->audio.sample_size;
      size_t bytes_in_buffer = handle->audio.frames_in_buffer *
         handle->params.channels * handle->audio.sample_size;
      size_t written_bytes   = written_frames *
         handle->params.channels * handle->audio.sample_size;

      if (handle->audio.buffer)
         memcpy(handle->audio.buffer + bytes_in_buffer,
               (const uint8_t*)aud->data + written_bytes,
               write_size);

      written_frames                 += write_frames;
      handle->audio.frames_in_buffer += write_frames;

      if ((handle->audio.frames_in_buffer
               < (size_t)handle->audio.codec->frame_size)
            && require_block)
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
   size_t avail = retro_spsc_read_avail(&handle->audio_fifo);

   if (avail)
   {
      struct record_audio_data aud = {0};

      retro_spsc_read(&handle->audio_fifo, audio_buf, avail);

      aud.frames = avail / (sizeof(int16_t) * handle->params.channels);
      aud.data = audio_buf;

      ffmpeg_push_audio_thread(handle, &aud, false);
   }

   encode_audio(handle, true);
}

static void ffmpeg_flush_buffers(ffmpeg_t *handle)
{
   void *audio_buf       = NULL;
   bool did_work         = false;
   void *video_buf       = av_malloc(2 *
         VIDEO_SCALE_AREA(handle->params.fb_dims) * handle->video.pix_size);
   size_t audio_buf_size = handle->config.audio_enable ?
      (handle->audio.codec->frame_size *
       handle->params.channels * sizeof(int16_t)) : 0;

   if (audio_buf_size)
      audio_buf = av_malloc(audio_buf_size);
   /* Try pushing data in an interleaving pattern to
    * ease the work of the muxer a bit. */

   do
   {
      struct ff_video_attr attr_buf;

      did_work = false;

      /* Gate audio-fifo drain on audio_buf non-NULL: audio_buf
       * is an OOM-prone av_malloc above, and fifo_read does
       * memcpy-into-destination which NULL-derefs on a NULL
       * audio_buf.  On OOM we skip audio flush - the fifo
       * retains its data and is freed unread by the caller's
       * subsequent ffmpeg_free teardown. */
      if (handle->config.audio_enable && audio_buf)
      {
         if (retro_spsc_read_avail(&handle->audio_fifo) >= audio_buf_size)
         {
            struct record_audio_data aud = {0};

            retro_spsc_read(&handle->audio_fifo, audio_buf, audio_buf_size);
            aud.frames = handle->audio.codec->frame_size;
            aud.data   = audio_buf;
            ffmpeg_push_audio_thread(handle, &aud, true);

            did_work = true;
         }
      }

      /* Gate video-fifo drain on video_buf non-NULL: same
       * reasoning as the audio branch. */
      if (video_buf && retro_spsc_read_avail(&handle->attr_fifo) >= sizeof(attr_buf))
      {
         retro_spsc_read(&handle->attr_fifo, &attr_buf, sizeof(attr_buf));
         retro_spsc_read(&handle->video_fifo, video_buf,
               VIDEO_SCALE_H(attr_buf.vid.dims) * attr_buf.vid.pitch);
         attr_buf.vid.data = video_buf;
         ffmpeg_push_video_thread(handle, &attr_buf.vid, attr_buf.pts_skip);

         did_work = true;
      }
   }while (did_work);

   /* Pay back silence still owed, so audio runs as long as video. */
   if (handle->config.audio_enable && audio_buf)
   {
      memset(audio_buf, 0, audio_buf_size);
      while (handle->audio_silence_pending >= audio_buf_size)
      {
         struct record_audio_data aud = {0};
         aud.frames = handle->audio.codec->frame_size;
         aud.data   = audio_buf;
         ffmpeg_push_audio_thread(handle, &aud, true);
         handle->audio_silence_pending -= audio_buf_size;
      }
      handle->audio_silence_pending = 0;
   }

   /* Flush out last audio.  Skip on OOM - audio_buf is the
    * destination for ffmpeg_flush_audio's internal fifo_read
    * (via ffmpeg_push_audio_thread) and NULL would NULL-deref. */
   if (handle->config.audio_enable && audio_buf)
      ffmpeg_flush_audio(handle, audio_buf, audio_buf_size);

   /* Flush out last video. */
   encode_video(handle, NULL);

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
   ffmpeg_t *ff          = (ffmpeg_t*)data;
   /* For some reason, FFmpeg has a tendency to crash
    * if we don't overallocate a bit. */
   void *video_buf       = av_malloc(2 *
         VIDEO_SCALE_AREA(ff->params.fb_dims) * ff->video.pix_size);
   size_t audio_buf_size = ff->config.audio_enable ?
      (ff->audio.codec->frame_size * ff->params.channels * sizeof(int16_t)) : 0;
   void *audio_buf       = audio_buf_size ? av_malloc(audio_buf_size) : NULL;

#ifdef FFMPEG_HAVE_THREAD_NICE
   /* Also covers threads an encoder spawns lazily from here. */
   if (ff->allow_frame_drop)
      ffmpeg_renice_tid((pid_t)syscall(SYS_gettid), ENCODER_NICE_DELTA);
#endif

   while (retro_atomic_load_acquire_int(&ff->alive))
   {
      struct ff_video_attr attr_buf;

      bool avail_video = false;
      bool avail_audio = false;

      if (retro_spsc_read_avail(&ff->attr_fifo) >= sizeof(attr_buf))
         avail_video = true;

      if (ff->config.audio_enable)
         if (retro_spsc_read_avail(&ff->audio_fifo) >= audio_buf_size)
            avail_audio = true;

      if (!avail_video && !avail_audio)
      {
         int key = retro_eventcount_prepare_wait(&ff->data);
         if (     !retro_atomic_load_acquire_int(&ff->alive)
               || retro_spsc_read_avail(&ff->attr_fifo) >= sizeof(attr_buf)
               || (  ff->config.audio_enable
                  && retro_spsc_read_avail(&ff->audio_fifo) >= audio_buf_size))
            retro_eventcount_cancel_wait(&ff->data);
         else
            retro_eventcount_commit_wait(&ff->data, key);
         continue;
      }

      if (avail_video && video_buf)
      {
         retro_spsc_read(&ff->attr_fifo, &attr_buf, sizeof(attr_buf));
         retro_spsc_read(&ff->video_fifo, video_buf,
               VIDEO_SCALE_H(attr_buf.vid.dims) * attr_buf.vid.pitch);
         retro_eventcount_notify(&ff->space);

         attr_buf.vid.data = video_buf;
         ffmpeg_push_video_thread(ff, &attr_buf.vid, attr_buf.pts_skip);
      }

      if (avail_audio && audio_buf)
      {
         struct record_audio_data aud = {0};

         retro_spsc_read(&ff->audio_fifo, audio_buf, audio_buf_size);
         retro_eventcount_notify(&ff->space);

         aud.frames = ff->audio.codec->frame_size;
         aud.data   = audio_buf;

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
