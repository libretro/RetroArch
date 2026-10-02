/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (wayland_present_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* gfx/common/wayland_present.c against an in-process compositor.
 *
 * Under threaded video the video thread requests presentation
 * feedback and paces on it, while the main thread reads the display
 * and dispatches input. The feedback events must run on the video
 * thread only, so they live on a queue of their own:
 *
 * 1. Events the main thread reads and dispatches on the default queue
 *    leave the feedback untouched; the video thread's dispatch then
 *    takes the presented timing and frees the feedback.
 * 2. A discarded frame frees its feedback and leaves the timing.
 * 3. The pacing wait spends the timing it used.
 * 4. Destroying with a feedback in flight leaks nothing (ASan).
 * 5. A main thread reading and dispatching input while a video thread
 *    requests, commits and dispatches, frame after frame; under TSan
 *    any feedback event dispatched on the main thread is a race on
 *    the feedback list and the timing.
 * 6. The GL context's frame-callback wait works the same way: done is
 *    left alone by the default queue, ends the wait when it comes, and
 *    a wait that times out leaves no callback behind for another
 *    thread to destroy again.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include "presentation-time-server-protocol.h"
#include "gfx/common/wayland_present.h"
#include "gfx/common/wayland/presentation-time.h"
#include <features/features_cpu.h>

#define LOG_FN(name) \
   void name(const char *fmt, ...) \
   { va_list ap; va_start(ap, fmt); vfprintf(stdout, fmt, ap); va_end(ap); }
LOG_FN(RARCH_LOG)
LOG_FN(RARCH_WARN)
LOG_FN(RARCH_ERR)
LOG_FN(RARCH_DBG)

#define THREADED_FRAMES 2000

static int failures = 0;

#define CHECK(cond, msg) do { if (!(cond)) { \
   printf("FAIL %d: %s\n", __LINE__, msg); failures++; } } while (0)

/* ---- the compositor, on its own thread ---- */

enum answer { ANSWER_PRESENT, ANSWER_DISCARD };

struct comp_feedback
{
   struct wl_resource *res;
   struct wl_list      link;
};

static struct wl_display *sdpy;
static struct wl_list     comp_feedbacks;
static struct wl_list     comp_frames;
static int                comp_hold_frames;
static pthread_mutex_t    comp_lock = PTHREAD_MUTEX_INITIALIZER;
static int                comp_stop;
static int                comp_answer;
static uint64_t           comp_ust;
static uint32_t           comp_refresh;

static uint64_t now_ns(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void s_feedback_gone(struct wl_resource *res)
{
   struct comp_feedback *fb = (struct comp_feedback*)
      wl_resource_get_user_data(res);
   wl_list_remove(&fb->link);
   free(fb);
}

static void s_surface_destroy(struct wl_client *c, struct wl_resource *r)
{
   wl_resource_destroy(r);
}
static void s_surface_attach(struct wl_client *c, struct wl_resource *r,
      struct wl_resource *b, int32_t x, int32_t y) { }
static void s_surface_damage(struct wl_client *c, struct wl_resource *r,
      int32_t x, int32_t y, int32_t w, int32_t h) { }
static void s_surface_frame(struct wl_client *c, struct wl_resource *r,
      uint32_t id)
{
   struct comp_feedback *fb = (struct comp_feedback*)calloc(1, sizeof(*fb));
   fb->res = wl_resource_create(c, &wl_callback_interface, 1, id);
   wl_resource_set_implementation(fb->res, NULL, fb, s_feedback_gone);
   wl_list_insert(comp_frames.prev, &fb->link);
}
static void s_surface_region(struct wl_client *c, struct wl_resource *r,
      struct wl_resource *region) { }

/* A commit answers every feedback asked for since the last one */
static void s_surface_commit(struct wl_client *c, struct wl_resource *r)
{
   struct comp_feedback *fb, *tmp;
   int      answer;
   uint64_t ust;
   uint32_t refresh;

   int      hold;

   pthread_mutex_lock(&comp_lock);
   hold    = comp_hold_frames;
   answer  = comp_answer;
   ust     = comp_ust ? comp_ust : now_ns();
   refresh = comp_refresh;
   pthread_mutex_unlock(&comp_lock);

   wl_list_for_each_safe(fb, tmp, &comp_feedbacks, link)
   {
      if (answer == ANSWER_PRESENT)
         wp_presentation_feedback_send_presented(fb->res,
               (uint32_t)((ust / 1000000000ULL) >> 32),
               (uint32_t)(ust / 1000000000ULL),
               (uint32_t)(ust % 1000000000ULL),
               refresh, 0, 1, 0);
      else
         wp_presentation_feedback_send_discarded(fb->res);
      wl_resource_destroy(fb->res);
   }
   if (!hold)
      wl_list_for_each_safe(fb, tmp, &comp_frames, link)
      {
         wl_callback_send_done(fb->res, 0);
         wl_resource_destroy(fb->res);
      }
}

static const struct wl_surface_interface s_surface_impl = {
   s_surface_destroy, s_surface_attach, s_surface_damage, s_surface_frame,
   s_surface_region, s_surface_region, s_surface_commit,
};

static void s_create_surface(struct wl_client *c, struct wl_resource *r,
      uint32_t id)
{
   struct wl_resource *s = wl_resource_create(c, &wl_surface_interface, 1, id);
   wl_resource_set_implementation(s, &s_surface_impl, NULL, NULL);
}
static void s_create_region(struct wl_client *c, struct wl_resource *r,
      uint32_t id) { }

static const struct wl_compositor_interface s_compositor_impl = {
   s_create_surface, s_create_region,
};

static void s_bind_compositor(struct wl_client *c, void *data,
      uint32_t version, uint32_t id)
{
   struct wl_resource *r = wl_resource_create(c, &wl_compositor_interface,
         1, id);
   wl_resource_set_implementation(r, &s_compositor_impl, NULL, NULL);
}

static void s_presentation_destroy(struct wl_client *c,
      struct wl_resource *r)
{
   wl_resource_destroy(r);
}

static void s_presentation_feedback(struct wl_client *c,
      struct wl_resource *r, struct wl_resource *surface, uint32_t id)
{
   struct comp_feedback *fb = (struct comp_feedback*)calloc(1, sizeof(*fb));
   fb->res = wl_resource_create(c, &wp_presentation_feedback_interface,
         wl_resource_get_version(r), id);
   wl_resource_set_implementation(fb->res, NULL, fb, s_feedback_gone);
   wl_list_insert(comp_feedbacks.prev, &fb->link);
}

static const struct wp_presentation_interface s_presentation_impl = {
   s_presentation_destroy, s_presentation_feedback,
};

static void s_bind_presentation(struct wl_client *c, void *data,
      uint32_t version, uint32_t id)
{
   struct wl_resource *r = wl_resource_create(c, &wp_presentation_interface,
         version, id);
   wl_resource_set_implementation(r, &s_presentation_impl, NULL, NULL);
   wp_presentation_send_clock_id(r, CLOCK_MONOTONIC);
}

static void *comp_thread(void *data)
{
   struct wl_event_loop *loop = wl_display_get_event_loop(sdpy);
   for (;;)
   {
      int stop;
      pthread_mutex_lock(&comp_lock);
      stop = comp_stop;
      pthread_mutex_unlock(&comp_lock);
      if (stop)
         break;
      wl_event_loop_dispatch(loop, 5);
      wl_display_flush_clients(sdpy);
   }
   return NULL;
}

static void comp_set(int answer, uint64_t ust, uint32_t refresh)
{
   pthread_mutex_lock(&comp_lock);
   comp_answer  = answer;
   comp_ust     = ust;
   comp_refresh = refresh;
   pthread_mutex_unlock(&comp_lock);
}

/* ---- the client ---- */

static struct wl_display    *cdpy;
static struct wl_compositor *compositor;
static wl_present_t          present;
static wl_frame_t            frame;
static unsigned              frames_done;
static int                   input_stop;

static void c_global(void *data, struct wl_registry *reg, uint32_t name,
      const char *iface, uint32_t version)
{
   if (!strcmp(iface, wl_compositor_interface.name))
      compositor = (struct wl_compositor*)wl_registry_bind(reg, name,
            &wl_compositor_interface, 1);
   else if (!strcmp(iface, wp_presentation_interface.name))
      wl_present_bind(&present, cdpy, reg, name, version);
}
static void c_global_remove(void *data, struct wl_registry *reg,
      uint32_t name) { }
static const struct wl_registry_listener c_registry = {
   c_global, c_global_remove,
};

/* What the input driver does each poll */
static void input_poll(void)
{
   struct pollfd fd;
   while (wl_display_prepare_read(cdpy))
      wl_display_dispatch_pending(cdpy);
   wl_display_flush(cdpy);
   fd.fd     = wl_display_get_fd(cdpy);
   fd.events = POLLIN;
   if (poll(&fd, 1, 1) > 0)
   {
      wl_display_read_events(cdpy);
      wl_display_dispatch_pending(cdpy);
   }
   else
      wl_display_cancel_read(cdpy);
}

static void *input_thread(void *data)
{
   for (;;)
   {
      int stop;
      pthread_mutex_lock(&comp_lock);
      stop = input_stop;
      pthread_mutex_unlock(&comp_lock);
      if (stop)
         break;
      input_poll();
   }
   return NULL;
}

static void *video_thread(void *data)
{
   struct wl_surface *surface = (struct wl_surface*)data;
   unsigned i;
   for (i = 0; i < THREADED_FRAMES; i++)
   {
      wl_present_dispatch(&present, cdpy);
      wl_present_wait(&present, 1);
      wl_present_request(&present, surface);
      wl_frame_request(&frame, cdpy, surface);
      wl_surface_commit(surface);
      wl_display_flush(cdpy);
      if (wl_frame_wait(&frame, cdpy, cpu_features_get_time_usec() + 50000))
         frames_done++;
      /* A frame per answer, as vsync would pace it */
      while (!wl_list_empty(&present.feedbacks))
      {
         struct timespec ts;
         ts.tv_sec  = 0;
         ts.tv_nsec = 100000;
         nanosleep(&ts, NULL);
         wl_present_dispatch(&present, cdpy);
      }
   }
   return NULL;
}

int main(void)
{
   int sv[2];
   pthread_t comp, input, video;
   struct wl_registry *registry;
   struct wl_surface *surface;
   uint64_t ust;
   uint64_t t0;

   wl_list_init(&comp_feedbacks);
   wl_list_init(&comp_frames);
   sdpy = wl_display_create();
   wl_global_create(sdpy, &wl_compositor_interface, 1, NULL,
         s_bind_compositor);
   wl_global_create(sdpy, &wp_presentation_interface, 2, NULL,
         s_bind_presentation);
   if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0)
      return 1;
   wl_client_create(sdpy, sv[0]);
   pthread_create(&comp, NULL, comp_thread, NULL);

   cdpy     = wl_display_connect_to_fd(sv[1]);
   registry = wl_display_get_registry(cdpy);
   wl_registry_add_listener(registry, &c_registry, NULL);
   wl_display_roundtrip(cdpy);
   wl_display_roundtrip(cdpy);
   CHECK(compositor && present.presentation, "globals not bound");
   CHECK(present.clock && present.clock_id == CLOCK_MONOTONIC,
         "clock_id not taken");
   surface = wl_compositor_create_surface(compositor);

   /* 1. Read and dispatched on the default queue, the presented event
    * waits for the video thread's dispatch */
   ust = now_ns() - 20000000ULL;
   comp_set(ANSWER_PRESENT, ust, 16666667u);
   wl_present_request(&present, surface);
   wl_surface_commit(surface);
   wl_display_roundtrip(cdpy);
   input_poll();
   CHECK(!present.presented, "the default queue dispatched a feedback");
   CHECK(!wl_list_empty(&present.feedbacks),
         "the feedback was freed off the presenting thread");
   wl_present_dispatch(&present, cdpy);
   CHECK(present.presented, "presented not dispatched");
   CHECK(present.last_ust == ust, "wrong presentation time");
   CHECK(present.refresh_interval == 16666667u, "wrong refresh");
   CHECK(wl_list_empty(&present.feedbacks), "presented feedback kept");
   printf("ok:   feedback events wait for the presenting thread\n");

   /* 3. A vblank already gone is not waited for; one ahead is, and
    * the wait spends the timing */
   t0 = now_ns();
   wl_present_wait(&present, 1);
   CHECK(now_ns() - t0 < 10000000ULL, "slept for a vblank already gone");
   CHECK(present.presented, "a wait that did not sleep spent the timing");
   comp_set(ANSWER_PRESENT, now_ns(), 2000000u);
   wl_present_request(&present, surface);
   wl_surface_commit(surface);
   wl_display_roundtrip(cdpy);
   wl_present_dispatch(&present, cdpy);
   wl_present_wait(&present, 1);
   CHECK(!present.presented, "the wait did not spend the timing");
   wl_present_wait(&present, 0);
   printf("ok:   the pacing wait spends what it used\n");

   /* 2. A discarded frame frees its feedback, timing untouched */
   ust = present.last_ust;
   comp_set(ANSWER_DISCARD, 0, 0);
   wl_present_request(&present, surface);
   wl_surface_commit(surface);
   wl_display_roundtrip(cdpy);
   wl_present_dispatch(&present, cdpy);
   CHECK(wl_list_empty(&present.feedbacks), "discarded feedback kept");
   CHECK(!present.presented && present.last_ust == ust,
         "a discard changed the timing");
   printf("ok:   a discarded frame frees its feedback\n");

   /* 6. The frame callback waits for the presenting thread too */
   wl_frame_request(&frame, cdpy, surface);
   wl_surface_commit(surface);
   wl_display_roundtrip(cdpy);
   input_poll();
   CHECK(!frame.done && frame.cb, "the default queue dispatched done");
   CHECK(wl_frame_wait(&frame, cdpy, cpu_features_get_time_usec() + 50000),
         "the wait missed done");
   CHECK(!frame.cb, "done left the callback");
   pthread_mutex_lock(&comp_lock);
   comp_hold_frames = 1;
   pthread_mutex_unlock(&comp_lock);
   wl_frame_request(&frame, cdpy, surface);
   wl_surface_commit(surface);
   wl_display_flush(cdpy);
   CHECK(!wl_frame_wait(&frame, cdpy, cpu_features_get_time_usec() + 20000),
         "a held callback reported done");
   CHECK(!frame.cb, "a timed-out wait left its callback");
   pthread_mutex_lock(&comp_lock);
   comp_hold_frames = 0;
   pthread_mutex_unlock(&comp_lock);
   printf("ok:   frame callbacks wait for the presenting thread\n");

   /* 5. Input on one thread, frames on another */
   comp_set(ANSWER_PRESENT, 0, 100000u);
   pthread_create(&input, NULL, input_thread, NULL);
   pthread_create(&video, NULL, video_thread, surface);
   pthread_join(video, NULL);
   pthread_mutex_lock(&comp_lock);
   input_stop = 1;
   pthread_mutex_unlock(&comp_lock);
   pthread_join(input, NULL);
   wl_display_roundtrip(cdpy);
   wl_present_dispatch(&present, cdpy);
   CHECK(wl_list_empty(&present.feedbacks), "feedbacks left unanswered");
   CHECK(present.presented, "no frame was presented");
   CHECK(frames_done == THREADED_FRAMES, "a frame callback was missed");
   printf("ok:   %u frames with input dispatched on another thread\n",
         THREADED_FRAMES);

   /* 4. Torn down with a feedback in flight */
   wl_present_request(&present, surface);
   wl_present_destroy(&present);
   CHECK(!present.presentation && !present.queue, "not reset");
   wl_frame_request(&frame, cdpy, surface);
   wl_frame_destroy(&frame);
   CHECK(!frame.cb && !frame.queue, "frame not reset");

   wl_surface_destroy(surface);
   wl_compositor_destroy(compositor);
   wl_registry_destroy(registry);
   wl_display_roundtrip(cdpy);
   wl_display_disconnect(cdpy);

   pthread_mutex_lock(&comp_lock);
   comp_stop = 1;
   pthread_mutex_unlock(&comp_lock);
   pthread_join(comp, NULL);
   wl_display_destroy_clients(sdpy);
   wl_display_destroy(sdpy);

   if (failures)
   {
      printf("== wayland_present_test: %d failures ==\n", failures);
      return 1;
   }
   printf("== wayland_present_test: all tests pass ==\n");
   return 0;
}
