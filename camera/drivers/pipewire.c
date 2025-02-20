/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - Viachaslau Khalikin
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

#include <spa/utils/result.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/tag-utils.h>
#include <spa/param/props.h>
#include <spa/param/latency-utils.h>
#include <spa/debug/format.h>
#include <spa/debug/pod.h>
#include <pipewire/pipewire.h>

#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <libretro.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>

#include "../camera_driver.h"
#include "../../audio/common/pipewire.h"
#include "../../verbosity.h"


/* TODO/FIXME: detect size */
#define WIDTH           640
#define HEIGHT          480

#define MAX_BUFFERS     64

typedef struct pipewire_camera
{
   pipewire_core_t *pw;

   struct pw_stream *stream;
   struct spa_hook stream_listener;

   struct scaler_ctx scaler;
   uint32_t *buffer_output;

   struct spa_io_position *position;
   struct spa_video_info format;
   struct spa_rectangle size;
} pipewire_camera_t;

static struct
{
   uint32_t format;
   uint32_t id;
} scaler_video_formats[] = {
   { SCALER_FMT_ARGB8888, SPA_VIDEO_FORMAT_ARGB },
   { SCALER_FMT_ABGR8888, SPA_VIDEO_FORMAT_ABGR },
   { SCALER_FMT_0RGB1555, SPA_VIDEO_FORMAT_UNKNOWN },
   { SCALER_FMT_RGB565,   SPA_VIDEO_FORMAT_UNKNOWN },
   { SCALER_FMT_BGR24,    SPA_VIDEO_FORMAT_BGR },
   { SCALER_FMT_YUYV,     SPA_VIDEO_FORMAT_YUY2 },
   { SCALER_FMT_RGBA4444, SPA_VIDEO_FORMAT_UNKNOWN },
};

#if 0
{
   SPA_FOR_EACH_ELEMENT_VAR(scaler_video_formats, f)
   {
      if (f->format == format)
         return f->id;
   }
   return SPA_VIDEO_FORMAT_UNKNOWN;
}
#endif

static uint32_t id_to_scaler_format(uint32_t id)
{
   SPA_FOR_EACH_ELEMENT_VAR(scaler_video_formats, f)
   {
      if (f->id == id)
         return f->format;
   }
   return SCALER_FMT_ARGB8888;
}

static int build_format(struct spa_pod_builder *b, const struct spa_pod **params,
      uint32_t width, uint32_t height)
{
   struct spa_pod_frame frame[2];

   /* make an object of type SPA_TYPE_OBJECT_Format and id SPA_PARAM_EnumFormat.
    * The object type is important because it defines the properties that are
    * acceptable. The id gives more context about what the object is meant to
    * contain. In this case we enumerate supported formats. */
   spa_pod_builder_push_object(b, &frame[0], SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
   /* add media type and media subtype properties */
   spa_pod_builder_prop(b, SPA_FORMAT_mediaType, 0);
   spa_pod_builder_id(b, SPA_MEDIA_TYPE_video);
   spa_pod_builder_prop(b, SPA_FORMAT_mediaSubtype, 0);
   spa_pod_builder_id(b, SPA_MEDIA_SUBTYPE_raw);

   /* build an enumeration of formats */
   spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_format, 0);
   spa_pod_builder_push_choice(b, &frame[1], SPA_CHOICE_Enum, 0);
   spa_pod_builder_id(b, SPA_VIDEO_FORMAT_YUY2);  /* default */
   SPA_FOR_EACH_ELEMENT_VAR(scaler_video_formats, f)
   {
      uint32_t id = f->id;
      if (id != SPA_VIDEO_FORMAT_UNKNOWN)
         spa_pod_builder_id(b, id);  /* alternative */
   }

   spa_pod_builder_pop(b, &frame[1]);

   /* add size and framerate ranges */
   spa_pod_builder_add(b,
         SPA_FORMAT_VIDEO_size,      SPA_POD_CHOICE_RANGE_Rectangle(
               &SPA_RECTANGLE(MAX(width, 1), MAX(height, 1)),
               &SPA_RECTANGLE(1, 1),
               &SPA_RECTANGLE(MAX(width, WIDTH), MAX(height, HEIGHT))),
         SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
               &SPA_FRACTION(25, 1),
               &SPA_FRACTION(0, 1),
               &SPA_FRACTION(30, 1)),
         0);

   params[0] = spa_pod_builder_pop(b, &frame[0]);

   RARCH_LOG("[Camera] [PipeWire]: Supported raw formats:\n");
   spa_debug_format(2, NULL, params[0]);

   return 1;
}

static bool preprocess_image(pipewire_camera_t *camera)
{
   struct spa_buffer    *buf;
   struct spa_meta_header *h;
   struct pw_buffer       *b = NULL;
   struct scaler_ctx    *ctx = NULL;

   for (;;)
   {
      if ((b = pw_stream_dequeue_buffer(camera->stream)))
         break;
   }
   if (b == NULL)
   {
      RARCH_DBG("[Camera] [PipeWire]: Out of buffers\n");
      return false;
   }

   buf = b->buffer;

   if ((h = spa_buffer_find_meta_data(buf, SPA_META_Header, sizeof(*h))))
   {
      if (h->flags & SPA_META_HEADER_FLAG_CORRUPTED) {
         RARCH_LOG("[Camera] [PipeWire] Dropping corruped frame.\n");
         pw_stream_queue_buffer(camera->stream, b);
         return false;
      }
      uint64_t now = pw_stream_get_nsec(camera->stream);
      RARCH_DBG("[Camera] [PipeWire]: now:%"PRIu64" pts:%"PRIu64" diff:%"PRIi64"\n",
                now, h->pts, now - h->pts);
   }

   if (buf->datas[0].data == NULL)
      return false;

   ctx = &camera->scaler;
   scaler_ctx_scale_direct(ctx, camera->buffer_output, (const uint8_t*)buf->datas[0].data);

   pw_stream_queue_buffer(camera->stream, b);
   return true;
}

static void stream_state_changed_cb(void *data, enum pw_stream_state old,
                                    enum pw_stream_state state, const char *error)
{
   pipewire_camera_t *camera = (pipewire_camera_t*)data;

   RARCH_DBG("[Camera] [PipeWire]: Stream state changed %s -> %s\n",
             pw_stream_state_as_string(old),
             pw_stream_state_as_string(state));

   pw_thread_loop_signal(camera->pw->thread_loop, false);
}

static void stream_io_changed_cb(void *data, uint32_t id, void *area, uint32_t size)
{
   pipewire_camera_t *camera = (pipewire_camera_t*)data;

   switch (id)
   {
      case SPA_IO_Position:
         camera->position = area;
         break;
   }
}

static void stream_param_changed_cb(void *data, uint32_t id, const struct spa_pod *param)
{
   uint8_t params_buffer[1024];
   const struct spa_pod *params[5];
   pipewire_camera_t *camera = (pipewire_camera_t*)data;
   struct spa_pod_builder b = SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));

   if (param && id == SPA_PARAM_Tag)
   {
      spa_debug_pod(0, NULL, param);
      return;
   }
   if (param && id == SPA_PARAM_Latency)
   {
      struct spa_latency_info info;
      if (spa_latency_parse(param, &info) >= 0)
         RARCH_DBG("[Camera] [PipeWire]: Got latency: %"PRIu64"\n", (info.min_ns + info.max_ns) / 2);
      return;
   }
   /* NULL means to clear the format */
   if (param == NULL || id != SPA_PARAM_Format)
      return;

   RARCH_DBG("[Camera] [PipeWire]: Got format:\n");
   spa_debug_format(2, NULL, param);

   if (spa_format_parse(param, &camera->format.media_type, &camera->format.media_subtype) < 0)
   {
      RARCH_DBG("[Camera] [PipeWire]: Failed to parse video format.\n");
      return;
   }

   if (camera->format.media_type != SPA_MEDIA_TYPE_video)
      return;

   switch (camera->format.media_subtype)
   {
      case SPA_MEDIA_SUBTYPE_raw:
         spa_format_video_raw_parse(param, &camera->format.info.raw);
         camera->scaler.in_fmt = id_to_scaler_format(camera->format.info.raw.format);
         camera->size = SPA_RECTANGLE(camera->format.info.raw.size.width, camera->format.info.raw.size.height);
         RARCH_DBG("[Camera] [PipeWire]: Configured capture format = %d\n", camera->format.info.raw.format);
         break;
      default:
         RARCH_WARN("[Camera] [PipeWire]: Unsupported video format: %d\n", camera->format.media_subtype);
         return;
   }

   if (!camera->size.width || !camera->size.height)
   {
      pw_stream_set_error(camera->stream, -EINVAL, "invalid size");
      return;
   }

   struct spa_pod_frame frame;
   spa_pod_builder_push_object(&b, &frame, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers);
   spa_pod_builder_add(&b,
         SPA_PARAM_BUFFERS_stride,   SPA_POD_Int(camera->size.width * 2),
         0);

   spa_pod_builder_add(&b,
         SPA_PARAM_BUFFERS_buffers,  SPA_POD_CHOICE_RANGE_Int(8, 1, MAX_BUFFERS),
         SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(1 << SPA_DATA_MemPtr),
         0);

   params[0] = spa_pod_builder_pop(&b, &frame);

   params[1] = spa_pod_builder_add_object(&b,
         SPA_TYPE_OBJECT_ParamMeta,  SPA_PARAM_Meta,
         SPA_PARAM_META_type,        SPA_POD_Id(SPA_META_Header),
         SPA_PARAM_META_size,        SPA_POD_Int(sizeof(struct spa_meta_header)));
#if 0
   params[2] = spa_pod_builder_add_object(&b,
         SPA_TYPE_OBJECT_ParamMeta,  SPA_PARAM_Meta,
         SPA_PARAM_META_type,        SPA_POD_Id(SPA_META_VideoTransform),
         SPA_PARAM_META_size,        SPA_POD_Int(sizeof(struct spa_meta_videotransform)));
#endif

   camera->buffer_output = (uint32_t *)
         malloc(camera->size.width * camera->size.height * sizeof(uint32_t));
   if (!camera->buffer_output)
   {
      RARCH_ERR("[Camera] [PipeWire]: Failed to allocate output buffer.\n");
      return;
   }

   camera->scaler.in_width   = camera->scaler.out_width = camera->size.width;
   camera->scaler.in_height  = camera->scaler.out_height = camera->size.height;
   camera->scaler.out_fmt    = SCALER_FMT_ARGB8888;
   camera->scaler.in_stride  = camera->size.width * 2;
   camera->scaler.out_stride = camera->size.width * 4;
   if (!scaler_ctx_gen_filter(&camera->scaler))
   {
      RARCH_ERR("[Camera] [PipeWire]: Failed to create scaler.\n");
      return;
   }

   pw_stream_update_params(camera->stream, params, 2);
}

static const struct pw_stream_events stream_events = {
      PW_VERSION_STREAM_EVENTS,
      .state_changed = stream_state_changed_cb,
      .io_changed = stream_io_changed_cb,
      .param_changed = stream_param_changed_cb,
      .process = NULL,
};

static void pipewire_stop(void *data)
{
   pipewire_camera_t *camera = (pipewire_camera_t*)data;
   const char         *error = NULL;

   retro_assert(camera);
   retro_assert(camera->stream);
   retro_assert(camera->pw);

   if (pw_stream_get_state(camera->stream, &error) == PW_STREAM_STATE_PAUSED)
      pipewire_stream_set_active(camera->pw->thread_loop, camera->stream, false);
}

static bool pipewire_start(void *data)
{
   pipewire_camera_t *camera = (pipewire_camera_t*)data;
   const char         *error = NULL;

   retro_assert(camera);
   retro_assert(camera->stream);
   retro_assert(camera->pw);

   if (pw_stream_get_state(camera->stream, &error) == PW_STREAM_STATE_STREAMING)
      return true;

   return pipewire_stream_set_active(camera->pw->thread_loop, camera->stream, true);
}

static void pipewire_free(void *data)
{
   pipewire_camera_t *camera = (pipewire_camera_t*)data;

   if (!camera)
      return;

   if (camera->stream)
   {
      pw_thread_loop_lock(camera->pw->thread_loop);
      pw_stream_destroy(camera->stream);
      camera->stream = NULL;
      pw_thread_loop_unlock(camera->pw->thread_loop);
   }

   pipewire_core_deinit(camera->pw);

   if (camera->buffer_output)
      free(camera->buffer_output);

   scaler_ctx_gen_reset(&camera->scaler);
   free(camera);
}

static void *pipewire_init(const char *device, uint64_t caps,
      unsigned width, unsigned height)
{
   int               res, n_params;
   const struct spa_pod *params[3];
   struct     pw_properties *props;
   uint8_t            buffer[1024];
   pipewire_camera_t       *camera = (pipewire_camera_t*)calloc(1, sizeof(*camera));
   struct spa_pod_builder        b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

   if (!camera)
      goto error;

   if (!pipewire_core_init(&camera->pw, "camera_driver", NULL))
      goto error;

   pipewire_core_wait_resync(camera->pw);

   props = pw_properties_new(PW_KEY_MEDIA_TYPE,     PW_RARCH_MEDIA_TYPE_VIDEO,
                             PW_KEY_MEDIA_CATEGORY, PW_RARCH_MEDIA_CATEGORY_RECORD,
                             PW_KEY_MEDIA_ROLE,     PW_RARCH_MEDIA_ROLE,
                             NULL);
   if (!props)
      goto error;

   if (device)
      pw_properties_set(props, PW_KEY_TARGET_OBJECT, device);

   camera->stream = pw_stream_new(camera->pw->core, PW_RARCH_APPNAME, props);

   pw_stream_add_listener(camera->stream, &camera->stream_listener, &stream_events, camera);

   /* build the extra parameters to connect with. To connect, we can provide
    * a list of supported formats. We use a builder that writes the param
    * object to the stack. */
   n_params = build_format(&b, params, width, height);
   {
      struct spa_pod_frame f;
      struct spa_dict_item items[1];
      /* send a tag, input tags travel upstream */
      spa_tag_build_start(&b, &f, SPA_PARAM_Tag, SPA_DIRECTION_INPUT);
      items[0] = SPA_DICT_ITEM_INIT("my-tag-other-key", "my-special-other-tag-value");
      spa_tag_build_add_dict(&b, &SPA_DICT_INIT(items, 1));
      params[n_params++] = spa_tag_build_end(&b, &f);
   }

   res = pw_stream_connect(camera->stream,
                           PW_DIRECTION_INPUT,
                           PW_ID_ANY,
                           PW_STREAM_FLAG_AUTOCONNECT |
                           PW_STREAM_FLAG_INACTIVE |
                           PW_STREAM_FLAG_MAP_BUFFERS,
                           params, n_params);

   if (res < 0)
   {
      RARCH_ERR("[Camera] [PipeWire]: can't connect: %s\n", spa_strerror(res));
      goto error;
   }

   pw_thread_loop_unlock(camera->pw->thread_loop);

   return camera;

error:
   RARCH_ERR("[Camera] [PipeWire]: Failed to initialize camera\n");
   pipewire_free(camera);
   return NULL;
}

static bool pipewire_poll(void *data,
      retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   pipewire_camera_t *camera = (pipewire_camera_t*)data;
   const char         *error = NULL;

   (void)frame_gl_cb;

   retro_assert(camera);

   if (pw_stream_get_state(camera->stream, &error) != PW_STREAM_STATE_STREAMING)
      return false;

   if (!frame_raw_cb || !preprocess_image(camera))
      return false;

   frame_raw_cb(camera->buffer_output, camera->size.width,
         camera->size.height, camera->size.width * 4);
   return true;
}

camera_driver_t camera_pipewire = {
      pipewire_init,
      pipewire_free,
      pipewire_start,
      pipewire_stop,
      pipewire_poll,
      "pipewire",
};
