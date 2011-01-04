#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
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
   bool enabled;
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
   bool enabled;
   AVCodecContext *codec;

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
};

static int map_audio_codec(ffemu_audio_codec codec)
{
   (void)codec;
   return CODEC_ID_FLAC;
}

static int map_video_codec(ffemu_video_codec codec)
{
   (void)codec;
   return CODEC_ID_H264;
}

static int init_audio(struct audio_info *audio, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder(map_audio_codec(param->acodec));
   if (!codec)
      return -1;

   audio->codec = avcodec_alloc_context();

   avcodec_get_context_defaults(audio->codec);
   audio->codec->global_quality = 100000;
   audio->codec->flags |= CODEC_FLAG_QSCALE;
   audio->codec->sample_rate = param->samplerate;
   audio->codec->time_base = (AVRational) { 1, param->samplerate };
   audio->codec->channels = param->channels;
   audio->codec->sample_fmt = AV_SAMPLE_FMT_S16;
   if (avcodec_open(audio->codec, codec) != 0)
      return -1;


   audio->buffer = av_malloc(audio->codec->frame_size * param->channels * sizeof(int16_t));
   if (!audio->buffer)
      return -1;

   audio->outbuf_size = 50000;
   audio->outbuf = av_malloc(audio->outbuf_size);
   if (!audio->outbuf)
      return -1;

   return 0;
}

static void init_x264_param(AVCodecContext *c)
{
   c->coder_type = 1;  // coder = 1
   c->flags|=CODEC_FLAG_LOOP_FILTER;   // flags=+loop
   c->me_cmp|= 1;  // cmp=+chroma, where CHROMA = 1
   c->partitions|=X264_PART_I8X8+X264_PART_I4X4+X264_PART_P8X8+X264_PART_B8X8; // partitions=+parti8x8+parti4x4+partp8x8+partb8x8
   c->me_method=ME_HEX;    // me_method=hex
   c->me_subpel_quality = 7;   // subq=7
   c->me_range = 16;   // me_range=16
   c->gop_size = 250;  // g=250
   c->keyint_min = 25; // keyint_min=25
   c->scenechange_threshold = 40;  // sc_threshold=40
   c->i_quant_factor = 0.71; // i_qfactor=0.71
   c->b_frame_strategy = 1;  // b_strategy=1
   c->qcompress = 0.6; // qcomp=0.6
   c->qmin = 10;   // qmin=10
   c->qmax = 51;   // qmax=51
   c->max_qdiff = 4;   // qdiff=4
   c->max_b_frames = 3;    // bf=3
   c->refs = 3;    // refs=3
   c->directpred = 1;  // directpred=1
   c->trellis = 1; // trellis=1
   c->flags2|=CODEC_FLAG2_BPYRAMID+CODEC_FLAG2_MIXED_REFS+CODEC_FLAG2_WPRED+CODEC_FLAG2_8X8DCT+CODEC_FLAG2_FASTPSKIP;  // flags2=+bpyramid+mixed_refs+wpred+dct8x8+fastpskip
   c->weighted_p_pred = 2; // wpredp=2

   // libx264-main.ffpreset preset
   c->flags2|=CODEC_FLAG2_8X8DCT;
   c->flags2^=CODEC_FLAG2_8X8DCT;
}

static int init_video(struct video_info *video, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder(map_video_codec(param->vcodec));
   if (!codec)
      return -1;

   video->codec = avcodec_alloc_context();
   video->codec->width = param->out_width;
   video->codec->height = param->out_height;
   video->codec->time_base = (AVRational) {param->fps.den, param->fps.num};
   video->codec->crf = 25;
   video->codec->pix_fmt = PIX_FMT_YUV420P;
   ///// Is this element in all recent ffmpeg versions?
   video->codec->thread_count = 4;
   /////
   video->codec->sample_aspect_ratio = av_d2q(param->aspect_ratio * param->out_height / param->out_width, 255);
   init_x264_param(video->codec);

   if (avcodec_open(video->codec, codec) != 0)
      return -1;

   video->outbuf_size = 1000000;
   video->outbuf = av_malloc(video->outbuf_size);

   int size = avpicture_get_size(PIX_FMT_YUV420P, param->out_width, param->out_height);
   video->conv_frame_buf = av_malloc(size);
   video->conv_frame = avcodec_alloc_frame();
   avpicture_fill((AVPicture*)video->conv_frame, video->conv_frame_buf, PIX_FMT_YUV420P, param->out_width, param->out_height);

   return 0;
}

static int init_muxer(ffemu_t *handle)
{
   AVFormatContext *ctx = avformat_alloc_context();
   av_strlcpy(ctx->filename, handle->params.filename, sizeof(ctx->filename));
   ctx->oformat = av_guess_format(NULL, ctx->filename, NULL);
   if (url_fopen(&ctx->pb, ctx->filename, URL_WRONLY) < 0)
   {
      av_free(ctx);
      return -1;
   }

   int stream_cnt = 0;
   if (handle->video.enabled)
   {
      AVStream *stream = av_new_stream(ctx, stream_cnt++);
      stream->codec = handle->video.codec;

      if (ctx->oformat->flags & AVFMT_GLOBALHEADER)
         handle->video.codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
      handle->muxer.vstream = stream;
      handle->muxer.vstream->sample_aspect_ratio = handle->video.codec->sample_aspect_ratio;
   }

   if (handle->audio.enabled)
   {
      AVStream *stream = av_new_stream(ctx, stream_cnt++);
      stream->codec = handle->audio.codec;

      if (ctx->oformat->flags & AVFMT_GLOBALHEADER)
         handle->audio.codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
      handle->muxer.astream = stream;
   }

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
   if (handle->params.vcodec != FFEMU_VIDEO_NONE)
      handle->video.enabled = true;
   if (handle->params.acodec != FFEMU_AUDIO_NONE)
      handle->audio.enabled = true;

   if (handle->video.enabled)
      if (init_video(&handle->video, &handle->params) < 0)
         goto error;

   if (handle->audio.enabled)
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

int ffemu_push_video(ffemu_t *handle, const struct ffemu_video_data *data)
{
   if (!handle->video.enabled)
      return -1;

   struct SwsContext *conv_ctx = sws_getContext(data->width, data->height, PIX_FMT_RGB555LE,
         handle->params.out_width, handle->params.out_height, PIX_FMT_YUV420P, handle->params.rescaler == FFEMU_RESCALER_LANCZOS ? SWS_LANCZOS : SWS_POINT,
         NULL, NULL, NULL);

   int linesize = data->pitch;

   sws_scale(conv_ctx, (const uint8_t* const*)&data->data, &linesize, 0, handle->params.out_width, handle->video.conv_frame->data, handle->video.conv_frame->linesize);

   int outsize = avcodec_encode_video(handle->video.codec, handle->video.outbuf, handle->video.outbuf_size, handle->video.conv_frame);

   sws_freeContext(conv_ctx);

   AVPacket pkt;
   av_init_packet(&pkt);
   pkt.stream_index = handle->muxer.vstream->index;
   pkt.data = handle->video.outbuf;
   pkt.size = outsize;

   pkt.pts = av_rescale_q(handle->video.frame_cnt++, handle->video.codec->time_base, handle->muxer.vstream->time_base);
   pkt.dts = pkt.pts;
   fprintf(stderr, "Video PTS: %lld\n", (long long)pkt.pts);

   if (handle->video.codec->coded_frame->key_frame)
      pkt.flags |= AV_PKT_FLAG_KEY;

   if (av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
      return -1;

   return 0;
}

int ffemu_push_audio(ffemu_t *handle, const struct ffemu_audio_data *data)
{
   if (!handle->audio.enabled)
      return -1;

   AVPacket pkt;
   av_init_packet(&pkt);
   pkt.stream_index = handle->muxer.astream->index;
   pkt.dts = pkt.pts;
   pkt.data = handle->audio.outbuf;

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

         size_t out_size = avcodec_encode_audio(handle->audio.codec, handle->audio.outbuf, handle->audio.outbuf_size, handle->audio.buffer);
         //fwrite(handle->audio.outbuf, 1, out_size, handle->audio.file);
         pkt.size = out_size;
         if (handle->audio.codec->coded_frame && handle->audio.codec->coded_frame->pts != AV_NOPTS_VALUE)
         {
            pkt.pts = av_rescale_q(handle->audio.codec->coded_frame->pts, handle->audio.codec->time_base, handle->muxer.astream->time_base);
            pkt.dts = pkt.pts;
            fprintf(stderr, "Audio PTS: %d\n", (int)pkt.pts);
         }
         else
         {
            pkt.pts = av_rescale_q(handle->audio.frame_cnt, handle->audio.codec->time_base, handle->muxer.astream->time_base);
            pkt.dts = pkt.pts;
            fprintf(stderr, "Audio PTS (calculated): %d\n", (int)pkt.pts);
         }

         pkt.flags |= AV_PKT_FLAG_KEY;
         handle->audio.frames_in_buffer = 0;
         handle->audio.frame_cnt += handle->audio.codec->frame_size;

         if (av_interleaved_write_frame(handle->muxer.ctx, &pkt) < 0)
            return -1;
      }
   }
   return 0;
}

int ffemu_finalize(ffemu_t *handle)
{
   // Push out delayed frames.
   if (handle->video.enabled)
   {
      AVPacket pkt;
      av_init_packet(&pkt);
      pkt.stream_index = handle->muxer.vstream->index;
      pkt.data = handle->video.outbuf;

      int out_size = 0;
      do
      {
         out_size = avcodec_encode_video(handle->video.codec, handle->video.outbuf, handle->video.outbuf_size, NULL);
         pkt.pts = av_rescale_q(handle->video.frame_cnt++, handle->video.codec->time_base, handle->muxer.vstream->time_base);
         pkt.dts = pkt.pts;

         if (handle->video.codec->coded_frame->key_frame)
            pkt.flags |= AV_PKT_FLAG_KEY;


         pkt.size = out_size;

         int err = av_interleaved_write_frame(handle->muxer.ctx, &pkt);
         if (err < 0)
            break;

      } while (out_size > 0);
   }

   av_write_trailer(handle->muxer.ctx);

   return 0;
}

