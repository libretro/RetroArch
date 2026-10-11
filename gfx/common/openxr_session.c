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
   s->running = false;
   if (instance_lost)
      s->lost = true;
   if (s->ended)
      return;
   s->ended = true;
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
            memset(&bi, 0, sizeof(bi));
            bi.type                         = XR_TYPE_SESSION_BEGIN_INFO;
            bi.primaryViewConfigurationType =
               XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            openxr_session_lock(s);
            res = s->BeginSession(s->session, &bi);
            openxr_session_unlock(s);
            if (XR_SUCCEEDED(res))
               s->running = true;
            else
               RARCH_ERR("[OpenXR] xrBeginSession failed (%d).\n", (int)res);
         }
         break;
      case XR_SESSION_STATE_STOPPING:
         openxr_session_lock(s);
         s->EndSession(s->session);
         openxr_session_unlock(s);
         s->running = false;
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

void openxr_session_poll(openxr_session_t *s)
{
   XrResult res;
   XrEventDataBuffer ev;
   for (;;)
   {
      memset(&ev, 0, sizeof(ev));
      ev.type = XR_TYPE_EVENT_DATA_BUFFER;
      res     = s->PollEvent(s->rt->instance, &ev);
      if (res == XR_ERROR_INSTANCE_LOST)
         openxr_session_end(s, true);
      if (res != XR_SUCCESS)
         break;
      if (ev.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING)
      {
         const XrEventDataReferenceSpaceChangePending *sc =
            (const XrEventDataReferenceSpaceChangePending*)&ev;
         if (     sc->session == s->session
               && sc->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL
               && s->hooks.local_changed)
            s->hooks.local_changed(s->hooks.user);
      }
      if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
      {
         const XrEventDataSessionStateChanged *sc =
            (const XrEventDataSessionStateChanged*)&ev;
         /* A session destroyed for a new one may have some queued. */
         if (sc->session == s->session)
            openxr_session_state(s, sc->state);
      }
      else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
         openxr_session_end(s, true);
      else if (ev.type == XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB)
      {
         const XrEventDataDisplayRefreshRateChangedFB *rc =
            (const XrEventDataDisplayRefreshRateChangedFB*)&ev;
         RARCH_LOG("[OpenXR] The headset changed from %.2f to %.2f Hz.\n",
               rc->fromDisplayRefreshRate, rc->toDisplayRefreshRate);
      }
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

   memset(&sci, 0, sizeof(sci));
   sci.type     = XR_TYPE_SESSION_CREATE_INFO;
   sci.next     = graphics_binding;
   sci.systemId = s->rt->system;
   if (XR_FAILED(res = s->CreateSession(s->rt->instance, &sci, &s->session)))
   {
      RARCH_ERR("[OpenXR] xrCreateSession failed (%d).\n", (int)res);
      s->session = XR_NULL_HANDLE;
      return false;
   }

   memset(&rci, 0, sizeof(rci));
   rci.type                               = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
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
   if (s->view_space)
      s->DestroySpace(s->view_space);
   if (s->local_space)
      s->DestroySpace(s->local_space);
   if (s->session)
      s->DestroySession(s->session);
   s->view_space  = XR_NULL_HANDLE;
   s->local_space = XR_NULL_HANDLE;
   s->session     = XR_NULL_HANDLE;
}

/* Keeps lost: a lost instance cannot make a new session. */
void openxr_session_reset(openxr_session_t *s)
{
   openxr_session_destroy(s);
   s->ended = false;
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
