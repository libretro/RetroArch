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

#include "drm_common.h"

uint32_t g_connector_id;
int g_drm_fd;
uint32_t g_crtc_id;

drmModeCrtc *g_orig_crtc;

drmModeRes *g_drm_resources;
drmModeConnector *g_drm_connector;
drmModeEncoder *g_drm_encoder;
drmModeModeInfo *g_drm_mode;

/* Restore the original CRTC. */
void drm_restore_crtc(void)
{
   if (!g_orig_crtc)
      return;

   drmModeSetCrtc(g_drm_fd, g_orig_crtc->crtc_id,
         g_orig_crtc->buffer_id,
         g_orig_crtc->x,
         g_orig_crtc->y,
         &g_connector_id, 1, &g_orig_crtc->mode);

   drmModeFreeCrtc(g_orig_crtc);
   g_orig_crtc = NULL;
}

void drm_free(void)
{
   if (g_drm_encoder)
      drmModeFreeEncoder(g_drm_encoder);
   if (g_drm_connector)
      drmModeFreeConnector(g_drm_connector);
   if (g_drm_resources)
      drmModeFreeResources(g_drm_resources);

}
