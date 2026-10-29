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

#include "openxr_session.h"

#include "../../verbosity.h"

const char *openxr_session_state_name(XrSessionState state)
{
   switch (state)
   {
      case XR_SESSION_STATE_IDLE:
         return "idle";
      case XR_SESSION_STATE_READY:
         return "ready";
      case XR_SESSION_STATE_SYNCHRONIZED:
         return "synchronized";
      case XR_SESSION_STATE_VISIBLE:
         return "visible";
      case XR_SESSION_STATE_FOCUSED:
         return "focused";
      case XR_SESSION_STATE_STOPPING:
         return "stopping";
      case XR_SESSION_STATE_LOSS_PENDING:
         return "loss pending";
      case XR_SESSION_STATE_EXITING:
         return "exiting";
      default:
         break;
   }
   return "unknown";
}

void openxr_session_end(openxr_session_t *s, bool instance_lost)
{
   retro_atomic_store_release_int(&s->running, 0);
   if (instance_lost)
      s->lost = true;
   if (retro_atomic_load_acquire_int(&s->ended))
      return;
   retro_atomic_store_release_int(&s->ended, 1);
   retro_atomic_store_release_int(&s->alive, 0);
   if (s->hooks.ended)
      s->hooks.ended(s->hooks.user);
}

void openxr_session_lock(openxr_session_t *s)
{
   if (s->hooks.lock)
      s->hooks.lock(s->hooks.user);
}

void openxr_session_unlock(openxr_session_t *s)
{
   if (s->hooks.unlock)
      s->hooks.unlock(s->hooks.user);
}

static void openxr_session_state(openxr_session_t *s, XrSessionState state)
{
   XrResult res;
   retro_atomic_store_release_int(&s->state, (int)state);
   RARCH_LOG("[OpenXR] Session %s.\n", openxr_session_state_name(state));
   if (s->hooks.state_changed)
      s->hooks.state_changed(s->hooks.user, state);
   switch (state)
   {
      case XR_SESSION_STATE_READY:
         {
            XrSessionBeginInfo bi;
            bi.type                         = XR_TYPE_SESSION_BEGIN_INFO;
            bi.next                         = NULL;
            bi.primaryViewConfigurationType =
               XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            openxr_session_lock(s);
            res = s->BeginSession(s->session, &bi);
            openxr_session_unlock(s);
            if (XR_SUCCEEDED(res))
               retro_atomic_store_release_int(&s->running, 1);
            else
               RARCH_ERR("[OpenXR] xrBeginSession failed (%d).\n", (int)res);
         }
         break;
      case XR_SESSION_STATE_STOPPING:
         openxr_session_lock(s);
         s->EndSession(s->session);
         openxr_session_unlock(s);
         retro_atomic_store_release_int(&s->running, 0);
         break;
      case XR_SESSION_STATE_EXITING:
         openxr_session_end(s, false);
         if (s->hooks.exiting)
            s->hooks.exiting(s->hooks.user);
         break;
      case XR_SESSION_STATE_LOSS_PENDING:
         openxr_session_end(s, false);
         break;
      default:
         break;
   }
}

bool openxr_session_next_event(openxr_session_t *s, openxr_event_t *e)
{
   XrResult res;
   XrEventDataBuffer ev;
   for (;;)
   {
      ev.type = XR_TYPE_EVENT_DATA_BUFFER;
      ev.next = NULL;
      res     = s->PollEvent(s->rt->instance, &ev);
      memset(e, 0, sizeof(*e));
      if (res == XR_ERROR_INSTANCE_LOST)
      {
         e->kind = OPENXR_EVENT_INSTANCE_LOST;
         return true;
      }
      if (res != XR_SUCCESS)
         return false;
      switch (ev.type)
      {
         case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
            {
               const XrEventDataReferenceSpaceChangePending *sc =
                  (const XrEventDataReferenceSpaceChangePending*)&ev;
               e->kind    = OPENXR_EVENT_SPACE_CHANGE;
               e->session = sc->session;
               e->value   = (int)sc->referenceSpaceType;
            }
            return true;
         case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
            {
               const XrEventDataSessionStateChanged *sc =
                  (const XrEventDataSessionStateChanged*)&ev;
               e->kind    = OPENXR_EVENT_STATE;
               e->session = sc->session;
               e->value   = (int)sc->state;
            }
            return true;
         case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            e->kind = OPENXR_EVENT_LOSS_PENDING;
            return true;
         case XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB:
            {
               const XrEventDataDisplayRefreshRateChangedFB *rc =
                  (const XrEventDataDisplayRefreshRateChangedFB*)&ev;
               e->kind    = OPENXR_EVENT_REFRESH_RATE;
               e->from_hz = rc->fromDisplayRefreshRate;
               e->to_hz   = rc->toDisplayRefreshRate;
            }
            return true;
         default:
            break;
      }
   }
}

void openxr_session_apply(openxr_session_t *s, const openxr_event_t *e)
{
   switch (e->kind)
   {
      case OPENXR_EVENT_INSTANCE_LOST:
      case OPENXR_EVENT_LOSS_PENDING:
         openxr_session_end(s, true);
         break;
      case OPENXR_EVENT_SPACE_CHANGE:
         if (e->session != s->session)
            break;
         if (     e->value == (int)XR_REFERENCE_SPACE_TYPE_LOCAL
               && s->hooks.local_changed)
            s->hooks.local_changed(s->hooks.user);
         else if (e->value == (int)XR_REFERENCE_SPACE_TYPE_STAGE
               && s->hooks.stage_changed)
            s->hooks.stage_changed(s->hooks.user);
         break;
      case OPENXR_EVENT_STATE:
         /* A session destroyed for a new one may have some queued. */
         if (e->session == s->session)
            openxr_session_state(s, (XrSessionState)e->value);
         break;
      case OPENXR_EVENT_REFRESH_RATE:
         RARCH_LOG("[OpenXR] The headset changed from %.2f to %.2f Hz.\n",
               e->from_hz, e->to_hz);
         break;
   }
}

void openxr_session_poll(openxr_session_t *s)
{
   openxr_event_t e;
   while (openxr_session_next_event(s, &e))
   {
      openxr_session_apply(s, &e);
      if (e.kind == OPENXR_EVENT_INSTANCE_LOST)
         break;
   }
}

bool openxr_session_load(openxr_session_t *s, const openxr_runtime_t *rt)
{
   s->rt = rt;
   return OPENXR_FN(rt, s, PollEvent)
       && OPENXR_FN(rt, s, CreateSession)
       && OPENXR_FN(rt, s, DestroySession)
       && OPENXR_FN(rt, s, BeginSession)
       && OPENXR_FN(rt, s, EndSession)
       && OPENXR_FN(rt, s, CreateReferenceSpace)
       && OPENXR_FN(rt, s, DestroySpace)
       && OPENXR_FN(rt, s, LocateViews)
       && OPENXR_FN(rt, s, LocateSpace);
}

bool openxr_session_create(openxr_session_t *s, const void *graphics_binding)
{
   XrResult res;
   XrSessionCreateInfo sci;
   XrReferenceSpaceCreateInfo rci;

   sci.type        = XR_TYPE_SESSION_CREATE_INFO;
   sci.next        = graphics_binding;
   sci.createFlags = 0;
   sci.systemId    = s->rt->system;
   if (XR_FAILED(res = s->CreateSession(s->rt->instance, &sci, &s->session)))
   {
      RARCH_ERR("[OpenXR] xrCreateSession failed (%d).\n", (int)res);
      s->session = XR_NULL_HANDLE;
      return false;
   }

   rci.type                               = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
   rci.next                               = NULL;
   rci.poseInReferenceSpace.orientation.x = 0.0f;
   rci.poseInReferenceSpace.orientation.y = 0.0f;
   rci.poseInReferenceSpace.orientation.z = 0.0f;
   rci.poseInReferenceSpace.position.x    = 0.0f;
   rci.poseInReferenceSpace.position.y    = 0.0f;
   rci.poseInReferenceSpace.position.z    = 0.0f;
   rci.referenceSpaceType                 = XR_REFERENCE_SPACE_TYPE_LOCAL;
   rci.poseInReferenceSpace.orientation.w = 1.0f;
   if (XR_FAILED(res = s->CreateReferenceSpace(s->session, &rci,
               &s->local_space)))
   {
      RARCH_ERR("[OpenXR] No LOCAL space (%d).\n", (int)res);
      s->local_space = XR_NULL_HANDLE;
      goto error;
   }
   rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
   if (XR_FAILED(res = s->CreateReferenceSpace(s->session, &rci,
               &s->view_space)))
   {
      RARCH_ERR("[OpenXR] No VIEW space (%d).\n", (int)res);
      s->view_space = XR_NULL_HANDLE;
      goto error;
   }
   return true;

error:
   if (s->local_space)
      s->DestroySpace(s->local_space);
   s->local_space = XR_NULL_HANDLE;
   s->DestroySession(s->session);
   s->session     = XR_NULL_HANDLE;
   return false;
}

void openxr_session_destroy(openxr_session_t *s)
{
   openxr_track_t *t;
   openxr_session_take_track(s);
   if ((t = (openxr_track_t*)retro_atomic_exchange_ptr(&s->track, NULL)))
   {
      s->DestroySpace(t->space);
      free(t);
   }
   s->carry_from = s->carry_to = 0;
   if (s->view_space)
      s->DestroySpace(s->view_space);
   if (s->local_space)
      s->DestroySpace(s->local_space);
   if (s->session)
      s->DestroySession(s->session);
   s->track_space = XR_NULL_HANDLE;
   s->view_space  = XR_NULL_HANDLE;
   s->local_space = XR_NULL_HANDLE;
   s->session     = XR_NULL_HANDLE;
}

bool openxr_session_swap_track_space(openxr_session_t *s,
      enum retro_vr_reference_space *space, const XrPosef *origin,
      void (*swapped)(void *user, int from, int to), void *user)
{
   XrResult res;
   XrReferenceSpaceCreateInfo rci;
   openxr_track_t *t, *old;
   XrSpace fresh                     = XR_NULL_HANDLE;
   enum retro_vr_reference_space got = RETRO_VR_REFERENCE_SPACE_LOCAL;
   bool stage_failed                 = false;
   int from                          = retro_atomic_load_acquire_int(
         &s->track_seq);

   if (!s->session)
      return false;
   rci.type                               = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
   rci.next                               = NULL;
   rci.poseInReferenceSpace.orientation.x = 0.0f;
   rci.poseInReferenceSpace.orientation.y = 0.0f;
   rci.poseInReferenceSpace.orientation.z = 0.0f;
   rci.poseInReferenceSpace.position.x    = 0.0f;
   rci.poseInReferenceSpace.position.y    = 0.0f;
   rci.poseInReferenceSpace.position.z    = 0.0f;
   if (*space == RETRO_VR_REFERENCE_SPACE_STAGE)
   {
      rci.referenceSpaceType                 = XR_REFERENCE_SPACE_TYPE_STAGE;
      rci.poseInReferenceSpace.orientation.w = 1.0f;
      if (XR_SUCCEEDED(s->CreateReferenceSpace(s->session, &rci, &fresh)))
         got = RETRO_VR_REFERENCE_SPACE_STAGE;
      else
      {
         fresh        = XR_NULL_HANDLE;
         stage_failed = true;
      }
   }
   if (!fresh)
   {
      rci.referenceSpaceType   = XR_REFERENCE_SPACE_TYPE_LOCAL;
      rci.poseInReferenceSpace = *origin;
      if (XR_FAILED(res = s->CreateReferenceSpace(s->session, &rci, &fresh)))
      {
         RARCH_ERR("[OpenXR] No space for tracked content (%d).\n", (int)res);
         return false;
      }
      if (stage_failed)
         RARCH_WARN("[OpenXR] No STAGE space: tracked content is seated.\n");
   }
   if (!(t = (openxr_track_t*)malloc(sizeof(*t))))
   {
      s->DestroySpace(fresh);
      return false;
   }
   /* Consecutive carries make one range. */
   if (swapped)
   {
      if (from != s->carry_to)
         s->carry_from = from;
      s->carry_to = from + 1;
   }
   t->space       = fresh;
   t->origin      = *origin;
   t->seq         = from + 1;
   t->carry_from  = s->carry_from;
   t->carry_to    = s->carry_to;
   t->local       = (got == RETRO_VR_REFERENCE_SPACE_LOCAL);
   s->track_space = fresh;
   retro_atomic_store_release_int(&s->track_seq, from + 1);
   /* The frame loop may be submitting in the old one: it goes at that
    * thread's next frame, which takes this one. */
   if ((old = (openxr_track_t*)retro_atomic_exchange_ptr(&s->track, t)))
      mpsc_stack_push(&s->track_retired, &old->park);
   if (swapped)
      swapped(user, from, from + 1);
   *space = got;
   return true;
}

const openxr_track_t *openxr_session_take_track(openxr_session_t *s)
{
   mpsc_stack_node_t *n = mpsc_stack_drain(&s->track_retired);
   while (n)
   {
      openxr_track_t *t = (openxr_track_t*)n;
      n                 = n->next;
      s->DestroySpace(t->space);
      free(t);
   }
   return (const openxr_track_t*)retro_atomic_load_acquire_ptr(&s->track);
}

/* Keeps lost: a lost instance cannot make a new session. */
void openxr_session_reset(openxr_session_t *s)
{
   openxr_session_destroy(s);
   retro_atomic_store_release_int(&s->ended, 0);
   retro_atomic_store_release_int(&s->state, XR_SESSION_STATE_UNKNOWN);
}

void openxr_session_set_alive(openxr_session_t *s, bool alive)
{
   retro_atomic_store_release_int(&s->alive, alive ? 1 : 0);
}

bool openxr_session_alive(openxr_session_t *s)
{
   return retro_atomic_load_acquire_int(&s->alive) != 0;
}

bool openxr_session_ended(openxr_session_t *s)
{
   return retro_atomic_load_acquire_int(&s->ended) != 0;
}

bool openxr_session_running(openxr_session_t *s)
{
   return retro_atomic_load_acquire_int(&s->running) != 0;
}

bool openxr_session_focused(openxr_session_t *s)
{
   return retro_atomic_load_acquire_int(&s->alive)
      && retro_atomic_load_acquire_int(&s->state) == XR_SESSION_STATE_FOCUSED;
}

bool openxr_session_visible(openxr_session_t *s)
{
   int state = retro_atomic_load_acquire_int(&s->state);
   return retro_atomic_load_acquire_int(&s->alive)
      && (   state == XR_SESSION_STATE_VISIBLE
          || state == XR_SESSION_STATE_FOCUSED);
}
