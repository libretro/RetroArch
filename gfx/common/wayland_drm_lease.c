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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <boolean.h>
#include <compat/strl.h>

#include <wayland-client.h>

#include "wayland_drm_lease.h"
#include "wayland/drm-lease-v1.h"
#include "../../verbosity.h"

#define LEASE_MAX_CONNECTORS 8

typedef struct
{
   struct wl_display              *dpy;
   struct wl_registry             *registry;
   struct wp_drm_lease_device_v1  *dev;
   struct wp_drm_lease_v1         *lease;
   struct wp_drm_lease_connector_v1 *conn[LEASE_MAX_CONNECTORS];
   int      fd;             /* the leased DRM node, -1 when none */
   int      nconn;
   int      picked;         /* index into conn[], -1 before the pick */
   bool     done;           /* the device finished advertising */
   bool     refused;        /* the compositor said no */
   bool     borrowed;       /* the connection is someone else's */
   char     name[LEASE_MAX_CONNECTORS][64];
} wl_lease_t;

static wl_lease_t wl_lease =
{
   NULL, NULL, NULL, NULL,
   { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
   -1, 0, -1, false, false, false, { { 0 } }
};

/* ---- connector ---- */

static void lease_connector_name(void *data,
      struct wp_drm_lease_connector_v1 *conn, const char *name)
{
   wl_lease_t *l = (wl_lease_t*)data;
   int i;
   for (i = 0; i < l->nconn; i++)
      if (l->conn[i] == conn)
         strlcpy(l->name[i], name, sizeof(l->name[0]));
}

static void lease_connector_description(void *data,
      struct wp_drm_lease_connector_v1 *conn, const char *desc) { }

static void lease_connector_id(void *data,
      struct wp_drm_lease_connector_v1 *conn, uint32_t id) { }

static void lease_connector_done(void *data,
      struct wp_drm_lease_connector_v1 *conn) { }

/* The compositor is no longer willing to lease this one. It stays in
 * the list as a hole: dropping it would renumber the offer the
 * monitor index counts through, mid-enumeration. */
static void lease_connector_withdrawn(void *data,
      struct wp_drm_lease_connector_v1 *conn)
{
   wl_lease_t *l = (wl_lease_t*)data;
   int i;
   for (i = 0; i < l->nconn; i++)
   {
      if (l->conn[i] != conn)
         continue;
      wp_drm_lease_connector_v1_destroy(conn);
      l->conn[i]    = NULL;
      l->name[i][0] = '\0';
   }
}

static const struct wp_drm_lease_connector_v1_listener lease_connector_listener = {
   lease_connector_name,
   lease_connector_description,
   lease_connector_id,
   lease_connector_done,
   lease_connector_withdrawn,
};

/* ---- device ---- */

/* A non-master descriptor for picking a device. The connectors are
 * named over the protocol, so nothing here needs it. */
static void lease_device_drm_fd(void *data,
      struct wp_drm_lease_device_v1 *dev, int32_t fd)
{
   close(fd);
}

static void lease_device_connector(void *data,
      struct wp_drm_lease_device_v1 *dev,
      struct wp_drm_lease_connector_v1 *conn)
{
   wl_lease_t *l = (wl_lease_t*)data;
   if (l->nconn >= LEASE_MAX_CONNECTORS)
   {
      wp_drm_lease_connector_v1_destroy(conn);
      return;
   }
   l->conn[l->nconn++] = conn;
   wp_drm_lease_connector_v1_add_listener(conn,
         &lease_connector_listener, l);
}

static void lease_device_done(void *data,
      struct wp_drm_lease_device_v1 *dev)
{
   wl_lease_t *l = (wl_lease_t*)data;
   l->done       = true;
}

static void lease_device_released(void *data,
      struct wp_drm_lease_device_v1 *dev)
{
   wl_lease_t *l = (wl_lease_t*)data;
   wp_drm_lease_device_v1_destroy(dev);
   if (l->dev == dev)
      l->dev = NULL;
}

static const struct wp_drm_lease_device_v1_listener lease_device_listener = {
   lease_device_drm_fd,
   lease_device_connector,
   lease_device_done,
   lease_device_released,
};

/* ---- the lease itself ---- */

static void lease_handle_fd(void *data,
      struct wp_drm_lease_v1 *lease, int32_t fd)
{
   wl_lease_t *l = (wl_lease_t*)data;
   l->fd         = fd;
}

/* Sent instead of the descriptor when the compositor refuses, and
 * later if it takes the lease back. */
static void lease_handle_finished(void *data,
      struct wp_drm_lease_v1 *lease)
{
   wl_lease_t *l = (wl_lease_t*)data;
   l->refused    = true;
}

static const struct wp_drm_lease_v1_listener lease_listener = {
   lease_handle_fd,
   lease_handle_finished,
};

/* ---- registry ---- */

static void registry_handle_global(void *data,
      struct wl_registry *registry, uint32_t name,
      const char *interface, uint32_t version)
{
   wl_lease_t *l = (wl_lease_t*)data;
   if (l->dev || strcmp(interface, "wp_drm_lease_device_v1"))
      return;
   l->dev = (struct wp_drm_lease_device_v1*)
      wl_registry_bind(registry, name,
            &wp_drm_lease_device_v1_interface, 1);
   wp_drm_lease_device_v1_add_listener(l->dev, &lease_device_listener, l);
}

static void registry_handle_global_remove(void *data,
      struct wl_registry *registry, uint32_t name) { }

static const struct wl_registry_listener registry_listener = {
   registry_handle_global,
   registry_handle_global_remove,
};

/* ---- */

static void lease_teardown(wl_lease_t *l)
{
   int i;

   for (i = 0; i < LEASE_MAX_CONNECTORS; i++)
   {
      if (l->conn[i])
         wp_drm_lease_connector_v1_destroy(l->conn[i]);
      l->conn[i]    = NULL;
      l->name[i][0] = '\0';
   }
   if (l->lease)
      wp_drm_lease_v1_destroy(l->lease);
   if (l->dev)
   {
      /* The compositor answers with released and destroys its side;
       * one roundtrip gives that answer somewhere to land. No request
       * may follow release on this object. */
      wp_drm_lease_device_v1_release(l->dev);
      if (l->dpy)
         wl_display_roundtrip(l->dpy);
      if (l->dev)
         wp_drm_lease_device_v1_destroy(l->dev);
   }
   if (l->registry)
      wl_registry_destroy(l->registry);
   if (l->dpy && !l->borrowed)
      wl_display_disconnect(l->dpy);

   /* The descriptor is the caller's reason for being here; closing it
    * is the caller's business only through release() */
   if (l->fd >= 0)
      close(l->fd);

   memset(l, 0, sizeof(*l));
   l->fd     = -1;
   l->picked = -1;
}

void wayland_drm_lease_report(struct wl_display *dpy)
{
   wl_lease_t l;
   int i;
   int offered = 0;

   if (!dpy)
      return;

   memset(&l, 0, sizeof(l));
   l.fd       = -1;
   l.picked   = -1;
   l.dpy      = dpy;
   l.borrowed = true;

   l.registry = wl_display_get_registry(dpy);
   wl_registry_add_listener(l.registry, &registry_listener, &l);
   wl_display_roundtrip(dpy);

   if (!l.dev)
   {
      RARCH_LOG("[Lease] The compositor offers no DRM leases.\n");
      lease_teardown(&l);
      return;
   }

   /* the device's fd, connectors and done, then their own names */
   wl_display_roundtrip(dpy);
   wl_display_roundtrip(dpy);

   for (i = 0; i < l.nconn; i++)
   {
      if (!l.conn[i])
         continue;
      offered++;
      RARCH_LOG("[Lease] Connector \"%s\" is offered for DRM lease.\n",
            l.name[i]);
   }

   if (offered < 1)
      RARCH_LOG("[Lease] The compositor offers DRM leases, but no connector.\n");
   else
      RARCH_LOG("[Lease] Set video_context_driver to \"kms\" to drive one of these directly; a modeline cannot be put on the wire any other way here.\n");

   lease_teardown(&l);
}

int wayland_drm_lease_acquire(int monitor_index)
{
   wl_lease_t *l = &wl_lease;
   struct wp_drm_lease_request_v1 *req;
   int i;
   int want;
   int offered = 0;

   if (l->fd >= 0)
      return l->fd;

   memset(l, 0, sizeof(*l));
   l->fd     = -1;
   l->picked = -1;

   if (!(l->dpy = wl_display_connect(NULL)))
      return -1;   /* no Wayland session; not an error here */

   l->registry = wl_display_get_registry(l->dpy);
   wl_registry_add_listener(l->registry, &registry_listener, l);

   /* globals, then the device's fd, connectors and done, then the
    * connectors' own names */
   wl_display_roundtrip(l->dpy);
   if (!l->dev)
   {
      RARCH_LOG("[Lease] The compositor offers no DRM leases.\n");
      lease_teardown(l);
      return -1;
   }
   wl_display_roundtrip(l->dpy);
   wl_display_roundtrip(l->dpy);

   for (i = 0; i < l->nconn; i++)
      if (l->conn[i])
         offered++;

   if (offered < 1)
   {
      RARCH_LOG("[Lease] The compositor offers DRM leases, but no connector.\n");
      lease_teardown(l);
      return -1;
   }

   /* 0 is "whichever it offers first", 1..n count through the offer */
   want = (monitor_index > 0) ? monitor_index : 1;
   for (i = 0; i < l->nconn; i++)
   {
      if (!l->conn[i])
         continue;
      if (--want == 0)
      {
         l->picked = i;
         break;
      }
   }

   if (l->picked < 0)
   {
      RARCH_WARN("[Lease] Monitor index %d is past the %d connector(s) offered.\n",
            monitor_index, offered);
      lease_teardown(l);
      return -1;
   }

   req = wp_drm_lease_device_v1_create_lease_request(l->dev);
   wp_drm_lease_request_v1_request_connector(req, l->conn[l->picked]);
   l->lease = wp_drm_lease_request_v1_submit(req);
   wp_drm_lease_v1_add_listener(l->lease, &lease_listener, l);

   wl_display_roundtrip(l->dpy);

   if (l->fd < 0)
   {
      RARCH_WARN("[Lease] The compositor refused a lease of \"%s\".\n",
            l->name[l->picked]);
      lease_teardown(l);
      return -1;
   }

   RARCH_LOG("[Lease] Connector \"%s\" leased; scanning out on it directly.\n",
         l->name[l->picked]);
   return l->fd;
}

void wayland_drm_lease_release(void)
{
   wl_lease_t *l = &wl_lease;
   if (!l->dpy && l->fd < 0)
      return;
   if (l->picked >= 0 && l->name[l->picked][0])
      RARCH_LOG("[Lease] Giving connector \"%s\" back.\n", l->name[l->picked]);
   lease_teardown(l);
}

bool wayland_drm_lease_revoked(void)
{
   wl_lease_t *l = &wl_lease;
   struct pollfd pfd;

   if (!l->dpy || l->fd < 0)
      return false;
   if (l->refused)
      return true;

   /* The prepare/read pairing is what makes this safe to do without
    * blocking: everything already queued is dispatched, and the
    * socket is only read when it has something. */
   while (wl_display_prepare_read(l->dpy) != 0)
      wl_display_dispatch_pending(l->dpy);
   wl_display_flush(l->dpy);

   pfd.fd      = wl_display_get_fd(l->dpy);
   pfd.events  = POLLIN;
   pfd.revents = 0;
   if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN))
      wl_display_read_events(l->dpy);
   else
      wl_display_cancel_read(l->dpy);

   wl_display_dispatch_pending(l->dpy);
   return l->refused;
}

const char *wayland_drm_lease_connector(void)
{
   wl_lease_t *l = &wl_lease;
   if (l->fd < 0 || l->picked < 0 || !l->name[l->picked][0])
      return NULL;
   return l->name[l->picked];
}
