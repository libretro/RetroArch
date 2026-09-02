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

   RARCH_ERR("[PipeWire] Error id:%u seq:%d res:%d (%s): %s.\n",
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

/* Upper bound on how long a stream may take to reach the requested
 * state. A graph that is running the stream gets there in
 * milliseconds; one that never will - no session manager, no sink to
 * link to - is what the bound is for. */
#define PIPEWIRE_SET_ACTIVE_WAIT_SEC 2

bool pipewire_stream_set_active(struct pw_thread_loop *loop, struct pw_stream *stream, bool active)
{
   enum pw_stream_state st;
   enum pw_stream_state want = active ? PW_STREAM_STATE_STREAMING
                                      : PW_STREAM_STATE_PAUSED;
   const char       *error   = NULL;
   int               laps;

   retro_assert(loop);
   retro_assert(stream);

   pw_thread_loop_lock(loop);
   pw_stream_set_active(stream, active);

   /* The state callback signals the loop on each transition. Wait only
    * while the stream is still on its way: one already in the target
    * state raises no further event, an error is final, and a stream
    * the graph never runs is given up on at the bound rather than
    * holding the calling thread. */
   for (laps = 0; laps < PIPEWIRE_SET_ACTIVE_WAIT_SEC; laps++)
   {
      st = pw_stream_get_state(stream, &error);
      if (st == want || st == PW_STREAM_STATE_ERROR)
         break;
      pw_thread_loop_timed_wait(loop, 1);
   }
   st = pw_stream_get_state(stream, &error);
   pw_thread_loop_unlock(loop);

   if (st != want)
      RARCH_WARN("[PipeWire] Stream did not reach %s within %d seconds (state: %s%s%s).\n",
            active ? "streaming" : "paused", PIPEWIRE_SET_ACTIVE_WAIT_SEC,
            pw_stream_state_as_string(st),
            error ? ": " : "", error ? error : "");

   return st == want;
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
   {
      pw_deinit();
      return;
   }

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
