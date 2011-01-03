#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
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
   int64_t frame_count;

   FILE *file;
   char *file_name;

   uint8_t *outbuf;
   size_t outbuf_size;

} video;

struct audio_info
{
   bool enabled;
   AVCodecContext *codec;

   int16_t *buffer;
   size_t frames_in_buffer;

   void *outbuf;
   size_t outbuf_size;

   FILE *file;
   char *file_name;
} audio;

struct ffemu
{
   struct video_info video;
   struct audio_info audio;
   
   struct ffemu_params params;
};

static int map_audio_codec(ffemu_audio_codec codec)
{
   (void)codec;
   return CODEC_ID_AAC;
}

static int map_video_codec(ffemu_video_codec codec)
{
   (void)codec;
   return CODEC_ID_MPEG2VIDEO;
}

static int init_audio(struct audio_info *audio, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder(map_audio_codec(param->acodec));
   if (!codec)
      return -1;

   audio->codec = avcodec_alloc_context();
   audio->codec->bit_rate = 128000;
   audio->codec->sample_rate = param->samplerate;
   audio->codec->channels = param->channels;

   if (avcodec_open(audio->codec, codec) != 0)
      return -1;

   audio->buffer = av_malloc(audio->codec->frame_size * param->channels * sizeof(int16_t));
   if (!audio->buffer)
      return -1;

   audio->file = fopen("/tmp/audio.aac", "wb");
   if (!audio->file)
      return -1;

   audio->outbuf_size = 50000;
   audio->outbuf = av_malloc(audio->outbuf_size);
   if (!audio->outbuf)
      return -1;

   return 0;
}

static int init_video(struct video_info *video, struct ffemu_params *param)
{
   AVCodec *codec = avcodec_find_encoder(map_video_codec(param->vcodec));
   if (!codec)
      return -1;

   video->codec = avcodec_alloc_context();
   video->codec->bit_rate = 400000;
   video->codec->width = param->out_width;
   video->codec->height = param->out_height;
   video->codec->time_base = (AVRational) {param->fps.den, param->fps.num};
   video->codec->gop_size = 10;
   video->codec->max_b_frames = 1;
   video->codec->pix_fmt = PIX_FMT_YUV420P;

   if (avcodec_open(video->codec, codec) != 0)
      return -1;

   video->outbuf_size = 100000;
   video->outbuf = av_malloc(video->outbuf_size);

   int size = avpicture_get_size(PIX_FMT_YUV420P, param->out_width, param->out_height);
   video->conv_frame_buf = av_malloc(size);
   video->conv_frame = avcodec_alloc_frame();
   avpicture_fill((AVPicture*)video->conv_frame, video->conv_frame_buf, PIX_FMT_YUV420P, param->out_width, param->out_height);

   video->file = fopen("/tmp/video.mpg", "wb");

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

      if (handle->video.file)
         fclose(handle->video.file);
      if (handle->audio.file)
         fclose(handle->audio.file);

      free(handle);
   }
}

int ffemu_push_video(ffemu_t *handle, const struct ffemu_video_data *data)
{
   if (!handle->video.enabled)
      return -1;

   struct SwsContext *conv_ctx = sws_getContext(data->width, data->height, PIX_FMT_RGB555LE,
         handle->params.out_width, handle->params.out_height, PIX_FMT_YUV420P, SWS_BICUBIC,
         NULL, NULL, NULL);

   int linesize = data->pitch;

   sws_scale(conv_ctx, (const uint8_t* const*)&data->data, &linesize, 0, handle->params.out_width, handle->video.conv_frame->data, handle->video.conv_frame->linesize);

   handle->video.conv_frame->pts = handle->video.frame_count;
   handle->video.conv_frame->display_picture_number = handle->video.frame_count;
   handle->video.frame_count++;

   int outsize = avcodec_encode_video(handle->video.codec, handle->video.outbuf, handle->video.outbuf_size, handle->video.conv_frame);

   fwrite(handle->video.outbuf, 1, outsize, handle->video.file);

   sws_freeContext(conv_ctx);

   return 0;
}

int ffemu_push_audio(ffemu_t *handle, const struct ffemu_audio_data *data)
{
   if (!handle->audio.enabled)
      return -1;

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
         fwrite(handle->audio.outbuf, 1, out_size, handle->audio.file);
         handle->audio.frames_in_buffer = 0;
      }
   }
   return 0;
}

int ffemu_mux(ffemu_t *handle, const char *path, const struct ffemu_muxer *muxer)
{
   (void)handle;
   (void)path;
   (void)muxer;
   return -1;
}

