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

#ifndef __RARCH_WAYLAND_PRESENT_H
#define __RARCH_WAYLAND_PRESENT_H

#include <stdint.h>
#include <time.h>
#include <boolean.h>
#include <retro_common_api.h>

#ifdef HAVE_WAYLAND_BACKPORT
#include "wayland_common_backport.h"
#endif

#include <wayland-client.h>

RETRO_BEGIN_DECLS

struct wp_presentation;

/* Presentation-time feedback (wp_presentation). The feedback objects
 * live on their own event queue, which only the thread presenting
 * frames dispatches, so their events never run on the thread that
 * dispatches input. */
typedef struct wl_present
{
   uint64_t                last_ust;
   uint64_t                refresh_interval;
   struct wp_presentation *presentation;
   struct wl_event_queue  *queue;
   struct wl_list          feedbacks;
   clockid_t               clock_id;
   /* The compositor's clock is one clock_gettime can read */
   bool                    clock;
   bool                    presented;
} wl_present_t;

/* From the registry listener. */
void wl_present_bind(wl_present_t *present, struct wl_display *dpy,
      struct wl_registry *registry, uint32_t name, uint32_t version);

/* Asks for feedback on the next commit of 'surface'. */
void wl_present_request(wl_present_t *present, struct wl_surface *surface);

/* Runs the feedback events already read from the display. */
void wl_present_dispatch(wl_present_t *present, struct wl_display *dpy);

/* Sleeps until the vblank after the last presented frame. */
void wl_present_wait(wl_present_t *present, int swap_interval);

void wl_present_destroy(wl_present_t *present);

RETRO_END_DECLS

#endif
