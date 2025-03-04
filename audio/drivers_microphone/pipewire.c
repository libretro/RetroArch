/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024 Viachaslau Khalikin
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

#include <spa/param/audio/format-utils.h>
#include <spa/utils/ringbuffer.h>
#include <spa/utils/result.h>
#include <spa/param/props.h>
#include <pipewire/pipewire.h>

#include <boolean.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>

#include "../common/pipewire.h"
#include "../microphone_driver.h"
#include "../../verbosity.h"


#define DEFAULT_CHANNELS   1
#define RINGBUFFER_SIZE    (1u << 22)
#define RINGBUFFER_MASK    (RINGBUFFER_SIZE - 1)

typedef struct pipewire_microphone
{
   pipewire_core_t *pw;
   struct pw_stream *stream;
   struct spa_hook stream_listener;
   struct spa_audio_info_raw info;
   uint32_t frame_size;
   struct spa_ringbuffer ring;
   uint8_t buffer[RINGBUFFER_SIZE];
} pipewire_microphone_t;

static void stream_state_changed_cb(void *data,
      enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
   pipewire_microphone_t *mic = (pipewire_microphone_t*)data;

   RARCH_DBG("[Microphone] [PipeWire]: Stream state changed %s -> %s\n",
             pw_stream_state_as_string(old),
             pw_stream_state_as_string(state));

   pw_thread_loop_signal(mic->pw->thread_loop, false);
}

static void stream_destroy_cb(void *data)
{
   pipewire_microphone_t *mic = (pipewire_microphone_t*)data;
   spa_hook_remove(&mic->stream_listener);
   mic->stream = NULL;
}

static void capture_process_cb(void *data)
{
   void *p;
   int32_t filled;
   struct pw_buffer *b;
   struct spa_buffer *buf;
   uint32_t idx, offs, n_bytes;
   pipewire_microphone_t *mic = (pipewire_microphone_t*)data;

   retro_assert(mic);
   retro_assert(mic->stream);

   if (!(b = pw_stream_dequeue_buffer(mic->stream)))
   {
      RARCH_ERR("[Microphone] [PipeWire]: Out of buffers: %s\n", strerror(errno));
      return pw_thread_loop_signal(mic->pw->thread_loop, false);
   }

   buf = b->buffer;
   if ((p = buf->datas[0].data) == NULL)
      return pw_thread_loop_signal(mic->pw->thread_loop, false);

   offs    = MIN(buf->datas[0].chunk->offset, buf->datas[0].maxsize);
   n_bytes = MIN(buf->datas[0].chunk->size, buf->datas[0].maxsize - offs);

   if ((filled = spa_ringbuffer_get_write_index(&mic->ring, &idx)) < 0)
      RARCH_ERR("[Microphone] [PipeWire]: %p: underrun write:%u filled:%d\n", p, idx, filled);
   else
   {
      if ((uint32_t)filled + n_bytes > RINGBUFFER_SIZE)
         RARCH_ERR("[Microphone] [PipeWire]: %p: overrun write:%u filled:%d + size:%u > max:%u\n",
                   p, idx, filled, n_bytes, RINGBUFFER_SIZE);
   }
   spa_ringbuffer_write_data(&mic->ring,
         mic->buffer, RINGBUFFER_SIZE,
         idx & RINGBUFFER_MASK,
         SPA_PTROFF(p, offs, void), n_bytes);
   idx += n_bytes;
   spa_ringbuffer_write_update(&mic->ring, idx);

   pw_stream_queue_buffer(mic->stream, b);
   pw_thread_loop_signal(mic->pw->thread_loop, false);
}

static const struct pw_stream_events capture_stream_events = {
      PW_VERSION_STREAM_EVENTS,
     .destroy = stream_destroy_cb,
     .state_changed = stream_state_changed_cb,
     .process = capture_process_cb
};

static void registry_event_global(void *data, uint32_t id,
      uint32_t permissions, const char *type, uint32_t version,
      const struct spa_dict *props)
{
   union string_list_elem_attr attr;
   const struct spa_dict_item *item;
   pipewire_core_t              *pw = (pipewire_core_t*)data;
   const char                 *sink = NULL;

   if (!pw)
      return;

   if (   spa_streq(type, PW_TYPE_INTERFACE_Node)
       && spa_streq("Audio/Source", spa_dict_lookup(props, PW_KEY_MEDIA_CLASS)))
   {
      sink = spa_dict_lookup(props, PW_KEY_NODE_NAME);
      if (sink && pw->devicelist)
      {
         attr.i = id;
         string_list_append(pw->devicelist, sink, attr);
         RARCH_LOG("[Microphone] [PipeWire]: Found Source Node: %s\n", sink);
      }

      RARCH_DBG("[Microphone] [PipeWire]: Object: id:%u Type:%s/%d\n", id, type, version);
      spa_dict_for_each(item, props)
         RARCH_DBG("[Microphone] [PipeWire]: \t\t%s: \"%s\"\n", item->key, item->value);
   }
}

static const struct pw_registry_events registry_events = {
      PW_VERSION_REGISTRY_EVENTS,
      .global = registry_event_global,
};

static void pipewire_microphone_free(void *driver_context)
{
   pipewire_core_deinit((pipewire_core_t*)driver_context);
}

static void *pipewire_microphone_init(void)
{
   int res;
   uint8_t buffer[1024];
   uint64_t buf_samples;
   const struct spa_pod *params[1];
   struct pw_properties     *props = NULL;
   const char               *error = NULL;
   pipewire_core_t             *pw = NULL;
   struct spa_pod_builder        b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

   if (!pipewire_core_init(&pw, "microphone_driver", &registry_events))
      goto error;

   pipewire_core_wait_resync(pw);
   pw_thread_loop_unlock(pw->thread_loop);

   return pw;

error:
   RARCH_ERR("[Microphone] [PipeWire]: Failed to initialize microphone\n");
   pipewire_microphone_free(pw);
   return NULL;
}

static void pipewire_microphone_close_mic(void *driver_context, void *mic_context)
{
   pipewire_core_t        *pw = (pipewire_core_t*)driver_context;
   pipewire_microphone_t *mic = (pipewire_microphone_t*)mic_context;

   if (pw && mic)
   {
      pw_thread_loop_lock(pw->thread_loop);
      pw_stream_destroy(mic->stream);
      mic->stream = NULL;
      pw_thread_loop_unlock(pw->thread_loop);
      free(mic);
   }
}

static int pipewire_microphone_read(void *driver_context, void *mic_context, void *s, size_t len)
{
   uint32_t idx;
   int32_t readable;
   const char          *error = NULL;
   pipewire_core_t        *pw = (pipewire_core_t*)driver_context;
   pipewire_microphone_t *mic = (pipewire_microphone_t*)mic_context;

   if (pw_stream_get_state(mic->stream, &error) != PW_STREAM_STATE_STREAMING)
      return -1;

   pw_thread_loop_lock(pw->thread_loop);

   for (;;)
   {
      /* get no of available bytes to read data from buffer */
      readable = spa_ringbuffer_get_read_index(&mic->ring, &idx);

      if (readable < (int32_t)len)
      {
         if (pw->nonblock)
         {
            len = readable;
            break;
         }

         pw_thread_loop_wait(pw->thread_loop);
         if (pw_stream_get_state(mic->stream, &error) != PW_STREAM_STATE_STREAMING)
         {
            pw_thread_loop_unlock(mic->pw->thread_loop);
            return -1;
         }
      }
      else
         break;
   }

   spa_ringbuffer_read_data(&mic->ring,
         mic->buffer, RINGBUFFER_SIZE,
         idx & RINGBUFFER_MASK, s, len);
   idx += len;
   spa_ringbuffer_read_update(&mic->ring, idx);
   pw_thread_loop_unlock(pw->thread_loop);

   return len;
}

static bool pipewire_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   const char          *error = NULL;
   pipewire_microphone_t *mic = (pipewire_microphone_t*)mic_context;
   if (!mic)
      return false;
   return pw_stream_get_state(mic->stream, &error) == PW_STREAM_STATE_STREAMING;
}

static void pipewire_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   pipewire_core_t *pw = (pipewire_core_t*)driver_context;
   if (pw)
      pw->nonblock = nonblock;
}

static struct string_list *pipewire_microphone_device_list_new(const void *driver_context)
{
   pipewire_core_t *pw = (pipewire_core_t*)driver_context;

   if (pw && pw->devicelist)
      return string_list_clone(pw->devicelist);

   return NULL;
}

static void pipewire_microphone_device_list_free(const void *driver_context, struct string_list *devices)
{
   if (devices)
      string_list_free(devices);
}

static void *pipewire_microphone_open_mic(void *driver_context,
   const char *device, unsigned rate, unsigned latency,
   unsigned *new_rate)
{
   int res;
   uint64_t buf_samples;
   uint8_t buffer[1024];
   const struct spa_pod *params[1];
   struct pw_properties *props = NULL;
   const char           *error = NULL;
   struct spa_pod_builder    b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
   pipewire_microphone_t   *mic = NULL;

   if (!driver_context || (mic = calloc(1, sizeof(pipewire_microphone_t))) == NULL)
      goto error;

   mic->pw = (pipewire_core_t*)driver_context;

   pw_thread_loop_lock(mic->pw->thread_loop);

   mic->info.format   = is_little_endian() ? SPA_AUDIO_FORMAT_F32_LE : SPA_AUDIO_FORMAT_F32_BE;
   mic->info.channels = DEFAULT_CHANNELS;
   pipewire_set_position(DEFAULT_CHANNELS, mic->info.position);
   mic->info.rate     = rate;
   mic->frame_size    = pipewire_calc_frame_size(mic->info.format, DEFAULT_CHANNELS);

   props = pw_properties_new(PW_KEY_MEDIA_TYPE,          PW_RARCH_MEDIA_TYPE_AUDIO,
                             PW_KEY_MEDIA_CATEGORY,      PW_RARCH_MEDIA_CATEGORY_RECORD,
                             PW_KEY_MEDIA_ROLE,          PW_RARCH_MEDIA_ROLE,
                             PW_KEY_NODE_NAME,           PW_RARCH_APPNAME,
                             PW_KEY_NODE_DESCRIPTION,    PW_RARCH_APPNAME,
                             PW_KEY_APP_NAME,            PW_RARCH_APPNAME,
                             PW_KEY_APP_ID,              PW_RARCH_APPNAME,
                             PW_KEY_APP_ICON_NAME,       PW_RARCH_APPNAME,
                             NULL);
   if (!props)
      goto unlock_error;

   if (device)
      pw_properties_set(props, PW_KEY_TARGET_OBJECT, device);

   buf_samples = latency * rate / 1000;
   pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%" PRIu64 "/%u", buf_samples, rate);
   pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%d", rate);

   if (!(mic->stream = pw_stream_new(mic->pw->core, PW_RARCH_APPNAME, props)))
      goto unlock_error;

   pw_stream_add_listener(mic->stream, &mic->stream_listener, &capture_stream_events, mic);

   params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &mic->info);

   /* Now connect this stream. We ask that our process function is
    * called in a realtime thread. */
   res = pw_stream_connect(mic->stream, PW_DIRECTION_INPUT, PW_ID_ANY,
           PW_STREAM_FLAG_AUTOCONNECT
         | PW_STREAM_FLAG_INACTIVE
         | PW_STREAM_FLAG_MAP_BUFFERS
         | PW_STREAM_FLAG_RT_PROCESS,
         params, 1);

   if (res < 0)
      goto unlock_error;

   pw_thread_loop_wait(mic->pw->thread_loop);
   pw_thread_loop_unlock(mic->pw->thread_loop);

   *new_rate = mic->info.rate;

   return mic;

unlock_error:
   pw_thread_loop_unlock(mic->pw->thread_loop);
error:
   RARCH_ERR("[Microphone] [PipeWire]: Failed to initialize microphone...\n");
   pipewire_microphone_close_mic(mic->pw, mic);
   return NULL;
}

static bool pipewire_microphone_start_mic(void *driver_context, void *mic_context)
{
   enum pw_stream_state st;
   pipewire_core_t        *pw = (pipewire_core_t*)driver_context;
   pipewire_microphone_t *mic = (pipewire_microphone_t*)mic_context;
   const char          *error = NULL;
   bool                   res = false;

   if (!pw || !mic)
      return false;

   st = pw_stream_get_state(mic->stream, &error);
   switch (st)
   {
      case PW_STREAM_STATE_STREAMING:
         res = true;
         break;
      case PW_STREAM_STATE_PAUSED:
         res = pipewire_stream_set_active(pw->thread_loop, mic->stream, true);
         break;
      default:
         break;
   }

   return res;
}

static bool pipewire_microphone_stop_mic(void *driver_context, void *mic_context)
{
   pipewire_core_t       *pw = (pipewire_core_t*)driver_context;
   pipewire_microphone_t *mic = (pipewire_microphone_t*)mic_context;
   const char          *error = NULL;
   bool                   res = false;

   if (!pw || !mic)
      return false;

   if (pw_stream_get_state(mic->stream, &error) == PW_STREAM_STATE_STREAMING)
      res = pipewire_stream_set_active(pw->thread_loop, mic->stream, false);
   else
      /* For other states we assume that the stream is inactive */
      res = true;

   spa_ringbuffer_read_update(&mic->ring, 0);
   spa_ringbuffer_write_update(&mic->ring, 0);

   return res;
}

static bool pipewire_microphone_mic_use_float(const void *a, const void *b)
{
   return true;
}

microphone_driver_t microphone_pipewire = {
      pipewire_microphone_init,
      pipewire_microphone_free,
      pipewire_microphone_read,
      pipewire_microphone_set_nonblock_state,
      "pipewire",
      pipewire_microphone_device_list_new,
      pipewire_microphone_device_list_free,
      pipewire_microphone_open_mic,
      pipewire_microphone_close_mic,
      pipewire_microphone_mic_alive,
      pipewire_microphone_start_mic,
      pipewire_microphone_stop_mic,
      pipewire_microphone_mic_use_float
};
