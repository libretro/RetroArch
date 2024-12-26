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


#define APPNAME "RetroArch"
#define DEFAULT_CHANNELS 1
#define QUANTUM 1024  /* TODO: detect */

typedef pipewire_core_t pipewire_microphone_t;
typedef pipewire_device_handle_t pipewire_microphone_handle_t;

static void on_stream_state_changed(void *data,
      enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
   pipewire_microphone_handle_t *microphone = (pipewire_microphone_handle_t*)data;

   RARCH_DBG("[PipeWire]: New state for Source Node %d : %s\n",
             pw_stream_get_node_id(microphone->stream),
             pw_stream_state_as_string(state));

   switch(state)
   {
      case PW_STREAM_STATE_STREAMING:
         microphone->is_paused = false;
         pw_thread_loop_signal(microphone->pw->thread_loop, false);
         break;
      case PW_STREAM_STATE_ERROR:
      case PW_STREAM_STATE_PAUSED:
         microphone->is_paused = true;
         pw_thread_loop_signal(microphone->pw->thread_loop, false);
         break;
      default:
         break;
   }
}

static void stream_destroy(void *data)
{
   pipewire_microphone_handle_t *microphone = (pipewire_microphone_handle_t*)data;
   spa_hook_remove(&microphone->stream_listener);
   microphone->stream = NULL;
}

static void on_process(void *data)
{
   pipewire_microphone_handle_t *microphone = (pipewire_microphone_handle_t *)data;
   void *p;
   struct pw_buffer *b;
   struct spa_buffer *buf;
   int32_t filled;
   uint32_t index, offs, n_bytes;

   assert(microphone->stream);

   b = pw_stream_dequeue_buffer(microphone->stream);
   if (b == NULL)
   {
      RARCH_ERR("[PipeWire]: out of buffers: %s\n", strerror(errno));
      return;
   }

   buf = b->buffer;
   p = buf->datas[0].data;
   if (p == NULL)
      return;

   offs = SPA_MIN(buf->datas[0].chunk->offset, buf->datas[0].maxsize);
   n_bytes = SPA_MIN(buf->datas[0].chunk->size, buf->datas[0].maxsize - offs);

   filled = spa_ringbuffer_get_write_index(&microphone->ring, &index);
   if (filled < 0)
      RARCH_ERR("[PipeWire]: %p: underrun write:%u filled:%d\n", p, index, filled);
   else
   {
      if ((uint32_t) filled + n_bytes > RINGBUFFER_SIZE)
         RARCH_ERR("[PipeWire]: %p: overrun write:%u filled:%d + size:%u > max:%u\n",
                   p, index, filled, n_bytes, RINGBUFFER_SIZE);
   }
   spa_ringbuffer_write_data(&microphone->ring,
                             microphone->buffer, RINGBUFFER_SIZE,
                             index & RINGBUFFER_MASK,
                             SPA_PTROFF(p, offs, void), n_bytes);
   index += n_bytes;
   spa_ringbuffer_write_update(&microphone->ring, index);

   pw_stream_queue_buffer(microphone->stream, b);
}

static const struct pw_stream_events capture_stream_events = {
      PW_VERSION_STREAM_EVENTS,
     .destroy = stream_destroy,
     .state_changed = on_stream_state_changed,
     .process = on_process
};

static void on_core_error(void *data, uint32_t id, int seq, int res, const char *message)
{
   pipewire_microphone_t *pipewire = data;

   RARCH_ERR("[PipeWire]: error id:%u seq:%d res:%d (%s): %s\n",
             id, seq, res, spa_strerror(res), message);

   /* stop and exit the thread loop */
   pw_thread_loop_signal(pipewire->thread_loop, false);
}

static void on_core_done(void *data, uint32_t id, int seq)
{
   pipewire_microphone_t *pipewire = (pipewire_microphone_t*)data;

   retro_assert(id == PW_ID_CORE);

   pipewire->last_seq = seq;
   if (pipewire->pending_seq == seq)
   {
      /* stop and exit the thread loop */
      pw_thread_loop_signal(pipewire->thread_loop, false);
   }
}

static const struct pw_core_events core_events = {
      PW_VERSION_CORE_EVENTS,
      .done = on_core_done,
      .error = on_core_error,
};

static void registry_event_global(void *data, uint32_t id,
                uint32_t permissions, const char *type, uint32_t version,
                const struct spa_dict *props)
{
   union string_list_elem_attr attr;
   pipewire_microphone_t *pipewire = (pipewire_microphone_t*)data;
   const char               *media = NULL;
   const char                *sink = NULL;

   if (!pipewire)
      return;

   if (spa_streq(type, PW_TYPE_INTERFACE_Node))
   {
      media = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
      if (media && strcmp(media, "Audio/Source") == 0)
      {
         if ((sink = spa_dict_lookup(props, PW_KEY_NODE_NAME)) != NULL)
         {
            attr.i = id;
            string_list_append(pipewire->devicelist, sink, attr);
            RARCH_LOG("[PipeWire]: Found Source Node: %s\n", sink);
         }
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
   int                           res;
   uint64_t              buf_samples;
   const struct spa_pod   *params[1];
   uint8_t              buffer[1024];
   struct pw_properties       *props = NULL;
   const char                 *error = NULL;
   pipewire_microphone_t   *pipewire = (pipewire_microphone_t*)calloc(1, sizeof(*pipewire));
   struct spa_pod_builder          b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

   if (!pipewire)
      goto error;

   pw_init(NULL, NULL);

   pipewire->devicelist = string_list_new();
   if (!pipewire->devicelist)
      goto error;

   pipewire->thread_loop = pw_thread_loop_new("microphone_driver", NULL);
   if (!pipewire->thread_loop)
      goto error;

   pipewire->context = pw_context_new(pw_thread_loop_get_loop(pipewire->thread_loop), NULL, 0);
   if (!pipewire->context)
      goto error;

   if (pw_thread_loop_start(pipewire->thread_loop) < 0)
      goto error;

   pw_thread_loop_lock(pipewire->thread_loop);

   pipewire->core = pw_context_connect(pipewire->context, NULL, 0);
   if(!pipewire->core)
      goto error;

   if (pw_core_add_listener(pipewire->core,
                            &pipewire->core_listener,
                            &core_events, pipewire) < 0)
      goto error;

   pipewire->registry = pw_core_get_registry(pipewire->core, PW_VERSION_REGISTRY, 0);

   spa_zero(pipewire->registry_listener);
   pw_registry_add_listener(pipewire->registry, &pipewire->registry_listener, &registry_events, pipewire);

   if (pipewire_wait_resync(pipewire) < 0)
      pw_thread_loop_unlock(pipewire->thread_loop);


   pw_thread_loop_unlock(pipewire->thread_loop);

   return pipewire;

error:
   RARCH_ERR("[PipeWire]: Failed to initialize microphone\n");
   pipewire_microphone_free(pipewire);
   return NULL;
}

static void pipewire_microphone_close_mic(void *driver_context, void *microphone_context);
static void pipewire_microphone_free(void *driver_context)
{
   pipewire_microphone_t *pipewire = (pipewire_microphone_t*)driver_context;

   if (!pipewire)
      return pw_deinit();

   if (pipewire->thread_loop)
      pw_thread_loop_stop(pipewire->thread_loop);

   if (pipewire->client)
      pw_proxy_destroy((struct pw_proxy *)pipewire->client);

   if (pipewire->registry)
      pw_proxy_destroy((struct pw_proxy*)pipewire->registry);

   if (pipewire->core)
   {
      spa_hook_remove(&pipewire->core_listener);
      spa_zero(pipewire->core_listener);
      pw_core_disconnect(pipewire->core);
   }

   if (pipewire->context)
      pw_context_destroy(pipewire->context);

   pw_thread_loop_destroy(pipewire->thread_loop);

   free(pipewire);
   pw_deinit();
}

static bool pipewire_microphone_start_mic(void *driver_context, void *microphone_context);
static int pipewire_microphone_read(void *driver_context, void *microphone_context, void *buf_, size_t size_)
{
   int32_t                         readable;
   uint32_t                           index;
   const char                        *error = NULL;
   pipewire_microphone_t          *pipewire = (pipewire_microphone_t*)driver_context;
   pipewire_microphone_handle_t *microphone = (pipewire_microphone_handle_t*)microphone_context;

   pw_thread_loop_lock(pipewire->thread_loop);
   if (pw_stream_get_state(microphone->stream, &error) != PW_STREAM_STATE_STREAMING)
      goto unlock;

   /* get no of available bytes to read data from buffer */
   readable = spa_ringbuffer_get_read_index(&microphone->ring, &index);

   if (readable < (int32_t)size_)
      size_ = readable;

   spa_ringbuffer_read_data(&microphone->ring,
                            microphone->buffer, RINGBUFFER_SIZE,
                            index & RINGBUFFER_MASK, buf_, size_);
   index += size_;
   spa_ringbuffer_read_update(&microphone->ring, index);

unlock:
   pw_thread_loop_unlock(pipewire->thread_loop);
   return size_;
}

static bool pipewire_microphone_mic_alive(const void *driver_context, const void *microphone_context)
{
   const char                        *error = NULL;
   pipewire_microphone_handle_t *microphone = (pipewire_microphone_handle_t*)microphone_context;
   (void)driver_context;

   if (!microphone)
      return false;

   return pw_stream_get_state(microphone->stream, &error) == PW_STREAM_STATE_STREAMING;
}

static void pipewire_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   pipewire_microphone_t *pipewire = (pipewire_microphone_t*)driver_context;
   if (pipewire)
      pipewire->nonblock = nonblock;
}

static struct string_list *pipewire_microphone_device_list_new(const void *driver_context)
{
   pipewire_microphone_t *pipewire = (pipewire_microphone_t*)driver_context;

   if (pipewire && pipewire->devicelist)
      return string_list_clone(pipewire->devicelist);

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
   int                                  res;
   uint64_t                     buf_samples;
   const struct spa_pod          *params[1];
   uint8_t                     buffer[1024];
   struct pw_properties              *props = NULL;
   const char                        *error = NULL;
   struct spa_pod_builder                 b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
   pipewire_microphone_handle_t *microphone = calloc(1, sizeof(pipewire_microphone_handle_t));

   retro_assert(driver_context);

   if (!microphone)
      goto error;

   microphone->pw = (pipewire_microphone_t*)driver_context;

   pw_thread_loop_lock(microphone->pw->thread_loop);

   microphone->info.format = is_little_endian() ? SPA_AUDIO_FORMAT_F32_LE : SPA_AUDIO_FORMAT_F32_BE;
   microphone->info.channels = DEFAULT_CHANNELS;
   set_position(DEFAULT_CHANNELS, microphone->info.position);
   microphone->info.rate = rate;
   microphone->frame_size = calc_frame_size(microphone->info.format, DEFAULT_CHANNELS);

   props = pw_properties_new(PW_KEY_MEDIA_TYPE,          "Audio",
                             PW_KEY_MEDIA_CATEGORY,      "Capture",
                             PW_KEY_MEDIA_ROLE,          "Game",
                             PW_KEY_NODE_NAME,           APPNAME,
                             PW_KEY_NODE_DESCRIPTION,    APPNAME,
                             PW_KEY_APP_NAME,            APPNAME,
                             PW_KEY_APP_ID,              APPNAME,
                             PW_KEY_APP_ICON_NAME,       APPNAME,
                             PW_KEY_NODE_ALWAYS_PROCESS, "true",
                             NULL);
   if (!props)
      goto unlock_error;

   if (device)
      pw_properties_set(props, PW_KEY_TARGET_OBJECT, device);

   buf_samples = QUANTUM * rate * 3 / 4 / 100000;

   pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%" PRIu64 "/%u",
                      buf_samples, rate);

   pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%d", rate);

   microphone->stream = pw_stream_new(microphone->pw->core, APPNAME, props);

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
   if(pw_stream_get_state(microphone->stream, &error) != PW_STREAM_STATE_STREAMING)
      microphone->is_paused = true;

   pw_thread_loop_unlock(microphone->pw->thread_loop);
   *new_rate = microphone->info.rate;

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
   pipewire_microphone_t          *pipewire = (pipewire_microphone_t*)driver_context;
   pipewire_microphone_handle_t *microphone = (pipewire_microphone_handle_t*)microphone_context;

   if (pipewire && microphone)
   {
      pw_thread_loop_lock(pipewire->thread_loop);
      pw_stream_destroy(microphone->stream);
      microphone->stream = NULL;
      pw_thread_loop_unlock(pipewire->thread_loop);
   }
}

static bool pipewire_microphone_start_mic(void *driver_context, void *microphone_context)
{
   pipewire_microphone_t          *pipewire = (pipewire_microphone_t*)driver_context;
   pipewire_microphone_handle_t *microphone = (pipewire_microphone_handle_t*)microphone_context;

   if (!microphone->is_paused)
      return true;

   return pipewire_set_active(pipewire, microphone, true);
}

static bool pipewire_microphone_stop_mic(void *driver_context, void *microphone_context)
{
   pipewire_microphone_t          *pipewire = (pipewire_microphone_t*)driver_context;
   pipewire_microphone_handle_t *microphone = (pipewire_microphone_handle_t*)microphone_context;
   if (microphone->is_paused)
      return true;

   return pipewire_set_active(pipewire, microphone, false);
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
