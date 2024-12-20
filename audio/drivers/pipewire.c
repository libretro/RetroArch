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

#include "../audio_driver.h"
#include "../../verbosity.h"

#define APPNAME "RetroArch"
#define DEFAULT_CHANNELS 2
#define QUANTUM 1024  /* TODO: detect */

#define RINGBUFFER_SIZE (1u << 22)
#define RINGBUFFER_MASK (RINGBUFFER_SIZE - 1)

typedef struct
{
   struct pw_thread_loop *thread_loop;
   struct pw_context *context;
   struct pw_core *core;
   struct spa_hook core_listener;
   int last_seq, pending_seq, error;

   struct pw_stream *stream;
   struct spa_hook stream_listener;
   struct spa_audio_info_raw info;
   uint32_t highwater_mark;
   uint32_t frame_size, req;
   struct spa_ringbuffer ring;
   uint8_t buffer[RINGBUFFER_SIZE];

   struct pw_registry *registry;
   struct spa_hook registry_listener;
   struct pw_client *client;
   struct spa_hook client_listener;

   bool nonblock;
   bool is_paused;
   struct string_list *devicelist;
} pw_t;

size_t calc_frame_size(enum spa_audio_format fmt, uint32_t nchannels)
{
   uint32_t sample_size = 1;
   switch (fmt)
   {
      case SPA_AUDIO_FORMAT_S8:
      case SPA_AUDIO_FORMAT_U8:
         sample_size = 1;
         break;
      case SPA_AUDIO_FORMAT_S16_BE:
      case SPA_AUDIO_FORMAT_S16_LE:
      case SPA_AUDIO_FORMAT_U16_BE:
      case SPA_AUDIO_FORMAT_U16_LE:
         sample_size = 2;
         break;
      case SPA_AUDIO_FORMAT_S32_BE:
      case SPA_AUDIO_FORMAT_S32_LE:
      case SPA_AUDIO_FORMAT_U32_BE:
      case SPA_AUDIO_FORMAT_U32_LE:
      case SPA_AUDIO_FORMAT_F32_BE:
      case SPA_AUDIO_FORMAT_F32_LE:
         sample_size = 4;
         break;
      default:
         RARCH_ERR("[PipeWire]: Bad spa_audio_format %d\n", fmt);
         break;
   }
   return sample_size * nchannels;
}

static void set_position(uint32_t channels, uint32_t position[SPA_AUDIO_MAX_CHANNELS])
{
   memcpy(position, (uint32_t[SPA_AUDIO_MAX_CHANNELS]) { SPA_AUDIO_CHANNEL_UNKNOWN, },
         sizeof(uint32_t) * SPA_AUDIO_MAX_CHANNELS);

   switch (channels)
   {
      case 8:
         position[6] = SPA_AUDIO_CHANNEL_SL;
         position[7] = SPA_AUDIO_CHANNEL_SR;
         /* fallthrough */
      case 6:
         position[2] = SPA_AUDIO_CHANNEL_FC;
         position[3] = SPA_AUDIO_CHANNEL_LFE;
         position[4] = SPA_AUDIO_CHANNEL_RL;
         position[5] = SPA_AUDIO_CHANNEL_RR;
         /* fallthrough */
      case 2:
         position[0] = SPA_AUDIO_CHANNEL_FL;
         position[1] = SPA_AUDIO_CHANNEL_FR;
         break;
      case 1:
         position[0] = SPA_AUDIO_CHANNEL_MONO;
         break;
      default:
         RARCH_ERR("[PipeWire]: Internal error: unsupported channel count %d\n", channels);
    }
}

static void stream_destroy(void *data)
{
    pw_t *pw = (pw_t*)data;
    spa_hook_remove(&pw->stream_listener);
    pw->stream = NULL;
}

static void on_process(void *data)
{
    pw_t *pw = (pw_t*)data;
    void *p;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    uint32_t req, index, n_bytes;
    int32_t avail;

    retro_assert(pw->stream);

    if ((b = pw_stream_dequeue_buffer(pw->stream)) == NULL)
    {
       RARCH_WARN("[PipeWire]: Out of buffers: %s", strerror(errno));
       return;
    }

    buf = b->buffer;
    p = buf->datas[0].data;
    if (p == NULL)
       return;

    /* calculate the total no of bytes to read data from buffer */
    req = b->requested * pw->frame_size;

    if (req == 0)
       req = pw->req;

    n_bytes = SPA_MIN(req, buf->datas[0].maxsize);

    /* get no of available bytes to read data from buffer */
    avail = spa_ringbuffer_get_read_index(&pw->ring, &index);

    if (avail < (int32_t)n_bytes)
       n_bytes = avail;

    spa_ringbuffer_read_data(&pw->ring,
                              pw->buffer, RINGBUFFER_SIZE,
                              index & RINGBUFFER_MASK, p, n_bytes);

    index += n_bytes;
    spa_ringbuffer_read_update(&pw->ring, index);

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = pw->frame_size;
    buf->datas[0].chunk->size   = n_bytes;

    /* queue the buffer for playback */
    pw_stream_queue_buffer(pw->stream, b);
}

static void on_stream_state_changed(void *data,
      enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
   pw_t *pw = (pw_t*)data;

   RARCH_DBG("[PipeWire]: New state for Node %d : %s\n",
             pw_stream_get_node_id(pw->stream),
             pw_stream_state_as_string(state));

   switch(state)
   {
      case PW_STREAM_STATE_STREAMING:
         pw->is_paused = false;
         pw_thread_loop_signal(pw->thread_loop, false);
         break;
      case PW_STREAM_STATE_ERROR:
      case PW_STREAM_STATE_PAUSED:
         pw->is_paused = true;
         pw_thread_loop_signal(pw->thread_loop, false);
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

static int wait_resync(pw_t *pw)
{
   retro_assert(pw != NULL);

   int res;
   pw->pending_seq = pw_core_sync(pw->core, PW_ID_CORE, pw->pending_seq);

   while (true)
   {
      pw_thread_loop_wait(pw->thread_loop);

      res = pw->error;
      if (res < 0)
      {
         pw->error = 0;
         return res;
      }
      if (pw->pending_seq == pw->last_seq)
         break;
   }
   return 0;
}

static void client_info(void *data, const struct pw_client_info *info)
{
   pw_t *pw = (pw_t*)data;
   const struct spa_dict_item *item;

   RARCH_DBG("[PipeWire]: client: id:%u\n", info->id);
   RARCH_DBG("[PipeWire]: \tprops:\n");
   spa_dict_for_each(item, info->props)
      RARCH_DBG("[PipeWire]: \t\t%s: \"%s\"\n", item->key, item->value);

   pw_thread_loop_signal(pw->thread_loop, false);
}

static const struct pw_client_events client_events = {
   PW_VERSION_CLIENT_EVENTS,
   .info = client_info,
};

static void on_core_error(void *data, uint32_t id, int seq, int res, const char *message)
{
   pw_t *pw = data;

   RARCH_ERR("[PipeWire]: error id:%u seq:%d res:%d (%s): %s\n",
             id, seq, res, spa_strerror(res), message);

   /* stop and exit the thread loop */
   pw_thread_loop_signal(pw->thread_loop, false);
}

static void on_core_done(void *data, uint32_t id, int seq)
{
   pw_t *pw = (pw_t*)data;

   retro_assert(id == PW_ID_CORE);

   pw->last_seq = seq;
   if (pw->pending_seq == seq)
   {
      /* stop and exit the thread loop */
      pw_thread_loop_signal(pw->thread_loop, false);
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
   pw_t *pw = (pw_t*)data;
   union string_list_elem_attr attr;
   const char* media = NULL;
   const char* sink = NULL;

   if (!pw)
     return;

   if (!pw->client && spa_streq(type, PW_TYPE_INTERFACE_Client))
   {
      pw->client = pw_registry_bind(pw->registry,
                                    id, type, PW_VERSION_CLIENT, 0);
      pw_client_add_listener(pw->client,
                             &pw->client_listener,
                             &client_events, pw);
   }
   else if (spa_streq(type, PW_TYPE_INTERFACE_Node))
   {
      const char* media = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
      if (media && strcmp(media, "Audio/Sink") == 0)
      {
         sink = spa_dict_lookup(props, PW_KEY_NODE_NAME);
         attr.i = id;
         string_list_append(pw->devicelist, sink, attr);
         RARCH_LOG("[PipeWire]: Found Sink: %s\n", sink);
      }
   }

#ifdef DEBUG
   const struct spa_dict_item *item;
   RARCH_DBG("[PipeWire]: Object: id:%u Type:%s/%d\n", id, type, version);
   spa_dict_for_each(item, props)
      RARCH_DBG("[PipeWire]: \t\t%s: \"%s\"\n", item->key, item->value);
#endif
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
   const struct spa_pod   *params[1];
   uint8_t              buffer[1024];
   struct pw_properties       *props = NULL;
   const char                 *error = NULL;
   pw_t                          *pw = (pw_t*)calloc(1, sizeof(*pw));
   struct spa_pod_builder          b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
   uint64_t buf_samples;

   if (!pw)
      goto error;

   pw_init(NULL, NULL);

   pw->devicelist = string_list_new();
   if (!pw->devicelist)
      goto error;

   pw->thread_loop = pw_thread_loop_new("audio_driver", NULL);
   if (!pw->thread_loop)
      goto error;

   pw->context = pw_context_new(pw_thread_loop_get_loop(pw->thread_loop), NULL, 0);
   if (!pw->context)
      goto error;

   if (pw_thread_loop_start(pw->thread_loop) < 0)
      goto error;

   pw_thread_loop_lock(pw->thread_loop);

   pw->core = pw_context_connect(pw->context, NULL, 0);
   if(!pw->core)
      goto unlock_error;

   if (pw_core_add_listener(pw->core,
                            &pw->core_listener,
                            &core_events, pw) < 0)
      goto unlock_error;

   pw->info.format = is_little_endian() ? SPA_AUDIO_FORMAT_F32_LE : SPA_AUDIO_FORMAT_F32_BE;
   pw->info.channels = DEFAULT_CHANNELS;
   set_position(DEFAULT_CHANNELS, pw->info.position);
   pw->info.rate = rate;
   pw->frame_size = calc_frame_size(pw->info.format, DEFAULT_CHANNELS);
   pw->req = QUANTUM * rate * 1 / 2 / 100000 * pw->frame_size;

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
      goto unlock_error;

   if (device)
      pw_properties_set(props, PW_KEY_TARGET_OBJECT, device);

   buf_samples = QUANTUM * rate * 3 / 4 / 100000;

   pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%" PRIu64 "/%u",
                      buf_samples, rate);

   pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%d", rate);

   pw->stream = pw_stream_new(pw->core, APPNAME, props);

   if (!pw->stream)
      goto unlock_error;

   pw_stream_add_listener(pw->stream, &pw->stream_listener, &playback_stream_events, pw);

   params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &pw->info);

   /* Now connect this stream. We ask that our process function is
    * called in a realtime thread. */
   res = pw_stream_connect(pw->stream,
                           PW_DIRECTION_OUTPUT,
                           PW_ID_ANY,
                           PW_STREAM_FLAG_AUTOCONNECT |
                           PW_STREAM_FLAG_INACTIVE |
                           PW_STREAM_FLAG_MAP_BUFFERS |
                           PW_STREAM_FLAG_RT_PROCESS,
                           params, 1);

   if (res < 0)
      goto unlock_error;

   pw->highwater_mark = MIN(RINGBUFFER_SIZE,
                            latency? (latency * 1000): 46440 * (uint64_t)rate / 1000000 * pw->frame_size);

   RARCH_DBG("[PipeWire]: Bufer size: %u, RingBuffer size: %u\n", pw->highwater_mark, RINGBUFFER_SIZE);

   pw->registry = pw_core_get_registry(pw->core, PW_VERSION_REGISTRY, 0);

   spa_zero(pw->registry_listener);
   pw_registry_add_listener(pw->registry, &pw->registry_listener, &registry_events, pw);

   /* unlock, run the loop and wait, this will trigger the callbacks */
   if (wait_resync(pw) < 0)
   {
     pw_thread_loop_unlock(pw->thread_loop);
   }

   if(pw_stream_get_state(pw->stream, &error) != PW_STREAM_STATE_STREAMING)
      pw->is_paused = true;

   pw_thread_loop_unlock(pw->thread_loop);

   return pw;

unlock_error:
   if (pw->thread_loop)
      pw_thread_loop_stop(pw->thread_loop);
   pw_context_destroy(pw->context);
   pw_thread_loop_destroy(pw->thread_loop);

error:
   pipewire_free(pw);
   RARCH_ERR("[PipeWire]: Failed to initialize driver\n");
   return NULL;
}

static bool pipewire_start(void *data, bool is_shutdown);

static ssize_t pipewire_write(void *data, const void *buf_, size_t size)
{
   pw_t            *pw = (pw_t*)data;
   const char   *error = NULL;
   int32_t     writable;
   int32_t       avail;
   uint32_t      index;

   /* Workaround buggy menu code.
    * If a write happens while we're paused, we might never progress. */
   if (pw->is_paused)
      if (!pipewire_start(pw, false))
         return -1;

   pw_thread_loop_lock(pw->thread_loop);

   if (pw_stream_get_state(pw->stream, &error) != PW_STREAM_STATE_STREAMING)
   {
      /* wait for stream to become ready */
      size = 0;
      goto unlock;
   }
   writable = spa_ringbuffer_get_write_index(&pw->ring, &index);
   avail = pw->highwater_mark - writable;

#if 0  /* Useful for tracing */
   RARCH_DBG("[PipeWire]: Playback progress:  written %d, avail %d, index %d, size %d\n",
             writable, avail, index, size);
#endif

   if (size > (size_t)avail)
      size = avail;

   if (writable < 0)
      RARCH_ERR("%p: underrun write:%u filled:%d\n", pw, index, writable);
   else
   {
      if ((uint32_t) writable + size > RINGBUFFER_SIZE)
      {
         RARCH_ERR("%p: overrun write:%u filled:%d + size:%zu > max:%u\n",
         pw, index, writable, size, RINGBUFFER_SIZE);
      }
   }

   spa_ringbuffer_write_data(&pw->ring,
                             pw->buffer, RINGBUFFER_SIZE,
                             index & RINGBUFFER_MASK, buf_, size);
   index += size;
   spa_ringbuffer_write_update(&pw->ring, index);

unlock:
   pw_thread_loop_unlock(pw->thread_loop);
   return size;
}

static bool pipewire_stop(void *data)
{
   pw_t *pw = (pw_t*)data;
   if (pw->is_paused)
      return true;

   RARCH_LOG("[PipeWire]: Pausing.\n");

   pw_thread_loop_lock(pw->thread_loop);
   pw_stream_set_active(pw->stream, false);
   pw_thread_loop_wait(pw->thread_loop);
   pw_thread_loop_unlock(pw->thread_loop);

   return pw->is_paused;
}

static bool pipewire_start(void *data, bool is_shutdown)
{
   pw_t *pw = (pw_t*)data;
   if (!pw->is_paused)
      return true;

   RARCH_LOG("[PipeWire]: Unpausing.\n");

   pw_thread_loop_lock(pw->thread_loop);
   pw_stream_set_active(pw->stream, true);
   pw_thread_loop_wait(pw->thread_loop);
   pw_thread_loop_unlock(pw->thread_loop);

   return !pw->is_paused;
}

static bool pipewire_alive(void *data)
{
   pw_t *pw = (pw_t*)data;

   if (!pw)
      return false;
   return !pw->is_paused;
}

static void pipewire_set_nonblock_state(void *data, bool state)
{
   pw_t *pw = (pw_t*)data;
   if (pw)
      pw->nonblock = state;
}

static void pipewire_free(void *data)
{
   pw_t *pw = (pw_t*)data;

   if (!pw)
      return pw_deinit();

   if (pw->thread_loop)
      pw_thread_loop_stop(pw->thread_loop);

   if (pw->client)
      pw_proxy_destroy((struct pw_proxy *)pw->client);

   if (pw->registry)
      pw_proxy_destroy((struct pw_proxy*)pw->registry);

   if (pw->core)
   {
      spa_hook_remove(&pw->core_listener);
      spa_zero(pw->core_listener);
      pw_core_disconnect(pw->core);
   }

   if (pw->context)
      pw_context_destroy(pw->context);

   pw_thread_loop_destroy(pw->thread_loop);

   free(pw);
   pw_deinit();
}

static bool pipewire_use_float(void *data)
{
   (void)data;
   return true;
}

static void *pipewire_device_list_new(void *data)
{
   pw_t *pw = (pw_t*)data;
   if (!pw)
      return NULL;

   if (pw->devicelist)
      return string_list_clone(pw->devicelist);

   return NULL;
}

static void pipewire_device_list_free(void *data, void *array_list_data)
{
   struct string_list *s = (struct string_list*)array_list_data;

   if (!s)
      return;

   string_list_free(s);
}

static size_t pipewire_write_avail(void *data)
{
   uint32_t index, written, length;
   pw_t *pw = (pw_t*)data;
   const char *error = NULL;

   pw_thread_loop_lock(pw->thread_loop);

   if (pw_stream_get_state(pw->stream, &error) != PW_STREAM_STATE_STREAMING)
   {
      /* wait for stream to become ready */
      length = 0;
      goto unlock;
   }

   written = spa_ringbuffer_get_write_index(&pw->ring, &index);
   length = pw->highwater_mark - written;
   audio_driver_set_buffer_size(pw->highwater_mark);

unlock:
   pw_thread_loop_unlock(pw->thread_loop);
   return length;
}

static size_t pipewire_buffer_size(void *data)
{
   pw_t *pw = (pw_t*)data;
   return pw->highwater_mark;
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
