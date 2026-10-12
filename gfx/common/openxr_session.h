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

#ifndef __OPENXR_SESSION_H
#define __OPENXR_SESSION_H

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <libretro.h>

#include <openxr/openxr.h>

#include "openxr_runtime.h"

RETRO_BEGIN_DECLS

/* What the host does when the session changes; any may be NULL. The
 * first five are called on the thread that polls; ended also on the one
 * whose frame finds the session lost. */
typedef struct openxr_session_hooks
{
   /* After the state is stored and logged, before it is acted on. */
   void (*state_changed)(void *user, XrSessionState state);
   /* The first time the session ends, for whatever reason. */
   void (*ended)(void *user);
   /* Every XR_SESSION_STATE_EXITING. */
   void (*exiting)(void *user);
   /* The runtime recentered this session's LOCAL space. */
   void (*local_changed)(void *user);
   /* The runtime moved this session's STAGE. */
   void (*stage_changed)(void *user);
   /* The graphics queue's lock, around every runtime call that may use
    * the queue (xrBeginSession and xrEndSession, a slot's create and
    * destroy, xrBeginFrame to xrEndFrame) and replacing the track space,
    * on whichever thread makes it. */
   void (*lock)(void *user);
   void (*unlock)(void *user);
   void *user;
} openxr_session_hooks_t;

typedef struct openxr_session
{
   openxr_session_hooks_t hooks;
   const openxr_runtime_t *rt;
   XrSession session;
   XrSpace local_space;
   XrSpace view_space;
   XrSpace track_space;           /* tracked content's, once it asks */
   retro_atomic_int_t track_seq;  /* bumped each time it is replaced */
   /* A LOCAL track_space's pose in LOCAL: a seated world's origin. */
   XrPosef track_origin;
   bool track_local;
   retro_atomic_int_t state;   /* XrSessionState */
   retro_atomic_int_t alive;   /* the host's frames reach the headset */
   retro_atomic_int_t running; /* between xrBeginSession and xrEndSession */
   retro_atomic_int_t ended;   /* set by the polling thread, read by any */
   bool lost;                  /* the instance was lost too */
   PFN_xrPollEvent PollEvent;
   PFN_xrCreateSession CreateSession;
   PFN_xrDestroySession DestroySession;
   PFN_xrBeginSession BeginSession;
   PFN_xrEndSession EndSession;
   PFN_xrCreateReferenceSpace CreateReferenceSpace;
   PFN_xrDestroySpace DestroySpace;
   PFN_xrLocateViews LocateViews;
   PFN_xrLocateSpace LocateSpace;
} openxr_session_t;

/* The session functions; false when the runtime lacks one. */
bool openxr_session_load(openxr_session_t *s, const openxr_runtime_t *rt);
/* The session on graphics_binding (chained into XrSessionCreateInfo),
 * with LOCAL and VIEW spaces. */
bool openxr_session_create(openxr_session_t *s, const void *graphics_binding);
/* After the frame loop has stopped. */
void openxr_session_destroy(openxr_session_t *s);

/* Tracked content's space: STAGE at the room's origin when asked for
 * and the runtime has one, else LOCAL with origin as its pose there;
 * what was granted is written back. It replaces the last one under the
 * lock, and swapped, when given, runs in that locked span with the old
 * and new track_seq: a frame sees both or neither. */
bool openxr_session_swap_track_space(openxr_session_t *s,
      enum retro_vr_reference_space *space, const XrPosef *origin,
      void (*swapped)(void *user, int from, int to), void *user);
void openxr_session_lock(openxr_session_t *s);
void openxr_session_unlock(openxr_session_t *s);
/* After a session the runtime ended, and after the frame loop has stopped:
 * forget it, ready for a new one. */
void openxr_session_reset(openxr_session_t *s);
void openxr_session_poll(openxr_session_t *s);
void openxr_session_end(openxr_session_t *s, bool instance_lost);
void openxr_session_set_alive(openxr_session_t *s, bool alive);
bool openxr_session_alive(openxr_session_t *s);
/* Exiting, loss pending or the instance lost, until a reset. */
bool openxr_session_ended(openxr_session_t *s);
bool openxr_session_running(openxr_session_t *s);
bool openxr_session_focused(openxr_session_t *s);
/* Alive and VISIBLE or FOCUSED: the headset shows the session. */
bool openxr_session_visible(openxr_session_t *s);
const char *openxr_session_state_name(XrSessionState state);

RETRO_END_DECLS

#endif
