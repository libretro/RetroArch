#ifndef OPENXR_TRACKING_H
#define OPENXR_TRACKING_H

#include <retro_inline.h>
#include <boolean.h>
#include <libretro.h>
#include <openxr/openxr.h>

#include "../common/openxr_tracking.h"

/* The Quest contexts' own reference space, until they run on
 * openxr_session. Recreates 'space' as the requested reference space,
 * downgrading STAGE -> LOCAL if needed; writes back what was granted. */
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
