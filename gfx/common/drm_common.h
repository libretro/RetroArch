/*  RetroArch - A frontend for libretro.
 *  copyright (c) 2011-2015 - Daniel De Matteis
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

#include <xf86drmMode.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t g_connector_id;
extern int g_drm_fd;
extern uint32_t g_crtc_id;

extern drmModeCrtc *g_orig_crtc;
extern drmModeRes *g_drm_resources;
extern drmModeConnector *g_drm_connector;
extern drmModeEncoder *g_drm_encoder;
extern drmModeModeInfo *g_drm_mode;

/* Restore the original CRTC. */
void drm_restore_crtc(void);

void drm_free(void);

#ifdef __cplusplus
}
#endif

#endif
