/*  RetroArch - A frontend for libretro.
 *  Copyright (c) 2011-2017 - Daniel De Matteis
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

#ifndef __DRM_COMMON_H
#define __DRM_COMMON_H

#include <stdint.h>
#include <stddef.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <poll.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>

#include "../../retroarch.h"

RETRO_BEGIN_DECLS

extern uint32_t g_connector_id;
extern int g_drm_fd;
extern uint32_t g_crtc_id;

extern struct pollfd g_drm_fds;

extern drmModeConnector *g_drm_connector;
extern drmModeModeInfo *g_drm_mode;
extern drmModeCrtc *g_orig_crtc;

extern drmEventContext g_drm_evctx;

bool drm_get_encoder(int fd);

/* Restore the original CRTC. */
void drm_restore_crtc(void);

bool drm_get_resources(int fd);

void drm_setup(int fd);

void drm_free(void);

bool drm_get_connector(int fd, video_frame_info_t *video_info);

float drm_get_refresh_rate(void *data);

static INLINE bool drm_wait_flip(int timeout)
{
   g_drm_fds.revents = 0;

   if (poll(&g_drm_fds, 1, timeout) < 0)
      return false;

   if (g_drm_fds.revents & (POLLHUP | POLLERR))
      return false;

   if (g_drm_fds.revents & POLLIN)
   {
      drmHandleEvent(g_drm_fd, &g_drm_evctx);
      return true;
   }

   return false;
}

RETRO_END_DECLS

#endif
