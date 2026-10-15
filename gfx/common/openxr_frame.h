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
#include <libretro.h>

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

/* Where a headset frame shows the screens, and whether in VIEW space. */
typedef struct openxr_screens
{
   video_xr_pose_t pose;
   int view;
} openxr_screens_t;

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

/* Set before start and unchanged until join; stop also cleans failed start. */
typedef struct openxr_frame_thread_hooks
{
   bool (*start)(void *user);
   void (*fill)(void *user, XrTime display_time);
   void (*stop)(void *user);
   void *user;
} openxr_frame_thread_hooks_t;

typedef struct openxr_frame
{
   const openxr_runtime_t *rt;
   openxr_session_t *session;
   openxr_swapchains_t *sc;
   /* Layers after the screens (the laser, its cursor); may be NULL. */
   unsigned (*extra_layers)(void *user, XrTime display_time,
         const XrCompositionLayerBaseHeader **layers, unsigned cap);
   void *extra_user;
   /* The hands; may be NULL. */
   bool (*locate_hands)(void *user, XrSpace base, XrTime time,
         struct retro_vr_controller_poses *out);
   /* The laser's dot the host made for the input's layers, start to
    * stop; the frame loop reads it. */
   XrSwapchain cursor;
   /* From the input's attach to its detach. */
   bool input_attached;
   sthread_t *thread;
   openxr_frame_thread_hooks_t thread_hooks;
   retro_atomic_int_t thread_started;
   retro_atomic_int_t quit;
   bool frame_failed;              /* the frame loop's thread */
   /* The frames are the drawing thread's own: begun and ended in its
    * frame, no XR thread. */
   bool inline_host;
   bool repeating;                 /* inside openxr_frame_repeat */
   /* A seqlock over its words like video_driver.c's viewport
    * parameters: the writer's own copy, the stamp and the words. */
   openxr_tracked_t tracked;       /* the frame loop's thread */
   retro_atomic_int_t tracked_seq;
   retro_atomic_int_t tracked_words[OPENXR_WORDS(openxr_tracked_t)];
   /* The screens as each frame submits them, the same way. */
   openxr_screens_t screens;       /* the frame loop's thread */
   retro_atomic_int_t screens_seq;
   retro_atomic_int_t screens_words[OPENXR_WORDS(openxr_screens_t)];
   /* The drawing thread's quads for the XR thread and the pointer, the
    * same way. */
   retro_atomic_int_t quads_seq;
   retro_atomic_int_t quads_words[OPENXR_WORDS(video_xr_quad_set_t)];
   /* The drawing thread's projection, the same way. */
   retro_atomic_int_t proj_seq;
   retro_atomic_int_t proj_words[OPENXR_WORDS(openxr_projection_t)];
   /* Consecutive carried spaces, under the graphics lock. */
   int carry_from;
   int carry_to;
   retro_atomic_int_t recenter;
   /* Each move of the anchor, the hotkey's or the runtime's. */
   retro_atomic_int_t recenters;
   /* Screen Placement, the drawing thread's (enum
    * video_openxr_placement); Lazy Follow's anchor, the frame loop's. */
   retro_atomic_int_t placement;
   video_xr_pose_t lazy;
   XrTime lazy_time;
   int lazy_recenters;
   bool lazy_valid;
   bool lazy_gliding;
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
    * to ask for (float bits, 0 for none), and requested/applied rates. */
   float rates[VIDEO_HEADSET_MAX_RATES];
   unsigned num_rates;
   retro_atomic_int_t want_rate;
   float asked_rate;               /* XR thread */
   float requested_rate;           /* XR thread */
   int64_t rate_retry_usec;        /* XR thread */
   int64_t perf_retry_usec;        /* XR thread */
   unsigned perf_applied;         /* CPU/GPU success bits, XR thread */
   /* XR_EXT_performance_settings: the clocks last asked for this
    * session, -1 none yet, 0 sustained high, 1 boosted. XR thread. */
   int asked_perf;
   PFN_xrWaitFrame WaitFrame;
   PFN_xrBeginFrame BeginFrame;
   PFN_xrEndFrame EndFrame;
} openxr_frame_t;

/* What the headset input needs of a session: valid from its
 * session_created to its session_destroying. Handles rather than the
 * runtime, since the stand-in builds no openxr_runtime_t. */
typedef struct openxr_input_handles
{
   openxr_frame_t *frame;      /* its session, predicted time, quads, dot */
   XrInstance instance;
   XrSession session;
   PFN_xrGetInstanceProcAddr get_proc;
   bool frame_controller;      /* XR_VALVE_frame_controller_interaction */
   bool palm_pose;             /* XR_EXT_palm_pose */
   bool hand_tracking;         /* XR_EXT_hand_tracking, hands tracked */
   bool hand_data_source;      /* XR_EXT_hand_tracking_data_source */
} openxr_input_handles_t;

typedef struct openxr_input_hooks
{
   /* The session and its spaces exist and its frame loop has not
    * started: make, suggest and attach action sets here. A kept
    * device's instance gets a new session after the runtime ends one,
    * so this runs again for each. */
   void (*session_created)(void *user, const openxr_input_handles_t *h);
   /* The frame loop has stopped; the session is destroyed next. */
   void (*session_destroying)(void *user);
   /* The frame loop's thread, each headset frame: up to cap layers over
    * the screens. Returns how many were written. */
   unsigned (*frame_layers)(void *user, XrTime display_time,
         const XrCompositionLayerBaseHeader **layers, unsigned cap);
   /* The two hands' poses and joints in base at time.
    * False when none could be located. */
   bool (*locate_hands)(void *user, XrSpace base, XrTime time,
         struct retro_vr_controller_poses *out);
   void *user;
} openxr_input_hooks_t;

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
/* An in-line host's core frame shown for n more headset frames, each
 * waited, begun and ended with what was last published. */
void openxr_frame_repeat(openxr_frame_t *f, unsigned n);
void openxr_frame_tick_notify(openxr_frame_t *f);
/* The runtime recentered LOCAL. */
void openxr_frame_local_changed(openxr_frame_t *f);
void openxr_frame_clear_quads(openxr_frame_t *f);

XrTime openxr_frame_predicted_time(openxr_frame_t *f);
bool openxr_frame_locate_hands(openxr_frame_t *f, XrSpace base, XrTime time,
      struct retro_vr_controller_poses *out);
float openxr_frame_pixels_per_radian(openxr_frame_t *f);
float openxr_frame_refresh_rate(openxr_frame_t *f);
void openxr_frame_set_pacing(openxr_frame_t *f, unsigned interval);
void openxr_frame_pace_wait(openxr_frame_t *f);
void openxr_frame_pace_skip(openxr_frame_t *f);
unsigned openxr_frame_refresh_rates(const openxr_frame_t *f,
      float *rates, unsigned cap);
void openxr_frame_request_rate(openxr_frame_t *f, float hz);
/* The drawing thread's quads, placed from an identity anchor: each
 * headset frame puts them where the screens are. */
void openxr_frame_publish(openxr_frame_t *f, const video_xr_quad_set_t *set);
/* The projection the XR thread submits from now on, behind the
 * screens; left out until its slot releases an image, and once the
 * track space it was drawn in is replaced. Published in the same
 * graphics-lock span as its slot's release, so a new image never goes
 * out with the previous poses. */
void openxr_frame_publish_projection(openxr_frame_t *f,
      const openxr_projection_t *p);
void openxr_frame_clear_projection(openxr_frame_t *f);
/* The drawing thread: a projection drawn in the track space numbered
 * from goes on, with the same poses, in the one numbered to. */
void openxr_frame_move_projection(openxr_frame_t *f, int from, int to);
/* A queued projection's space after carries; caller holds the graphics lock. */
int openxr_frame_carried_seq(openxr_frame_t *f, int seq);
void openxr_frame_get_anchor(openxr_frame_t *f, video_xr_pose_t *anchor);
void openxr_frame_get_quads(openxr_frame_t *f, video_xr_quad_set_t *out);
/* Any thread: where the screens go (enum video_openxr_placement). */
void openxr_frame_set_placement(openxr_frame_t *f, unsigned placement);
/* Any thread: the published quads where the last frame submitted
 * showed them, and the space they are in: what the laser points at. */
void openxr_frame_get_shown_quads(openxr_frame_t *f,
      video_xr_quad_set_t *out, XrSpace *space);
void openxr_frame_request_recenter(openxr_frame_t *f);
/* Any thread: how often the anchor has moved, and the display period
 * the frames publish, 0 until measured. */
int openxr_frame_recenters(openxr_frame_t *f);
XrDuration openxr_frame_period(openxr_frame_t *f);

/* Main thread, while no host's frame loop runs. NULL clears. */
void openxr_frame_set_input_hooks(const openxr_input_hooks_t *hooks);
/* Every host: after each new session, before its frame loop starts;
 * and before destroying a session, once the loop has stopped. */
void openxr_frame_input_attach(openxr_frame_t *f);
void openxr_frame_input_detach(openxr_frame_t *f);
XrSwapchain openxr_frame_cursor(const openxr_frame_t *f);

RETRO_END_DECLS

#endif
