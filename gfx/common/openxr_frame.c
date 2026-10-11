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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <features/features_cpu.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>

#include "openxr_frame.h"

#include "../video_defines.h"

#include "../../runloop.h"
#include "../../verbosity.h"

/* One writer stamps odd, stores the words, stamps even; a reader that
 * sees the stamp move across its copy starts over. */
static void openxr_seq_publish(retro_atomic_int_t *seq,
      retro_atomic_int_t *words, const void *src, size_t len)
{
   int tmp[OPENXR_WORDS(video_xr_quad_set_t)];
   size_t n = (len + sizeof(int) - 1) / sizeof(int);
   size_t i;
   int s    = retro_atomic_load_relaxed_int(seq);

   tmp[n - 1] = 0;
   memcpy(tmp, src, len);
   retro_atomic_store_release_int(seq, s + 1);
   retro_atomic_thread_fence_release();
   for (i = 0; i < n; i++)
      retro_atomic_store_relaxed_int(&words[i], tmp[i]);
   retro_atomic_store_release_int(seq, s + 2);
}

static void openxr_seq_read(retro_atomic_int_t *seq,
      retro_atomic_int_t *words, void *dst, size_t len)
{
   int tmp[OPENXR_WORDS(video_xr_quad_set_t)];
   size_t n = (len + sizeof(int) - 1) / sizeof(int);
   for (;;)
   {
      size_t i;
      int s1 = retro_atomic_load_acquire_int(seq);
      if (s1 & 1)
      {
         retro_cpu_relax();
         continue;
      }
      for (i = 0; i < n; i++)
         tmp[i] = retro_atomic_load_relaxed_int(&words[i]);
      retro_atomic_thread_fence_acquire();
      if (retro_atomic_load_relaxed_int(seq) == s1)
         break;
   }
   memcpy(dst, tmp, len);
}

static void openxr_frame_publish_tracked(openxr_frame_t *f)
{
   openxr_seq_publish(&f->tracked_seq, f->tracked_words,
         &f->tracked, sizeof(f->tracked));
}

static void openxr_frame_read_tracked(openxr_frame_t *f,
      openxr_tracked_t *out)
{
   openxr_seq_read(&f->tracked_seq, f->tracked_words,
         out, sizeof(*out));
}

void openxr_frame_tick_notify(openxr_frame_t *f)
{
   if (f->tick_ready)
      retro_eventcount_notify(&f->tick);
}

/* Pixels per radian across one eye, from the first located views. */
static void openxr_frame_measure(openxr_frame_t *f, XrTime time)
{
   float fov;
   uint32_t n = 0;
   XrViewLocateInfo li;
   XrViewState vs;
   XrView views[2];

   memset(&li, 0, sizeof(li));
   li.type                  = XR_TYPE_VIEW_LOCATE_INFO;
   li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
   li.displayTime           = time;
   li.space                 = f->session->local_space;
   memset(&vs, 0, sizeof(vs));
   vs.type                  = XR_TYPE_VIEW_STATE;
   memset(views, 0, sizeof(views));
   views[0].type            = XR_TYPE_VIEW;
   views[1].type            = XR_TYPE_VIEW;
   if (     XR_FAILED(f->session->LocateViews(f->session->session, &li,
                  &vs, 2, &n, views))
         || !n)
      return;
   fov = views[0].fov.angleRight - views[0].fov.angleLeft;
   if (fov < 0.1f || !f->rt->rec_width)
      return;
   f->tracked.px_per_rad = (float)f->rt->rec_width / fov;
   openxr_frame_publish_tracked(f);
   RARCH_LOG("[OpenXR] %u pixels across %.0f degrees per eye.\n",
         (unsigned)f->rt->rec_width, fov * 57.29578f);
}

static void openxr_frame_error(openxr_frame_t *f,
      const char *fn, XrResult res)
{
   if (!f->frame_failed)
   {
      f->frame_failed = true;
      RARCH_ERR("[OpenXR] %s failed (%d).\n", fn, (int)res);
   }
   /* A runtime may report the loss here without an event. */
   if (res == XR_ERROR_SESSION_LOST || res == XR_ERROR_INSTANCE_LOST)
      openxr_session_end(f->session, res == XR_ERROR_INSTANCE_LOST);
}

/* The published quads whose slot has released an image. The caller
 * holds the graphics lock, so no slot changes under it. */
static unsigned openxr_frame_layers(openxr_frame_t *f,
      XrCompositionLayerQuad *layers,
      const XrCompositionLayerBaseHeader **ptrs)
{
   unsigned i;
   unsigned n = 0;
   video_xr_quad_set_t quads;
   openxr_seq_read(&f->quads_seq, f->quads_words,
         &quads, sizeof(quads));
   for (i = 0; i < quads.num_quads; i++)
   {
      const video_xr_quad_t *q        = &quads.quads[i];
      openxr_slot_t *slot             = &f->sc->slots[q->slot];
      XrCompositionLayerQuad *l       = &layers[n];
      if (     !slot->swapchains[0] || q->layer >= slot->layers
            || !retro_atomic_load_acquire_int(&slot->content))
         continue;
      memset(l, 0, sizeof(*l));
      l->type          = XR_TYPE_COMPOSITION_LAYER_QUAD;
      /* The UI is drawn over transparent black: premultiplied. */
      l->layerFlags    = (q->kind == VIDEO_XR_QUAD_MENU)
         ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
      l->space         = f->session->local_space;
      l->eyeVisibility = (q->eye == VIDEO_XR_EYE_LEFT)
         ? XR_EYE_VISIBILITY_LEFT
         : ((q->eye == VIDEO_XR_EYE_RIGHT)
               ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_BOTH);
      l->subImage.swapchain               = slot->swapchains[q->layer];
      l->subImage.imageRect.extent.width  = (int32_t)VIDEO_SCALE_W(slot->dims);
      l->subImage.imageRect.extent.height = (int32_t)VIDEO_SCALE_H(slot->dims);
      l->pose.orientation.x = q->pose.orientation.x;
      l->pose.orientation.y = q->pose.orientation.y;
      l->pose.orientation.z = q->pose.orientation.z;
      l->pose.orientation.w = q->pose.orientation.w;
      l->pose.position.x    = q->pose.position.x;
      l->pose.position.y    = q->pose.position.y;
      l->pose.position.z    = q->pose.position.z;
      l->size.width         = q->width;
      l->size.height        = q->height;
      ptrs[n++]             = (const XrCompositionLayerBaseHeader*)l;
   }
   return n;
}

/* The head's position and heading at time become the anchor. */
static void openxr_frame_recenter(openxr_frame_t *f, XrTime time)
{
   XrSpaceLocation loc;
   video_xr_pose_t head, anchor;
   XrSpaceLocationFlags valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;

   memset(&loc, 0, sizeof(loc));
   loc.type = XR_TYPE_SPACE_LOCATION;
   if (     XR_FAILED(f->session->LocateSpace(f->session->view_space,
               f->session->local_space, time, &loc))
         || (loc.locationFlags & valid) != valid)
   {
      RARCH_WARN("[OpenXR] Recenter: the headset is not tracked.\n");
      return;
   }
   head.orientation.x = loc.pose.orientation.x;
   head.orientation.y = loc.pose.orientation.y;
   head.orientation.z = loc.pose.orientation.z;
   head.orientation.w = loc.pose.orientation.w;
   head.position.x    = loc.pose.position.x;
   head.position.y    = loc.pose.position.y;
   head.position.z    = loc.pose.position.z;
   if (!video_xr_anchor_from_head(&head, &anchor))
      return;
   f->tracked.anchor = anchor;
   openxr_frame_publish_tracked(f);
   RARCH_LOG("[OpenXR] Recentered at %.2f, %.2f, %.2f.\n",
         anchor.position.x, anchor.position.y, anchor.position.z);
}

/* Every interval headset frames, a tick for the core. */
static void openxr_frame_tick(openxr_frame_t *f)
{
   unsigned interval = (unsigned)retro_atomic_load_acquire_int(
         &f->interval);
   if (interval != f->tick_interval)
   {
      f->tick_interval = interval;
      f->tick_count    = 0;
   }
   if (!interval || ++f->tick_count < interval)
      return;
   f->tick_count = 0;
   retro_atomic_fetch_add_int(&f->tick_seq, 1);
   openxr_frame_tick_notify(f);
}

/* The rate the drawing thread wants, asked once a session and value. */
static void openxr_frame_ask_rate(openxr_frame_t *f)
{
   XrResult res;
   float hz;
   int bits = retro_atomic_load_acquire_int(&f->want_rate);
   memcpy(&hz, &bits, sizeof(hz));
   if (!f->rt->refresh_ext || hz <= 0.0f || hz == f->asked_rate)
      return;
   f->asked_rate = hz;
   res           = f->rt->RequestDisplayRefreshRateFB(f->session->session, hz);
   RARCH_LOG("[OpenXR] Asked the headset for %.2f Hz (%d).\n", hz,
         (int)res);
}

/* One headset frame. xrWaitFrame paces this thread at the headset's
 * rate, and the core too while the headset paces it. */
static void openxr_frame_run(openxr_frame_t *f)
{
   XrResult res;
   int session_state;
   const char *fn = NULL;
   XrFrameWaitInfo wait_info;
   XrFrameState state;
   XrFrameBeginInfo begin_info;
   XrFrameEndInfo end_info;
   XrCompositionLayerQuad layers[VIDEO_XR_MAX_QUADS];
   const XrCompositionLayerBaseHeader *ptrs[VIDEO_XR_MAX_QUADS
      + OPENXR_MAX_EXTRA_LAYERS];

   memset(&wait_info, 0, sizeof(wait_info));
   wait_info.type = XR_TYPE_FRAME_WAIT_INFO;
   memset(&state, 0, sizeof(state));
   state.type     = XR_TYPE_FRAME_STATE;
   if (XR_FAILED(res = f->WaitFrame(f->session->session, &wait_info, &state)))
   {
      openxr_frame_error(f, "xrWaitFrame", res);
      retro_sleep(1);
      return;
   }
   session_state = retro_atomic_load_acquire_int(&f->session->state);
   /* A headset that is not worn reports periods that are not its own. */
   if (    (   session_state == XR_SESSION_STATE_VISIBLE
            || session_state == XR_SESSION_STATE_FOCUSED)
         && video_xr_period_add(&f->period,
            (int64_t)state.predictedDisplayPeriod))
   {
      retro_atomic_store_release_int(&f->period_ns,
            (int)f->period.published);
      RARCH_LOG("[OpenXR] The headset runs at %.2f Hz.\n",
            1000000000.0 / (double)f->period.published);
   }
   openxr_frame_tick(f);
   openxr_frame_ask_rate(f);
   f->tracked.predicted_time = state.predictedDisplayTime;
   openxr_frame_publish_tracked(f);
   if (f->tracked.px_per_rad <= 0.0f)
      openxr_frame_measure(f, state.predictedDisplayTime);
   if (retro_atomic_load_acquire_int(&f->recenter))
   {
      retro_atomic_store_release_int(&f->recenter, 0);
      openxr_frame_recenter(f, state.predictedDisplayTime);
   }

   memset(&begin_info, 0, sizeof(begin_info));
   begin_info.type               = XR_TYPE_FRAME_BEGIN_INFO;
   memset(&end_info, 0, sizeof(end_info));
   end_info.type                 = XR_TYPE_FRAME_END_INFO;
   end_info.displayTime          = state.predictedDisplayTime;
   end_info.environmentBlendMode = f->rt->blend_mode;

   openxr_session_lock(f->session);
   if (XR_FAILED(res = f->BeginFrame(f->session->session, &begin_info)))
      fn = "xrBeginFrame";
   else
   {
      session_state = retro_atomic_load_acquire_int(&f->session->state);
      /* Layers only while the headset shows the session. */
      if (     state.shouldRender
            && (   session_state == XR_SESSION_STATE_VISIBLE
                || session_state == XR_SESSION_STATE_FOCUSED))
      {
         end_info.layerCount = openxr_frame_layers(f, layers, ptrs);
         if (f->extra_layers)
            end_info.layerCount += f->extra_layers(f->extra_user,
                  state.predictedDisplayTime,
                  ptrs + end_info.layerCount,
                  OPENXR_MAX_EXTRA_LAYERS);
      }
      end_info.layers = end_info.layerCount ? ptrs : NULL;
      if (XR_FAILED(res = f->EndFrame(f->session->session, &end_info)))
         fn = "xrEndFrame";
   }
   openxr_session_unlock(f->session);
   if (fn)
      openxr_frame_error(f, fn, res);
   else
      f->frame_failed = false;
}

static void openxr_frame_thread(void *data)
{
   openxr_frame_t *f = (openxr_frame_t*)data;
   while (!retro_atomic_load_acquire_int(&f->quit))
   {
      openxr_session_poll(f->session);
      if (f->session->running)
         openxr_frame_run(f);
      else
         retro_sleep(10);
   }
}

/* The rates XR_FB_display_refresh_rate lists for this session. */
static void openxr_frame_list_rates(openxr_frame_t *f)
{
   char s[256];
   uint32_t i;
   size_t len   = 0;
   uint32_t n   = 0;
   f->num_rates = 0;
   if (     !f->rt->refresh_ext
         || XR_FAILED(f->rt->EnumerateDisplayRefreshRatesFB(
               f->session->session, 0, &n, NULL))
         || !n)
      return;
   if (n > VIDEO_HEADSET_MAX_RATES)
   {
      /* The runtime takes no less room than it lists. */
      uint32_t all = n;
      float *tmp   = (float*)malloc(all * sizeof(*tmp));
      if (!tmp)
         return;
      if (XR_FAILED(f->rt->EnumerateDisplayRefreshRatesFB(
               f->session->session, all, &all, tmp)))
      {
         free(tmp);
         return;
      }
      n = (all < VIDEO_HEADSET_MAX_RATES) ? all : VIDEO_HEADSET_MAX_RATES;
      memcpy(f->rates, tmp, n * sizeof(*tmp));
      free(tmp);
      if (all > n)
         RARCH_WARN("[OpenXR] The headset offers %u rates; %u dropped.\n",
               (unsigned)all, (unsigned)(all - n));
   }
   else if (XR_FAILED(f->rt->EnumerateDisplayRefreshRatesFB(
            f->session->session, n, &n, f->rates)))
      return;
   f->num_rates = n;
   s[0]         = '\0';
   for (i = 0; i < n && len < sizeof(s); i++)
      len += snprintf(s + len, sizeof(s) - len, "%s%.2f",
            i ? ", " : "", f->rates[i]);
   RARCH_LOG("[OpenXR] The headset offers %s Hz.\n", s);
}

bool openxr_frame_load(openxr_frame_t *f, const openxr_runtime_t *rt,
      openxr_session_t *session, openxr_swapchains_t *sc)
{
   f->rt      = rt;
   f->session = session;
   f->sc      = sc;
   if (!f->tick_ready)
   {
      if (!retro_eventcount_init(&f->tick))
         return false;
      f->tick_ready = true;
   }
   return OPENXR_FN(rt, f, WaitFrame)
       && OPENXR_FN(rt, f, BeginFrame)
       && OPENXR_FN(rt, f, EndFrame);
}

void openxr_frame_deinit(openxr_frame_t *f)
{
   openxr_frame_stop(f);
   if (f->tick_ready)
      retro_eventcount_free(&f->tick);
   f->tick_ready = false;
}

void openxr_frame_session_created(openxr_frame_t *f)
{
   video_xr_pose_identity(&f->tracked.anchor);
   openxr_frame_publish_tracked(f);
   retro_atomic_store_release_int(&f->recenter, 0);
   openxr_frame_list_rates(f);
}

bool openxr_frame_start(openxr_frame_t *f)
{
   video_xr_period_init(&f->period);
   retro_atomic_store_release_int(&f->period_ns, 0);
   f->tick_count    = 0;
   f->tick_interval = 0;
   f->asked_rate    = 0.0f;
   f->frame_failed  = false;
   retro_atomic_store_release_int(&f->quit, 0);
   openxr_session_set_alive(f->session, true);
   if (!(f->thread = sthread_create(openxr_frame_thread, f)))
   {
      openxr_session_set_alive(f->session, false);
      return false;
   }
   return true;
}

void openxr_frame_stop(openxr_frame_t *f)
{
   if (!f->thread)
      return;
   retro_atomic_store_release_int(&f->quit, 1);
   sthread_join(f->thread);
   f->thread = NULL;
   /* Nothing shows the session's frames now: a driver that presents on
    * (a staged content load keeps it up) draws none for it. */
   openxr_session_set_alive(f->session, false);
}

/* The runtime's own recenter moves LOCAL, which the anchor is in: the
 * screens go back straight ahead, and a hotkey request with them. */
void openxr_frame_local_changed(openxr_frame_t *f)
{
   retro_atomic_store_release_int(&f->recenter, 0);
   video_xr_pose_identity(&f->tracked.anchor);
   openxr_frame_publish_tracked(f);
   RARCH_LOG("[OpenXR] Recentered by the runtime.\n");
}

void openxr_frame_clear_quads(openxr_frame_t *f)
{
   video_xr_quad_set_t none;
   memset(&none, 0, sizeof(none));
   openxr_seq_publish(&f->quads_seq, f->quads_words, &none, sizeof(none));
}

XrTime openxr_frame_predicted_time(openxr_frame_t *f)
{
   openxr_tracked_t t;
   openxr_frame_read_tracked(f, &t);
   return t.predicted_time;
}

float openxr_frame_pixels_per_radian(openxr_frame_t *f)
{
   openxr_tracked_t t;
   openxr_frame_read_tracked(f, &t);
   return t.px_per_rad;
}

float openxr_frame_refresh_rate(openxr_frame_t *f)
{
   int ns = retro_atomic_load_acquire_int(&f->period_ns);
   return (ns > 0) ? (float)(1000000000.0 / (double)ns) : 0.0f;
}

void openxr_frame_set_pacing(openxr_frame_t *f, unsigned interval)
{
   retro_atomic_store_release_int(&f->interval, (int)interval);
   if (!interval)
      f->pace_mode = 0;
}

/* The next tick after the last one seen, or false after timeout_ns. */
static bool openxr_frame_wait_tick(openxr_frame_t *f, int64_t timeout_ns)
{
   int seq;
   retro_time_t deadline = cpu_features_get_time_usec()
      + (retro_time_t)(timeout_ns / 1000);
   for (;;)
   {
      int key;
      retro_time_t left;
      seq = retro_atomic_load_acquire_int(&f->tick_seq);
      if (seq != f->tick_seen || !openxr_session_visible(f->session))
         break;
      left = deadline - cpu_features_get_time_usec();
      if (left <= 0)
         break;
      key = retro_eventcount_prepare_wait(&f->tick);
      seq = retro_atomic_load_acquire_int(&f->tick_seq);
      if (seq != f->tick_seen || !openxr_session_visible(f->session))
      {
         retro_eventcount_cancel_wait(&f->tick);
         break;
      }
      if (!retro_eventcount_commit_wait_timeout(&f->tick, key, left))
      {
         seq = retro_atomic_load_acquire_int(&f->tick_seq);
         break;
      }
   }
   if (seq == f->tick_seen)
      return false;
   f->tick_seen = seq;
   return true;
}

void openxr_frame_pace_skip(openxr_frame_t *f)
{
   if (!f->tick_ready)
      return;
   f->tick_seen = retro_atomic_load_acquire_int(&f->tick_seq);
}

void openxr_frame_pace_wait(openxr_frame_t *f)
{
   int64_t period = (int64_t)retro_atomic_load_acquire_int(&f->period_ns)
      * retro_atomic_load_acquire_int(&f->interval);
   if (period <= 0 || !f->tick_ready)
      return;
   if (openxr_session_visible(f->session))
   {
      if (f->pace_mode != 1)
         RARCH_LOG("[OpenXR] Pacing on the headset's frames.\n");
      f->pace_mode = 1;
      if (openxr_frame_wait_tick(f, period * 2))
         f->tick_late = false;
      else if (openxr_session_visible(f->session) && !f->tick_late)
      {
         f->tick_late = true;
         RARCH_WARN("[OpenXR] No headset frame for two intervals; the core carries on.\n");
      }
      f->pace_anchor_ns = (int64_t)cpu_features_get_time_usec() * 1000;
   }
   else
   {
      /* No ticks to wait on: keep the core's rate on the clock, from
       * the last tick on. */
      retro_time_t sleep_us;
      if (f->pace_mode != 2)
         RARCH_LOG("[OpenXR] Pacing on the clock while the headset does not show the session.\n");
      f->pace_mode = 2;
      openxr_frame_pace_skip(f);
      sleep_us     = runloop_pace_schedule(&f->pace_anchor_ns, period,
            cpu_features_get_time_usec());
      if (sleep_us > 0)
         retro_sleep_us((unsigned)sleep_us);
   }
}

unsigned openxr_frame_refresh_rates(const openxr_frame_t *f,
      float *rates, unsigned cap)
{
   unsigned i;
   for (i = 0; i < f->num_rates && i < cap; i++)
      rates[i] = f->rates[i];
   return i;
}

void openxr_frame_request_rate(openxr_frame_t *f, float hz)
{
   int bits;
   memcpy(&bits, &hz, sizeof(bits));
   retro_atomic_store_release_int(&f->want_rate, bits);
}

void openxr_frame_publish(openxr_frame_t *f, const video_xr_quad_set_t *set)
{
   openxr_seq_publish(&f->quads_seq, f->quads_words, set, sizeof(*set));
}

void openxr_frame_get_anchor(openxr_frame_t *f, video_xr_pose_t *anchor)
{
   openxr_tracked_t t;
   openxr_frame_read_tracked(f, &t);
   *anchor = t.anchor;
}

void openxr_frame_get_quads(openxr_frame_t *f, video_xr_quad_set_t *out)
{
   openxr_seq_read(&f->quads_seq, f->quads_words, out, sizeof(*out));
}

void openxr_frame_request_recenter(openxr_frame_t *f)
{
   retro_atomic_store_release_int(&f->recenter, 1);
}
