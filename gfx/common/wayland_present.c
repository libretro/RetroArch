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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <time.h>

#include "wayland_present.h"
#include "wayland/presentation-time.h"

#include "../../verbosity.h"

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW 4
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef struct wl_present_feedback
{
   struct wp_presentation_feedback *feedback;
   struct wl_list                   link;
} wl_present_feedback_t;

static void wl_present_clock_id(void *data,
      struct wp_presentation *presentation, uint32_t clock_id)
{
   wl_present_t *present = (wl_present_t*)data;

   if (clock_id == CLOCK_MONOTONIC || clock_id == CLOCK_MONOTONIC_RAW)
   {
      present->clock    = true;
      present->clock_id = (clockid_t)clock_id;
   }
}

static const struct wp_presentation_listener wl_present_listener = {
   wl_present_clock_id,
};

static void wl_present_remove(wl_present_t *present,
      struct wp_presentation_feedback *feedback)
{
   wl_present_feedback_t *fb, *tmp;
   wl_list_for_each_safe(fb, tmp, &present->feedbacks, link)
   {
      if (fb->feedback == feedback)
      {
         wl_list_remove(&fb->link);
         wp_presentation_feedback_destroy(fb->feedback);
         free(fb);
         return;
      }
   }
}

static void wl_present_feedback_sync_output(void *data,
      struct wp_presentation_feedback *feedback, struct wl_output *output)
{
}

static void wl_present_feedback_presented(void *data,
      struct wp_presentation_feedback *feedback,
      uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec,
      uint32_t refresh, uint32_t seq_hi, uint32_t seq_lo, uint32_t flags)
{
   wl_present_t *present = (wl_present_t*)data;
   uint64_t      sec     = ((uint64_t)tv_sec_hi << 32) | (uint64_t)tv_sec_lo;

   wl_present_remove(present, feedback);

   present->last_ust         = sec * 1000000000ULL + (uint64_t)tv_nsec;
   present->refresh_interval = (uint64_t)refresh;
   present->presented        = true;
}

static void wl_present_feedback_discarded(void *data,
      struct wp_presentation_feedback *feedback)
{
   wl_present_remove((wl_present_t*)data, feedback);
}

static const struct wp_presentation_feedback_listener wl_present_feedback_listener = {
   wl_present_feedback_sync_output,
   wl_present_feedback_presented,
   wl_present_feedback_discarded,
};

void wl_present_bind(wl_present_t *present, struct wl_display *dpy,
      struct wl_registry *registry, uint32_t name, uint32_t version)
{
   if (present->presentation)
      return;
   if (!(present->queue = wl_display_create_queue(dpy)))
      return;
   wl_list_init(&present->feedbacks);
   present->presentation = (struct wp_presentation*)wl_registry_bind(
         registry, name, &wp_presentation_interface, MIN(version, 2));
   wp_presentation_add_listener(present->presentation,
         &wl_present_listener, present);
}

void wl_present_request(wl_present_t *present, struct wl_surface *surface)
{
   wl_present_feedback_t *fb = (wl_present_feedback_t*)calloc(1, sizeof(*fb));
   if (!fb)
      return;

   if (!(fb->feedback = wp_presentation_feedback(present->presentation,
               surface)))
   {
      RARCH_ERR("[Wayland] Failed to create feedback object.\n");
      free(fb);
      return;
   }

   /* Its events only follow the surface's next commit, which the
    * caller has not made yet, so none can have been queued before
    * this. */
   wl_proxy_set_queue((struct wl_proxy*)fb->feedback, present->queue);
   wp_presentation_feedback_add_listener(fb->feedback,
         &wl_present_feedback_listener, present);
   wl_list_insert(&present->feedbacks, &fb->link);
}

void wl_present_dispatch(wl_present_t *present, struct wl_display *dpy)
{
   if (present->queue)
      wl_display_dispatch_queue_pending(dpy, present->queue);
}

void wl_present_wait(wl_present_t *present, int swap_interval)
{
   struct timespec ts;
   struct timespec now;
   clockid_t clock_type;
   uint64_t now_ns;
   uint64_t next_frame_ns;

   if (!present->clock || !present->presented)
      return;
   if (swap_interval == 0 || present->refresh_interval == 0)
      return;

   clock_type = (present->clock_id == CLOCK_MONOTONIC_RAW)
      ? CLOCK_MONOTONIC_RAW : CLOCK_MONOTONIC;

   /* The next vblank: when the last frame was shown plus the
    * compositor's refresh interval */
   next_frame_ns = present->last_ust + present->refresh_interval;

   if (clock_gettime(clock_type, &now) < 0)
      return;

   now_ns = (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;

   if (now_ns >= next_frame_ns)
      return;

   ts.tv_sec  = (time_t)(next_frame_ns / 1000000000ULL);
   ts.tv_nsec = (long)(next_frame_ns % 1000000000ULL);

   clock_nanosleep(clock_type, TIMER_ABSTIME, &ts, NULL);

   /* Spent; the next presented event brings new timing */
   present->presented = false;
}

void wl_present_destroy(wl_present_t *present)
{
   if (present->presentation)
   {
      wl_present_feedback_t *fb, *tmp;
      wl_list_for_each_safe(fb, tmp, &present->feedbacks, link)
      {
         wl_list_remove(&fb->link);
         wp_presentation_feedback_destroy(fb->feedback);
         free(fb);
      }
      wp_presentation_destroy(present->presentation);
   }
   if (present->queue)
      wl_event_queue_destroy(present->queue);
   present->last_ust         = 0;
   present->refresh_interval = 0;
   present->presentation     = NULL;
   present->queue            = NULL;
   present->clock            = false;
   present->presented        = false;
}
