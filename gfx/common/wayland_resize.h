/*  RetroArch - A frontend for libretro.
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

#ifndef __WAYLAND_RESIZE_H
#define __WAYLAND_RESIZE_H

#ifdef HAVE_WAYLAND_BACKPORT
#include "wayland_common_backport.h"
#endif

#include <wayland-client.h>

#include <boolean.h>
#include <retro_inline.h>

/* What a context driver does once its buffers are resized: the surface
 * takes the buffer scale they are drawn at - unless fractional scaling
 * has the viewport carry it, or the surface is too old to take one -
 * and the compositor's configures, ignored while the splash was up,
 * count from here on. */
static INLINE void wl_surface_resized(struct wl_surface *surface,
      bool fractional, unsigned buffer_scale, bool *ignore_configuration)
{
   if (     !fractional
         && wl_surface_get_version(surface)
            >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
      wl_surface_set_buffer_scale(surface, (int32_t)buffer_scale);
   *ignore_configuration = false;
}

#endif
