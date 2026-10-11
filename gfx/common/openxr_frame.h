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

/* The tracked eyes as the drawing thread drew them: each eye's pose and
 * field of view in the session's track space as track_seq numbered it,
 * and the rectangle drawn in its image. */
typedef struct openxr_projection
{
   XrPosef pose[2];
   XrFovf fov[2];
   unsigned width;
   unsigned height;
   int track_seq;
   bool active;
} openxr_projection_t;

/* One headset frame as far as its begin: what the end needs. */
typedef struct openxr_frame_begun
{
   XrTime display_time;     /* the wait's predicted display time */
   XrDuration period;       /* and its predicted period, as given */
   bool waited;             /* xrWaitFrame succeeded */
   bool begun;              /* xrBeginFrame succeeded: end it */
   bool render;             /* shouldRender, and the headset shows the session */
} openxr_frame_begun_t;

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
   bool frame_failed;              /* the frame loop's thread */
   /* The frames are the drawing thread's own: begun and ended in its
    * frame, no XR thread. */
   bool inline_host;
   /* A seqlock over its words like video_driver.c's viewport
    * parameters: the writer's own copy, the stamp and the words. */
   openxr_tracked_t tracked;       /* the frame loop's thread */
   retro_atomic_int_t tracked_seq;
   retro_atomic_int_t tracked_words[OPENXR_WORDS(openxr_tracked_t)];
   /* The drawing thread's quads for the XR thread and the pointer, the
    * same way. */
   retro_atomic_int_t quads_seq;
   retro_atomic_int_t quads_words[OPENXR_WORDS(video_xr_quad_set_t)];
   /* The drawing thread's projection, the same way. */
   retro_atomic_int_t proj_seq;
   retro_atomic_int_t proj_words[OPENXR_WORDS(openxr_projection_t)];
   retro_atomic_int_t recenter;
   /* Each move of the anchor, the hotkey's or the runtime's. */
   retro_atomic_int_t recenters;
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

/* A host that draws the headset's frames in its own (the Quest's): no
 * thread; begin polls, waits and begins, end submits and ends, each
 * taking the graphics lock around its runtime calls alone. A frame
 * that was not begun still ends: the core keeps the headset's period
 * on the clock. */
bool openxr_frame_start_inline(openxr_frame_t *f);
void openxr_frame_stop_inline(openxr_frame_t *f);
bool openxr_frame_begin(openxr_frame_t *f, openxr_frame_begun_t *b);
void openxr_frame_end(openxr_frame_t *f, const openxr_frame_begun_t *b);
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
/* The projection the XR thread submits from now on, behind the
 * screens; left out until its slot releases an image, and once the
 * track space it was drawn in is replaced. Published in the same
 * graphics-lock span as its slot's release, so a new image never goes
 * out with the previous poses. */
void openxr_frame_publish_projection(openxr_frame_t *f,
      const openxr_projection_t *p);
void openxr_frame_clear_projection(openxr_frame_t *f);
void openxr_frame_get_anchor(openxr_frame_t *f, video_xr_pose_t *anchor);
void openxr_frame_get_quads(openxr_frame_t *f, video_xr_quad_set_t *out);
void openxr_frame_request_recenter(openxr_frame_t *f);
/* Any thread: how often the anchor has moved, and the display period
 * the frames publish, 0 until measured. */
int openxr_frame_recenters(openxr_frame_t *f);
XrDuration openxr_frame_period(openxr_frame_t *f);

RETRO_END_DECLS

#endif
