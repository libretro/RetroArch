/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (wayland_drm_lease_test.c).
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

/* The DRM lease discovery in gfx/display_servers/dispserv_wl.c, run
 * against a compositor.
 *
 * The real display server is linked, including its wl_display_connect
 * and its roundtrips. What is stubbed is the compositor, built here on
 * libwayland-server and serving a socket this process points
 * WAYLAND_DISPLAY at, so the sequencing under test is the real one:
 * the registry bind, the non-master fd, the connector objects with
 * their name and done, and the release handshake.
 *
 * WHY A COMPOSITOR AND NOT A MOCK
 *
 * Every mistake this code can make is a protocol mistake, and
 * libwayland answers those by killing the connection. A request sent
 * after release, a connector destroyed twice, a listener whose
 * function order does not match the interface: each is a wl_display
 * error that takes RetroArch's Wayland session down at startup, and
 * none of them is visible to a compile, or to a mock that agrees with
 * whatever the client does. A real server on a real socket
 * disconnects exactly the way a real compositor would.
 *
 * WHAT IT PINS
 *
 * 1. Two offered connectors are both reported, the device is
 *    released, and the report's own connection closed afterwards; the
 *    display server's connection is a separate one.
 * 2. The non-master fd the compositor sends is closed. It is sent
 *    from a pipe whose write end is kept here: if the client leaks
 *    its copy, writing to that end does not raise EPIPE.
 * 3. The global present with no connectors, and the global absent
 *    altogether, are each reported, and neither is an error.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include <boolean.h>
#include <compat/strl.h>

#include <wayland-server-core.h>

#include "drm-lease-v1-server-protocol.h"

#include "../../../gfx/video_display_server.h"
#include "../../../gfx/common/wayland_drm_lease.h"

/* ---- the log, captured ---- */

#define LOG_MAX  128
#define LOG_LINE 512

static char log_lines[LOG_MAX][LOG_LINE];
static int  log_count;
static bool log_echo;
/* The lease report logs from a thread of its own */
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static void log_put(const char *fmt, va_list ap)
{
   char line[LOG_LINE];
   vsnprintf(line, sizeof(line), fmt, ap);
   if (log_echo)
      fputs(line, stdout);
   pthread_mutex_lock(&log_lock);
   if (log_count < LOG_MAX)
      strlcpy(log_lines[log_count++], line, LOG_LINE);
   pthread_mutex_unlock(&log_lock);
}

#define LOG_FN(name) \
   void name(const char *fmt, ...) \
   { \
      va_list ap; \
      va_start(ap, fmt); \
      log_put(fmt, ap); \
      va_end(ap); \
   }

LOG_FN(RARCH_LOG)
LOG_FN(RARCH_WARN)
LOG_FN(RARCH_ERR)
LOG_FN(RARCH_DBG)

static bool log_saw(const char *needle)
{
   int i;
   bool saw = false;
   pthread_mutex_lock(&log_lock);
   for (i = 0; i < log_count && !saw; i++)
      if (strstr(log_lines[i], needle))
         saw = true;
   pthread_mutex_unlock(&log_lock);
   return saw;
}

/* ---- the compositor ---- */

struct comp
{
   struct wl_display   *dpy;
   struct wl_listener   client_destroyed;
   const char          *socket;
   int                  fd_write;   /* our end of the pipe sent as drm_fd */
   int                  lease_write;/* ...and the one sent as lease_fd */
   int                  nconn;
   int                  leases_made;
   int                  leases_gone;
   struct wl_resource  *lease_res;
   struct wl_event_source *revoke_timer;
   bool                 released;
   bool                 client_gone;
   bool                 device_bound;
   bool                 grant;      /* answer a request, or refuse it */
   bool                 revoke;     /* ...then take it back again */
   char                 requested[64];
};

static struct comp comp;

static const char *conn_names[2] = { "DP-1", "VGA-1" };

static void connector_handle_destroy(struct wl_client *client,
      struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct wp_drm_lease_connector_v1_interface connector_impl = {
   connector_handle_destroy,
};

/* ---- the lease request, and the lease ---- */

static void lease_handle_destroy(struct wl_client *client,
      struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void lease_resource_destroyed(struct wl_resource *resource)
{
   comp.leases_gone++;
}

static const struct wp_drm_lease_v1_interface lease_impl = {
   lease_handle_destroy,
};

static void request_handle_request_connector(struct wl_client *client,
      struct wl_resource *resource, struct wl_resource *connector)
{
   const char *name = (const char*)wl_resource_get_user_data(connector);
   if (name)
      strlcpy(comp.requested, name, sizeof(comp.requested));
}

/* Runs on the compositor's own thread, which is the only one that
 * may touch its resources. */
static int revoke_timer_fired(void *data)
{
   if (comp.lease_res)
      wp_drm_lease_v1_send_finished(comp.lease_res);
   return 0;
}

static void request_handle_submit(struct wl_client *client,
      struct wl_resource *resource, uint32_t id)
{
   int pipefd[2];
   struct wl_resource *lease = wl_resource_create(client,
         &wp_drm_lease_v1_interface, 1, id);

   wl_resource_destroy(resource);          /* submit destroys the request */
   if (!lease)
      return;
   wl_resource_set_implementation(lease, &lease_impl, NULL,
         lease_resource_destroyed);

   comp.lease_res = lease;

   if (!comp.grant)
   {
      wp_drm_lease_v1_send_finished(lease);
      return;
   }

   /* A real compositor sends a DRM descriptor for the leased objects;
    * a pipe stands in, so the test can see it closed again. */
   if (pipe(pipefd) == 0)
   {
      wp_drm_lease_v1_send_lease_fd(lease, pipefd[0]);
      close(pipefd[0]);
      comp.lease_write = pipefd[1];
      comp.leases_made++;
   }

   if (comp.revoke)
   {
      comp.revoke_timer = wl_event_loop_add_timer(
            wl_display_get_event_loop(comp.dpy), revoke_timer_fired, NULL);
      if (comp.revoke_timer)
         wl_event_source_timer_update(comp.revoke_timer, 20);
   }
}

static const struct wp_drm_lease_request_v1_interface request_impl = {
   request_handle_request_connector,
   request_handle_submit,
};

static void device_handle_create_lease_request(struct wl_client *client,
      struct wl_resource *resource, uint32_t id)
{
   struct wl_resource *req = wl_resource_create(client,
         &wp_drm_lease_request_v1_interface, 1, id);
   if (req)
      wl_resource_set_implementation(req, &request_impl, NULL, NULL);
}

static void device_handle_release(struct wl_client *client,
      struct wl_resource *resource)
{
   comp.released = true;
   wp_drm_lease_device_v1_send_released(resource);
   wl_resource_destroy(resource);
}

static const struct wp_drm_lease_device_v1_interface device_impl = {
   device_handle_create_lease_request,
   device_handle_release,
};

static void client_destroyed(struct wl_listener *listener, void *data)
{
   __atomic_store_n(&comp.client_gone, true, __ATOMIC_RELEASE);
}

static void device_bind(struct wl_client *client, void *data,
      uint32_t version, uint32_t id)
{
   int i;
   int pipefd[2];
   struct wl_resource *res = wl_resource_create(client,
         &wp_drm_lease_device_v1_interface, 1, id);

   if (!res)
      return;
   wl_resource_set_implementation(res, &device_impl, NULL, NULL);

   comp.client_destroyed.notify = client_destroyed;
   wl_client_add_destroy_listener(client, &comp.client_destroyed);
   __atomic_store_n(&comp.device_bound, true, __ATOMIC_RELEASE);

   /* A compositor sends a non-master DRM fd here. A pipe stands in:
    * it is an fd like any other, and keeping the write end lets the
    * test see whether the client closed its copy. */
   if (pipe(pipefd) == 0)
   {
      wp_drm_lease_device_v1_send_drm_fd(res, pipefd[0]);
      close(pipefd[0]);           /* the compositor's own copy */
      comp.fd_write = pipefd[1];
   }

   for (i = 0; i < comp.nconn && i < 2; i++)
   {
      struct wl_resource *c = wl_resource_create(client,
            &wp_drm_lease_connector_v1_interface, 1, 0);
      if (!c)
         continue;
      wl_resource_set_implementation(c, &connector_impl,
            (void*)conn_names[i], NULL);
      wp_drm_lease_device_v1_send_connector(res, c);
      wp_drm_lease_connector_v1_send_name(c, conn_names[i]);
      wp_drm_lease_connector_v1_send_description(c, "test connector");
      wp_drm_lease_connector_v1_send_connector_id(c, 32 + i);
      wp_drm_lease_connector_v1_send_done(c);
   }

   wp_drm_lease_device_v1_send_done(res);
}

static void *comp_thread(void *arg)
{
   wl_display_run(comp.dpy);
   return NULL;
}

static bool comp_start(int nconn, bool offer_global)
{
   memset(&comp, 0, sizeof(comp));
   comp.nconn       = nconn;
   comp.fd_write    = -1;
   comp.lease_write = -1;
   comp.grant       = true;
   comp.revoke      = false;

   if (!(comp.dpy = wl_display_create()))
      return false;
   if (!(comp.socket = wl_display_add_socket_auto(comp.dpy)))
      return false;
   if (offer_global && !wl_global_create(comp.dpy,
            &wp_drm_lease_device_v1_interface, 1, NULL, device_bind))
      return false;

   setenv("WAYLAND_DISPLAY", comp.socket, 1);
   return true;
}

static void comp_stop(void)
{
   wl_display_terminate(comp.dpy);
}

/* A descriptor the client held is gone when a write to the pipe's
 * other end raises EPIPE, which it does once every read end closes. */
static bool closed_by_client(int write_end)
{
   char c = 'x';
   ssize_t n;

   if (write_end < 0)
      return false;
   n = write(write_end, &c, 1);
   return (n < 0 && errno == EPIPE);
}

static bool client_closed_the_fd(void)
{
   return closed_by_client(comp.fd_write);
}

/* ---- checks ---- */

static int fails;

static void check(const char *what, bool ok)
{
   if (!ok)
      fails++;
   printf("%s %s\n", ok ? "[pass]" : "[FAIL]", what);
}

static pthread_t comp_tid;

static void finish(void *serv)
{
   if (serv)
      dispserv_wl.destroy(serv);
   comp_stop();
   pthread_join(comp_tid, NULL);
   wl_display_destroy(comp.dpy);
   if (comp.fd_write >= 0)
      close(comp.fd_write);
   if (comp.lease_write >= 0)
      close(comp.lease_write);
}

static void *start(int nconn, bool offer_global)
{
   void *serv;

   log_count = 0;
   if (!comp_start(nconn, offer_global))
   {
      printf("[FAIL] could not start the test compositor\n");
      exit(1);
   }
   pthread_create(&comp_tid, NULL, comp_thread, NULL);
   serv = dispserv_wl.init();
   /* As video_display_server_init() does once the log is on */
   wl_display_server_report_lease(serv);
   /* The report runs on a thread and a connection of its own, which the
    * display server never waits for; the harness does. It is over once
    * it has logged and, where it bound the lease device, gone. */
   {
      int i;
      for (i = 0; i < 500; i++)
      {
         if (     log_saw("[Lease]")
               && (   !__atomic_load_n(&comp.device_bound, __ATOMIC_ACQUIRE)
                   ||  __atomic_load_n(&comp.client_gone, __ATOMIC_ACQUIRE)))
            break;
         usleep(10000);
      }
   }
   return serv;
}

static void test_two_connectors(void)
{
   void *serv;

   printf("-- a compositor offering two connectors --\n");
   serv = start(2, true);

   check("the display server came up", serv != NULL);
   check("the first connector was reported",  log_saw("\"DP-1\" is offered"));
   check("the second connector was reported", log_saw("\"VGA-1\" is offered"));
   check("the device was released", comp.released);
   check("the non-master fd was closed", client_closed_the_fd());
   check("the report closed its own connection after it", comp.client_gone);
   check("and the route out of here is named",
         log_saw("video_context_driver") && log_saw("\"kms\""));

   finish(serv);
}

static void test_global_without_connectors(void)
{
   void *serv;

   printf("\n-- the global, offering nothing --\n");
   serv = start(0, true);

   check("the display server came up", serv != NULL);
   check("says the compositor offers no connector",
         log_saw("no connector"));
   check("and does not name a route that is not there",
         !log_saw("video_context_driver"));
   check("the device was released", comp.released);
   check("the report closed its own connection after it", comp.client_gone);

   finish(serv);
}

static void test_no_global(void)
{
   void *serv;

   printf("\n-- a compositor without the global --\n");
   serv = start(0, false);

   check("the display server came up", serv != NULL);
   check("says the compositor offers no leases",
         log_saw("offers no DRM leases"));
   check("the client is still connected", !comp.client_gone);

   finish(serv);
}

/* ---- the lease itself ---- */

/* Bring the compositor up without the display server: the lease
 * module opens its own connection, which is the point of it. */
static void lease_comp_start(int nconn, bool grant)
{
   log_count = 0;
   if (!comp_start(nconn, true))
   {
      printf("[FAIL] could not start the test compositor\n");
      exit(1);
   }
   comp.grant = grant;
   pthread_create(&comp_tid, NULL, comp_thread, NULL);
}

static void lease_comp_stop(void)
{
   /* Defensive: a case that failed to release would otherwise leave
    * the module holding a descriptor from a compositor that is about
    * to go away, and the next case would wait on it. */
   wayland_drm_lease_release();
   comp_stop();
   pthread_join(comp_tid, NULL);
   wl_display_destroy(comp.dpy);
   if (comp.fd_write >= 0)
      close(comp.fd_write);
   if (comp.lease_write >= 0)
      close(comp.lease_write);
}

static void test_lease_acquire_and_release(void)
{
   int fd;

   printf("\n-- leasing the first offered connector --\n");
   lease_comp_start(2, true);

   fd = wayland_drm_lease_acquire(0);

   check("a descriptor came back", fd >= 0);
   check("the compositor granted one lease", comp.leases_made == 1);
   check("the first connector was the one asked for",
         !strcmp(comp.requested, "DP-1"));
   check("and it is the one reported",
         wayland_drm_lease_connector() != NULL
         && !strcmp(wayland_drm_lease_connector(), "DP-1"));
   check("the device's non-master fd was closed", client_closed_the_fd());

   wayland_drm_lease_release();

   check("release closed the leased descriptor",
         closed_by_client(comp.lease_write));
   check("and dropped the lease object", comp.leases_gone >= 1);
   check("nothing is reported as leased now",
         wayland_drm_lease_connector() == NULL);
   check("the client is gone, not killed mid-protocol", !fails);

   lease_comp_stop();
}

static void test_lease_monitor_index(void)
{
   int fd;

   printf("\n-- the monitor index picks the head --\n");
   lease_comp_start(2, true);

   fd = wayland_drm_lease_acquire(2);

   check("a descriptor came back", fd >= 0);
   check("the second connector was asked for",
         !strcmp(comp.requested, "VGA-1"));
   check("and it is the one reported",
         wayland_drm_lease_connector() != NULL
         && !strcmp(wayland_drm_lease_connector(), "VGA-1"));

   wayland_drm_lease_release();
   lease_comp_stop();
}

static void test_lease_index_past_the_end(void)
{
   printf("\n-- an index past the offer --\n");
   lease_comp_start(2, true);

   check("no descriptor", wayland_drm_lease_acquire(5) < 0);
   check("and no lease was asked for", comp.leases_made == 0);
   check("nothing is reported as leased",
         wayland_drm_lease_connector() == NULL);

   lease_comp_stop();
}

static void test_lease_refused(void)
{
   printf("\n-- a compositor that refuses --\n");
   lease_comp_start(1, false);

   check("no descriptor", wayland_drm_lease_acquire(0) < 0);
   check("the refusal was said out loud", log_saw("refused a lease"));
   check("nothing is reported as leased",
         wayland_drm_lease_connector() == NULL);

   /* A second attempt on a fresh compositor must still work: the
    * refusal must not have left the module holding anything. */
   lease_comp_stop();
   lease_comp_start(1, true);
   check("a later attempt still succeeds",
         wayland_drm_lease_acquire(0) >= 0);
   wayland_drm_lease_release();
   lease_comp_stop();
}

/* The compositor wants the connector back. A client that only looks
 * when a DRM call has failed still has to see it. */
static void test_lease_revoked(void)
{
   int i;
   bool seen = false;

   printf("\n-- the compositor takes the lease back --\n");
   lease_comp_start(1, true);
   comp.revoke = true;   /* armed on its own thread, once granted */

   check("a descriptor came back", wayland_drm_lease_acquire(0) >= 0);

   /* Each call is non-blocking, so the wait is the test's, not the
    * client's: a caller that only looks when a DRM call failed has
    * to see it without ever blocking on the socket. */
   for (i = 0; i < 200 && !seen; i++)
   {
      seen = wayland_drm_lease_revoked();
      if (!seen)
         usleep(10000);
   }

   check("the revoke is seen without blocking", seen);
   check("and stays seen", wayland_drm_lease_revoked());

   wayland_drm_lease_release();
   lease_comp_stop();
}

static void test_lease_without_a_session(void)
{
   char saved[256];
   const char *cur = getenv("WAYLAND_DISPLAY");

   printf("\n-- no Wayland session at all --\n");
   strlcpy(saved, cur ? cur : "", sizeof(saved));
   unsetenv("WAYLAND_DISPLAY");
   log_count = 0;

   check("no descriptor", wayland_drm_lease_acquire(0) < 0);
   check("and it is not reported as a failure",
         !log_saw("refused") && !log_saw("past the"));

   if (saved[0])
      setenv("WAYLAND_DISPLAY", saved, 1);
}

int main(int argc, char **argv)
{
   int i;

   for (i = 1; i < argc; i++)
      if (!strcmp(argv[i], "--verbose"))
         log_echo = true;

   /* A write to a pipe with no reader must return EPIPE, not kill us */
   signal(SIGPIPE, SIG_IGN);

   /* wl_display_connect(NULL) reads these; a stray XDG_RUNTIME_DIR
    * from the host would send the server's socket elsewhere */
   if (!getenv("XDG_RUNTIME_DIR"))
      setenv("XDG_RUNTIME_DIR", "/tmp", 1);

   test_two_connectors();
   test_global_without_connectors();
   test_no_global();

   test_lease_acquire_and_release();
   test_lease_monitor_index();
   test_lease_index_past_the_end();
   test_lease_refused();
   test_lease_revoked();
   test_lease_without_a_session();

   printf("\n%s\n", fails ? "FAILED" : "all checks passed");
   return fails ? 1 : 0;
}
