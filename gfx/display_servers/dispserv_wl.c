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

#include <wayland-client.h>

#include "../video_display_server.h"

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

static const struct wl_output_listener output_listener = {
   output_handle_geometry,
   output_handle_mode,
   output_handle_done,
   output_handle_scale,
};

/* wl_registry listener */
static void registry_handle_global(void *data,
      struct wl_registry *registry,
      uint32_t name, const char *interface,
      uint32_t version)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;

   /* Bind to the first wl_output we find */
   if (!serv->output && strcmp(interface, "wl_output") == 0)
   {
      serv->output = (struct wl_output*)
         wl_registry_bind(registry, name, &wl_output_interface, 2);
      wl_output_add_listener(serv->output, &output_listener, serv);
   }
}

static void registry_handle_global_remove(void *data,
      struct wl_registry *registry, uint32_t name) { }

static const struct wl_registry_listener registry_listener = {
   registry_handle_global,
   registry_handle_global_remove,
};

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

   /* First roundtrip: discover globals (binds wl_output) */
   wl_display_roundtrip(serv->dpy);
   /* Second roundtrip: receive wl_output events (mode, geometry) */
   wl_display_roundtrip(serv->dpy);

   return serv;
}

static void wl_display_server_destroy(void *data)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   if (!serv)
      return;
   if (serv->output)
      wl_output_destroy(serv->output);
   if (serv->registry)
      wl_registry_destroy(serv->registry);
   if (serv->dpy)
      wl_display_disconnect(serv->dpy);
   free(serv);
}

static float wl_display_server_get_refresh_rate(void *data)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   if (!serv || !serv->have_mode || serv->refresh <= 0)
      return 0.0f;
   return (float)serv->refresh / 1000.0f;
}

static void wl_display_server_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *s, size_t len)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;
   if (!serv || !serv->have_mode)
      return;
   if (width)
      *width  = serv->width;
   if (height)
      *height = serv->height;
}

static bool wl_display_server_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   dispserv_wl_t *serv = (dispserv_wl_t*)data;

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

const video_display_server_t dispserv_wl = {
   wl_display_server_init,
   wl_display_server_destroy,
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   wl_display_server_get_refresh_rate,
   wl_display_server_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   wl_display_server_get_metrics,
   NULL, /* get_flags */
   "wayland"
};
