/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024 - Viachaslau Khalikin
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
#include <string.h>

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
#include "audio/audio_driver.h"
#include "verbosity.h"


#define APPNAME "RetroArch"
#define DEFAULT_CHANNELS 2
#define QUANTUM 1024  /* TODO: detect */

static void stream_destroy(void *data)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   spa_hook_remove(&audio->stream_listener);
   audio->stream = NULL;
}

static void on_process(void *data)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   void *p;
   struct pw_buffer *b;
   struct spa_buffer *buf;
   uint32_t req, index, n_bytes;
   int32_t avail;

   retro_assert(audio->stream);

   if ((b = pw_stream_dequeue_buffer(audio->stream)) == NULL)
   {
      RARCH_WARN("[PipeWire]: Out of buffers: %s\n", strerror(errno));
      return;
   }

   buf = b->buffer;
   p = buf->datas[0].data;
   if (p == NULL)
      return;

   /* calculate the total no of bytes to read data from buffer */
   req = b->requested * audio->frame_size;

   if (req == 0)
      req = audio->req;

   n_bytes = SPA_MIN(req, buf->datas[0].maxsize);

   /* get no of available bytes to read data from buffer */
   avail = spa_ringbuffer_get_read_index(&audio->ring, &index);

   if (avail <= 0)
      /* fill rest buffer with silence */
      memset(p, 0x00, n_bytes);
   else
   {
      if (avail < (int32_t)n_bytes)
         n_bytes = avail;

      spa_ringbuffer_read_data(&audio->ring,
                               audio->buffer, RINGBUFFER_SIZE,
                               index & RINGBUFFER_MASK, p, n_bytes);

      index += n_bytes;
      spa_ringbuffer_read_update(&audio->ring, index);
   }

   buf->datas[0].chunk->offset = 0;
   buf->datas[0].chunk->stride = audio->frame_size;
   buf->datas[0].chunk->size   = n_bytes;

   /* queue the buffer for playback */
   pw_stream_queue_buffer(audio->stream, b);
}

static void on_stream_state_changed(void *data,
      enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;

   RARCH_DBG("[PipeWire]: New state for Sink Node %d : %s\n",
             pw_stream_get_node_id(audio->stream),
             pw_stream_state_as_string(state));

   switch(state)
   {
      case PW_STREAM_STATE_STREAMING:
         audio->is_paused = false;
         pw_thread_loop_signal(audio->pw->thread_loop, false);
         break;
      case PW_STREAM_STATE_ERROR:
      case PW_STREAM_STATE_PAUSED:
         audio->is_paused = true;
         pw_thread_loop_signal(audio->pw->thread_loop, false);
         break;
      default:
         break;
   }
}

static const struct pw_stream_events playback_stream_events = {
      PW_VERSION_STREAM_EVENTS,
      .destroy = stream_destroy,
      .process = on_process,
      .state_changed = on_stream_state_changed,
};

static void client_info(void *data, const struct pw_client_info *info)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   const struct spa_dict_item *item;

   RARCH_DBG("[PipeWire]: client: id:%u\n", info->id);
   RARCH_DBG("[PipeWire]: \tprops:\n");
   spa_dict_for_each(item, info->props)
      RARCH_DBG("[PipeWire]: \t\t%s: \"%s\"\n", item->key, item->value);

   pw_thread_loop_signal(audio->pw->thread_loop, false);
}

static const struct pw_client_events client_events = {
      PW_VERSION_CLIENT_EVENTS,
      .info = client_info,
};

static void on_core_error(void *data, uint32_t id, int seq, int res, const char *message)
{
   pipewire_device_handle_t *audio = data;

   RARCH_ERR("[PipeWire]: error id:%u seq:%d res:%d (%s): %s\n",
             id, seq, res, spa_strerror(res), message);

   /* stop and exit the thread loop */
   pw_thread_loop_signal(audio->pw->thread_loop, false);
}

static void on_core_done(void *data, uint32_t id, int seq)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;

   retro_assert(id == PW_ID_CORE);

   audio->pw->last_seq = seq;
   if (audio->pw->pending_seq == seq)
   {
      /* stop and exit the thread loop */
      pw_thread_loop_signal(audio->pw->thread_loop, false);
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
   pipewire_device_handle_t   *audio = (pipewire_device_handle_t*)data;
   pipewire_core_t        *pipewire = NULL;
   const char                *media = NULL;
   const char                 *sink = NULL;

   if (!audio || !audio->pw)
      return;

   pipewire = audio->pw;

   if (!pipewire->client && spa_streq(type, PW_TYPE_INTERFACE_Client))
   {
      pipewire->client = pw_registry_bind(audio->pw->registry,
                                          id, type, PW_VERSION_CLIENT, 0);
      pw_client_add_listener(audio->pw->client,
                             &audio->pw->client_listener,
                             &client_events, audio);
   }
   else if (spa_streq(type, PW_TYPE_INTERFACE_Node))
   {
      media = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
      if (media && strcmp(media, "Audio/Sink") == 0)
      {
         if ((sink = spa_dict_lookup(props, PW_KEY_NODE_NAME)) != NULL)
         {
            attr.i = id;
            string_list_append(audio->pw->devicelist, sink, attr);
            RARCH_LOG("[PipeWire]: Found Sink Node: %s\n", sink);
         }
      }
   }

   const struct spa_dict_item *item;
   RARCH_DBG("[PipeWire]: Object: id:%u Type:%s/%d\n", id, type, version);
   spa_dict_for_each(item, props)
      RARCH_DBG("[PipeWire]: \t\t%s: \"%s\"\n", item->key, item->value);
}

static const struct pw_registry_events registry_events = {
      PW_VERSION_REGISTRY_EVENTS,
      .global = registry_event_global,
};

static void pipewire_free(void *data);

static void *pipewire_init(const char *device, unsigned rate,
      unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   int                           res;
   uint64_t              buf_samples;
   const struct spa_pod   *params[1];
   uint8_t              buffer[1024];
   struct pw_properties       *props = NULL;
   const char                 *error = NULL;
   pipewire_device_handle_t    *audio = (pipewire_device_handle_t*)calloc(1, sizeof(*audio));
   pipewire_core_t         *pipewire = NULL;
   struct spa_pod_builder          b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

   if (!audio)
      goto error;

   pw_init(NULL, NULL);

   pipewire = audio->pw = (pipewire_core_t*)calloc(1, sizeof(*audio->pw));

   pipewire->devicelist = string_list_new();
   if (!pipewire->devicelist)
      goto error;

   pipewire->thread_loop = pw_thread_loop_new("audio_driver", NULL);
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
                            &core_events, audio) < 0)
      goto error;

   audio->info.format = is_little_endian() ? SPA_AUDIO_FORMAT_F32_LE : SPA_AUDIO_FORMAT_F32_BE;
   audio->info.channels = DEFAULT_CHANNELS;
   set_position(DEFAULT_CHANNELS, audio->info.position);
   audio->info.rate = rate;
   audio->frame_size = calc_frame_size(audio->info.format, DEFAULT_CHANNELS);
   audio->req = QUANTUM * rate * 1 / 2 / 100000 * audio->frame_size;

   props = pw_properties_new(PW_KEY_MEDIA_TYPE,          "Audio",
                             PW_KEY_MEDIA_CATEGORY,      "Playback",
                             PW_KEY_MEDIA_ROLE,          "Game",
                             PW_KEY_NODE_NAME,           APPNAME,
                             PW_KEY_NODE_DESCRIPTION,    APPNAME,
                             PW_KEY_APP_NAME,            APPNAME,
                             PW_KEY_APP_ID,              APPNAME,
                             PW_KEY_APP_ICON_NAME,       APPNAME,
                             PW_KEY_NODE_ALWAYS_PROCESS, "true",
                             NULL);
   if (!props)
      goto error;

   if (device)
      pw_properties_set(props, PW_KEY_TARGET_OBJECT, device);

   buf_samples = QUANTUM * rate * 3 / 4 / 100000;

   pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%" PRIu64 "/%u",
                      buf_samples, rate);

   pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%d", rate);

   audio->stream = pw_stream_new(pipewire->core, APPNAME, props);

   if (!audio->stream)
      goto error;

   pw_stream_add_listener(audio->stream, &audio->stream_listener, &playback_stream_events, audio);

   params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio->info);

   /* Now connect this stream. We ask that our process function is
    * called in a realtime thread. */
   res = pw_stream_connect(audio->stream,
                           PW_DIRECTION_OUTPUT,
                           PW_ID_ANY,
                           PW_STREAM_FLAG_AUTOCONNECT |
                           PW_STREAM_FLAG_INACTIVE |
                           PW_STREAM_FLAG_MAP_BUFFERS |
                           PW_STREAM_FLAG_RT_PROCESS,
                           params, 1);

   if (res < 0)
      goto error;

   audio->highwater_mark = MIN(RINGBUFFER_SIZE,
                               latency? (latency * 1000): 46440 * (uint64_t)rate / 1000000 * audio->frame_size);
   RARCH_DBG("[PipeWire]: Bufer size: %u, RingBuffer size: %u\n", audio->highwater_mark, RINGBUFFER_SIZE);

   audio->pw->registry = pw_core_get_registry(pipewire->core, PW_VERSION_REGISTRY, 0);

   spa_zero(pipewire->registry_listener);
   pw_registry_add_listener(pipewire->registry, &pipewire->registry_listener, &registry_events, audio);

   /* unlock, run the loop and wait, this will trigger the callbacks */
   if (pipewire_wait_resync(audio->pw) < 0)
      pw_thread_loop_unlock(pipewire->thread_loop);


   if(pw_stream_get_state(audio->stream, &error) != PW_STREAM_STATE_STREAMING)
      audio->is_paused = true;

   pw_thread_loop_unlock(audio->pw->thread_loop);
   *new_rate = audio->info.rate;

   return audio;

error:
   RARCH_ERR("[PipeWire]: Failed to initialize audio\n");
   pipewire_free(audio);
   return NULL;
}

static bool pipewire_start(void *data, bool is_shutdown);

static ssize_t pipewire_write(void *data, const void *buf_, size_t size)
{
   int32_t               writable;
   int32_t                  avail;
   uint32_t                 index;
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   const char              *error = NULL;

   /* Workaround buggy menu code.
    * If a write happens while we're paused, we might never progress. */
   if (audio->is_paused)
      if (!pipewire_start(audio, false))
         return -1;

   pw_thread_loop_lock(audio->pw->thread_loop);

   if (pw_stream_get_state(audio->stream, &error) != PW_STREAM_STATE_STREAMING)
   {
      /* wait for stream to become ready */
      size = 0;
      goto unlock;
   }
   writable = spa_ringbuffer_get_write_index(&audio->ring, &index);
   avail = audio->highwater_mark - writable;

#if 0  /* Useful for tracing */
   RARCH_DBG("[PipeWire]: Playback progress:  written %d, avail %d, index %d, size %d\n",
             writable, avail, index, size);
#endif

   if (size > (size_t)avail)
      size = avail;

   if (writable < 0)
      RARCH_ERR("%p: underrun write:%u filled:%d\n", audio, index, writable);
   else
   {
      if ((uint32_t) writable + size > RINGBUFFER_SIZE)
      {
         RARCH_ERR("%p: overrun write:%u filled:%d + size:%zu > max:%u\n",
         audio, index, writable, size, RINGBUFFER_SIZE);
      }
   }

   spa_ringbuffer_write_data(&audio->ring,
                             audio->buffer, RINGBUFFER_SIZE,
                             index & RINGBUFFER_MASK, buf_, size);
   index += size;
   spa_ringbuffer_write_update(&audio->ring, index);

unlock:
   pw_thread_loop_unlock(audio->pw->thread_loop);
   return size;
}

static bool pipewire_stop(void *data)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   if (audio->is_paused)
      return true;

   return pipewire_set_active(audio->pw, audio, false);
}

static bool pipewire_start(void *data, bool is_shutdown)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   if (!audio->is_paused)
      return true;

   return pipewire_set_active(audio->pw, audio, true);
}

static bool pipewire_alive(void *data)
{
   const char *error = NULL;
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   if (!audio)
      return false;

   return pw_stream_get_state(audio->stream, &error) == PW_STREAM_STATE_STREAMING;
}

static void pipewire_set_nonblock_state(void *data, bool state)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   if (audio && audio->pw)
      audio->pw->nonblock = state;
}

static void pipewire_free(void *data)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;

   if (!audio)
      return pw_deinit();

   if (audio->pw->thread_loop)
      pw_thread_loop_stop(audio->pw->thread_loop);

   if (audio->stream)
   {
      pw_stream_destroy(audio->stream);
      audio->stream = NULL;
   }

   if (audio->pw->client)
      pw_proxy_destroy((struct pw_proxy *)audio->pw->client);

   if (audio->pw->registry)
      pw_proxy_destroy((struct pw_proxy*)audio->pw->registry);

   if (audio->pw->core)
   {
      spa_hook_remove(&audio->pw->core_listener);
      spa_zero(audio->pw->core_listener);
      pw_core_disconnect(audio->pw->core);
   }

   if (audio->pw->context)
      pw_context_destroy(audio->pw->context);

   pw_thread_loop_destroy(audio->pw->thread_loop);

   free(audio->pw);
   free(audio);
   pw_deinit();
}

static bool pipewire_use_float(void *data)
{
   (void)data;
   return true;
}

static void *pipewire_device_list_new(void *data)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;

   if (audio && audio->pw->devicelist)
      return string_list_clone(audio->pw->devicelist);

   return NULL;
}

static void pipewire_device_list_free(void *data, void *array_list_data)
{
   struct string_list *s = (struct string_list*)array_list_data;

   if (s)
      string_list_free(s);
}

static size_t pipewire_write_avail(void *data)
{
   uint32_t index, written, length;
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   const char               *error = NULL;

   retro_assert(audio->pw);
   pw_thread_loop_lock(audio->pw->thread_loop);

   if (pw_stream_get_state(audio->stream, &error) != PW_STREAM_STATE_STREAMING)
   {
      /* wait for stream to become ready */
      length = 0;
      goto unlock;
   }

   written = spa_ringbuffer_get_write_index(&audio->ring, &index);
   length = audio->highwater_mark - written;
   audio_driver_set_buffer_size(audio->highwater_mark);

unlock:
   pw_thread_loop_unlock(audio->pw->thread_loop);
   return length;
}

static size_t pipewire_buffer_size(void *data)
{
   pipewire_device_handle_t *audio = (pipewire_device_handle_t*)data;
   return audio->highwater_mark;
}

audio_driver_t audio_pipewire = {
      pipewire_init,
      pipewire_write,
      pipewire_stop,
      pipewire_start,
      pipewire_alive,
      pipewire_set_nonblock_state,
      pipewire_free,
      pipewire_use_float,
      "pipewire",
      pipewire_device_list_new,
      pipewire_device_list_free,
      pipewire_write_avail,
      pipewire_buffer_size,
};
