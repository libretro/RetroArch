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

#include <lists/string_list.h>
#include <retro_assert.h>

#include <spa/param/audio/format-utils.h>
#include <spa/utils/ringbuffer.h>
#include <spa/utils/result.h>
#include <spa/param/props.h>

#include <pipewire/pipewire.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>

#include "audio/common/pipewire.h"
#include "audio/microphone_driver.h"
#include "verbosity.h"


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
   bool is_ready;
} pipewire_microphone_t;

static void stream_state_changed_cb(void *data,
      enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
   pipewire_microphone_t *microphone = (pipewire_microphone_t*)data;

   RARCH_DBG("[PipeWire]: New state for Source Node %d : %s\n",
             pw_stream_get_node_id(microphone->stream),
             pw_stream_state_as_string(state));

   switch(state)
   {
      case PW_STREAM_STATE_UNCONNECTED:
         microphone->is_ready = false;
         pw_thread_loop_stop(microphone->pw->thread_loop);
         break;
      case PW_STREAM_STATE_STREAMING:
      case PW_STREAM_STATE_ERROR:
      case PW_STREAM_STATE_PAUSED:
         pw_thread_loop_signal(microphone->pw->thread_loop, false);
         break;
      default:
         break;
   }
}

static void stream_destroy_cb(void *data)
{
   pipewire_microphone_t *microphone = (pipewire_microphone_t*)data;
   spa_hook_remove(&microphone->stream_listener);
   microphone->stream = NULL;
}

static void capture_process_cb(void *data)
{
   pipewire_microphone_t *microphone = (pipewire_microphone_t *)data;
   void *p;
   struct pw_buffer *b;
   struct spa_buffer *buf;
   int32_t filled;
   uint32_t idx, offs, n_bytes;

   assert(microphone->stream);

   b = pw_stream_dequeue_buffer(microphone->stream);
   if (b == NULL)
   {
      RARCH_ERR("[PipeWire]: out of buffers: %s\n", strerror(errno));
      return;
   }

   buf = b->buffer;
   if ((p = buf->datas[0].data) == NULL)
      goto done;

   offs = SPA_MIN(buf->datas[0].chunk->offset, buf->datas[0].maxsize);
   n_bytes = SPA_MIN(buf->datas[0].chunk->size, buf->datas[0].maxsize - offs);

   filled = spa_ringbuffer_get_write_index(&microphone->ring, &idx);
   if (filled < 0)
      RARCH_ERR("[PipeWire]: %p: underrun write:%u filled:%d\n", p, idx, filled);
   else
   {
      if ((uint32_t)filled + n_bytes > RINGBUFFER_SIZE)
         RARCH_ERR("[PipeWire]: %p: overrun write:%u filled:%d + size:%u > max:%u\n",
                   p, idx, filled, n_bytes, RINGBUFFER_SIZE);
   }
   spa_ringbuffer_write_data(&microphone->ring,
                             microphone->buffer, RINGBUFFER_SIZE,
                             idx & RINGBUFFER_MASK,
                             SPA_PTROFF(p, offs, void), n_bytes);
   idx += n_bytes;
   spa_ringbuffer_write_update(&microphone->ring, idx);

done:
   pw_stream_queue_buffer(microphone->stream, b);
   pw_thread_loop_signal(microphone->pw->thread_loop, false);
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
   const char                *media = NULL;
   const char                 *sink = NULL;

   if (!pw)
      return;

   if (spa_streq(type, PW_TYPE_INTERFACE_Node))
   {
      media = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
      if (media && strcmp(media, "Audio/Source") == 0)
      {
         if ((sink = spa_dict_lookup(props, PW_KEY_NODE_NAME)) != NULL)
         {
            attr.i = id;
            string_list_append(pw->devicelist, sink, attr);
            RARCH_LOG("[PipeWire]: Found Source Node: %s\n", sink);
         }

         RARCH_DBG("[PipeWire]: Object: id:%u Type:%s/%d\n", id, type, version);
         spa_dict_for_each(item, props)
            RARCH_DBG("[PipeWire]: \t\t%s: \"%s\"\n", item->key, item->value);
      }
   }
}

static const struct pw_registry_events registry_events = {
      PW_VERSION_REGISTRY_EVENTS,
      .global = registry_event_global,
};

static void pipewire_microphone_free(void *driver_context);

static void *pipewire_microphone_init(void)
{
   int                         res;
   uint64_t            buf_samples;
   const struct spa_pod *params[1];
   uint8_t            buffer[1024];
   struct pw_properties     *props = NULL;
   const char               *error = NULL;
   pipewire_core_t             *pw = (pipewire_core_t*)calloc(1, sizeof(*pw));
   struct spa_pod_builder        b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

   if (!pw)
      goto error;

   pw_init(NULL, NULL);

   pw->devicelist = string_list_new();
   if (!pw->devicelist)
      goto error;

   if (!pipewire_core_init(pw, "microphone_driver"))
      goto error;

   pw->registry = pw_core_get_registry(pw->core, PW_VERSION_REGISTRY, 0);

   spa_zero(pw->registry_listener);
   pw_registry_add_listener(pw->registry, &pw->registry_listener, &registry_events, pw);

   pipewire_core_wait_resync(pw);
   pw_thread_loop_unlock(pw->thread_loop);

   return pw;

error:
   RARCH_ERR("[PipeWire]: Failed to initialize microphone\n");
   pipewire_microphone_free(pw);
   return NULL;
}

static void pipewire_microphone_close_mic(void *driver_context, void *microphone_context);
static void pipewire_microphone_free(void *driver_context)
{
   pipewire_core_t *pw = (pipewire_core_t*)driver_context;

   if (!pw)
      return pw_deinit();

   if (pw->thread_loop)
      pw_thread_loop_stop(pw->thread_loop);

   if (pw->registry)
      pw_proxy_destroy((struct pw_proxy*)pw->registry);

   if (pw->core)
   {
      spa_hook_remove(&pw->core_listener);
      spa_zero(pw->core_listener);
      pw_core_disconnect(pw->core);
   }

   if (pw->ctx)
      pw_context_destroy(pw->ctx);

   pw_thread_loop_destroy(pw->thread_loop);

   if (pw->devicelist)
      string_list_free(pw->devicelist);

   free(pw);
   pw_deinit();
}

static int pipewire_microphone_read(void *driver_context, void *microphone_context, void *buf_, size_t size_)
{
   int32_t                  readable;
   uint32_t                      idx;
   const char                 *error = NULL;
   pipewire_core_t               *pw = (pipewire_core_t*)driver_context;
   pipewire_microphone_t *microphone = (pipewire_microphone_t*)microphone_context;

   if (  !microphone->is_ready
       || pw_stream_get_state(microphone->stream, &error) != PW_STREAM_STATE_STREAMING)
      return -1;

   pw_thread_loop_lock(pw->thread_loop);

   while (size_)
   {
      /* get no of available bytes to read data from buffer */
      readable = spa_ringbuffer_get_read_index(&microphone->ring, &idx);

      if (readable < (int32_t)size_)
      {
         if (pw->nonblock)
         {
            size_ = readable;
            break;
         }

         pw_thread_loop_wait(pw->thread_loop);
      }
      else
         break;
   }

   spa_ringbuffer_read_data(&microphone->ring,
                            microphone->buffer, RINGBUFFER_SIZE,
                            idx & RINGBUFFER_MASK, buf_, size_);
   idx += size_;
   spa_ringbuffer_read_update(&microphone->ring, idx);
   pw_thread_loop_unlock(pw->thread_loop);

   return size_;
}

static bool pipewire_microphone_mic_alive(const void *driver_context, const void *microphone_context)
{
   const char                 *error = NULL;
   pipewire_microphone_t *microphone = (pipewire_microphone_t*)microphone_context;
   (void)driver_context;

   if (!microphone)
      return false;

   return pw_stream_get_state(microphone->stream, &error) == PW_STREAM_STATE_STREAMING;
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
   (void)driver_context;
   if (devices)
      string_list_free(devices);
}

static void *pipewire_microphone_open_mic(void *driver_context,
   const char *device,
   unsigned rate,
   unsigned latency,
   unsigned *new_rate)
{
   int                             res;
   uint64_t                buf_samples;
   const struct spa_pod     *params[1];
   uint8_t                buffer[1024];
   struct pw_properties         *props = NULL;
   const char                   *error = NULL;
   struct spa_pod_builder            b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
   pipewire_microphone_t   *microphone = calloc(1, sizeof(pipewire_microphone_t));

   retro_assert(driver_context);

   if (!microphone)
      goto error;

   microphone->is_ready = false;
   microphone->pw = (pipewire_core_t*)driver_context;

   pw_thread_loop_lock(microphone->pw->thread_loop);

   microphone->info.format = is_little_endian() ? SPA_AUDIO_FORMAT_F32_LE : SPA_AUDIO_FORMAT_F32_BE;
   microphone->info.channels = DEFAULT_CHANNELS;
   pipewire_set_position(DEFAULT_CHANNELS, microphone->info.position);
   microphone->info.rate = rate;
   microphone->frame_size = pipewire_calc_frame_size(microphone->info.format, DEFAULT_CHANNELS);

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
   microphone->stream = pw_stream_new(microphone->pw->core, PW_RARCH_APPNAME, props);

   if (!microphone->stream)
      goto unlock_error;

   pw_stream_add_listener(microphone->stream, &microphone->stream_listener, &capture_stream_events, microphone);

   params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &microphone->info);

   /* Now connect this stream. We ask that our process function is
    * called in a realtime thread. */
   res = pw_stream_connect(microphone->stream,
                           PW_DIRECTION_INPUT,
                           PW_ID_ANY,
                           PW_STREAM_FLAG_AUTOCONNECT |
                           PW_STREAM_FLAG_INACTIVE |
                           PW_STREAM_FLAG_MAP_BUFFERS |
                           PW_STREAM_FLAG_RT_PROCESS,
                           params, 1);

   if (res < 0)
      goto unlock_error;

   pw_thread_loop_wait(microphone->pw->thread_loop);
   pw_thread_loop_unlock(microphone->pw->thread_loop);

   *new_rate = microphone->info.rate;

   microphone->is_ready = true;
   return microphone;

unlock_error:
   pw_thread_loop_unlock(microphone->pw->thread_loop);
error:
   RARCH_ERR("[PipeWire]: Failed to initialize microphone...\n");
   pipewire_microphone_close_mic(microphone->pw, microphone);
   return NULL;
}

static void pipewire_microphone_close_mic(void *driver_context, void *microphone_context)
{
   pipewire_core_t               *pw = (pipewire_core_t*)driver_context;
   pipewire_microphone_t *microphone = (pipewire_microphone_t*)microphone_context;

   if (pw && microphone)
   {
      pw_thread_loop_lock(pw->thread_loop);
      pw_stream_destroy(microphone->stream);
      microphone->stream = NULL;
      pw_thread_loop_unlock(pw->thread_loop);
      free(microphone);
   }
}

static bool pipewire_microphone_start_mic(void *driver_context, void *microphone_context)
{
   pipewire_core_t               *pw = (pipewire_core_t*)driver_context;
   pipewire_microphone_t *microphone = (pipewire_microphone_t*)microphone_context;
   const char                 *error = NULL;

   if (!pw || !microphone || !microphone->is_ready)
      return false;
   if (pw_stream_get_state(microphone->stream, &error) == PW_STREAM_STATE_STREAMING)
      return true;

   return pipewire_stream_set_active(pw->thread_loop, microphone->stream, true);
}

static bool pipewire_microphone_stop_mic(void *driver_context, void *microphone_context)
{
   pipewire_core_t               *pw = (pipewire_core_t*)driver_context;
   pipewire_microphone_t *microphone = (pipewire_microphone_t*)microphone_context;
   const char                 *error = NULL;
   bool                          res = false;

   if (!pw || !microphone || !microphone->is_ready)
      return false;
   if (pw_stream_get_state(microphone->stream, &error) == PW_STREAM_STATE_PAUSED)
      return true;

   res = pipewire_stream_set_active(pw->thread_loop, microphone->stream, false);
   spa_ringbuffer_read_update(&microphone->ring, 0);
   spa_ringbuffer_write_update(&microphone->ring, 0);

   return res;
}

static bool pipewire_microphone_mic_use_float(const void *driver_context, const void *microphone_context)
{
   (void)driver_context;
   (void)microphone_context;
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
