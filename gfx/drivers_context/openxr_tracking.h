#ifndef OPENXR_TRACKING_H
#define OPENXR_TRACKING_H

#include <math.h>
#include <string.h>
#include <retro_inline.h>
#include <boolean.h>
#include <libretro.h>
#include <openxr/openxr.h>

typedef struct openxr_tracking
{
   XrView views[2];
   struct retro_vr_head_pose head;
   bool valid;
} openxr_tracking_t;

static INLINE void openxr_view_to_eye(const XrView *v,
      struct retro_vr_eye_state *out)
{
   out->position[0]    = v->pose.position.x;
   out->position[1]    = v->pose.position.y;
   out->position[2]    = v->pose.position.z;
   out->orientation[0] = v->pose.orientation.x;
   out->orientation[1] = v->pose.orientation.y;
   out->orientation[2] = v->pose.orientation.z;
   out->orientation[3] = v->pose.orientation.w;
   out->fov_tan[0]     = tanf(v->fov.angleLeft);
   out->fov_tan[1]     = tanf(v->fov.angleRight);
   out->fov_tan[2]     = tanf(v->fov.angleUp);
   out->fov_tan[3]     = tanf(v->fov.angleDown);
}

/* Locates both eyes and the head (view space) in 'base' at 'time'. */
static INLINE bool openxr_tracking_sample(openxr_tracking_t *t,
      XrSession session, XrSpace base, XrSpace head_space, XrTime time)
{
   XrViewLocateInfo li  = { XR_TYPE_VIEW_LOCATE_INFO };
   XrViewState vs       = { XR_TYPE_VIEW_STATE };
   XrSpaceVelocity vel  = { XR_TYPE_SPACE_VELOCITY };
   XrSpaceLocation loc  = { XR_TYPE_SPACE_LOCATION };
   uint32_t n           = 0;

   t->valid = false;
   if (session == XR_NULL_HANDLE || head_space == XR_NULL_HANDLE
         || base == XR_NULL_HANDLE || time <= 0)
      return false;

   li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
   li.displayTime           = time;
   li.space                 = base;
   t->views[0].type         = XR_TYPE_VIEW;
   t->views[1].type         = XR_TYPE_VIEW;

   if (xrLocateViews(session, &li, &vs, 2, &n, t->views) != XR_SUCCESS
         || n != 2
         || !(vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT))
      return false;

   loc.next = &vel;
   if (xrLocateSpace(head_space, base, time, &loc) != XR_SUCCESS
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

/* Recreates 'space' as the requested reference space, downgrading
 * STAGE -> LOCAL if needed; writes back what was actually granted. */
static INLINE bool openxr_reference_space_set(XrSession session,
      XrSpace *space, enum retro_vr_reference_space *req)
{
   XrReferenceSpaceCreateInfo si = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
   XrSpace fresh                 = XR_NULL_HANDLE;
   enum retro_vr_reference_space got = RETRO_VR_REFERENCE_SPACE_LOCAL;

   if (session == XR_NULL_HANDLE)
      return false;
   si.poseInReferenceSpace.orientation.w = 1.0f;

   if (*req == RETRO_VR_REFERENCE_SPACE_STAGE)
   {
      si.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
      if (xrCreateReferenceSpace(session, &si, &fresh) == XR_SUCCESS)
         got = RETRO_VR_REFERENCE_SPACE_STAGE;
   }
   if (fresh == XR_NULL_HANDLE)
   {
      si.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
      if (xrCreateReferenceSpace(session, &si, &fresh) != XR_SUCCESS)
         return false;
   }
   if (*space != XR_NULL_HANDLE)
      xrDestroySpace(*space);
   *space = fresh;
   *req   = got;
   return true;
}

#endif
