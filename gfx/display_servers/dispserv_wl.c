/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - RetroArch
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#include <poll.h>
#include <wayland-client.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "../video_display_server.h"
#include "edid_sysfs.h"
#include "../common/wayland_drm_lease.h"

#include "../../verbosity.h"

#include "../common/mutter_displayconfig.h"
#include "../common/wayland_kwin_output.h"
#include "../common/wayland_wlr_output.h"

typedef struct
{
   struct wl_display    *dpy;
   struct wl_registry   *registry;
   struct wl_output     *output;
   int      width;
   int      height;
   int      physical_width;   /* mm */
   int      physical_height;  /* mm */
   int      refresh;          /* mHz */
   bool     have_mode;
   bool     have_geometry;
   /* wl_output v4 name: the DRM connector ("HDMI-A-1"), which is the
    * sysfs node the EDID lives under; Wayland itself has no
    * protocol for the EDID */
   char     name[32];
   bool     lease_reported;
   /* KWin's outputs and their modes, where the compositor is KWin */
   kwin_outputs_t kwin;
   /* wlroots compositors' heads and modes (sway, Hyprland, river) */
   wlr_outputs_t  wlr;
} dispserv_wl_t;

/* wl_output listener callbacks */
static void output_handle_geometry(void *data,
      struct wl_output *output,
      int32_t x, int32_t y,
      int32_t physical_width, int32_t physical_height,
      int32_t subpixel,
      const char *make, const char *model,
      int32_t transform)
{
   dispserv_wl_t *serv  = (dispserv_wl_t*)data;
   serv->physical_width  = physical_width;
   serv->physical_height = physical_height;
   serv->have_geometry   = true;
}

static void output_handle_mode(void *data,
      struct wl_output *output,
      uint32_t flags,
      int32_t width, int32_t height,
      int32_t refresh)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   /* Only use the current/preferred mode */
   if (flags & WL_OUTPUT_MODE_CURRENT)
   {
      serv->width     = width;
      serv->height    = height;
      serv->refresh   = refresh;
      serv->have_mode = true;
   }
}

static void output_handle_done(void *data,
      struct wl_output *output) { }
static void output_handle_scale(void *data,
      struct wl_output *output, int32_t factor) { }

#ifdef WL_OUTPUT_NAME_SINCE_VERSION
static void output_handle_name(void *data,
      struct wl_output *output, const char *name)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   strlcpy(serv->name, name ? name : "", sizeof(serv->name));
}
static void output_handle_description(void *data,
      struct wl_output *output, const char *description) { }
#endif

static const struct wl_output_listener output_listener = {
   output_handle_geometry,
   output_handle_mode,
   output_handle_done,
   output_handle_scale,
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
   output_handle_name,
   output_handle_description,
#endif
};

/* wl_registry listener */
static void registry_handle_global(void *data,
      struct wl_registry *registry,
      uint32_t name, const char *interface,
      uint32_t version)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;

   if (kwin_outputs_bind(&serv->kwin, registry, name, interface, version))
      return;
   if (wlr_outputs_bind(&serv->wlr, registry, name, interface, version))
      return;

   /* Bind to the first wl_output we find */
   if (!serv->output && strcmp(interface, "wl_output") == 0)
   {
      uint32_t want = 2;
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
      /* v4 adds the name event; take it when the compositor has it */
      if (version >= WL_OUTPUT_NAME_SINCE_VERSION)
         want = WL_OUTPUT_NAME_SINCE_VERSION;
#endif
      serv->output = (struct wl_output*)
         wl_registry_bind(registry, name, &wl_output_interface, want);
      wl_output_add_listener(serv->output, &output_listener, serv);
   }
}

static void registry_handle_global_remove(void *data,
      struct wl_registry *registry, uint32_t name) { }

static const struct wl_registry_listener registry_listener = {
   registry_handle_global,
   registry_handle_global_remove,
};

/* Takes whatever the compositor has sent on this connection, without
 * waiting for more: the output is bound and described over the first
 * few calls after init, which report nothing until then. The
 * connection is this display server's own, used only on the thread
 * that calls it, so the context's queue is never touched. */
static void wl_display_server_pump(dispserv_wl_t *serv)
{
   if (!serv || !serv->dpy)
      return;
   wl_display_flush(serv->dpy);
   if (wl_display_prepare_read(serv->dpy) == 0)
   {
      struct pollfd pfd;
      pfd.fd      = wl_display_get_fd(serv->dpy);
      pfd.events  = POLLIN;
      pfd.revents = 0;
      if (poll(&pfd, 1, 0) > 0)
         wl_display_read_events(serv->dpy);
      else
         wl_display_cancel_read(serv->dpy);
   }
   wl_display_dispatch_pending(serv->dpy);
   /* A bind made while dispatching goes out now, not next time */
   wl_display_flush(serv->dpy);
}

#ifdef HAVE_THREADS
/* The DRM lease report needs answers from the compositor; it gets them
 * on a thread and a connection of its own, which nothing waits for. */
static void wl_display_server_lease_report(void *data)
{
   struct wl_display *dpy = wl_display_connect(NULL);
   (void)data;
   if (!dpy)
      return;
   wayland_drm_lease_report(dpy);
   wl_display_disconnect(dpy);
}
#endif

void wl_display_server_report_lease(void *data)
{
#ifdef HAVE_THREADS
   sthread_t *report;
#endif
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   if (!serv || serv->lease_reported)
      return;
   serv->lease_reported = true;
#ifdef HAVE_THREADS
   if ((report = sthread_create(wl_display_server_lease_report, NULL)))
      sthread_detach(report);
#endif
}

static void *wl_display_server_init(void)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)calloc(1, sizeof(*serv));
   if (!serv)
      return NULL;

   serv->dpy = wl_display_connect(NULL);
   if (!serv->dpy)
   {
      free(serv);
      return NULL;
   }

   serv->registry = wl_display_get_registry(serv->dpy);
   wl_registry_add_listener(serv->registry, &registry_listener, serv);

   wl_display_flush(serv->dpy);

#ifdef RARCH_HAVE_MUTTER_DC
   /* Starts the Mutter worker; its answer is read at each call. */
   mutter_displayconfig_available();
#endif

   return serv;
}

static void wl_display_server_destroy(void *data)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   if (!serv)
      return;
   kwin_outputs_destroy(&serv->kwin);
   wlr_outputs_destroy(&serv->wlr);
   if (serv->output)
      wl_output_destroy(serv->output);
   if (serv->registry)
      wl_registry_destroy(serv->registry);
   if (serv->dpy)
      wl_display_disconnect(serv->dpy);
   free(serv);
}

#ifdef RARCH_HAVE_MUTTER_DC
/* The head this client's wl_output names (wl_output v4), else
 * Mutter's primary */
static void wl_display_server_mutter_target(dispserv_wl_t *serv,
      int monitor_index, mutter_dc_target_t *t)
{
   memset(t, 0, sizeof(*t));
   t->connector     = serv->name[0] ? serv->name : NULL;
   t->monitor_index = monitor_index;
}
#endif

/* Modes are listed and switched through Mutter on GNOME, as before,
 * through KWin's own output protocols on KDE, and through wlroots'
 * output management on sway, Hyprland and the like; elsewhere there is
 * nothing to list, and the menu entry and the refresh rate autoswitch
 * stay off as they always were. */
static void *wl_display_server_get_resolution_list(void *data,
      unsigned *len)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   wl_display_server_pump((dispserv_wl_t*)data);

   *len = 0;
   if (!serv)
      return NULL;
#ifdef RARCH_HAVE_MUTTER_DC
   if (mutter_displayconfig_available())
   {
      mutter_dc_target_t t;
      video_display_config_t *list = NULL;
      wl_display_server_mutter_target(serv, 0, &t);
      if (mutter_displayconfig_get_resolution_list(&t, &list, len)
            != MUTTER_DC_OK)
         return NULL;
      return list;
   }
#endif
   if (kwin_outputs_ready(&serv->kwin))
      return kwin_outputs_resolution_list(&serv->kwin,
            serv->name[0] ? serv->name : NULL, len);
   if (wlr_outputs_ready(&serv->wlr))
      return wlr_outputs_resolution_list(&serv->wlr,
            serv->name[0] ? serv->name : NULL, len);
   return NULL;
}

static bool wl_display_server_set_resolution(void *data,
      unsigned dims, int int_hz, float hz, int center,
      int monitor_index, int xoffset, int padjust)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   wl_display_server_pump((dispserv_wl_t*)data);

   if (!serv)
      return false;
#ifdef RARCH_HAVE_MUTTER_DC
   if (mutter_displayconfig_available())
   {
      mutter_dc_target_t t;
      wl_display_server_mutter_target(serv, monitor_index, &t);
      /* Asked of Mutter's worker, which applies it later; the new
       * mode's wl_output events arrive through this connection's pump
       * then, so there is nothing here to wait for */
      return mutter_displayconfig_set_resolution(&t, dims, int_hz, hz)
         == MUTTER_DC_OK;
   }
#endif
   /* KWin answers applied or failed later, through this connection's
    * events; the request is what can be known now */
   if (kwin_outputs_ready(&serv->kwin))
      return kwin_outputs_set_mode(&serv->kwin,
            serv->name[0] ? serv->name : NULL,
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), int_hz, hz);
   if (wlr_outputs_ready(&serv->wlr))
      return wlr_outputs_set_mode(&serv->wlr,
            serv->name[0] ? serv->name : NULL,
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), int_hz, hz);
   return false;
}

static uint32_t wl_display_server_get_flags(void *data)
{
   uint32_t flags      = 0;
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   bool modes          = false;
   wl_display_server_pump((dispserv_wl_t*)data);
#ifdef RARCH_HAVE_MUTTER_DC
   modes = mutter_displayconfig_available();
#endif
   if (!modes && serv)
      modes =  kwin_outputs_ready(&serv->kwin)
            || wlr_outputs_ready(&serv->wlr);
   if (!serv || !modes)
      BIT32_SET(flags, DISPSERV_CTX_NO_RESOLUTION_LIST);
   return flags;
}


static float wl_display_server_get_refresh_rate(void *data)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   wl_display_server_pump((dispserv_wl_t*)data);
   if (!serv || !serv->have_mode || serv->refresh <= 0)
      return 0.0f;
   return (float)serv->refresh / 1000.0f;
}

static void wl_display_server_get_video_output_size(void *data,
      unsigned *dims, char *s, size_t len)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   wl_display_server_pump((dispserv_wl_t*)data);
   if (!serv || !serv->have_mode)
      return;
   if (dims)
      *dims = VIDEO_SCALE_PACK(serv->width, serv->height);
}

static bool wl_display_server_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   wl_display_server_pump((dispserv_wl_t*)data);

   if (!serv || !value)
      return false;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         if (!serv->have_geometry)
            return false;
         *value = (float)serv->physical_width;
         break;
      case DISPLAY_METRIC_MM_HEIGHT:
         if (!serv->have_geometry)
            return false;
         *value = (float)serv->physical_height;
         break;
      case DISPLAY_METRIC_DPI:
         if (!serv->have_mode || !serv->have_geometry
               || serv->physical_width <= 0)
            return false;
         *value = (float)serv->width * 25.4f
               / (float)serv->physical_width;
         break;
      case DISPLAY_METRIC_PIXEL_WIDTH:
         if (!serv->have_mode)
            return false;
         *value = (float)serv->width;
         break;
      case DISPLAY_METRIC_PIXEL_HEIGHT:
         if (!serv->have_mode)
            return false;
         *value = (float)serv->height;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0.0f;
         return false;
   }

   return true;
}

/* No Wayland protocol exposes the EDID; the kernel's sysfs copy for
 * the output's connector name is the only route. Without a name (a
 * compositor below wl_output v4) the first enabled connector's. */
static int wl_display_server_get_edid(void *data, uint8_t *out, size_t max)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   const char *name    = (serv && serv->name[0]) ? serv->name : NULL;
   int n               = edid_sysfs_read(name, out, max);
   wl_display_server_pump((dispserv_wl_t*)data);
   if (n < 0 && name)
      n = edid_sysfs_read(NULL, out, max);
   return n;
}

/* No idle_wait yet, and not by oversight. This server holds its own
 * wl_display connection; input travels on the video context's, a
 * different connection with a different fd, so waiting here would
 * wake on registry and output events and never on a key. The real
 * wait needs the context's display and Wayland's prepare-read pairing
 * (wl_display_prepare_read, poll, then read_events or cancel_read),
 * which other threads dispatching that display must also honor; it
 * wants a live compositor to verify against. Until then the caller
 * sleeps. */

const video_display_server_t dispserv_wl = {
   wl_display_server_init,
   wl_display_server_destroy,
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   wl_display_server_set_resolution,
   wl_display_server_get_resolution_list,
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   wl_display_server_get_refresh_rate,
   wl_display_server_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   wl_display_server_get_metrics,
   wl_display_server_get_flags,
   NULL, /* get_scanline */
   NULL, /* wait_vblank */
   NULL, /* modeline_list_outputs */
   NULL, /* modeline_open */
   NULL, /* modeline_close */
   NULL, /* modeline_caps */
   NULL, /* modeline_enum */
   NULL, /* modeline_add */
   NULL, /* modeline_update */
   NULL, /* modeline_delete */
   NULL, /* modeline_set */
   NULL, /* modeline_flush */
   wl_display_server_get_edid,
   NULL /* idle_wait: see the note above */,
   "wayland"
};
