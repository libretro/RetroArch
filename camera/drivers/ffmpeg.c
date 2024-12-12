/*  RetroArch - A frontend for libretro.
*  Copyright (C) 2010-2023 - Hans-Kristian Arntzen
*  Copyright (C) 2023 - Jesse Talavera-Greenberg
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

#include <libretro.h>
#include <libavdevice/avdevice.h>

#include "../camera_driver.h"
#include "lists/string_list.h"
#include "verbosity.h"

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <memalign.h>
#include <retro_assert.h>

/* ffmpeg supports a specific range of video devices;
 * some video backends are better than others,
 * so we'll prioritize those as defaults.
 */
const char *const FFMPEG_CAMERA_DEVICE_TYPE_PRIORITIES[] = {
   /* Recommended camera backends */
#ifdef ANDROID
   "android_camera",
#endif
#ifdef __linux__
   "v4l2",
#endif
#ifdef __APPLE__
   "avfoundation",
#endif
#ifdef __WIN32__
   "dshow",
   "vfwcap",
#endif

   /* Uncommon backends */
#ifdef __linux__
   "fbdev",
   "kmsgrab",
   "x11grab",
#endif
#ifdef __WIN32__
   "gdigrab",
#endif

   "iec61883", /* FireWire */
   "decklink",
   "libdc1394",
   "lavfi", /* Libavfilter */

   /* Deprecated backends */
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
   "bktr",
#endif
   NULL

   /* Unlisted backends lack video support
    * or were introduced after this driver was written
    * (it's okay, we can still use them if they're available).
    */
};


typedef struct ffmpeg_camera
{
   AVFormatContext *format_context;
   AVCodecContext *decoder_context;
   const AVCodec *decoder;
   const AVInputFormat *input_format; /* owned by ffmpeg, don't free it */
   AVDictionary *demuxer_options;
   AVPacket *packet;
   AVFrame *camera_frame;
   AVFrame *target_frame;
   unsigned target_width;
   unsigned target_height;
   uint8_t *target_buffer;
   size_t target_buffer_length;
   struct SwsContext *scale_context;

   /* "name" for the camera device.
    * (Not just the reported name, there may be a bit of extra syntax.) */
   char url[512];
} ffmpeg_camera_t;

static void ffmpeg_camera_free(void *data);

static const AVInputFormat *ffmpeg_camera_choose_format(const char *device)
{
   // TODO: If `device` is not NULL or empty, parse it to get the backend and device name
   // TODO: Pick the best backend for the current platform, don't just return the first
   return av_input_video_device_next(NULL);
}

// TODO: device format shall be: "<device type>/<device name>"
static void *ffmpeg_camera_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   ffmpeg_camera_t *ffmpeg = NULL;
   AVDictionary *options = NULL;

   if ((caps & (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)) == 0)
   { /* If the core didn't ask for raw framebuffers... */
      RARCH_ERR("[FFMPEG]: Camera driver only supports raw framebuffer output.\n");
      return NULL;
   }

   ffmpeg = (ffmpeg_camera_t*)calloc(1, sizeof(*ffmpeg));
   if (!ffmpeg)
   {
      RARCH_ERR("[FFMPEG]: Failed to allocate memory for camera driver.\n");
      return NULL;
   }

   ffmpeg->target_width = width;
   ffmpeg->target_height = height;

   avdevice_register_all();
   RARCH_LOG("[FFMPEG]: Initialized libavdevice.\n");

   ffmpeg->input_format = ffmpeg_camera_choose_format(device);
   if (!ffmpeg->input_format)
   {
      RARCH_ERR("[FFMPEG]: No suitable video input devices found.\n");
      goto error;
   }

   AVDeviceInfoList *device_list = NULL;
   int num_sources = avdevice_list_input_sources(ffmpeg->input_format, NULL, NULL, &device_list);
   // TODO: Handle case where zero sources are returned
   if (num_sources < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to list video input sources: %s\n", av_err2str(num_sources));
      goto error;
   }

   RARCH_LOG("[FFMPEG]: Using video input device: %s (%s, flags=0x%x)\n", ffmpeg->input_format->name, ffmpeg->input_format->long_name, ffmpeg->input_format->flags);
   snprintf(ffmpeg->url, sizeof(ffmpeg->url), "video=%s", device_list->devices[0]->device_name);
   // TODO: this url is specific to dshow, need to generalize it

   avdevice_free_list_devices(&device_list);
   return ffmpeg;
error:
   avdevice_free_list_devices(&device_list);
   ffmpeg_camera_free(ffmpeg);
   return NULL;
}

static void ffmpeg_camera_stop(void *data);
static void ffmpeg_camera_free(void *data)
{
   ffmpeg_camera_t *ffmpeg = data;

   if (!ffmpeg)
      return;

   ffmpeg_camera_stop(ffmpeg);

   /* these functions are noops for NULL pointers */
   avcodec_free_context(&ffmpeg->decoder_context);
   avformat_close_input(&ffmpeg->format_context);
   av_frame_free(&ffmpeg->camera_frame);
   av_frame_free(&ffmpeg->target_frame);
   av_packet_free(&ffmpeg->packet);
   av_dict_free(&ffmpeg->demuxer_options);
   sws_freeContext(ffmpeg->scale_context);
   memalign_free(ffmpeg->target_buffer);

   free(ffmpeg);
}

static bool ffmpeg_camera_start(void *data)
{
   ffmpeg_camera_t *ffmpeg = data;
   int result = 0;
   AVStream *stream = NULL;

   if (ffmpeg->format_context)
   { // TODO: Check the actual format context, not just the pointer
      RARCH_LOG("[FFMPEG]: Camera %s is already started, no action needed.\n", ffmpeg->format_context->url);
      return true;
   }

   result = avformat_open_input(&ffmpeg->format_context, ffmpeg->url, ffmpeg->input_format, &ffmpeg->demuxer_options);
   if (result < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to open video input device: %s\n", av_err2str(result));
      goto error;
   }

   result = avformat_find_stream_info(ffmpeg->format_context, NULL);
   if (result < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to find stream info: %s\n", av_err2str(result));
      goto error;
   }

   result = av_find_best_stream(ffmpeg->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &ffmpeg->decoder, 0);
   if (result < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to find video stream: %s\n", av_err2str(result));
      goto error;
   }
   stream = ffmpeg->format_context->streams[result];

   RARCH_LOG("[FFMPEG]: Using video stream #%d with decoder \"%s\" (%s).\n", result, ffmpeg->decoder->name, ffmpeg->decoder->long_name);

   ffmpeg->decoder_context = avcodec_alloc_context3(ffmpeg->decoder);
   if (!ffmpeg->decoder_context)
   {
      RARCH_ERR("[FFMPEG]: Failed to allocate decoder context.\n");
      goto error;
   }

   result = avcodec_parameters_to_context(ffmpeg->decoder_context, stream->codecpar);
   if (result < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to copy codec parameters to decoder context: %s\n", av_err2str(result));
      goto error;
   }

   if (!sws_isSupportedInput(ffmpeg->decoder_context->pix_fmt))
   {
      RARCH_ERR("[FFMPEG]: Unsupported scaler input pixel format: %s\n", av_get_pix_fmt_name(ffmpeg->decoder_context->pix_fmt));
      goto error;
   }

   AVDictionary *opts = NULL;
   result = avcodec_open2(ffmpeg->decoder_context, ffmpeg->decoder, &opts);
   if (result < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to open decoder: %s\n", av_err2str(result));
      goto error;
   }

   ffmpeg->packet = av_packet_alloc();
   if (!ffmpeg->packet)
   {
      RARCH_ERR("[FFMPEG]: Failed to allocate packet.\n");
      goto error;
   }

   ffmpeg->camera_frame = av_frame_alloc();
   if (!ffmpeg->camera_frame)
   {
      RARCH_ERR("[FFMPEG]: Failed to allocate camera frame.\n");
      goto error;
   }

   ffmpeg->target_frame = av_frame_alloc();
   if (!ffmpeg->target_frame)
   {
      RARCH_ERR("[FFMPEG]: Failed to allocate target frame.\n");
      goto error;
   }

   int buffer_length = av_image_get_buffer_size(AV_PIX_FMT_ARGB, ffmpeg->decoder_context->width, ffmpeg->decoder_context->height, 4);
   if (buffer_length < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to get buffer size for target frame: %s\n", av_err2str(buffer_length));
      goto error;
   }

   /* target buffer aligned to 4 bytes because it's exposed to the core as a uint32_t[] */
   ffmpeg->target_buffer_length = buffer_length;
   ffmpeg->target_buffer = memalign_alloc(4, buffer_length);
   if (!ffmpeg->target_buffer)
   {
      RARCH_ERR("[FFMPEG]: Failed to allocate target %d-byte buffer for %dx%d %s-formatted video data.\n",
         buffer_length,
         ffmpeg->decoder_context->width,
         ffmpeg->decoder_context->height,
         av_get_pix_fmt_name(AV_PIX_FMT_ARGB)
      );
      goto error;
   }

   RARCH_LOG("[FFMPEG]: Allocated %d bytes for %dx%d %s-formatted video data.\n",
      buffer_length,
      ffmpeg->decoder_context->width,
      ffmpeg->decoder_context->height,
      av_get_pix_fmt_name(ffmpeg->decoder_context->pix_fmt)
   );

   ffmpeg->scale_context = sws_getContext(
      ffmpeg->decoder_context->width,
      ffmpeg->decoder_context->height,
      ffmpeg->decoder_context->pix_fmt,
      ffmpeg->target_width ? ffmpeg->target_width : ffmpeg->decoder_context->width,
      ffmpeg->target_height ? ffmpeg->target_height : ffmpeg->decoder_context->height,
      AV_PIX_FMT_ARGB,
      SWS_BILINEAR,
      NULL, NULL, NULL
   );
   if (!ffmpeg->scale_context)
   {
      RARCH_ERR("[FFMPEG]: Failed to create scale context.\n");
      goto error;
   }

   return true;
error:
   ffmpeg_camera_stop(ffmpeg);

   return false;
}

static void ffmpeg_camera_stop(void *data)
{
   ffmpeg_camera_t *ffmpeg = data;

   if (!ffmpeg->format_context)
   {
      RARCH_LOG("[FFMPEG]: Camera %s is already stopped, no flush needed.\n", ffmpeg->url);
   }
   else
   {
      int result = avcodec_send_packet(ffmpeg->decoder_context, NULL);
      if (result < 0)
      {
         RARCH_ERR("[FFMPEG]: Failed to flush decoder: %s\n", av_err2str(result));
      }
   }

   /* these functions are noops for NULL pointers */
   memalign_free(ffmpeg->target_buffer);
   ffmpeg->target_buffer = NULL;
   ffmpeg->target_buffer_length = 0;
   avcodec_free_context(&ffmpeg->decoder_context);
   avformat_close_input(&ffmpeg->format_context);
   sws_freeContext(ffmpeg->scale_context);
   ffmpeg->scale_context = NULL;
   RARCH_LOG("[FFMPEG]: Closed video input device %s.\n", ffmpeg->url);
}

// TODO: Put this in another thread
static bool ffmpeg_camera_poll(
   void *data,
   retro_camera_frame_raw_framebuffer_t frame_raw_cb,
   retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   ffmpeg_camera_t *ffmpeg = data;
   int result = 0;

   if (!ffmpeg->format_context)
   {
      RARCH_ERR("[FFMPEG]: Camera is not started, cannot poll.\n");
      return false;
   }

   if (!frame_raw_cb)
   {
      RARCH_ERR("[FFMPEG]: No callback provided, cannot poll.\n");
      return false;
   }

   result = av_read_frame(ffmpeg->format_context, ffmpeg->packet);
   if (result < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to read frame: %s\n", av_err2str(result));
      goto error;
   }

   result = avcodec_send_packet(ffmpeg->decoder_context, ffmpeg->packet);
   if (result < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to send packet to decoder: %s\n", av_err2str(result));
      goto error;
   }

   /* video streams consist of exactly one frame per packet */
   result = avcodec_receive_frame(ffmpeg->decoder_context, ffmpeg->camera_frame);
   if (result < 0)
   { // TODO: Handle AVERROR_EOF and AVERROR(EAGAIN)
      RARCH_ERR("[FFMPEG]: Failed to receive camera frame from decoder: %s\n", av_err2str(result));
      goto error;
   }

   retro_assert(ffmpeg->decoder->type == AVMEDIA_TYPE_VIDEO);

   result = sws_scale_frame(ffmpeg->scale_context, ffmpeg->target_frame, ffmpeg->camera_frame);
   if (result < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to scale frame: %s\n", av_err2str(result));
      goto error;
   }

   result = av_image_copy_to_buffer(
      ffmpeg->target_buffer,
      ffmpeg->target_buffer_length,
      (const uint8_t* const *)ffmpeg->target_frame->data,
      ffmpeg->target_frame->linesize,
      ffmpeg->target_frame->format,
      ffmpeg->target_frame->width,
      ffmpeg->target_frame->height,
      4
   );
   if (result < 0)
   {
      RARCH_ERR("[FFMPEG]: Failed to copy frame to buffer: %s\n", av_err2str(result));
      goto error;
   }

   frame_raw_cb((uint32_t*)ffmpeg->target_buffer, ffmpeg->target_frame->width, ffmpeg->target_frame->height, ffmpeg->target_frame->linesize[0]);
   av_packet_unref(ffmpeg->packet);

   return false;
error:
   /* must be called when we're done with it */
   av_packet_unref(ffmpeg->packet);
   return false;
}

static struct string_list *ffmpeg_camera_device_list_new(const void *driver_context)
{
   const AVInputFormat *input = NULL;
   struct string_list *list = string_list_new();

   if (!list)
      return NULL;

   for (input = av_input_video_device_next(NULL); input != NULL; input = av_input_video_device_next(input))
   {
      // if (input->priv_class && input->priv_class->category == AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT)
      RARCH_LOG("[FFMPEG]: Found input device: %s (%s, %x)\n", input->name, input->long_name, input->flags);
      // TODO: Find guide for creating and using a camera
   }

   return list;
}

static void ffmpeg_camera_device_list_free(const void *driver_context, struct string_list *devices)
{
   struct string_list *sl = (struct string_list*)devices;

   if (sl)
      string_list_free(sl);
}

camera_driver_t camera_ffmpeg = {
   ffmpeg_camera_init,
   ffmpeg_camera_free,
   ffmpeg_camera_start,
   ffmpeg_camera_stop,
   ffmpeg_camera_poll,
   ffmpeg_camera_device_list_new,
   ffmpeg_camera_device_list_free,
   "ffmpeg",
};
