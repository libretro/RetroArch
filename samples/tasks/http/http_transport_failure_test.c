/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (http_transport_failure_test.c).
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

/* What a failed transfer is allowed to hand back, across
 * libretro-common/net/net_http.c and tasks/task_http.c.
 *
 * Both halves were wrong at once, which is what made it hard to see:
 * the accessor published a buffer that looked like a body, and the
 * task layer withheld the error that would have told the caller not
 * to read it.
 *
 * net_http_new() allocates response.data up front as the receive
 * buffer -- 64KiB of plain malloc().  On a transport failure nothing
 * is written into it and response.len stays 0, but the pointer is
 * non-NULL, and net_http_data()'s accept_err returned it.  On a torn
 * body it was worse: the caller got the Content-Length remainder as
 * the length, over bytes that never arrived.  Meanwhile task_http.c
 * gated task_set_error() on RETRO_TASK_FLG_MUTE, which only ever
 * suppressed the on-screen notification, so muted transfers (the
 * common case internally) reported failure to their callbacks as
 * success.
 *
 * Four failure lanes, chosen because they enter net_http_update() at
 * four different points: resolution never succeeds, connect() is
 * refused, the peer closes before the status line, the peer closes
 * mid-body.  Each must leave a negative status and no body or headers
 * under either value of accept_err.
 *
 * The two contrast lanes matter as much.  A 200 must still deliver
 * body, headers and status, and a 404 must still deliver its body
 * under accept_err -- otherwise "return NULL on failure" is
 * satisfiable by an accessor that returns NULL rather more often than
 * that, and the WWW-Authenticate path accept_err exists for breaks
 * silently in cloud sync.  Both pass on the pre-fix tree, which makes
 * them a guard rather than a restatement.
 *
 * The task lanes run the same failure through the real task_http.c
 * and task_queue.c, muted and unmuted: muting must change nothing a
 * caller can observe.
 *
 * getaddrinfo is wrapped at link time rather than relying on a name
 * that does not resolve -- CI runners sit behind resolvers that
 * answer for everything, and a lane that silently starts testing a
 * successful lookup is worse than no lane.
 *
 * To confirm this test still bites, drop the `status < 0` guard from
 * net_http_data() and rerun: the four failure lanes report a non-NULL
 * body.  Restore the `!mute &&` in task_http.c and the muted task
 * lane reports a missing error.
 *
 *   make -f Makefile.transport_fail check SANITIZER=address
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/net_http.h>
#include <net/net_compat.h>
#include <queues/task_queue.h>

#include "../../../tasks/tasks_internal.h"
#include "../../../tasks/task_file_transfer.h"

/* Hostname the wrapped resolver always fails; anything else passes
 * through to the real getaddrinfo. */
#define NX_HOST "host.that.never.resolves.test"

/* Declared length the truncating server advertises but does not
 * deliver. */
#define TRUNC_DECLARED 65536
/* Bytes it actually sends before closing. */
#define TRUNC_SENT     512

/* Body the success and 404 lanes serve. */
#define OK_BODY_BYTES  4096

static int failures;
static int checks;

#define CHECK(cond, ...) \
   do { \
      checks++; \
      if (!(cond)) \
      { \
         printf("    FAIL: "); printf(__VA_ARGS__); printf("\n"); \
         failures++; \
      } \
   } while (0)

/* ================================================================= */
/* Stubs                                                             */
/* ================================================================= */

/* Task titles and progress reporting only; neither participates in
 * error propagation or response ownership. */
const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "msg";
}

void task_window_progress_cb(retro_task_t *task)
{
   (void)task;
}

/* ================================================================= */
/* Wrapped resolver                                                  */
/* ================================================================= */

extern int __real_getaddrinfo(const char *node, const char *service,
      const struct addrinfo *hints, struct addrinfo **res);

int __wrap_getaddrinfo(const char *node, const char *service,
      const struct addrinfo *hints, struct addrinfo **res)
{
   if (node && strcmp(node, NX_HOST) == 0)
      return EAI_NONAME;
   return __real_getaddrinfo(node, service, hints, res);
}

/* ================================================================= */
/* Loopback server                                                   */
/* ================================================================= */

enum srv_mode
{
   /* Close without sending a byte: dies before the status line. */
   SRV_CLOSE_EARLY = 0,
   /* Declare TRUNC_DECLARED bytes, send TRUNC_SENT, close: dies
    * mid-body, with a status line already parsed. */
   SRV_TRUNCATE,
   SRV_OK,
   /* 404 with a body and headers -- the shape accept_err exists for. */
   SRV_NOT_FOUND
};

static int           g_listen_fd = -1;
static int           g_port;
static enum srv_mode g_mode;
static pthread_t     g_server_th;

static void srv_drain_request(int cs)
{
   char   req[2048];
   size_t got = 0;

   while (got < sizeof(req) - 1)
   {
      ssize_t r = read(cs, req + got, sizeof(req) - 1 - got);
      if (r <= 0)
         break;
      got     += (size_t)r;
      req[got] = '\0';
      if (strstr(req, "\r\n\r\n"))
         break;
   }
}

static void srv_send_body(int cs, const char *head, size_t body_len)
{
   char *body = (char*)malloc(body_len);

   if (!body)
      return;
   memset(body, 'z', body_len);
   if (send(cs, head, strlen(head), MSG_NOSIGNAL) > 0)
      send(cs, body, body_len, MSG_NOSIGNAL);
   free(body);
}

static void *server_thread(void *unused)
{
   (void)unused;

   for (;;)
   {
      char head[192];
      int  cs = accept(g_listen_fd, NULL, NULL);

      /* No stop flag: server_stop() shuts the listening socket down,
       * which breaks this loop.  A flag would race the harness. */
      if (cs < 0)
         break;

      srv_drain_request(cs);

      switch (g_mode)
      {
         case SRV_CLOSE_EARLY:
            /* Nothing.  The close below is the whole response. */
            break;
         case SRV_TRUNCATE:
            snprintf(head, sizeof(head),
                  "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n"
                  "Connection: close\r\n\r\n", TRUNC_DECLARED);
            srv_send_body(cs, head, TRUNC_SENT);
            break;
         case SRV_OK:
            snprintf(head, sizeof(head),
                  "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n"
                  "X-Test-Marker: present\r\n"
                  "Connection: close\r\n\r\n", OK_BODY_BYTES);
            srv_send_body(cs, head, OK_BODY_BYTES);
            break;
         case SRV_NOT_FOUND:
            snprintf(head, sizeof(head),
                  "HTTP/1.1 404 Not Found\r\nContent-Length: %d\r\n"
                  "X-Test-Marker: present\r\n"
                  "Connection: close\r\n\r\n", OK_BODY_BYTES);
            srv_send_body(cs, head, OK_BODY_BYTES);
            break;
      }

      shutdown(cs, SHUT_RDWR);
      close(cs);
   }
   return NULL;
}

static int server_start(enum srv_mode mode)
{
   struct sockaddr_in sa;
   socklen_t sl = sizeof(sa);
   int one      = 1;

   g_mode = mode;

   if ((g_listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      return 0;
   setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
   memset(&sa, 0, sizeof(sa));
   sa.sin_family      = AF_INET;
   sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   sa.sin_port        = 0;
   if (bind(g_listen_fd, (struct sockaddr*)&sa, sizeof(sa)) < 0)
      return 0;
   if (listen(g_listen_fd, 16) < 0)
      return 0;
   if (getsockname(g_listen_fd, (struct sockaddr*)&sa, &sl) < 0)
      return 0;
   g_port = ntohs(sa.sin_port);
   return pthread_create(&g_server_th, NULL, server_thread, NULL) == 0;
}

static void server_stop(void)
{
   shutdown(g_listen_fd, SHUT_RDWR);
   close(g_listen_fd);
   pthread_join(g_server_th, NULL);
   g_listen_fd = -1;
}

/* A port nothing listens on, so connect() is refused rather than
 * timing out: bind an ephemeral port, note it, close it. */
static int dead_port(void)
{
   struct sockaddr_in sa;
   socklen_t sl = sizeof(sa);
   int port     = 0;
   int fd       = socket(AF_INET, SOCK_STREAM, 0);

   if (fd < 0)
      return 0;
   memset(&sa, 0, sizeof(sa));
   sa.sin_family      = AF_INET;
   sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   sa.sin_port        = 0;
   if (   bind(fd, (struct sockaddr*)&sa, sizeof(sa)) == 0
       && getsockname(fd, (struct sockaddr*)&sa, &sl) == 0)
      port = ntohs(sa.sin_port);
   close(fd);
   return port;
}

/* ================================================================= */
/* Raw net_http lanes                                                */
/* ================================================================= */

struct raw_result
{
   uint8_t            *strict;
   uint8_t            *accept;
   struct string_list *headers;
   size_t              strict_len;
   size_t              accept_len;
   int                 status;
   int                 err;
   int                 ran;
};

/* Drive one transfer to completion exactly as task_http.c does:
 * net_http_data() strictly, then again with accept_err, then the
 * headers. */
static void raw_transfer(const char *url, struct raw_result *out)
{
   struct http_connection_t *conn;
   struct http_t            *http;
   size_t pos   = 0, tot = 0;
   int    iters = 0;

   memset(out, 0, sizeof(*out));
   /* Poisoned, so an unwritten length is not read as a legitimate 0. */
   out->strict_len = (size_t)-1;
   out->accept_len = (size_t)-1;

   if (!(conn = net_http_connection_new(url, "GET", NULL)))
      return;
   while (net_http_connection_iterate(conn) == 0) ;
   if (!net_http_connection_done(conn))
   {
      net_http_connection_free(conn);
      return;
   }
   if (!(http = net_http_new(conn)))
   {
      net_http_connection_free(conn);
      return;
   }
   net_http_connection_free(conn);

   /* Capped: a transfer that never terminates must report, not hang. */
   while (!net_http_update(http, &pos, &tot) && iters++ < 200000) ;

   out->status  = net_http_status(http);
   out->err     = net_http_error(http) ? 1 : 0;
   out->strict  = net_http_data(http, &out->strict_len, false);
   out->accept  = net_http_data(http, &out->accept_len, true);
   out->headers = net_http_headers_ex(http, true);
   out->ran     = 1;

   net_http_delete(http);
}

static void raw_result_free(struct raw_result *r)
{
   /* Both net_http_data() calls return the same pointer when they
    * return one at all, so this frees at most one buffer. */
   if (r->accept)
      free(r->accept);
   else if (r->strict)
      free(r->strict);
   if (r->headers)
      string_list_free(r->headers);
}

/* The contract every transport failure owes, wherever it died. */
static void assert_no_response(const char *label, struct raw_result *r)
{
   if (!r->ran)
   {
      printf("    FAIL: %s: transfer never started\n", label);
      failures++;
      checks++;
      return;
   }

   CHECK(r->err, "%s: failure not reported by net_http_error()", label);
   CHECK(r->status < 0,
         "%s: status %d, expected negative -- a failed transfer that "
         "leaves a non-negative status defeats the predicate every "
         "caller keys off", label, r->status);
   CHECK(!r->strict, "%s: strict net_http_data() returned a body",
         label);
   CHECK(!r->accept,
         "%s: net_http_data(accept_err=true) returned %p -- this is "
         "net_http_new()'s never-written receive buffer, not a body",
         label, (void*)r->accept);
   CHECK(r->accept_len == 0,
         "%s: accept_err length %lu, expected 0", label,
         (unsigned long)r->accept_len);
   CHECK(!r->headers,
         "%s: net_http_headers_ex(accept_err=true) returned a header "
         "list for a response that never arrived", label);
}

static void test_dns_failure(void)
{
   struct raw_result r;
   char url[256];

   printf("  resolution fails\n");
   snprintf(url, sizeof(url), "http://%s/dorequest.php", NX_HOST);
   raw_transfer(url, &r);
   assert_no_response("dns", &r);
   raw_result_free(&r);
}

static void test_connect_refused(void)
{
   struct raw_result r;
   char url[128];
   int  port = dead_port();

   printf("  connect() refused\n");
   if (!port)
   {
      printf("    SKIP: could not reserve a dead port\n");
      return;
   }
   snprintf(url, sizeof(url), "http://127.0.0.1:%d/x", port);
   raw_transfer(url, &r);
   assert_no_response("refused", &r);
   raw_result_free(&r);
}

static void test_close_before_status(void)
{
   struct raw_result r;
   char url[128];

   printf("  peer closes before the status line\n");
   if (!server_start(SRV_CLOSE_EARLY))
   {
      printf("    SKIP: could not start loopback server\n");
      return;
   }
   snprintf(url, sizeof(url), "http://127.0.0.1:%d/x", g_port);
   raw_transfer(url, &r);
   assert_no_response("close-early", &r);
   raw_result_free(&r);
   server_stop();
}

static void test_close_mid_body(void)
{
   struct raw_result r;
   char url[128];

   printf("  peer closes mid-body\n");
   if (!server_start(SRV_TRUNCATE))
   {
      printf("    SKIP: could not start loopback server\n");
      return;
   }
   snprintf(url, sizeof(url), "http://127.0.0.1:%d/x", g_port);
   raw_transfer(url, &r);
   /* Same contract: response.len is the Content-Length remainder
    * here, not the bytes that landed, so the fragment is unusable. */
   assert_no_response("truncated", &r);
   raw_result_free(&r);
   server_stop();
}

/* ================================================================= */
/* Contrast lanes -- the fix must not over-reach                     */
/* ================================================================= */

static void test_success_still_delivers(void)
{
   struct raw_result r;
   char url[128];

   printf("  200 still delivers body, headers and status\n");
   if (!server_start(SRV_OK))
   {
      printf("    SKIP: could not start loopback server\n");
      return;
   }
   snprintf(url, sizeof(url), "http://127.0.0.1:%d/x", g_port);
   raw_transfer(url, &r);

   CHECK(r.ran, "success: transfer never started");
   if (r.ran)
   {
      CHECK(!r.err, "success: net_http_error() set on a clean 200");
      CHECK(r.status == 200, "success: status %d, expected 200",
            r.status);
      CHECK(r.strict != NULL,
            "success: strict net_http_data() returned no body");
      CHECK(r.strict_len == OK_BODY_BYTES,
            "success: body length %lu, expected %d",
            (unsigned long)r.strict_len, OK_BODY_BYTES);
      CHECK(r.headers != NULL, "success: no headers");
      if (r.strict && r.strict_len == OK_BODY_BYTES)
         CHECK(r.strict[0] == 'z' && r.strict[OK_BODY_BYTES - 1] == 'z',
               "success: body content is not what the server sent");
   }

   raw_result_free(&r);
   server_stop();
}

static void test_http_error_body_still_delivered(void)
{
   struct raw_result r;
   char url[128];

   printf("  404 still delivers its body under accept_err\n");
   if (!server_start(SRV_NOT_FOUND))
   {
      printf("    SKIP: could not start loopback server\n");
      return;
   }
   snprintf(url, sizeof(url), "http://127.0.0.1:%d/x", g_port);
   raw_transfer(url, &r);

   CHECK(r.ran, "404: transfer never started");
   if (r.ran)
   {
      CHECK(r.status == 404, "404: status %d, expected 404", r.status);
      /* Strict withholds it, which is the existing contract. */
      CHECK(!r.strict,
            "404: strict net_http_data() returned a body");
      /* Why task_http.c calls twice: an auth challenge or error page
       * has to reach the callback.  A fix that closed this path would
       * break WWW-Authenticate handling in cloud sync. */
      CHECK(r.accept != NULL,
            "404: accept_err withheld a real error body -- this is the "
            "case accept_err exists for");
      CHECK(r.accept_len == OK_BODY_BYTES,
            "404: accept_err length %lu, expected %d",
            (unsigned long)r.accept_len, OK_BODY_BYTES);
      CHECK(r.headers != NULL,
            "404: accept_err withheld the headers of a real response");
   }

   raw_result_free(&r);
   server_stop();
}

/* ================================================================= */
/* Task-layer lanes                                                  */
/* ================================================================= */

static pthread_mutex_t g_cb_lock = PTHREAD_MUTEX_INITIALIZER;

struct cb_observation
{
   int   fired;
   int   had_error;
   int   error_empty;
   int   data_present;
   int   body_present;
   int   status;
};

static struct cb_observation g_obs;

static void failure_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   (void)task; (void)user_data;

   pthread_mutex_lock(&g_cb_lock);
   g_obs.fired        = 1;
   g_obs.had_error    = err != NULL;
   g_obs.error_empty  = (err != NULL && *err == '\0');
   g_obs.data_present = data != NULL;
   g_obs.body_present = (data && data->data);
   g_obs.status       = data ? data->status : 0;
   pthread_mutex_unlock(&g_cb_lock);
}

static void run_task_lane(const char *label, bool mute)
{
   char url[256];
   int  spins = 0;

   printf("  task layer, %s\n", label);

   pthread_mutex_lock(&g_cb_lock);
   memset(&g_obs, 0, sizeof(g_obs));
   pthread_mutex_unlock(&g_cb_lock);

   snprintf(url, sizeof(url), "http://%s/dorequest.php", NX_HOST);

   if (!task_push_http_transfer(url, mute, NULL, failure_cb, NULL))
   {
      printf("    SKIP: push refused (duplicate still in flight)\n");
      return;
   }

   /* Bounded, so a task that never retires reports rather than hangs. */
   while (spins++ < 20000)
   {
      task_queue_check();
      pthread_mutex_lock(&g_cb_lock);
      if (g_obs.fired)
      {
         pthread_mutex_unlock(&g_cb_lock);
         break;
      }
      pthread_mutex_unlock(&g_cb_lock);
      usleep(200);
   }

   task_queue_wait(NULL, NULL);
   task_queue_check();

   pthread_mutex_lock(&g_cb_lock);

   CHECK(g_obs.fired, "%s: callback never ran", label);
   if (g_obs.fired)
   {
      /* The reported symptom: MUTE suppresses the on-screen
       * notification only, so the callback's error argument must
       * carry the failure whether muted or not. */
      CHECK(g_obs.had_error,
            "%s: callback received err == NULL for a transfer that "
            "never reached a host", label);
      CHECK(!g_obs.error_empty,
            "%s: callback received an empty error string, which every "
            "caller testing (!err || !*err) reads as success", label);
      /* And the body the accessor half of the fix withholds. */
      CHECK(!g_obs.body_present,
            "%s: http_transfer_data_t::data is non-NULL after a "
            "transport failure", label);
      CHECK(g_obs.status < 0,
            "%s: http_transfer_data_t::status is %d, expected negative",
            label, g_obs.status);
   }

   pthread_mutex_unlock(&g_cb_lock);
}

static void test_task_muted(void)
{
   run_task_lane("muted", true);
}

static void test_task_unmuted(void)
{
   run_task_lane("unmuted", false);
}

/* ================================================================= */

int main(void)
{
   printf("net_http / task_http transport-failure contract\n\n");

   network_init();
   net_http_init();

   printf("[raw API: transport failures]\n");
   test_dns_failure();
   test_connect_refused();
   test_close_before_status();
   test_close_mid_body();

   printf("\n[raw API: what must still be delivered]\n");
   test_success_still_delivers();
   test_http_error_body_still_delivered();

   printf("\n[task layer]\n");
   task_queue_init(true, NULL);
   test_task_muted();
   test_task_unmuted();
   task_queue_deinit();

   net_http_deinit();

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
