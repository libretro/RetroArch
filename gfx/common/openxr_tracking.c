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

#include <math.h>
#include <string.h>

#include "openxr_tracking.h"

void openxr_tracking_eye(const XrView *v, struct retro_vr_eye_state *out)
{
   out->position[0]    = v->pose.position.x;
   out->position[1]    = v->pose.position.y;
   out->position[2]    = v->pose.position.z;
   out->orientation[0] = v->pose.orientation.x;
   out->orientation[1] = v->pose.orientation.y;
   out->orientation[2] = v->pose.orientation.z;
   out->orientation[3] = v->pose.orientation.w;
   /* Magnitudes, as libretro.h has them: OpenXR's left and down
    * angles are negative. */
   out->fov_tan[0]     = (float)fabs(tan(v->fov.angleLeft));
   out->fov_tan[1]     = (float)fabs(tan(v->fov.angleRight));
   out->fov_tan[2]     = (float)fabs(tan(v->fov.angleUp));
   out->fov_tan[3]     = (float)fabs(tan(v->fov.angleDown));
}

bool openxr_tracking_sample(openxr_tracking_t *t, const openxr_session_t *s,
      XrSpace base, XrTime time)
{
   uint32_t n = 0;
   XrViewLocateInfo li;
   XrViewState vs;
   XrSpaceVelocity vel;
   XrSpaceLocation loc;

   t->valid = false;
   if (!s->session || !s->view_space || !base || time <= 0)
      return false;

   li.type                  = XR_TYPE_VIEW_LOCATE_INFO;
   li.next                  = NULL;
   li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
   li.displayTime           = time;
   li.space                 = base;
   vs.type = XR_TYPE_VIEW_STATE;
   vs.next = NULL;
   t->views[0].type         = XR_TYPE_VIEW;
   t->views[0].next         = NULL;
   t->views[1].type         = XR_TYPE_VIEW;
   t->views[1].next         = NULL;
   if (     s->LocateViews(s->session, &li, &vs, 2, &n, t->views) != XR_SUCCESS
         || n != 2
         || !(vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT))
      return false;

   vel.type = XR_TYPE_SPACE_VELOCITY;
   vel.next = NULL;
   loc.type = XR_TYPE_SPACE_LOCATION;
   loc.next = &vel;
   if (     s->LocateSpace(s->view_space, base, time, &loc) != XR_SUCCESS
         || !(loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
      return false;

   memset(&t->head, 0, sizeof(t->head));
   t->head.orientation[0] = loc.pose.orientation.x;
   t->head.orientation[1] = loc.pose.orientation.y;
   t->head.orientation[2] = loc.pose.orientation.z;
   t->head.orientation[3] = loc.pose.orientation.w;
   t->head.flags         |= RETRO_VR_HEAD_POSE_ORIENTATION_VALID;
   if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)
   {
      t->head.position[0] = loc.pose.position.x;
      t->head.position[1] = loc.pose.position.y;
      t->head.position[2] = loc.pose.position.z;
      t->head.flags      |= RETRO_VR_HEAD_POSE_POSITION_VALID;
   }
   if (vel.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT)
   {
      t->head.linear_velocity[0] = vel.linearVelocity.x;
      t->head.linear_velocity[1] = vel.linearVelocity.y;
      t->head.linear_velocity[2] = vel.linearVelocity.z;
   }
   if (vel.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT)
   {
      t->head.angular_velocity[0] = vel.angularVelocity.x;
      t->head.angular_velocity[1] = vel.angularVelocity.y;
      t->head.angular_velocity[2] = vel.angularVelocity.z;
   }
   if (vel.velocityFlags & (XR_SPACE_VELOCITY_LINEAR_VALID_BIT
            | XR_SPACE_VELOCITY_ANGULAR_VALID_BIT))
      t->head.flags |= RETRO_VR_HEAD_POSE_VELOCITY_VALID;

   t->valid = true;
   return true;
}
