/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>

#include "android_windows.h"
#include "../video_defines.h"
#include "../../frontend/drivers/platform_unix.h"

/* By window id; one Android display takes them all, so in practice
 * there is one */
static android_window_t *android_windows[RETRO_VIDEO_VIEWS_MAX];

static video_window_common_t *android_windows_at(unsigned screen)
{
   return android_windows[screen] ? &android_windows[screen]->common : NULL;
}

bool android_windows_create(void *owner,
      const video_window_request_t *request, video_window_native_t *window)
{
   android_window_t *state;
   unsigned screen = request->screen;
   /* Display 0 is no list from Java yet: nothing to place it on */
   if (     !screen || screen >= RETRO_VIDEO_VIEWS_MAX
         || !request->display
         || android_windows[screen])
      return false;
   if (!(state = (android_window_t*)calloc(1, sizeof(*state))))
      return false;
   state->common.owner     = owner;
   state->common.native    = window;
   state->common.request   = *request;
   android_windows[screen] = state;
   window->state           = state;
   window->display         = NULL;
   window->surface         = NULL;
   window->kind            = VIDEO_WINDOW_NATIVE_ANDROID;
   window->generation      = request->generation;
   window->dims            = 0;
   window->configured      = false;
   window->focused         = false;
   window->presentable     = false;
   android_second_display_request(request->display, false);
   return true;
}

void android_windows_update(void *owner, video_window_native_t *window,
      const video_window_request_t *request)
{
   android_window_t *state = window ? (android_window_t*)window->state : NULL;
   /* A new display is a new window, the renderer's to make */
   if (     state && state->common.owner == owner && request
         && request->screen     == state->common.request.screen
         && request->generation == state->common.request.generation)
      state->common.request.settings = request->settings;
}

unsigned android_windows_poll(void *owner, video_window_event_t *events,
      unsigned capacity)
{
   unsigned i;
   struct android_app *android_app = (struct android_app*)g_android;
   android_window_t *state         = NULL;
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX && !state; i++)
      if (android_windows[i] && android_windows[i]->common.owner == owner)
         state = android_windows[i];
   /* A surface waits for a window to take it */
   if (state && android_app)
   {
      struct android_second_display *second = &android_app->second_display;
      struct android_second_surface *node   = (struct android_second_surface*)
         retro_atomic_exchange_ptr(&second->surface, NULL);
      /* Made for a request before a hide Java has yet to do */
      if (     node
            && ANDROID_SECOND_BEFORE(node->request,
               retro_atomic_load_acquire_int(&second->released)))
      {
         android_second_surface_free(node);
         node = NULL;
      }
      /* Java took the surface back: drawn no more, and the renderer
       * lets go of what it made of it */
      if (     !node && state->node
            && state->node->generation
               == retro_atomic_load_acquire_int(&second->surface_gone))
      {
         video_window_native_t *native = state->common.native;
         android_second_surface_free(state->node);
         state->node         = NULL;
         native->surface     = NULL;
         native->dims        = 0;
         native->configured  = false;
         native->presentable = false;
         retro_atomic_store_release_int(&second->bound_surface, 0);
      }
      if (node)
      {
         video_window_native_t *native = state->common.native;
         if (state->node)
            android_second_surface_free(state->node);
         state->node         = node;
         native->surface     = node->window;
         native->dims        = node->dims;
         native->configured  = true;
         native->presentable = true;
         state->common.pending |= VIDEO_WINDOW_EVENT_BIT(GEOMETRY);
         /* The input poll reads surface, window, generation: stored the
          * other way round, a window it sees comes with its generation
          * and a surface with both */
         retro_atomic_store_release_int(&second->bound_generation,
               (int)state->common.request.generation);
         retro_atomic_store_release_int(&second->bound_window,
               (int)state->common.request.screen);
         retro_atomic_store_release_int(&second->bound_surface,
               node->generation);
      }
   }
   return video_windows_drain(owner, android_windows_at, events, capacity);
}

void android_windows_destroy(void *owner, video_window_native_t *window)
{
   struct android_app *android_app = (struct android_app*)g_android;
   android_window_t *state = window ? (android_window_t*)window->state : NULL;
   unsigned screen, i;
   if (!state || state->common.owner != owner)
      return;
   screen                  = state->common.request.screen;
   android_windows[screen] = NULL;
   if (state->node)
      android_second_surface_free(state->node);
   free(state);
   memset(window, 0, sizeof(*window));
   if (android_app)
   {
      struct android_second_display *second = &android_app->second_display;
      if (retro_atomic_load_acquire_int(&second->bound_window)
            == (int)screen)
      {
         /* Surface first; generation before window, as in poll */
         retro_atomic_store_release_int(&second->bound_surface, 0);
         retro_atomic_store_release_int(&second->bound_generation, 0);
         retro_atomic_store_release_int(&second->bound_window, 0);
      }
   }
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
      if (android_windows[i])
         break;
   /* Holding nothing now: the surfaces asked for so far need no handoff */
   android_second_display_request(0, i == RETRO_VIDEO_VIEWS_MAX);
}

android_window_t *android_windows_find(void *owner, unsigned screen)
{
   android_window_t *state = screen < RETRO_VIDEO_VIEWS_MAX
      ? android_windows[screen] : NULL;
   return (state && state->common.owner == owner) ? state : NULL;
}

void android_windows_free(void *owner,
      void (*destroy)(void *owner, video_window_native_t *window))
{
   video_windows_sweep(owner, android_windows_at, destroy);
}
