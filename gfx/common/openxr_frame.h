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
#include <retro_spsc.h>

#include <openxr/openxr.h>
#include <libretro.h>

#include "openxr_runtime.h"
#include "openxr_session.h"
#include "openxr_swapchain.h"
#include "../video_xr.h"
#include "../video_views.h"

RETRO_BEGIN_DECLS

/* Layers the input may add to the screens: the laser's dots, kept
 * where the screens must make room, then edit mode's outlines and the
 * menu's handle, then its hinges' lines, each hand's, then up to
 * OPENXR_REST_HINGES between screens at rest, then its tag, then the
 * beams, only in room they leave. */
#define OPENXR_KEPT_EXTRA_LAYERS 2
/* The most edge to edge pairs eight equal screens make in a plane. */
#define OPENXR_REST_HINGES       10
#define OPENXR_HINGE_LAYERS      (2 + OPENXR_REST_HINGES)
#define OPENXR_BEAM_LAYERS       2
#define OPENXR_TAG_LAYERS        1
#define OPENXR_MAX_EXTRA_LAYERS  \
   (OPENXR_KEPT_EXTRA_LAYERS + OPENXR_HINGE_LAYERS + OPENXR_BEAM_LAYERS \
    + OPENXR_TAG_LAYERS + VIDEO_XR_MAX_SLOTS + 1)

/* Edit mode's hands, published by the main thread for the frame loop. */
typedef struct openxr_grabs
{
   video_xr_grab_t hand[2];
   video_xr_pose_t start[2]; /* items as taken; snaps fade in */
   /* Each slot's last let-go, until quads planned at it show it. */
   video_xr_grab_t let_go[VIDEO_XR_MAX_SLOTS];
   unsigned chosen;    /* its slot plus one, 0 the laser's */
   unsigned stuck;     /* hands with the menu's handle stuck, bits */
   unsigned tag;       /* the hand that chose last (0, the left) */
   unsigned num_rests; /* screens no hand holds resting in a hinge */
   video_xr_rest_t rests[OPENXR_REST_HINGES];
   bool snap;          /* Snap to Screens */
   bool editing;       /* Edit Screens is on */
} openxr_grabs_t;

/* Edit Smoothing's aims, the frame loop's for the main thread: each
 * hand's, in the frame's space, as the frame last built from it took
 * it, and that frame's display time; bit h of valid while hand h's is
 * tracked. */
typedef struct openxr_aims
{
   video_xr_pose_t pose[2];
   XrTime time[2];
   unsigned valid;
} openxr_aims_t;

/* Where a hand's held item was last shown, for the hold (slot and
 * state) it was shown for, and the hinge the main thread marks, if
 * hinged, in the screens' frame. */
typedef struct openxr_held
{
   video_xr_pose_t pose;
   video_xr_hinge_t hinge;
   unsigned slot;
   unsigned state;
   bool hinged;
} openxr_held_t;

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
   /* Layers after the screens (the laser, its cursor), the first *under
    * of them under the screens instead, shown holding this frame's quads
    * where it shows them, in space; may be NULL. */
   unsigned (*extra_layers)(void *user, XrTime display_time,
         const video_xr_quad_set_t *shown, XrSpace space,
         const XrCompositionLayerBaseHeader **layers, unsigned cap,
         unsigned *under);
   /* The input's: a hand's aim pose at a frame's time in space, for what
    * it holds; may be NULL. */
   bool (*aim)(void *user, unsigned hand, XrSpace space, XrTime time,
         video_xr_pose_t *pose);
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
   /* A latched seqlock over two copies of its words (openxr_frame.c):
    * the writer's own copy, the stamp and the words. */
   openxr_tracked_t tracked;       /* the frame loop's thread */
   retro_atomic_int_t tracked_seq;
   retro_atomic_int_t tracked_words[2 * OPENXR_WORDS(openxr_tracked_t)];
   /* The screens as each frame submits them, the same way. */
   openxr_screens_t screens;       /* the frame loop's thread */
   retro_atomic_int_t screens_seq;
   retro_atomic_int_t screens_words[2 * OPENXR_WORDS(openxr_screens_t)];
   /* Edit mode's hands as the main thread publishes them, the same way,
    * and the frame loop's copy, read again only once the stamp it was
    * read at moves. The main thread is the one writer. */
   retro_atomic_int_t grabs_seq;
   retro_atomic_int_t grabs_words[2 * OPENXR_WORDS(openxr_grabs_t)];
   openxr_grabs_t grabs;           /* the frame loop's */
   int grabs_read;                 /* the frame loop's */
   openxr_held_t held[2];          /* the frame loop's */
   /* Each held hinge where this frame draws, bit h for hand h's. */
   video_xr_hinge_t hinges[2];     /* the frame loop's */
   unsigned hinged;                /* the frame loop's */
   /* Edit Smoothing's aims as the frame loop publishes them. */
   retro_atomic_int_t aims_seq;
   retro_atomic_int_t aims_words[2 * OPENXR_WORDS(openxr_aims_t)];
   /* This frame's quads where it shows them, for the input's layers,
    * and those it shows by their backs, a bit each. */
   video_xr_quad_set_t shown;      /* the frame loop's */
   uint32_t backs;                 /* the frame loop's */
   /* The menu's index among this frame's quad layers, or -1. */
   int menu_layer;                 /* the frame loop's */
   /* The drawing thread's quads for the XR thread and the pointer, the
    * same way. */
   retro_atomic_int_t quads_seq;
   retro_atomic_int_t quads_words[2 * OPENXR_WORDS(video_xr_quad_set_t)];
   /* The drawing thread's projection, the same way. */
   retro_atomic_int_t proj_seq;
   retro_atomic_int_t proj_words[2 * OPENXR_WORDS(openxr_projection_t)];
   /* The track space this frame uses (openxr_session_take_track). */
   const openxr_track_t *track;    /* the frame loop's thread */
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
   /* While the session is down the XR thread parks on idle and the
    * drawing thread fetches the runtime's events for it, into events
    * (openxr_frame_host_poll); poller says whose xrPollEvent it is. */
   retro_spsc_t events;            /* openxr_event_t */
   retro_eventcount_t idle;
   retro_atomic_int_t poller;      /* OPENXR_POLLER_* (openxr_frame.c) */
   bool idle_ready;
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
   /* The frame loop's thread, each headset frame: up to cap layers with
    * the screens, which shown holds where this frame shows them, in
    * space; the first OPENXR_KEPT_EXTRA_LAYERS for the dots, the rest
    * for what only fits in room the screens leave. Returns how many were
    * written: the first *under go under the screens, the rest over. */
   unsigned (*frame_layers)(void *user, XrTime display_time,
         const video_xr_quad_set_t *shown, XrSpace space,
         const XrCompositionLayerBaseHeader **layers, unsigned cap,
         unsigned *under);
   /* The two hands' poses and joints in base at time.
    * False when none could be located. */
   bool (*locate_hands)(void *user, XrSpace base, XrTime time,
         struct retro_vr_controller_poses *out);
   /* The frame loop's thread: a hand's aim pose at a frame's time in
    * space; false while it is not tracked. */
   bool (*aim)(void *user, unsigned hand, XrSpace space, XrTime time,
         video_xr_pose_t *pose);
   void *user;
} openxr_input_hooks_t;

bool openxr_frame_load(openxr_frame_t *f, const openxr_runtime_t *rt,
      openxr_session_t *session, openxr_swapchains_t *sc);
void openxr_frame_deinit(openxr_frame_t *f);
/* A new session: the anchor straight ahead, nothing held, the rates it
 * offers. */
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
/* The drawing thread, once a frame: while the XR thread is parked with
 * the session down, the runtime's events, handed to it to act on. */
void openxr_frame_host_poll(openxr_frame_t *f);
void openxr_frame_pace_skip(openxr_frame_t *f);
unsigned openxr_frame_refresh_rates(const openxr_frame_t *f,
      float *rates, unsigned cap);
void openxr_frame_request_rate(openxr_frame_t *f, float hz);
/* The drawing thread's quads, placed from an identity anchor: each
 * headset frame puts them where the screens are. */
void openxr_frame_publish(openxr_frame_t *f, const video_xr_quad_set_t *set);
/* Main thread: edit mode's hands, for each frame to place what they
 * hold at its own aim. */
void openxr_frame_publish_grabs(openxr_frame_t *f, const openxr_grabs_t *g);
/* The frame loop's thread: the hands' smoothed aims. */
void openxr_frame_publish_aims(openxr_frame_t *f, const openxr_aims_t *a);
/* The main thread: the aims last published. */
void openxr_frame_get_aims(openxr_frame_t *f, openxr_aims_t *out);
/* The projection the XR thread submits from now on, behind the
 * screens; left out until its slot releases an image, and once the
 * track space it was drawn in is replaced without a carry. Published
 * right after its slot's release, in the same graphics-lock span where
 * the host has a lock, so a new image never goes out with the previous
 * poses. */
void openxr_frame_publish_projection(openxr_frame_t *f,
      const openxr_projection_t *p);
void openxr_frame_clear_projection(openxr_frame_t *f);
void openxr_frame_get_anchor(openxr_frame_t *f, video_xr_pose_t *anchor);
void openxr_frame_get_quads(openxr_frame_t *f, video_xr_quad_set_t *out);
/* Any thread: where the screens go (enum video_openxr_placement). */
void openxr_frame_set_placement(openxr_frame_t *f, unsigned placement);
/* Any thread: the published quads where the last frame submitted
 * showed them, and the space they are in: what the laser points at. */
void openxr_frame_get_shown_quads(openxr_frame_t *f,
      video_xr_quad_set_t *out, XrSpace *space);
/* Any thread: where the last frame showed the screens (the anchor,
 * VIEW's identity, or Lazy Follow's), and their space. */
void openxr_frame_get_screens(openxr_frame_t *f, video_xr_pose_t *pose,
      XrSpace *space);
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
/* The width hosts make the laser's image: VIDEO_XR_CURSOR_W; where the
 * runtime's swapchains are narrower (4096 if it does not say),
 * VIDEO_XR_CURSOR_TAGGED_W, without the cut outlines, or
 * VIDEO_XR_CURSOR_UNTAGGED_W, without the tags too. */
unsigned openxr_frame_cursor_w(const openxr_frame_t *f);
/* What the image at that width has past the outlines, for the log. */
const char *openxr_frame_cursor_parts(const openxr_frame_t *f);
/* The laser's image (video_xr_cursor_pixels, video_xr_cut_pixels) w
 * wide, as openxr_frame_cursor_w says, in px's VIDEO_XR_CURSOR_W by
 * VIDEO_XR_CURSOR_H texels; from VIDEO_XR_CURSOR_TAGGED_W wide with
 * edit mode's tags written in the default font and the interface
 * language as it is now; bottom_up for an image whose rows run bottom
 * up, as GL's. Without a font, the tags stay clear. */
void openxr_frame_cursor_pixels(uint8_t *px, unsigned w, bool bottom_up);

RETRO_END_DECLS

#endif
