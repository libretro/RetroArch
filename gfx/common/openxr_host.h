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

#ifndef __OPENXR_HOST_H
#define __OPENXR_HOST_H

#include <stdint.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <libretro.h>

#include <openxr/openxr.h>

#include "openxr_runtime.h"
#include "openxr_session.h"
#include "openxr_frame.h"
#include "openxr_tracking.h"

RETRO_BEGIN_DECLS

/* A host's answers for tracked content, whatever its graphics API and
 * frame loop: the eyes and the head sampled in a track space for the
 * frame's display time, and the views the core drew its frame from. */
typedef struct openxr_host
{
   const openxr_runtime_t *rt;
   openxr_session_t *session;
   openxr_frame_t *frame;
   /* The drawing thread's: the last sample and the track space's count
    * when it was taken; the views the core was given from it. */
   openxr_tracking_t tracking;
   XrTime tracking_time;
   int tracking_seq;
   XrView given[2];
   int given_seq;
   bool given_valid;
   /* The eyes handed out lately, by token, for give() to record the
    * ones a frame was drawn from when it is drawn */
   struct
   {
      XrView views[2];
      int    seq;
   } handed[4];
   unsigned handed_count;
   /* Views kept across a reinit, their poses in STAGE, until a frame
    * time places them in the track space. */
   XrView kept[2];
   bool kept_pending;
   /* given are kept views, not ones the core asked for. */
   bool given_kept;
   /* The space the core asked for and the one it got; the anchor's and
    * STAGE's move counts the core was last told of. */
   enum retro_vr_reference_space track_asked;
   enum retro_vr_reference_space track_type;
   int track_recenters;
   int track_stage_moves;
   /* STAGE was asked and refused: seated for the session. */
   bool stage_missing;
   /* The last make of the space finished: a half-made one is made again. */
   bool track_done;
   /* A space was made in some session; a new one starts at a new origin
    * the core has yet to be told of. */
   bool track_made;
   bool origin_changed;
   /* This session's number, unique in the process. */
   int session_serial;
   /* The polling thread's: the runtime moved STAGE. */
   retro_atomic_int_t stage_moves;
} openxr_host_t;

/* Once, with the parts it reads; they outlive it. */
void openxr_host_init(openxr_host_t *h, const openxr_runtime_t *rt,
      openxr_session_t *session, openxr_frame_t *frame);
/* After each new session: nothing sampled or given in it yet. */
void openxr_host_session_created(openxr_host_t *h);
/* The session's stage_changed hook. */
void openxr_host_stage_changed(openxr_host_t *h);

/* video_xr_host_t's pose calls, on its threads. The sample is for one
 * period past the latest predicted display time, and @ahead periods
 * more for frames already handed over: right when the frame loop waits
 * for the frame the core is drawing after the core runs (the desktop's
 * XR thread); a host that waits before the core runs would sample one
 * period too far ahead. A sample that fails still writes the flags it
 * took. */
bool openxr_host_sample(openxr_host_t *h, unsigned ahead,
      uint32_t *frame_flags);
bool openxr_host_eye_state(openxr_host_t *h,
      struct retro_vr_eye_state out[2], unsigned *token);
/* The drawing thread, before the frame drawn from the eyes @token
 * names: the projection shows them. */
void openxr_host_give(openxr_host_t *h, unsigned token);
bool openxr_host_head_pose(const openxr_host_t *h,
      struct retro_vr_head_pose *out);
bool openxr_host_locate_hands(openxr_host_t *h);
bool openxr_host_controller_poses(const openxr_host_t *h,
      struct retro_vr_controller_poses *out);
bool openxr_host_eye_size(const openxr_host_t *h, unsigned *width,
      unsigned *height);
bool openxr_host_set_space(openxr_host_t *h,
      enum retro_vr_reference_space *space, unsigned *width,
      unsigned *height);

/* The drawing thread: the projection of a frame drawn from the views
 * the core was last given, each eye width x height from its image's top
 * left; inactive before the core was given any. */
void openxr_host_projection(const openxr_host_t *h, unsigned width,
      unsigned height, openxr_projection_t *out);

/* The drawing thread: the core was given views in the current track
 * space, so a projection of its frame would be shown. */
bool openxr_host_has_given(const openxr_host_t *h);

/* What a host carries to the next across a video driver reinit: the
 * session and space the core had, and, while the session showed
 * tracked content, the views it was last given with their poses in
 * STAGE. A new instance has the room where the last one had it, while
 * LOCAL and the anchor start again wherever the head is. */
typedef struct openxr_host_kept
{
   XrView views[2];
   enum retro_vr_reference_space track_type;
   int session;            /* 0: nothing kept */
   bool valid;             /* the views */
} openxr_host_kept_t;

/* Before the host stops showing the session. */
void openxr_host_keep(const openxr_host_t *h);
/* A start that gave up: no later start resumes a record kept before it. */
void openxr_host_forget(void);
/* After the same content's space was set again: a core whose origin
 * moved hears RECENTERED, and the views it was last given are placed in
 * the track space through STAGE. The record serves this start alone. */
void openxr_host_resume(openxr_host_t *h);
/* The drawing thread, on a frame the core did not draw: a seated space
 * follows a recenter; kept views wait for a frame time and a location
 * to be placed by, and are dropped where there is no STAGE. */
void openxr_host_place_kept(openxr_host_t *h);
/* The drawing thread, a new core frame: kept views drew none of it, and
 * the record serves the driver start after its stop, not a later one. */
void openxr_host_new_frame(openxr_host_t *h);

RETRO_END_DECLS

#endif
