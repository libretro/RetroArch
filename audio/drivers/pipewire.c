/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024-2025 - Viachaslau Khalikin
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


#define DEFAULT_CHANNELS   2
#define RINGBUFFER_SIZE    (1u << 22)
#define RINGBUFFER_MASK    (RINGBUFFER_SIZE - 1)

typedef struct pipewire_audio
{
   pipewire_core_t *pw;
   struct pw_stream *stream;
   struct spa_hook stream_listener;
   struct spa_audio_info_raw info;
   uint32_t highwater_mark;
   uint32_t frame_size;
   struct spa_ringbuffer ring;
   uint8_t buffer[RINGBUFFER_SIZE];
} pipewire_audio_t;

static void stream_destroy_cb(void *data)
{
   pipewire_audio_t *audio = (pipewire_audio_t*)data;
   spa_hook_remove(&audio->stream_listener);
   audio->stream = NULL;
}

static void playback_process_cb(void *data)
{
   pipewire_audio_t *audio = (pipewire_audio_t*)data;
   void *p;
   struct pw_buffer *b;
   struct spa_buffer *buf;
   uint32_t req, idx, n_bytes;
   int32_t avail;

   retro_assert(audio->stream);

   if ((b = pw_stream_dequeue_buffer(audio->stream)) == NULL)
   {
      RARCH_WARN("[PipeWire]: Out of buffers: %s\n", strerror(errno));
      return;
   }

   buf = b->buffer;
   if ((p = buf->datas[0].data) == NULL)
      goto done;

   /* calculate the total no of bytes to read data from buffer */
   n_bytes = buf->datas[0].maxsize;
   if (b->requested)
      n_bytes = SPA_MIN(b->requested * audio->frame_size, n_bytes);

   avail = spa_ringbuffer_get_read_index(&audio->ring, &idx);

   if (avail <= 0)
      /* fill rest buffer with silence */
      memset(p, 0x00, n_bytes);
   else
   {
      if (avail < (int32_t)n_bytes)
         n_bytes = avail;

      spa_ringbuffer_read_data(&audio->ring,
                               audio->buffer, RINGBUFFER_SIZE,
                               idx & RINGBUFFER_MASK, p, n_bytes);

      idx += n_bytes;
      spa_ringbuffer_read_update(&audio->ring, idx);
   }

   buf->datas[0].chunk->offset = 0;
   buf->datas[0].chunk->stride = audio->frame_size;
   buf->datas[0].chunk->size   = n_bytes;

done:
   pw_stream_queue_buffer(audio->stream, b);
   pw_thread_loop_signal(audio->pw->thread_loop, false);
}

static void pipewire_free(void *data);

static void stream_state_changed_cb(void *data,
      enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
   pipewire_audio_t *audio = (pipewire_audio_t*)data;

   RARCH_DBG("[PipeWire]: New state for Sink Node %d : %s\n",
             pw_stream_get_node_id(audio->stream),
             pw_stream_state_as_string(state));

   switch(state)
   {
      case PW_STREAM_STATE_ERROR:
         RARCH_ERR("[PipeWire]: Stream error\n");
         pw_thread_loop_signal(audio->pw->thread_loop, false);
         break;
      case PW_STREAM_STATE_UNCONNECTED:
         RARCH_WARN("[PipeWire]: Stream unconnected\n");
         pw_thread_loop_stop(audio->pw->thread_loop);
         break;
      case PW_STREAM_STATE_STREAMING:
      case PW_STREAM_STATE_PAUSED:
         pw_thread_loop_signal(audio->pw->thread_loop, false);
         break;
      default:
         break;
   }
}

static const struct pw_stream_events playback_stream_events = {
      PW_VERSION_STREAM_EVENTS,
      .destroy = stream_destroy_cb,
      .process = playback_process_cb,
      .state_changed = stream_state_changed_cb,
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

   if (spa_streq(type, PW_TYPE_INTERFACE_Node))
   {
      media = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
      if (media && strcmp(media, "Audio/Sink") == 0)
      {
         sink = spa_dict_lookup(props, PW_KEY_NODE_NAME);
         if (sink && pw->devicelist)
         {
            attr.i = id;
            string_list_append(pw->devicelist, sink, attr);
            RARCH_LOG("[PipeWire]: Found Sink Node: %s\n", sink);
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

static void *pipewire_init(const char *device, unsigned rate,
      unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   int                         res;
   uint64_t            buf_samples;
   const struct spa_pod *params[1];
   uint8_t             buffer[1024];
   struct pw_properties     *props = NULL;
   const char               *error = NULL;
   pipewire_audio_t         *audio = (pipewire_audio_t*)calloc(1, sizeof(*audio));
   pipewire_core_t             *pw = NULL;
   struct spa_pod_builder        b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

   pw_init(NULL, NULL);

   if (!audio)
      goto error;

   pw = audio->pw = (pipewire_core_t*)calloc(1, sizeof(*audio->pw));
   pw->devicelist = string_list_new();

   if (!pipewire_core_init(pw, "audio_driver"))
      goto error;

   pw->registry = pw_core_get_registry(pw->core, PW_VERSION_REGISTRY, 0);

   spa_zero(pw->registry_listener);
   pw_registry_add_listener(pw->registry, &pw->registry_listener, &registry_events, pw);

   /* unlock, run the loop and wait, this will trigger the callbacks */
   pipewire_core_wait_resync(pw);

   audio->info.format = is_little_endian() ? SPA_AUDIO_FORMAT_F32_LE : SPA_AUDIO_FORMAT_F32_BE;
   audio->info.channels = DEFAULT_CHANNELS;
   pipewire_set_position(DEFAULT_CHANNELS, audio->info.position);
   audio->info.rate = rate;
   audio->frame_size = pipewire_calc_frame_size(audio->info.format, DEFAULT_CHANNELS);

   props = pw_properties_new(PW_KEY_MEDIA_TYPE,          PW_RARCH_MEDIA_TYPE_AUDIO,
                             PW_KEY_MEDIA_CATEGORY,      PW_RARCH_MEDIA_CATEGORY_PLAYBACK,
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
   audio->stream = pw_stream_new(pw->core, PW_RARCH_APPNAME, props);

   if (!audio->stream)
      goto unlock_error;

   pw_stream_add_listener(audio->stream, &audio->stream_listener, &playback_stream_events, audio);

   params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio->info);

   /* Now connect this stream. We ask that our process function is
    * called in a realtime thread. */
   res = pw_stream_connect(audio->stream,
                           PW_DIRECTION_OUTPUT,
                           PW_ID_ANY,
                           PW_STREAM_FLAG_AUTOCONNECT |
                           PW_STREAM_FLAG_MAP_BUFFERS |
                           PW_STREAM_FLAG_RT_PROCESS,
                           params, 1);

   if (res < 0)
      goto unlock_error;

   audio->highwater_mark = MIN(RINGBUFFER_SIZE,
                               latency * (uint64_t)rate / 1000 * audio->frame_size);

   pw_thread_loop_unlock(pw->thread_loop);

   return audio;

unlock_error:
   pw_thread_loop_unlock(audio->pw->thread_loop);
error:
   RARCH_ERR("[PipeWire]: Failed to initialize audio\n");
   pipewire_free(audio);
   return NULL;
}

static ssize_t pipewire_write(void *data, const void *buf_, size_t size)
{
   int32_t   filled, avail;
   uint32_t            idx;
   pipewire_audio_t *audio = (pipewire_audio_t*)data;
   const char       *error = NULL;

   if (pw_stream_get_state(audio->stream, &error) != PW_STREAM_STATE_STREAMING)
      return 0;  /* wait for stream to become ready */

   if (size > audio->highwater_mark)
   {
      RARCH_ERR("[PipeWire]: Buffer too small! Please try increasing the latency.\n");
      return 0;
   }

   pw_thread_loop_lock(audio->pw->thread_loop);

   while (size)
   {
      filled = spa_ringbuffer_get_write_index(&audio->ring, &idx);
      avail = audio->highwater_mark - filled;

#if 0  /* Useful for tracing */
      RARCH_DBG("[PipeWire]: Ringbuffer utilization: filled %d, avail %d, index %d, size %d\n",
                filled, avail, idx, size);
#endif

      /* in non-blocking mode we play as much as we can
       * in blocking mode we expect a freed buffer of at least the given size */
      if (size > (size_t)avail)
      {
         if (audio->pw->nonblock)
         {
            size = avail;
            break;
         }

         pw_thread_loop_wait(audio->pw->thread_loop);
      }
      else
         break;
   }

   if (filled < 0)
      RARCH_ERR("[Pipewire]: %p: underrun write:%u filled:%d\n", audio, idx, filled);
   else
   {
      if ((uint32_t) filled + size > RINGBUFFER_SIZE)
      {
         RARCH_ERR("[PipeWire]: %p: overrun write:%u filled:%d + size:%zu > max:%u\n",
         audio, idx, filled, size, RINGBUFFER_SIZE);
      }
   }

   spa_ringbuffer_write_data(&audio->ring,
                             audio->buffer, RINGBUFFER_SIZE,
                             idx & RINGBUFFER_MASK, buf_, size);
   idx += size;
   spa_ringbuffer_write_update(&audio->ring, idx);

   pw_thread_loop_unlock(audio->pw->thread_loop);
   return size;
}

static bool pipewire_stop(void *data)
{
   pipewire_audio_t *audio = (pipewire_audio_t*)data;
   const char       *error = NULL;

   if (!audio || !audio->pw)
      return false;
   if (pw_stream_get_state(audio->stream, &error) == PW_STREAM_STATE_PAUSED)
      return true;

   return pipewire_stream_set_active(audio->pw->thread_loop, audio->stream, false);
}

static bool pipewire_start(void *data, bool is_shutdown)
{
   pipewire_audio_t *audio = (pipewire_audio_t*)data;
   const char       *error = NULL;

   if (!audio || !audio->pw)
      return false;
   if (pw_stream_get_state(audio->stream, &error) == PW_STREAM_STATE_STREAMING)
      return true;

   return pipewire_stream_set_active(audio->pw->thread_loop, audio->stream, true);
}

static bool pipewire_alive(void *data)
{
   pipewire_audio_t *audio = (pipewire_audio_t*)data;
   const char       *error = NULL;

   if (!audio)
      return false;

   return pw_stream_get_state(audio->stream, &error) == PW_STREAM_STATE_STREAMING;
}

static void pipewire_set_nonblock_state(void *data, bool state)
{
   pipewire_audio_t *audio = (pipewire_audio_t*)data;
   if (audio && audio->pw)
      audio->pw->nonblock = state;
}

static void pipewire_free(void *data)
{
   pipewire_audio_t *audio = (pipewire_audio_t*)data;

   if (!audio)
      return pw_deinit();

   if (audio->pw->thread_loop)
      pw_thread_loop_stop(audio->pw->thread_loop);

   if (audio->stream)
   {
      pw_stream_destroy(audio->stream);
      audio->stream = NULL;
   }

   if (audio->pw->registry)
      pw_proxy_destroy((struct pw_proxy*)audio->pw->registry);

   if (audio->pw->core)
   {
      spa_hook_remove(&audio->pw->core_listener);
      spa_zero(audio->pw->core_listener);
      pw_core_disconnect(audio->pw->core);
   }

   if (audio->pw->ctx)
      pw_context_destroy(audio->pw->ctx);

   pw_thread_loop_destroy(audio->pw->thread_loop);

   if (audio->pw->devicelist)
      string_list_free(audio->pw->devicelist);

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
   pipewire_audio_t *audio = (pipewire_audio_t*)data;

   if (audio && audio->pw && audio->pw->devicelist)
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
   uint32_t idx, written, length;
   pipewire_audio_t *audio = (pipewire_audio_t*)data;
   const char       *error = NULL;

   retro_assert(audio->pw);
   retro_assert(audio->stream);

   if (pw_stream_get_state(audio->stream, &error) != PW_STREAM_STATE_STREAMING)
      return  0;  /* wait for stream to become ready */

   pw_thread_loop_lock(audio->pw->thread_loop);
   written = spa_ringbuffer_get_write_index(&audio->ring, &idx);
   length = audio->highwater_mark - written;
   pw_thread_loop_unlock(audio->pw->thread_loop);

   return length;
}

static size_t pipewire_buffer_size(void *data)
{
   pipewire_audio_t *audio = (pipewire_audio_t*)data;
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
