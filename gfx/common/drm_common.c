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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>

#include <retro_miscellaneous.h>

#include "../../verbosity.h"
#ifdef HAVE_MODELINE
#include "../video_driver.h"
#endif

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

bool drm_wait_flip_hotplug(int timeout, int hotplug_fd, short *hotplug)
{
   struct pollfd fds[2];
   nfds_t n = 1;

   fds[0]         = g_drm_fds;
   fds[0].revents = 0;
   if (hotplug_fd >= 0)
   {
      fds[1].fd      = hotplug_fd;
      fds[1].events  = POLLIN;
      fds[1].revents = 0;
      n              = 2;
   }

   for (;;)
   {
      if (poll(fds, n, timeout) < 0)
         return false;

      /* Left queued for the caller: drop it from the set, or a
       * blocking wait would spin on it */
      if (n == 2 && fds[1].revents)
      {
         *hotplug = fds[1].revents;
         n        = 1;
      }

      if (fds[0].revents & (POLLHUP | POLLERR))
         return false;

      if (fds[0].revents & POLLIN)
      {
         drmHandleEvent(g_drm_fd, &g_drm_evctx);
         return true;
      }

      if (timeout >= 0)
         return false;
   }
}

bool drm_mode_from_crt_switch(drmModeModeInfo *mode)
{
#ifdef HAVE_MODELINE
   video_driver_state_t *video_st = video_state_get_ptr();
   videocrt_switch_t *sw          = &video_st->crt_switch_st;
   if (sw->vdisplay < 1)
      return false;
   memset(mode, 0, sizeof(*mode));
   mode->clock       = sw->clock;
   mode->hdisplay    = sw->hdisplay;
   mode->hsync_start = sw->hsync_start;
   mode->hsync_end   = sw->hsync_end;
   mode->htotal      = sw->htotal;
   mode->vdisplay    = sw->vdisplay;
   mode->vsync_start = sw->vsync_start;
   mode->vsync_end   = sw->vsync_end;
   mode->vtotal      = sw->vtotal;
   mode->flags       = (sw->interlace  ? DRM_MODE_FLAG_INTERLACE : 0)
      | (sw->doublescan ? DRM_MODE_FLAG_DBLSCAN : 0)
      | (sw->hsync      ? DRM_MODE_FLAG_PHSYNC  : DRM_MODE_FLAG_NHSYNC)
      | (sw->vsync      ? DRM_MODE_FLAG_PVSYNC  : DRM_MODE_FLAG_NVSYNC);
   mode->vrefresh    = sw->vrefresh;
   mode->type        = DRM_MODE_TYPE_USERDEF;
   snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "RetroArch_CRT-%dx%d@%.02f%s",
         sw->hdisplay, sw->vdisplay,
         (mode->htotal && mode->vtotal) ? drm_calc_refresh_rate(mode) : 0.0f,
         sw->interlace ? "i" : "");
   RARCH_DBG("[KMS] Mode %s: %d %d %d %d %d %d %d %d %d\n", mode->name,
         mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
         mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
         mode->clock);
   /* Taken */
   sw->vdisplay = 0;
   return true;
#else
   return false;
#endif
}

bool drm_mode_from_timings(const char *timings, drmModeModeInfo *mode)
{
   /* https://www.raspberrypi.org/documentation/configuration/config-txt/video.md
    * h_active, h_sync_polarity, h_front_porch, h_sync_pulse,
    * h_back_porch, v_active, v_sync_polarity, v_front_porch,
    * v_sync_pulse, v_back_porch, v_sync_offset_a, v_sync_offset_b,
    * pixel_rep, frame_rate, interlaced, pixel_freq, aspect_ratio */
   long t[17];
   const char *p = timings;
   unsigned il;
   int i;

   if (!mode || !p || !*p)
      return false;
   for (i = 0; i < 17; i++)
   {
      char *end = NULL;
      while (*p == ' ' || *p == '\t')
         p++;
      if (*p)
         t[i] = strtol(p, &end, 10);
      if (!*p || end == p)
      {
         RARCH_ERR("[KMS] Malformed mode requested: %s.\n", timings);
         return false;
      }
      p = end;
   }

   il = t[14] ? 2 : 1;
   memset(mode, 0, sizeof(*mode));
   mode->clock       = (uint32_t)(t[15] / 1000);
   mode->hdisplay    = (uint16_t)t[0];
   mode->hsync_start = (uint16_t)(mode->hdisplay + t[2]);
   mode->hsync_end   = (uint16_t)(mode->hsync_start + t[3]);
   mode->htotal      = (uint16_t)(mode->hsync_end + t[4]);
   mode->vdisplay    = (uint16_t)t[5];
   mode->vsync_start = (uint16_t)(mode->vdisplay + t[7] * il);
   mode->vsync_end   = (uint16_t)(mode->vsync_start + t[8] * il);
   mode->vtotal      = (uint16_t)(mode->vsync_end + t[9] * il);
   mode->vrefresh    = (uint32_t)t[13];
   mode->flags       = (t[14] ? DRM_MODE_FLAG_INTERLACE : 0)
      | (t[6] ? DRM_MODE_FLAG_NVSYNC : DRM_MODE_FLAG_PVSYNC)
      | (t[1] ? DRM_MODE_FLAG_NHSYNC : DRM_MODE_FLAG_PHSYNC);
   snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "CRT_%ux%u_%u",
         mode->hdisplay, mode->vdisplay, mode->vrefresh);
   return true;
}

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
