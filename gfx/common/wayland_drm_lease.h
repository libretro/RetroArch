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

#ifndef __WAYLAND_DRM_LEASE_H
#define __WAYLAND_DRM_LEASE_H

#include <boolean.h>
#include <retro_common_api.h>

struct wl_display;

RETRO_BEGIN_DECLS

/* A DRM connector leased from a Wayland compositor.
 *
 * A Wayland client cannot put a modeline on the wire: the protocol
 * that changes a mode carries width, height and a refresh rate and
 * nothing that can express a porch, so a 15 kHz timing cannot be
 * asked for. drm-lease-v1 hands a connector over whole instead - the
 * compositor gives up the connector, its CRTC and a plane, and the
 * client gets a DRM file descriptor that is master for exactly those
 * objects. On that descriptor an atomic modeset carries a full
 * drmModeModeInfo, which is the timing the engine in gfx/modeline/
 * generates.
 *
 * What that buys is the whole KMS path unchanged: the DRM context
 * scans out on the leased descriptor and dispserv_kms applies
 * modelines to it exactly as it does on a bare KMS session, with no
 * root and no EDID override. The compositor keeps the desktop on the
 * heads it did not lease, so this is a second output rather than a
 * takeover - which is what a CRT beside a desktop monitor wants.
 *
 * The lease owns its own Wayland connection, because nothing else in
 * the process has one: under this arrangement the context driver is
 * the DRM one and the display server is the KMS one. It also has to
 * outlive a video re-init, or the connector goes back mid-session.
 *
 * A compositor may take the lease back. That arrives as
 * wp_drm_lease_v1.finished on this connection, and what the caller
 * sees first is a DRM call failing on objects that are no longer
 * ours, so the question is asked there rather than on every frame -
 * see wayland_drm_lease_revoked(). */

/* Lease a connector and return a DRM file descriptor for it, or -1.
 *
 * monitor_index is the one the video settings use: 0 takes whichever
 * connector the compositor offers first, and 1..n pick from the
 * offered list in order, so it means the same thing it does
 * everywhere else. -1 covers every ordinary case - no Wayland
 * session, a compositor without the protocol, one that offers no
 * connector, an index past the end, and a lease the compositor
 * refused - and says which in the log.
 *
 * The descriptor belongs to the lease: it stays valid until
 * wayland_drm_lease_release(), and the caller must not close it. */
int wayland_drm_lease_acquire(int monitor_index);

/* Give the connector back and drop the connection. Safe with no
 * lease held. */
void wayland_drm_lease_release(void);

/* The name of the leased connector ("DP-1"), or NULL. */
const char *wayland_drm_lease_connector(void);

/* Say what the compositor is willing to lease, on a connection the
 * caller already has - the display server's, which is up on an
 * ordinary Wayland session where no lease is taken. Nothing is
 * leased and the connection is left as it was found; this only
 * answers whether a CRT could be driven here at all. */
void wayland_drm_lease_report(struct wl_display *dpy);

/* Whether the compositor has taken the lease back, answered from
 * whatever has already arrived on the connection plus one
 * non-blocking read. Meant for the moment a DRM call has just
 * failed: it tells a revoked lease apart from a real error, and
 * costs a poll that only happens then. */
bool wayland_drm_lease_revoked(void);

RETRO_END_DECLS

#endif
