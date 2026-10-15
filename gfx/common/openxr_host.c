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

#include "openxr_host.h"
#include "../video_xr.h"

/* Sessions any host made, so a reinit's next session is another. */
static int openxr_host_sessions;
/* The last stopped host's, for the next to start after a reinit. */
static openxr_host_kept_t openxr_host_record;

void openxr_host_init(openxr_host_t *h, const openxr_runtime_t *rt,
      openxr_session_t *session, openxr_frame_t *frame)
{
   memset(h, 0, sizeof(*h));
   h->rt      = rt;
   h->session = session;
   h->frame   = frame;
}

void openxr_host_session_created(openxr_host_t *h)
{
   h->tracking.valid  = false;
   h->given_valid     = false;
   h->given_kept      = false;
   h->kept_pending    = false;
   h->stage_missing   = false;
   h->origin_changed  = h->track_made;
   h->session_serial  = ++openxr_host_sessions;
}

void openxr_host_stage_changed(openxr_host_t *h)
{
   retro_atomic_fetch_add_int(&h->stage_moves, 1);
}

/* A recenter's remake: what the core drew in the old seated space keeps
 * its poses in the new, so its world moves with the screens. Run in the
 * swap's locked span: no headset frame shows the new space without it. */
static void openxr_host_carry(void *user, int from, int to)
{
   openxr_host_t *h = (openxr_host_t*)user;
   unsigned i;
   if (h->given_seq == from)
      h->given_seq = to;
   for (i = 0; i < 4; i++)
      if (h->handed[i].seq == from)
         h->handed[i].seq = to;
   openxr_frame_move_projection(h->frame, from, to);
}

static bool openxr_host_make_space(openxr_host_t *h,
      enum retro_vr_reference_space *space, bool carry)
{
   XrPosef origin;
   video_xr_pose_t anchor;
   /* For a STAGE ask too: a LOCAL fallback is at the anchor from the
    * first frame shown in it. */
   openxr_frame_get_anchor(h->frame, &anchor);
   origin.orientation.x = anchor.orientation.x;
   origin.orientation.y = anchor.orientation.y;
   origin.orientation.z = anchor.orientation.z;
   origin.orientation.w = anchor.orientation.w;
   origin.position.x    = anchor.position.x;
   origin.position.y    = anchor.position.y;
   origin.position.z    = anchor.position.z;
   return openxr_session_swap_track_space(h->session, space, &origin,
         carry ? openxr_host_carry : NULL, h);
}

/* The track space: a seated world at the anchor, so a recenter moves it
 * and the screens' Fixed and Lazy anchors together; a standing one where
 * the room puts it. */
static bool openxr_host_track_space(openxr_host_t *h, bool carry)
{
   enum retro_vr_reference_space space = h->track_asked;
   int recenters;
   int stage_moves;
   h->track_done = false;
   if (h->stage_missing)
      space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   /* Counts from before the anchor is read: a later move is not lost. */
   recenters   = openxr_frame_recenters(h->frame);
   stage_moves = retro_atomic_load_acquire_int(&h->stage_moves);
   if (!openxr_host_make_space(h, &space, carry))
      return false;
   h->track_made = true;
   h->track_type = space;
   /* Seated for the session. */
   if (     space == RETRO_VR_REFERENCE_SPACE_LOCAL
         && h->track_asked == RETRO_VR_REFERENCE_SPACE_STAGE)
      h->stage_missing = true;
   h->track_recenters   = recenters;
   h->track_stage_moves = stage_moves;
   h->track_done        = true;
   return true;
}

bool openxr_host_sample(openxr_host_t *h, unsigned ahead,
      uint32_t *frame_flags)
{
   XrTime time;
   XrDuration period;
   int recenters;
   int stage;

   /* When the frame about to be built will be displayed. */
   time = openxr_frame_predicted_time(h->frame);
   if (time <= 0)
      return false;
   period = openxr_frame_period(h->frame);
   if (period > 0)
      time += period * (XrDuration)(1 + ahead);
   recenters = openxr_frame_recenters(h->frame);
   stage     = retro_atomic_load_acquire_int(&h->stage_moves);

   if (h->session->track_space)
   {
      bool stage_moved = h->track_type == RETRO_VR_REFERENCE_SPACE_STAGE
         && stage != h->track_stage_moves;
      if (recenters != h->track_recenters || stage_moved)
      {
         if (     recenters != h->track_recenters
               && h->track_type == RETRO_VR_REFERENCE_SPACE_LOCAL)
         {
            /* A failed remake is tried again at the next sample. */
            if (openxr_host_track_space(h, true))
               *frame_flags |= RETRO_VR_FRAME_RECENTERED;
         }
         else
         {
            *frame_flags        |= RETRO_VR_FRAME_RECENTERED;
            h->track_recenters   = recenters;
            h->track_stage_moves = stage;
         }
      }
   }
   if (!h->session->track_space && !openxr_host_track_space(h, false))
      return false;
   if (h->origin_changed)
   {
      *frame_flags     |= RETRO_VR_FRAME_RECENTERED;
      h->origin_changed = false;
   }
   h->tracking_seq = retro_atomic_load_acquire_int(&h->session->track_seq);
   h->tracking_time = time;
   return openxr_tracking_sample(&h->tracking, h->session,
         h->session->track_space, time);
}

/* The hands where the last sample put the head. */
bool openxr_host_locate_hands(openxr_host_t *h)
{
   if (!h->tracking.valid)
      return false;
   return openxr_frame_locate_hands(h->frame, h->session->track_space,
         h->tracking_time, &h->tracking.hands);
}

bool openxr_host_eye_state(openxr_host_t *h,
      struct retro_vr_eye_state out[2], unsigned *token)
{
   unsigned i;
   if (!h->tracking.valid)
      return false;
   openxr_tracking_eye(&h->tracking.views[0], &out[0]);
   openxr_tracking_eye(&h->tracking.views[1], &out[1]);
   i                      = h->handed_count & 3;
   h->handed[i].views[0]  = h->tracking.views[0];
   h->handed[i].views[1]  = h->tracking.views[1];
   h->handed[i].seq       = h->tracking_seq;
   *token                 = h->handed_count++;
   return true;
}

void openxr_host_give(openxr_host_t *h, unsigned token)
{
   unsigned i = token & 3;
   /* Overwritten since: four eyes handed out after it and none drawn */
   if (h->handed_count - token > 4)
      return;
   h->given[0]     = h->handed[i].views[0];
   h->given[1]     = h->handed[i].views[1];
   h->given_seq    = h->handed[i].seq;
   h->given_valid  = true;
   h->given_kept   = false;
   h->kept_pending = false;
}

bool openxr_host_head_pose(const openxr_host_t *h,
      struct retro_vr_head_pose *out)
{
   if (!h->tracking.valid)
      return false;
   *out = h->tracking.head;
   return true;
}

bool openxr_host_controller_poses(const openxr_host_t *h,
      struct retro_vr_controller_poses *out)
{
   if (!h->tracking.valid)
      return false;
   *out = h->tracking.hands;
   return true;
}

bool openxr_host_eye_size(const openxr_host_t *h, unsigned *width,
      unsigned *height)
{
   *width  = h->rt->rec_width;
   *height = h->rt->rec_height;
   return *width && *height;
}

bool openxr_host_set_space(openxr_host_t *h,
      enum retro_vr_reference_space *space, unsigned *width,
      unsigned *height)
{
   bool same = h->session->track_space && h->track_done
      && *space == h->track_asked
      && h->track_type == (h->stage_missing
         ? RETRO_VR_REFERENCE_SPACE_LOCAL : h->track_asked);
   h->track_asked  = *space;
   h->kept_pending = false;
   /* A remade space orphans the views given in it. */
   if (!same && !openxr_host_track_space(h, false))
      return false;
   *space = h->track_type;
   return openxr_host_eye_size(h, width, height);
}

void openxr_host_projection(const openxr_host_t *h, unsigned width,
      unsigned height, openxr_projection_t *out)
{
   memset(out, 0, sizeof(*out));
   if (!h->given_valid)
      return;
   out->pose[0]   = h->given[0].pose;
   out->pose[1]   = h->given[1].pose;
   out->fov[0]    = h->given[0].fov;
   out->fov[1]    = h->given[1].fov;
   out->width     = width;
   out->height    = height;
   out->track_seq = h->given_seq;
   out->active    = true;
}

bool openxr_host_has_given(const openxr_host_t *h)
{
   return h->given_valid && h->session->track_space
      && h->given_seq == retro_atomic_load_acquire_int(
            &h->session->track_seq);
}

static XrQuaternionf openxr_host_quat_mul(const XrQuaternionf *a,
      const XrQuaternionf *b)
{
   XrQuaternionf q;
   q.x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
   q.y = a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x;
   q.z = a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w;
   q.w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
   return q;
}

/* out: pose, given in a space that is at origin, in origin's parent;
 * with invert, the other way. */
static void openxr_host_pose(const XrPosef *origin, const XrPosef *pose,
      bool invert, XrPosef *out)
{
   video_xr_quat_t r;
   video_xr_vec3_t v;
   XrQuaternionf q = origin->orientation;
   if (invert)
   {
      q.x = -q.x;
      q.y = -q.y;
      q.z = -q.z;
      v.x = pose->position.x - origin->position.x;
      v.y = pose->position.y - origin->position.y;
      v.z = pose->position.z - origin->position.z;
   }
   else
   {
      v.x = pose->position.x;
      v.y = pose->position.y;
      v.z = pose->position.z;
   }
   r.x = q.x;
   r.y = q.y;
   r.z = q.z;
   r.w = q.w;
   video_xr_rotate(&r, &v, &v);
   out->position.x = v.x;
   out->position.y = v.y;
   out->position.z = v.z;
   if (!invert)
   {
      out->position.x += origin->position.x;
      out->position.y += origin->position.y;
      out->position.z += origin->position.z;
   }
   out->orientation = openxr_host_quat_mul(&q, &pose->orientation);
}

/* Where the track space is in STAGE at time: STAGE content's is STAGE
 * itself; a seated one's is located. 1 when it is, 0 not yet (no time
 * or space, or a location the runtime has not got), -1 without a
 * STAGE. */
static int openxr_host_track_in_stage(const openxr_host_t *h,
      XrTime time, XrPosef *out)
{
   bool ok;
   XrSpaceLocation loc;
   XrReferenceSpaceCreateInfo rci;
   XrSpace stage              = XR_NULL_HANDLE;
   XrSpaceLocationFlags valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;
   if (h->track_type == RETRO_VR_REFERENCE_SPACE_STAGE)
   {
      memset(out, 0, sizeof(*out));
      out->orientation.w = 1.0f;
      return 1;
   }
   if (h->stage_missing)
      return -1;
   if (time <= 0 || !h->session->track_space)
      return 0;
   rci.type                               = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
   rci.next                               = NULL;
   rci.poseInReferenceSpace.orientation.x = 0.0f;
   rci.poseInReferenceSpace.orientation.y = 0.0f;
   rci.poseInReferenceSpace.orientation.z = 0.0f;
   rci.poseInReferenceSpace.position.x    = 0.0f;
   rci.poseInReferenceSpace.position.y    = 0.0f;
   rci.poseInReferenceSpace.position.z    = 0.0f;
   rci.referenceSpaceType                 = XR_REFERENCE_SPACE_TYPE_STAGE;
   rci.poseInReferenceSpace.orientation.w = 1.0f;
   if (XR_FAILED(h->session->CreateReferenceSpace(h->session->session,
               &rci, &stage)))
      return -1;
   loc.type = XR_TYPE_SPACE_LOCATION;
   loc.next = NULL;
   ok       = XR_SUCCEEDED(h->session->LocateSpace(h->session->track_space,
            stage, time, &loc))
      && (loc.locationFlags & valid) == valid;
   h->session->DestroySpace(stage);
   if (!ok)
      return 0;
   *out = loc.pose;
   return 1;
}

void openxr_host_keep(const openxr_host_t *h)
{
   unsigned e;
   XrPosef origin;
   openxr_host_kept_t *out = &openxr_host_record;
   memset(out, 0, sizeof(*out));
   if (!h->session->track_space)
      return;
   out->session    = h->session_serial;
   out->track_type = h->track_type;
   /* Views given while the session showed no tracked content are not
    * the ones the kept frame was drawn from. */
   if (     !openxr_session_alive(h->session)
         || !openxr_session_running(h->session)
         || !openxr_host_has_given(h)
         || openxr_host_track_in_stage(h,
               openxr_frame_predicted_time(h->frame), &origin) <= 0)
      return;
   for (e = 0; e < 2; e++)
   {
      out->views[e] = h->given[e];
      openxr_host_pose(&origin, &h->given[e].pose, false,
            &out->views[e].pose);
   }
   out->valid = true;
}

void openxr_host_forget(void)
{
   memset(&openxr_host_record, 0, sizeof(openxr_host_record));
}

void openxr_host_resume(openxr_host_t *h)
{
   const openxr_host_kept_t kept = openxr_host_record;
   memset(&openxr_host_record, 0, sizeof(openxr_host_record));
   if (!kept.session)
      return;
   /* Another session's origin is not this one's, unless both are the
    * room. */
   if (     kept.session != h->session_serial
         && (   kept.track_type != RETRO_VR_REFERENCE_SPACE_STAGE
             || h->track_type   != RETRO_VR_REFERENCE_SPACE_STAGE))
      h->origin_changed = true;
   if (!kept.valid)
      return;
   h->kept[0]      = kept.views[0];
   h->kept[1]      = kept.views[1];
   h->kept_pending = true;
   openxr_host_place_kept(h);
}

void openxr_host_place_kept(openxr_host_t *h)
{
   unsigned e;
   int located;
   XrPosef origin;
   /* A paused core takes no sample, so its seated world follows a
    * recenter here, and the core hears of it at its next sample. */
   if (     h->session->track_space
         && h->track_type == RETRO_VR_REFERENCE_SPACE_LOCAL
         && openxr_frame_recenters(h->frame) != h->track_recenters
         && openxr_host_track_space(h, true))
      h->origin_changed = true;
   if (!h->kept_pending)
      return;
   /* A new session's first frames may have no time or location yet:
    * tried again at the next repeat, until a new frame drops them. */
   located = openxr_host_track_in_stage(h,
         openxr_frame_predicted_time(h->frame), &origin);
   if (!located)
      return;
   h->kept_pending = false;
   if (located < 0)
      return;
   for (e = 0; e < 2; e++)
   {
      h->given[e] = h->kept[e];
      openxr_host_pose(&origin, &h->kept[e].pose, true,
            &h->given[e].pose);
   }
   h->given_seq   = retro_atomic_load_acquire_int(&h->session->track_seq);
   h->given_valid = true;
   h->given_kept  = true;
}

void openxr_host_new_frame(openxr_host_t *h)
{
   memset(&openxr_host_record, 0, sizeof(openxr_host_record));
   h->kept_pending = false;
   if (h->given_kept)
      h->given_valid = false;
   h->given_kept   = false;
}
