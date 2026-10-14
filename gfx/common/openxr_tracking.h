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

#ifndef __OPENXR_TRACKING_H
#define __OPENXR_TRACKING_H

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>

#include <openxr/openxr.h>

#include "openxr_session.h"

RETRO_BEGIN_DECLS

/* Both eyes and the head for one display time. */
typedef struct openxr_tracking
{
   XrView views[2];
   struct retro_vr_head_pose head;
   /* The hands at the same time, in the same space; cleared when the
    * input located none. */
   struct retro_vr_controller_poses hands;
   bool valid;
} openxr_tracking_t;

/* Both eyes and the head (the session's VIEW space) in base at time;
 * false, and nothing valid, unless the eyes' and the head's
 * orientations are known. */
bool openxr_tracking_sample(openxr_tracking_t *t, const openxr_session_t *s,
      XrSpace base, XrTime time);

/* An eye as a core gets it: its pose, and its field of view as
 * tangents. */
void openxr_tracking_eye(const XrView *v, struct retro_vr_eye_state *out);

RETRO_END_DECLS

#endif
