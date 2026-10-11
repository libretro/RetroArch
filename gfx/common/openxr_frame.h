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

#ifndef __OPENXR_FRAME_H
#define __OPENXR_FRAME_H

#include <stdint.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>

#include <openxr/openxr.h>

#include "openxr_runtime.h"
#include "openxr_session.h"
#include "openxr_swapchain.h"
#include "../video_xr.h"
#include "../video_views.h"

RETRO_BEGIN_DECLS

#define OPENXR_MAX_EXTRA_LAYERS 4

/* What the XR thread publishes for the drawing thread. */
typedef struct openxr_tracked
{
   XrTime predicted_time;
   video_xr_pose_t anchor;
   float px_per_rad;
} openxr_tracked_t;

#define OPENXR_WORDS(t) ((sizeof(t) + sizeof(int) - 1) / sizeof(int))

typedef struct openxr_frame
{
   const openxr_runtime_t *rt;
   openxr_session_t *session;
   openxr_swapchains_t *sc;
   /* Layers after the screens (the laser, its cursor); may be NULL. */
   unsigned (*extra_layers)(void *user, XrTime display_time,
         const XrCompositionLayerBaseHeader **layers, unsigned cap);
   void *extra_user;
   sthread_t *thread;
   retro_atomic_int_t quit;
   bool frame_failed;              /* XR thread */
   /* A seqlock over its words like video_driver.c's viewport
    * parameters: the writer's own copy, the stamp and the words. */
   openxr_tracked_t tracked;       /* XR thread */
   retro_atomic_int_t tracked_seq;
   retro_atomic_int_t tracked_words[OPENXR_WORDS(openxr_tracked_t)];
   /* The drawing thread's quads for the XR thread and the pointer, the
    * same way. */
   retro_atomic_int_t quads_seq;
   retro_atomic_int_t quads_words[OPENXR_WORDS(video_xr_quad_set_t)];
   retro_atomic_int_t recenter;
   /* The headset's period as the XR thread measures it, and the one it
    * published, 0 until known. */
   video_xr_period_t period;       /* XR thread */
   retro_atomic_int_t period_ns;
   /* Pacing: every interval headset frames the XR thread bumps tick_seq
    * and signals tick; the drawing thread waits on it once a core
    * frame, or on the clock while the headset doesn't show the session. */
   retro_eventcount_t tick;
   retro_atomic_int_t tick_seq;
   bool tick_ready;
   retro_atomic_int_t interval;
   unsigned tick_count;            /* XR thread */
   unsigned tick_interval;         /* XR thread */
   int tick_seen;                  /* drawing thread */
   int64_t pace_anchor_ns;         /* drawing thread */
   unsigned pace_mode;             /* drawing thread: 0, 1 ticks, 2 clock */
   bool tick_late;                 /* drawing thread: warned, no tick since */
   /* XR_FB_display_refresh_rate: the rates the session lists, the one
    * to ask for (float bits, 0 for none), and the last one asked. */
   float rates[VIDEO_HEADSET_MAX_RATES];
   unsigned num_rates;
   retro_atomic_int_t want_rate;
   float asked_rate;               /* XR thread */
   PFN_xrWaitFrame WaitFrame;
   PFN_xrBeginFrame BeginFrame;
   PFN_xrEndFrame EndFrame;
} openxr_frame_t;

bool openxr_frame_load(openxr_frame_t *f, const openxr_runtime_t *rt,
      openxr_session_t *session, openxr_swapchains_t *sc);
void openxr_frame_deinit(openxr_frame_t *f);
/* A new session: the anchor straight ahead, the rates it offers. */
void openxr_frame_session_created(openxr_frame_t *f);
/* The desktop host: the XR thread. */
bool openxr_frame_start(openxr_frame_t *f);
void openxr_frame_stop(openxr_frame_t *f);
void openxr_frame_tick_notify(openxr_frame_t *f);
/* The runtime recentered LOCAL. */
void openxr_frame_local_changed(openxr_frame_t *f);
void openxr_frame_clear_quads(openxr_frame_t *f);

XrTime openxr_frame_predicted_time(openxr_frame_t *f);
float openxr_frame_pixels_per_radian(openxr_frame_t *f);
float openxr_frame_refresh_rate(openxr_frame_t *f);
void openxr_frame_set_pacing(openxr_frame_t *f, unsigned interval);
void openxr_frame_pace_wait(openxr_frame_t *f);
void openxr_frame_pace_skip(openxr_frame_t *f);
unsigned openxr_frame_refresh_rates(const openxr_frame_t *f,
      float *rates, unsigned cap);
void openxr_frame_request_rate(openxr_frame_t *f, float hz);
void openxr_frame_publish(openxr_frame_t *f, const video_xr_quad_set_t *set);
void openxr_frame_get_anchor(openxr_frame_t *f, video_xr_pose_t *anchor);
void openxr_frame_get_quads(openxr_frame_t *f, video_xr_quad_set_t *out);
void openxr_frame_request_recenter(openxr_frame_t *f);

RETRO_END_DECLS

#endif
