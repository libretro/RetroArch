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

#include <math.h>

#include <compat/strl.h>

#include <retro_miscellaneous.h>

#include "../../verbosity.h"

#include "drm_common.h"

/* TODO/FIXME - globals */
drmEventContext g_drm_evctx;
struct pollfd g_drm_fds;
uint32_t g_connector_id               = 0;
int g_drm_fd                          = 0;
uint32_t g_crtc_id                    = 0;
drmModeCrtc *g_orig_crtc              = NULL;
drmModeConnector *g_drm_connector     = NULL;
drmModeModeInfo *g_drm_mode           = NULL;

/* TODO/FIXME - static globals */
static drmModeRes *g_drm_resources    = NULL;
static drmModeEncoder *g_drm_encoder  = NULL;
static uint32_t g_drm_encoder_crtc    = 0;

/* Restore the original CRTC. */
void drm_restore_crtc(void)
{
   if (!g_orig_crtc)
      return;

   if (g_orig_crtc->mode_valid)
      drmModeSetCrtc(g_drm_fd, g_orig_crtc->crtc_id,
            g_orig_crtc->buffer_id,
            g_orig_crtc->x,
            g_orig_crtc->y,
            &g_connector_id, 1, &g_orig_crtc->mode);
   else
      drmModeSetCrtc(g_drm_fd, g_orig_crtc->crtc_id,
            0, 0, 0, NULL, 0, NULL);

   drmModeFreeCrtc(g_orig_crtc);
   g_orig_crtc = NULL;
}

bool drm_get_resources(int fd)
{
   g_drm_resources = drmModeGetResources(fd);
   if (!g_drm_resources)
   {
      RARCH_WARN("[DRM] Couldn't get device resources.\n");
      return false;
   }

   return true;
}

bool drm_get_connector(int fd, unsigned monitor_index)
{
   unsigned i;
   unsigned monitor_index_count = 0;
   unsigned monitor       = MAX(monitor_index, 1);

   /* Enumerate all connectors. */

   RARCH_LOG("[DRM] Found %d connectors.\n", g_drm_resources->count_connectors);

   for (i = 0; (int)i < g_drm_resources->count_connectors; i++)
   {
      drmModeConnectorPtr conn = drmModeGetConnector(
            fd, g_drm_resources->connectors[i]);

      if (conn)
      {
         bool connected = conn->connection == DRM_MODE_CONNECTED;
         RARCH_LOG("[DRM] Connector %d connected: %s\n", i, connected ? "yes" : "no");
         RARCH_LOG("[DRM] Connector %d has %d modes.\n", i, conn->count_modes);
         if (DRM_CONNECTOR_USABLE(conn))
         {
            monitor_index_count++;
            RARCH_LOG("[DRM] Connector %d assigned to monitor index: #%u.\n", i, monitor_index_count);
         }
         drmModeFreeConnector(conn);
      }
   }

   monitor_index_count = 0;

   for (i = 0; (int)i < g_drm_resources->count_connectors; i++)
   {
      g_drm_connector = drmModeGetConnector(fd,
            g_drm_resources->connectors[i]);

      if (!g_drm_connector)
         continue;
      if (DRM_CONNECTOR_USABLE(g_drm_connector))
      {
         monitor_index_count++;
         if (monitor_index_count == monitor)
            break;
      }

      drmModeFreeConnector(g_drm_connector);
      g_drm_connector = NULL;
   }

   if (!g_drm_connector)
   {
      RARCH_WARN("[DRM] Couldn't get device connector.\n");
      return false;
   }
   return true;
}

#ifdef HAVE_UDEV
bool drm_hotplug_needs_restart(int fd, unsigned monitor_index, bool *lost)
{
   int i;
   unsigned count     = 0;
   unsigned monitor   = MAX(monitor_index, 1);
   uint32_t candidate = 0;
   bool usable        = false;
   drmModeRes *res    = drmModeGetResources(fd);

   if (!res)
      return false;

   /* Cached state only: the kernel probed before it sent the event */
   for (i = 0; i < res->count_connectors; i++)
   {
      drmModeConnector *conn = drmModeGetConnectorCurrent(fd,
            res->connectors[i]);

      if (!conn)
         continue;
      if (DRM_CONNECTOR_USABLE(conn))
      {
         if (conn->connector_id == g_connector_id)
            usable = true;
         if (++count == monitor)
            candidate = conn->connector_id;
      }
      drmModeFreeConnector(conn);
   }
   drmModeFreeResources(res);

   if (usable)
   {
      /* Back after going: maybe another display, which wants its own mode */
      if (*lost)
      {
         *lost = false;
         return true;
      }
      return false;
   }

   *lost = true;
   if (candidate && candidate != g_connector_id)
   {
      *lost = false;
      return true;
   }
   return false;
}
#endif

float drm_calc_refresh_rate(drmModeModeInfo *mode)
{
   unsigned int num, den;

   num = mode->clock;
   den = mode->htotal * mode->vtotal;

   if (mode->flags & DRM_MODE_FLAG_INTERLACE)
      num *= 2;
   if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
      den *= 2;
   if (mode->vscan > 1)
      den *= mode->vscan;

   return num * 1000.0f / den;
}

/* A CRTC the encoder can drive, preferring one that is off */
static uint32_t drm_pick_crtc(int fd, const drmModeEncoder *enc,
      bool *busy)
{
   int i;
   uint32_t fallback = 0;

   *busy = true;

   for (i = 0; i < g_drm_resources->count_crtcs && i < 32; i++)
   {
      bool lit;
      drmModeCrtc *crtc;

      if (!(enc->possible_crtcs & (1u << i)))
         continue;
      if (!(crtc = drmModeGetCrtc(fd, g_drm_resources->crtcs[i])))
         continue;
      lit = crtc->mode_valid != 0;
      drmModeFreeCrtc(crtc);

      if (!lit)
      {
         *busy = false;
         return g_drm_resources->crtcs[i];
      }
      if (!fallback)
         fallback = g_drm_resources->crtcs[i];
   }

   return fallback;
}

bool drm_get_encoder(int fd)
{
   int i;
   bool busy = true;
   uint32_t crtc;

   g_drm_encoder_crtc = 0;

   for (i = 0; i < g_drm_resources->count_encoders; i++)
   {
      g_drm_encoder = drmModeGetEncoder(fd, g_drm_resources->encoders[i]);

      if (!g_drm_encoder)
         continue;

      if (g_drm_encoder->encoder_id == g_drm_connector->encoder_id)
         break;

      drmModeFreeEncoder(g_drm_encoder);
      g_drm_encoder = NULL;
   }

   if (g_drm_encoder && g_drm_encoder->crtc_id)
      g_drm_encoder_crtc = g_drm_encoder->crtc_id;
   else
   {
      /* Not driven yet, as with a display plugged in after boot */
      if (g_drm_encoder)
      {
         drmModeFreeEncoder(g_drm_encoder);
         g_drm_encoder = NULL;
      }

      for (i = 0; i < g_drm_connector->count_encoders; i++)
      {
         drmModeEncoder *enc = drmModeGetEncoder(fd,
               g_drm_connector->encoders[i]);

         if (!enc)
            continue;

         crtc = drm_pick_crtc(fd, enc, &busy);
         if (crtc && (!busy || !g_drm_encoder))
         {
            if (g_drm_encoder)
               drmModeFreeEncoder(g_drm_encoder);
            g_drm_encoder      = enc;
            g_drm_encoder_crtc = crtc;
            if (!busy)
               break;
         }
         else
            drmModeFreeEncoder(enc);
      }

      if (g_drm_encoder && busy)
         RARCH_WARN("[DRM] No free CRTC, taking CRTC %u.\n",
               g_drm_encoder_crtc);
   }

   if (!g_drm_encoder)
   {
      RARCH_WARN("[DRM] Couldn't find DRM encoder.\n");
      return false;
   }

   for (i = 0; i < g_drm_connector->count_modes; i++)
   {
      RARCH_LOG("[DRM] Mode %d: (%s) %d x %d, %f Hz\n",
            i,
            g_drm_connector->modes[i].name,
            g_drm_connector->modes[i].hdisplay,
            g_drm_connector->modes[i].vdisplay,
            drm_calc_refresh_rate(&g_drm_connector->modes[i]));
   }

   return true;
}

drmModeModeInfo *drm_find_mode(drmModeConnector *conn,
      unsigned width, unsigned height,
      float refresh_mod, float refresh_rate)
{
   int i;
   float minimum_fps_diff = 0.0f;
   drmModeModeInfo *best  = NULL;

   if (!conn || conn->count_modes <= 0)
      return NULL;

   /* Lower resolutions tend to have several refresh rates */
   for (i = 0; i < conn->count_modes; i++)
   {
      float diff;
      drmModeModeInfo *mode = &conn->modes[i];

      if (   (width  != mode->hdisplay)
          || (height != mode->vdisplay))
         continue;

      diff = fabsf(refresh_mod * drm_calc_refresh_rate(mode)
            - refresh_rate);

      if (!best || diff < minimum_fps_diff)
      {
         best             = mode;
         minimum_fps_diff = diff;
      }
   }

   if (!best)
   {
      RARCH_WARN("[DRM] No %ux%u mode, falling back to mode 0 (default).\n",
            width, height);
      best = &conn->modes[0];
   }

   return best;
}

void drm_setup(int fd)
{
   g_crtc_id        = g_drm_encoder_crtc;
   g_connector_id   = g_drm_connector->connector_id;
   g_orig_crtc      = drmModeGetCrtc(fd, g_crtc_id);
   if (!g_orig_crtc)
      RARCH_WARN("[DRM] Cannot find original CRTC.\n");
}

float drm_get_refresh_rate(void *data)
{
   float refresh_rate = 0.0f;

   if (g_drm_mode)
   {
      refresh_rate = drm_calc_refresh_rate(g_drm_mode);
   }

   return refresh_rate;
}

void drm_free(void)
{
   if (g_drm_encoder)
      drmModeFreeEncoder(g_drm_encoder);
   if (g_drm_connector)
      drmModeFreeConnector(g_drm_connector);
   if (g_drm_resources)
      drmModeFreeResources(g_drm_resources);

   memset(&g_drm_fds,     0, sizeof(struct pollfd));
   memset(&g_drm_evctx,   0, sizeof(drmEventContext));

   g_drm_encoder      = NULL;
   g_drm_encoder_crtc = 0;
   g_drm_connector    = NULL;
   g_drm_resources    = NULL;
}
