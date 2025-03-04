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

#include "pipewire.h"

#include <spa/utils/result.h>
#include <pipewire/pipewire.h>

#include <retro_assert.h>

#include "../../verbosity.h"


static void core_error_cb(void *data, uint32_t id, int seq, int res, const char *message)
{
   pipewire_core_t *pw = (pipewire_core_t*)data;

   RARCH_ERR("[PipeWire]: error id:%u seq:%d res:%d (%s): %s\n",
             id, seq, res, spa_strerror(res), message);

   pw_thread_loop_stop(pw->thread_loop);
}

static void core_done_cb(void *data, uint32_t id, int seq)
{
   pipewire_core_t *pw = (pipewire_core_t*)data;

   retro_assert(id == PW_ID_CORE);

   pw->last_seq = seq;

   if (pw->pending_seq == seq)
      pw_thread_loop_signal(pw->thread_loop, false);
}

static const struct pw_core_events core_events = {
      PW_VERSION_CORE_EVENTS,
      .done = core_done_cb,
      .error = core_error_cb,
};

size_t pipewire_calc_frame_size(enum spa_audio_format fmt, uint32_t nchannels)
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

void pipewire_set_position(uint32_t channels, uint32_t position[SPA_AUDIO_MAX_CHANNELS])
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

void pipewire_core_wait_resync(pipewire_core_t *pw)
{
   retro_assert(pw);
   pw->pending_seq = pw_core_sync(pw->core, PW_ID_CORE, pw->pending_seq);

   for (;;)
   {
      pw_thread_loop_wait(pw->thread_loop);
      if (pw->pending_seq == pw->last_seq)
         break;
   }
}

bool pipewire_stream_set_active(struct pw_thread_loop *loop, struct pw_stream *stream, bool active)
{
   enum pw_stream_state st;
   const char       *error;

   retro_assert(loop);
   retro_assert(stream);

   pw_thread_loop_lock(loop);
   pw_stream_set_active(stream, active);
   pw_thread_loop_wait(loop);
   pw_thread_loop_unlock(loop);

   st = pw_stream_get_state(stream, &error);
   return active ? st == PW_STREAM_STATE_STREAMING : st == PW_STREAM_STATE_PAUSED;
}

bool pipewire_core_init(pipewire_core_t **pw, const char *loop_name, const struct pw_registry_events *events)
{
   retro_assert(!*pw);

   *pw = (pipewire_core_t*)calloc(1, sizeof(pipewire_core_t));
   if (!*pw)
      return false;

   (*pw)->devicelist = string_list_new();
   if (!(*pw)->devicelist)
   {
      free(*pw);
      *pw = NULL;
      return false;
   }

   pw_init(NULL, NULL);

   (*pw)->thread_loop = pw_thread_loop_new(loop_name, NULL);
   if (!(*pw)->thread_loop)
      return false;

   (*pw)->ctx = pw_context_new(pw_thread_loop_get_loop((*pw)->thread_loop), NULL, 0);
   if (!(*pw)->ctx)
      return false;

   if (pw_thread_loop_start((*pw)->thread_loop) < 0)
      return false;

   pw_thread_loop_lock((*pw)->thread_loop);

   (*pw)->core = pw_context_connect((*pw)->ctx, NULL, 0);
   if (!(*pw)->core)
      goto unlock;

   if (pw_core_add_listener((*pw)->core,
                            &(*pw)->core_listener,
                            &core_events, *pw) < 0)
      goto unlock;

   if (events)
   {
      (*pw)->registry = pw_core_get_registry((*pw)->core, PW_VERSION_REGISTRY, 0);
      spa_zero((*pw)->registry_listener);
      pw_registry_add_listener((*pw)->registry, &(*pw)->registry_listener, events, *pw);
   }

   return true;

unlock:
   pw_thread_loop_unlock((*pw)->thread_loop);
   return false;
}

void pipewire_core_deinit(pipewire_core_t *pw)
{
   if (!pw)
      return pw_deinit();

   if (pw->thread_loop)
      pw_thread_loop_stop(pw->thread_loop);

   if (pw->registry)
   {
      spa_hook_remove(&pw->registry_listener);
      pw_proxy_destroy((struct pw_proxy*)pw->registry);
   }

   if (pw->core)
   {
      spa_hook_remove(&pw->core_listener);
      pw_core_disconnect(pw->core);
   }

   if (pw->ctx)
      pw_context_destroy(pw->ctx);

   if (pw->thread_loop)
      pw_thread_loop_destroy(pw->thread_loop);

   if (pw->devicelist)
      string_list_free(pw->devicelist);

   free(pw);
   pw_deinit();
}
