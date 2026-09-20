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
 *    released, and the client is still connected afterwards.
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

/* ---- the log, captured ---- */

#define LOG_MAX  128
#define LOG_LINE 512

static char log_lines[LOG_MAX][LOG_LINE];
static int  log_count;
static bool log_echo;

static void log_put(const char *fmt, va_list ap)
{
   char line[LOG_LINE];
   vsnprintf(line, sizeof(line), fmt, ap);
   if (log_echo)
      fputs(line, stdout);
   if (log_count < LOG_MAX)
      strlcpy(log_lines[log_count++], line, LOG_LINE);
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
   for (i = 0; i < log_count; i++)
      if (strstr(log_lines[i], needle))
         return true;
   return false;
}

/* ---- the compositor ---- */

struct comp
{
   struct wl_display   *dpy;
   struct wl_listener   client_destroyed;
   const char          *socket;
   int                  fd_write;   /* our end of the pipe sent as drm_fd */
   int                  nconn;
   bool                 released;
   bool                 client_gone;
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

static void device_handle_create_lease_request(struct wl_client *client,
      struct wl_resource *resource, uint32_t id)
{
   /* Discovery never reaches here; a lease is not taken */
   wl_client_post_implementation_error(client,
         "the test compositor grants no leases");
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
   comp.client_gone = true;
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
      wl_resource_set_implementation(c, &connector_impl, NULL, NULL);
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
   comp.nconn    = nconn;
   comp.fd_write = -1;

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

/* The client's copy of the drm_fd is gone when a write to the other
 * end raises EPIPE, which a pipe does once every read end is closed. */
static bool client_closed_the_fd(void)
{
   char c = 'x';
   ssize_t n;

   if (comp.fd_write < 0)
      return false;
   n = write(comp.fd_write, &c, 1);
   return (n < 0 && errno == EPIPE);
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
   check("the client is still connected", !comp.client_gone);

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
   check("the device was released", comp.released);
   check("the client is still connected", !comp.client_gone);

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

   printf("\n%s\n", fails ? "FAILED" : "all checks passed");
   return fails ? 1 : 0;
}
