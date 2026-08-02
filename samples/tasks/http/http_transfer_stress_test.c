/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (http_transfer_stress_test.c).
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

/* Transport-level regression tests for libretro-common/net/net_http.c
 * as driven by tasks/task_http.c.
 *
 * task_http.c is a thin state machine over net_http_update(); it owns
 * no I/O of its own.  Every property that matters to it -- does the
 * body arrive intact, how many task-queue ticks does a transfer cost,
 * does a failure tear down cleanly -- is a property of net_http.c
 * reached through that one call.  So this test drives the real
 * net_http.c against a real loopback server and asserts on what
 * task_http.c would observe.
 *
 * WHAT THIS PINS DOWN
 *
 * 1. Framing correctness.  Content-Length, chunked (across a spread of
 *    chunk sizes), and connection-close-delimited bodies must all
 *    reconstruct byte-for-byte.  The dribble mode re-runs them with the
 *    server writing a few bytes at a time, so every header line, chunk
 *    header and chunk boundary is torn across receive calls.
 *
 * 2. Tick cost.  This is the throughput guard, and it is the reason the
 *    test bothers with sockets at all.
 *
 *    net_http_update() issues exactly one recv() per call --
 *    socket_receive_all_nonblocking() is a single recv() despite the
 *    name.  task_http_iterate_transfer() calls net_http_update() once
 *    per task-queue tick.  Transfer time is therefore
 *
 *        updates_required x tick_period
 *
 *    and updates_required is body_size / (bytes drained per recv),
 *    which is capped by the kernel receive buffer.  The tick period is
 *    ~1ms threaded (the retro_sleep(1) in task_http_iterate_transfer)
 *    and one frame -- 16.7ms at 60Hz -- unthreaded.  A 16 MiB download
 *    over a 64 KiB receive buffer needs ~370 updates, which is ~0.4s
 *    threaded and ~6.2s unthreaded, against ~0.09s for a plain drain
 *    loop.  Nothing is slow here except the number of round trips.
 *
 *    ASSERT_BYTES_PER_UPDATE below is the floor.  It fails today, on
 *    purpose: this test is written to hold once net_http_update()
 *    drains the socket rather than taking a single bite from it.
 *
 * 3. Receive window.  For chunked and EOF-delimited bodies the buffer
 *    grows by doubling and the window handed to recv() is
 *    buflen - pos, so it decays toward zero just before each doubling.
 *    Those near-empty recv() calls are pure round-trip overhead.
 *    ASSERT_MIN_WINDOW is the floor, measured only while enough body
 *    remains that a full-size read was possible.
 *
 * 4. Failure teardown.  Truncated bodies, malformed status lines and
 *    absurd Content-Length values must all end in a clean error with
 *    no leak -- which is what makes this worth running under
 *    ASan/LSan/UBSan.
 *
 * 5. Concurrency.  net_http.c keeps a process-global DNS cache and
 *    connection pool.  The concurrent test drives several transfers at
 *    once so TSan can see the shared-state accesses, including the
 *    lazy `if (!dns_cache_lock) dns_cache_lock = slock_new();` in
 *    net_http_new_socket() -- an unsynchronised first-use
 *    initialisation of the very lock that is supposed to serialise
 *    that cache.
 *
 * DETERMINISM
 *
 * Tick cost depends on the socket receive buffer, which is host-tuned
 * and autoscaled.  The test therefore interposes socket() and pins
 * SO_RCVBUF to TEST_RCVBUF on every stream socket it creates, so the
 * per-update byte yield does not drift with /proc/sys/net tuning.  It
 * also interposes recv() to count calls and record the requested
 * window; the server side uses read(), so only net_socket.c's traffic
 * is recorded.  Both interpositions are plain strong definitions
 * resolved through RTLD_NEXT -- no LD_PRELOAD needed, since net_http.c
 * and net_socket.c are linked into this binary.
 *
 * Build and run via the sample Makefile:
 *   make -f Makefile.stress check
 *   make -f Makefile.stress check SANITIZER=address
 *   make -f Makefile.stress check SANITIZER=thread
 *   make -f Makefile.stress check SANITIZER=undefined
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/net_http.h>
#include <net/net_compat.h>

/* Pinned so per-update yield does not track host tuning. */
#define TEST_RCVBUF              (64 * 1024)

/* Throughput floor.  A transfer must move at least this many bytes per
 * net_http_update() call, averaged over the body.  One recv() per
 * update against a TEST_RCVBUF socket yields well under this. */
#define ASSERT_BYTES_PER_UPDATE  (256 * 1024)

/* Receive-window floor, applied only while at least this much body is
 * still outstanding -- the genuinely final read is allowed to be
 * short. */
#define ASSERT_MIN_WINDOW        4096

/* Tick period used for the throughput measurement: the threaded
 * cadence set by the retro_sleep(1) in task_http_iterate_transfer().
 * Ticks-per-body is cadence-independent once the receive buffer
 * saturates, so the 60Hz unthreaded figure is this same tick count
 * scaled by 16.7ms -- measuring at 1ms just keeps the test fast. */
#define TEST_TICK_US             1000

#define BODY_LARGE               (4 * 1024 * 1024)

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
/* recv()/socket() interposition                                     */
/* ================================================================= */

static ssize_t (*real_recv)(int, void*, size_t, int);
static int     (*real_socket)(int, int, int);

/* Recording is enabled only around the client drive loop, and the
 * server thread uses read() rather than recv(), so no cross-thread
 * contention on these counters arises in the single-transfer tests.
 * The concurrent test leaves recording off for exactly that reason --
 * it is there for TSan to look at net_http.c's globals, not to
 * measure. */
static int    g_record;
static long   g_recv_calls;
static size_t g_min_window;
static size_t g_max_window;
static size_t g_window_at_min_outstanding;
static size_t g_outstanding;   /* body bytes still expected */

static void record_reset(size_t body)
{
   g_recv_calls                = 0;
   g_min_window                = (size_t)-1;
   g_max_window                = 0;
   g_window_at_min_outstanding = 0;
   g_outstanding               = body;
   g_record                    = 1;
}

static void record_stop(void)
{
   g_record = 0;
}

ssize_t recv(int fd, void *buf, size_t len, int flags)
{
   ssize_t r;
   if (!real_recv)
      real_recv = (ssize_t(*)(int, void*, size_t, int))dlsym(RTLD_NEXT, "recv");
   if (g_record)
   {
      g_recv_calls++;
      /* Only judge the window while a full-size read was actually
       * possible; the tail of a transfer legitimately asks for less. */
      if (len > g_max_window)
         g_max_window = len;
      if (g_outstanding > ASSERT_MIN_WINDOW && len < g_min_window)
      {
         g_min_window                = len;
         g_window_at_min_outstanding = g_outstanding;
      }
   }
   r = real_recv(fd, buf, len, flags);
   if (g_record && r > 0)
      g_outstanding = (g_outstanding > (size_t)r) ? g_outstanding - (size_t)r : 0;
   return r;
}

int socket(int domain, int type, int protocol)
{
   int fd;
   if (!real_socket)
      real_socket = (int(*)(int, int, int))dlsym(RTLD_NEXT, "socket");
   fd = real_socket(domain, type, protocol);
   if (fd >= 0 && type == SOCK_STREAM
         && (domain == AF_INET || domain == AF_INET6))
   {
      int sz = TEST_RCVBUF;
      setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));
   }
   return fd;
}

/* ================================================================= */
/* Loopback server                                                   */
/* ================================================================= */

enum framing
{
   FRAME_LEN = 0,   /* Content-Length */
   FRAME_CHUNKED,   /* Transfer-Encoding: chunked */
   FRAME_EOF        /* neither; body ends at close */
};

struct srv_spec
{
   size_t       body;
   size_t       chunk;       /* FRAME_CHUNKED only */
   size_t       dribble;     /* bytes per write; 0 = write greedily */
   enum framing frame;
   int          truncate;    /* close early, mid-body */
   const char  *raw_head;    /* override the status line + headers */
   int          listen_fd;
   int          port;
};

static const unsigned char *g_pattern;
static size_t               g_pattern_len;

/* A non-uniform payload, so a de-chunker that drops or duplicates a
 * span is caught rather than masked by a run of identical bytes. */
static void pattern_init(size_t n)
{
   size_t i;
   unsigned char *p = (unsigned char*)malloc(n);
   unsigned int   s = 0x1234567u;
   if (!p)
      abort();
   for (i = 0; i < n; i++)
   {
      s = s * 1103515245u + 12345u;
      p[i] = (unsigned char)(s >> 16);
   }
   g_pattern     = p;
   g_pattern_len = n;
}

/* Write with optional dribbling, so boundaries land mid-buffer. */
static int srv_write(int fd, const void *buf, size_t len, size_t dribble)
{
   const char *p = (const char*)buf;
   size_t sent   = 0;
   while (sent < len)
   {
      size_t want = len - sent;
      ssize_t w;
      if (dribble && want > dribble)
         want = dribble;
      w = send(fd, p + sent, want, MSG_NOSIGNAL);
      if (w <= 0)
         return 0;
      sent += (size_t)w;
   }
   return 1;
}

static void *server_thread(void *arg)
{
   struct srv_spec *sp = (struct srv_spec*)arg;
   int    cs           = accept(sp->listen_fd, NULL, NULL);
   char   req[4096];
   char   head[512];
   size_t got  = 0;
   size_t sent = 0;
   size_t stop;
   int    n;

   if (cs < 0)
      return NULL;

   /* read(), not recv(), so the interposer records only client I/O. */
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

   if (sp->raw_head)
      n = snprintf(head, sizeof(head), "%s", sp->raw_head);
   else if (sp->frame == FRAME_LEN)
      n = snprintf(head, sizeof(head),
            "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\n\r\n",
            (unsigned long)sp->body);
   else if (sp->frame == FRAME_CHUNKED)
      n = snprintf(head, sizeof(head),
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
   else
      n = snprintf(head, sizeof(head), "HTTP/1.1 200 OK\r\n\r\n");

   if (!srv_write(cs, head, (size_t)n, sp->dribble))
      goto done;

   /* Truncated runs stop a little past halfway -- far enough in that
    * the client has committed to a body, short enough that it cannot
    * be mistaken for a complete one. */
   stop = sp->truncate ? (sp->body / 2 + 7) : sp->body;

   if (sp->frame == FRAME_CHUNKED)
   {
      while (sent < stop)
      {
         char   cl[32];
         size_t c = stop - sent;
         int    cn;
         if (sp->chunk && c > sp->chunk)
            c = sp->chunk;
         cn = snprintf(cl, sizeof(cl), "%lx\r\n", (unsigned long)c);
         if (!srv_write(cs, cl, (size_t)cn, sp->dribble))
            goto done;
         if (!srv_write(cs, g_pattern + sent, c, sp->dribble))
            goto done;
         if (!srv_write(cs, "\r\n", 2, sp->dribble))
            goto done;
         sent += c;
      }
      if (!sp->truncate)
         srv_write(cs, "0\r\n\r\n", 5, sp->dribble);
   }
   else
      srv_write(cs, g_pattern, stop, sp->dribble);

done:
   shutdown(cs, SHUT_RDWR);
   close(cs);
   return NULL;
}

static int srv_start(struct srv_spec *sp, pthread_t *th)
{
   struct sockaddr_in sa;
   socklen_t sl = sizeof(sa);
   int one      = 1;

   sp->listen_fd = real_socket ? real_socket(AF_INET, SOCK_STREAM, 0)
                               : socket(AF_INET, SOCK_STREAM, 0);
   if (sp->listen_fd < 0)
      return 0;
   setsockopt(sp->listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
   memset(&sa, 0, sizeof(sa));
   sa.sin_family      = AF_INET;
   sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   sa.sin_port        = 0;
   if (bind(sp->listen_fd, (struct sockaddr*)&sa, sizeof(sa)) < 0)
      return 0;
   if (listen(sp->listen_fd, 4) < 0)
      return 0;
   if (getsockname(sp->listen_fd, (struct sockaddr*)&sa, &sl) < 0)
      return 0;
   sp->port = ntohs(sa.sin_port);
   return pthread_create(th, NULL, server_thread, sp) == 0;
}

/* ================================================================= */
/* Client driver                                                     */
/* ================================================================= */

struct xfer_result
{
   char  *data;
   size_t len;
   long   updates;        /* every net_http_update() call */
   long   setup_updates;  /* calls before the first body byte landed */
   long   body_updates;   /* calls from the first body byte onward */
   int    status;
   int    err;
};

/* Drive one transfer the way task_http_transfer_handler() does: build
 * the connection, then call net_http_update() once per tick until it
 * reports done. */
static int run_transfer_sink(int port, struct xfer_result *out,
      unsigned tick_us, net_http_sink_t sink, void *sink_data)
{
   char url[128];
   struct http_connection_t *conn;
   struct http_t *h;
   size_t pos = 0, tot = 0;
   /* Generous ceiling; only trips if the state machine wedges. */
   const long tick_cap = 40000000L;

   memset(out, 0, sizeof(*out));

   snprintf(url, sizeof(url), "http://127.0.0.1:%d/payload", port);

   if (!(conn = net_http_connection_new(url, "GET", NULL)))
      return 0;
   if (sink)
      net_http_connection_set_sink(conn, sink, sink_data);
   net_http_connection_iterate(conn);
   if (!net_http_connection_done(conn))
   {
      net_http_connection_free(conn);
      return 0;
   }
   if (!(h = net_http_new(conn)))
   {
      net_http_connection_free(conn);
      return 0;
   }
   net_http_connection_free(conn);

   /* Separate the two phases.  Before the first body byte lands,
    * net_http_update() is polling for DNS resolution (which runs on
    * its own thread and is only observed at tick granularity), then
    * for connect, then sending the request -- none of which is
    * throughput.  Only the calls from the first body byte onward
    * measure how many task-queue ticks the payload costs. */
   while (!net_http_update(h, &pos, &tot))
   {
      out->updates++;
      if (pos > 0)
         out->body_updates++;
      /* Model the real task-queue cadence.  Without this the client
       * polls far faster than the peer can fill the socket, so each
       * recv() takes a sliver and the measurement reports the
       * server's write rate rather than the structural per-tick cap.
       * Sleeping the tick period lets the receive buffer saturate
       * between calls, which is the regime RetroArch actually runs
       * in and the only one where ticks-per-body is stable. */
      if (tick_us)
         usleep(tick_us);
      if (out->updates > tick_cap)
      {
         printf("    (transfer wedged after %ld updates)\n", out->updates);
         break;
      }
   }
   out->setup_updates = out->updates - out->body_updates;

   out->err    = net_http_error(h) ? 1 : 0;
   out->status = net_http_status(h);
   out->data   = (char*)net_http_data(h, &out->len, true);
   net_http_delete(h);
   return 1;
}

static int run_transfer(int port, struct xfer_result *out, unsigned tick_us)
{
   return run_transfer_sink(port, out, tick_us, NULL, NULL);
}

/* ================================================================= */
/* Tests                                                             */
/* ================================================================= */

static const char *frame_name(enum framing f)
{
   if (f == FRAME_LEN)     return "content-length";
   if (f == FRAME_CHUNKED) return "chunked";
   return "eof";
}

/* Body must reconstruct byte-for-byte under every framing, at every
 * chunk size, torn or not. */
static void test_framing(enum framing f, size_t body, size_t chunk,
      size_t dribble)
{
   struct srv_spec sp;
   pthread_t th;
   struct xfer_result r;

   memset(&sp, 0, sizeof(sp));
   sp.body    = body;
   sp.chunk   = chunk;
   sp.frame   = f;
   sp.dribble = dribble;

   printf("  framing=%-14s body=%-8lu chunk=%-6lu dribble=%lu\n",
         frame_name(f), (unsigned long)body, (unsigned long)chunk,
         (unsigned long)dribble);

   if (!srv_start(&sp, &th))
   {
      printf("    SKIP: server start failed\n");
      return;
   }

   record_reset(body);
   if (!run_transfer(sp.port, &r, 0))
   {
      printf("    SKIP: client setup failed\n");
      record_stop();
      pthread_join(th, NULL);
      close(sp.listen_fd);
      return;
   }
   record_stop();

   CHECK(r.len == body, "short body: got %lu of %lu",
         (unsigned long)r.len, (unsigned long)body);
   if (r.data && r.len == body)
      CHECK(memcmp(r.data, g_pattern, body) == 0,
            "body content mismatch");
   CHECK(r.status == 200, "status %d, expected 200", r.status);

   free(r.data);
   pthread_join(th, NULL);
   close(sp.listen_fd);
}

/* The throughput guard.  Reports the achieved bytes-per-update and
 * the smallest receive window offered while the body was still
 * substantially outstanding. */
static void test_tick_cost(enum framing f, size_t chunk)
{
   struct srv_spec sp;
   pthread_t th;
   struct xfer_result r;
   double per_update;
   long   body_ticks;

   memset(&sp, 0, sizeof(sp));
   sp.body  = BODY_LARGE;
   sp.chunk = chunk;
   sp.frame = f;

   printf("  tick cost: framing=%s chunk=%lu\n",
         frame_name(f), (unsigned long)chunk);

   if (!srv_start(&sp, &th))
   {
      printf("    SKIP: server start failed\n");
      return;
   }

   record_reset(sp.body);
   if (!run_transfer(sp.port, &r, TEST_TICK_US))
   {
      printf("    SKIP: client setup failed\n");
      record_stop();
      pthread_join(th, NULL);
      close(sp.listen_fd);
      return;
   }
   record_stop();

   /* A transfer that completes inside a single net_http_update() call
    * exits the drive loop on that same call, so body_updates is 0.
    * That is the best possible result, not a divide-by-zero: charge it
    * one tick. */
   body_ticks = r.body_updates ? r.body_updates : 1;
   per_update = (double)r.len / (double)body_ticks;

   printf("    %lu bytes in %ld body ticks (%.0f B/tick), "
          "%ld setup ticks, %ld recv() calls, min window %lu B "
          "(with %lu B outstanding)\n",
          (unsigned long)r.len, body_ticks, per_update,
          r.setup_updates, g_recv_calls,
          g_min_window == (size_t)-1 ? 0UL : (unsigned long)g_min_window,
          (unsigned long)g_window_at_min_outstanding);

   CHECK(r.len == sp.body, "short body: got %lu of %lu",
         (unsigned long)r.len, (unsigned long)sp.body);

   /* At 60Hz a tick is a frame.  Below this floor a core download is
    * measured in seconds of wall clock spent waiting for ticks. */
   CHECK(per_update >= (double)ASSERT_BYTES_PER_UPDATE,
         "only %.0f bytes per net_http_update() (floor %d) -- "
         "%ld body ticks needed; at 16.7ms/tick unthreaded that is "
         "%.1fs for %luMiB",
         per_update, ASSERT_BYTES_PER_UPDATE, body_ticks,
         body_ticks * 0.0167, (unsigned long)(sp.body >> 20));

   /* DNS resolution runs on its own thread but is only ever observed
    * from net_http_update(), so its latency is quantised to the tick
    * period -- a frame at 60Hz, whatever the resolver actually took. */
   printf("    (setup phase: %ld ticks polling DNS/connect/send)\n",
         r.setup_updates);

   /* Doubling-growth framings decay the window to nothing just before
    * each realloc; those reads are round trips that move almost no
    * data. */
   if (g_min_window != (size_t)-1)
      CHECK(g_min_window >= ASSERT_MIN_WINDOW,
            "receive window collapsed to %lu B while %lu B of body "
            "remained (floor %d)",
            (unsigned long)g_min_window,
            (unsigned long)g_window_at_min_outstanding,
            ASSERT_MIN_WINDOW);

   free(r.data);
   pthread_join(th, NULL);
   close(sp.listen_fd);
}

/* A server that promises N bytes and delivers fewer must surface an
 * error rather than a silently short body. */
static void test_truncated(void)
{
   struct srv_spec sp;
   pthread_t th;
   struct xfer_result r;

   memset(&sp, 0, sizeof(sp));
   sp.body     = 512 * 1024;
   sp.frame    = FRAME_LEN;
   sp.truncate = 1;

   printf("  truncated content-length body\n");

   if (!srv_start(&sp, &th))
   {
      printf("    SKIP: server start failed\n");
      return;
   }
   if (!run_transfer(sp.port, &r, 0))
   {
      printf("    SKIP: client setup failed\n");
      pthread_join(th, NULL);
      close(sp.listen_fd);
      return;
   }

   CHECK(r.err || r.len != sp.body,
         "truncated transfer reported success with a full-length body");

   free(r.data);
   pthread_join(th, NULL);
   close(sp.listen_fd);
}

/* Malformed head lines must terminate the transfer cleanly rather
 * than being parsed into junk state.  Run these under ASan/UBSan --
 * that is where the value is. */
static void test_malformed_head(const char *label, const char *head)
{
   struct srv_spec sp;
   pthread_t th;
   struct xfer_result r;

   memset(&sp, 0, sizeof(sp));
   sp.body     = 4096;
   sp.frame    = FRAME_EOF;
   sp.raw_head = head;

   printf("  malformed head: %s\n", label);

   if (!srv_start(&sp, &th))
   {
      printf("    SKIP: server start failed\n");
      return;
   }
   if (!run_transfer(sp.port, &r, 0))
   {
      printf("    SKIP: client setup failed\n");
      pthread_join(th, NULL);
      close(sp.listen_fd);
      return;
   }

   CHECK(r.err || r.status <= 0 || r.len == 0,
         "malformed head accepted: status=%d len=%lu",
         r.status, (unsigned long)r.len);

   free(r.data);
   pthread_join(th, NULL);
   close(sp.listen_fd);
}

/* Several transfers in flight at once, so TSan can watch the
 * process-global DNS cache and connection pool -- including the
 * unsynchronised lazy creation of the locks meant to protect them. */
#define CONCURRENT_N 6

struct conc_arg
{
   int    port;
   size_t body;
   int    ok;
};

static void *conc_thread(void *a)
{
   struct conc_arg *ca = (struct conc_arg*)a;
   struct xfer_result r;
   if (run_transfer(ca->port, &r, 0))
   {
      ca->ok = (r.len == ca->body
            && r.data
            && memcmp(r.data, g_pattern, ca->body) == 0);
      free(r.data);
   }
   return NULL;
}

static void test_concurrent(void)
{
   struct srv_spec  sp[CONCURRENT_N];
   pthread_t        srv[CONCURRENT_N];
   pthread_t        cli[CONCURRENT_N];
   struct conc_arg  ca[CONCURRENT_N];
   size_t body = 256 * 1024;
   int i, started = 0;

   printf("  %d concurrent transfers\n", CONCURRENT_N);

   for (i = 0; i < CONCURRENT_N; i++)
   {
      memset(&sp[i], 0, sizeof(sp[i]));
      sp[i].body  = body;
      sp[i].frame = FRAME_LEN;
      if (!srv_start(&sp[i], &srv[i]))
         break;
      ca[i].port = sp[i].port;
      ca[i].body = body;
      ca[i].ok   = 0;
      if (pthread_create(&cli[i], NULL, conc_thread, &ca[i]) != 0)
         break;
      started++;
   }

   for (i = 0; i < started; i++)
   {
      pthread_join(cli[i], NULL);
      pthread_join(srv[i], NULL);
      close(sp[i].listen_fd);
   }

   CHECK(started == CONCURRENT_N,
         "only started %d of %d concurrent transfers", started,
         CONCURRENT_N);
   for (i = 0; i < started; i++)
      CHECK(ca[i].ok, "concurrent transfer %d did not reconstruct", i);
}


/* ------------------------------------------------------------------ */
/* Streaming sink                                                      */
/* ------------------------------------------------------------------ */

/* Peak buffer ceiling for a streamed transfer.  Without a sink,
 * T_LEN sizes the buffer to Content-Length, so the receive window
 * grows to the whole body; with one it must stay near the receive
 * window.  Asserted against the largest window ever handed to
 * recv(), which tracks buflen. */
#define ASSERT_SINK_MAX_WINDOW (1024 * 1024)

struct sink_ctx
{
   unsigned char *buf;
   size_t         len;
   size_t         cap;
   int            fail_after;  /* -1 = never fail */
   int            calls;
};

static bool test_sink_cb(void *userdata, const void *data, size_t len)
{
   struct sink_ctx *sc = (struct sink_ctx*)userdata;

   sc->calls++;
   if (sc->fail_after >= 0 && sc->calls > sc->fail_after)
      return false;

   if (sc->len + len > sc->cap)
   {
      size_t ncap = sc->cap ? sc->cap : 4096;
      unsigned char *tmp;
      while (ncap < sc->len + len)
         ncap *= 2;
      if (!(tmp = (unsigned char*)realloc(sc->buf, ncap)))
         return false;
      sc->buf = tmp;
      sc->cap = ncap;
   }
   memcpy(sc->buf + sc->len, data, len);
   sc->len += len;
   return true;
}

/* The body must arrive at the sink byte-for-byte and in order, the
 * handle must retain nothing, and peak memory must stay near the
 * receive window rather than tracking the payload. */
static void test_sink(enum framing f, size_t body, size_t chunk,
      size_t dribble)
{
   struct srv_spec sp;
   pthread_t th;
   struct xfer_result r;
   struct sink_ctx sc;

   memset(&sp, 0, sizeof(sp));
   memset(&sc, 0, sizeof(sc));
   sc.fail_after = -1;
   sp.body    = body;
   sp.chunk   = chunk;
   sp.frame   = f;
   sp.dribble = dribble;

   printf("  sink: framing=%-14s body=%-8lu chunk=%-6lu dribble=%lu\n",
         frame_name(f), (unsigned long)body, (unsigned long)chunk,
         (unsigned long)dribble);

   if (!srv_start(&sp, &th))
   {
      printf("    SKIP: server start failed\n");
      return;
   }

   record_reset(body);
   if (!run_transfer_sink(sp.port, &r, 0, test_sink_cb, &sc))
   {
      printf("    SKIP: client setup failed\n");
      record_stop();
      pthread_join(th, NULL);
      close(sp.listen_fd);
      return;
   }
   record_stop();

   CHECK(sc.len == body, "sink got %lu of %lu bytes",
         (unsigned long)sc.len, (unsigned long)body);
   if (sc.len == body)
      CHECK(memcmp(sc.buf, g_pattern, body) == 0,
            "sink content mismatch (out of order or corrupted)");
   CHECK(r.data == NULL && r.len == 0,
         "handle retained %lu bytes despite a sink being set",
         (unsigned long)r.len);
   CHECK(r.status == 200, "status %d, expected 200", r.status);
   CHECK(g_max_window <= ASSERT_SINK_MAX_WINDOW,
         "peak receive window %lu B exceeds %d B -- the body is still "
         "being buffered",
         (unsigned long)g_max_window, ASSERT_SINK_MAX_WINDOW);

   free(sc.buf);
   free(r.data);
   pthread_join(th, NULL);
   close(sp.listen_fd);
}

/* Without a sink, T_LEN sizes the buffer to Content-Length.  Pinning
 * that here is what makes the ceiling above meaningful: it shows the
 * two paths genuinely differ rather than both happening to stay
 * small on this host. */
static void test_no_sink_buffers_whole_body(void)
{
   struct srv_spec sp;
   pthread_t th;
   struct xfer_result r;

   memset(&sp, 0, sizeof(sp));
   sp.body  = BODY_LARGE;
   sp.frame = FRAME_LEN;

   printf("  no sink: whole body is buffered (contrast case)\n");

   if (!srv_start(&sp, &th))
   {
      printf("    SKIP: server start failed\n");
      return;
   }
   record_reset(sp.body);
   if (!run_transfer(sp.port, &r, 0))
   {
      printf("    SKIP: client setup failed\n");
      record_stop();
      pthread_join(th, NULL);
      close(sp.listen_fd);
      return;
   }
   record_stop();

   printf("    peak receive window %lu B for a %lu B body\n",
         (unsigned long)g_max_window, (unsigned long)sp.body);
   CHECK(r.len == sp.body, "short body: got %lu of %lu",
         (unsigned long)r.len, (unsigned long)sp.body);
   CHECK(g_max_window > ASSERT_SINK_MAX_WINDOW,
         "buffered path peaked at only %lu B; the sink ceiling is not "
         "actually discriminating",
         (unsigned long)g_max_window);

   free(r.data);
   pthread_join(th, NULL);
   close(sp.listen_fd);
}

/* A sink that refuses a write -- full disk, I/O error -- must abort
 * the transfer with an error rather than silently truncating. */
static void test_sink_write_failure(void)
{
   struct srv_spec sp;
   pthread_t th;
   struct xfer_result r;
   struct sink_ctx sc;

   memset(&sp, 0, sizeof(sp));
   memset(&sc, 0, sizeof(sc));
   sc.fail_after = 1;
   sp.body  = BODY_LARGE;
   sp.frame = FRAME_LEN;

   printf("  sink: write failure aborts the transfer\n");

   if (!srv_start(&sp, &th))
   {
      printf("    SKIP: server start failed\n");
      return;
   }
   if (!run_transfer_sink(sp.port, &r, 0, test_sink_cb, &sc))
   {
      printf("    SKIP: client setup failed\n");
      pthread_join(th, NULL);
      close(sp.listen_fd);
      return;
   }

   CHECK(r.err, "sink write failure did not mark the transfer as failed");
   CHECK(sc.len < sp.body, "sink received the whole body despite failing");

   free(sc.buf);
   free(r.data);
   pthread_join(th, NULL);
   close(sp.listen_fd);
}

/* net_http_deinit() must close pooled connections and free the DNS
 * cache and the locks.  Nothing did this before -- pooled sockets and
 * their SSL contexts, cached addrinfo, strdup'd domains and both
 * mutexes lived until the process exited, which LeakSan could not
 * see because the lists hang off file-scope globals and so count as
 * still reachable.  Calling deinit here is what makes any real leak
 * in that state visible.
 *
 * Also covers re-init after deinit, since the locks are freed and
 * nulled and a second init has to recreate them. */
static void test_deinit_releases_globals(void)
{
   struct srv_spec sp;
   pthread_t th;
   struct xfer_result r;

   printf("  deinit, then re-init and transfer again\n");

   net_http_deinit();
   net_http_init();

   memset(&sp, 0, sizeof(sp));
   sp.body  = 64 * 1024;
   sp.frame = FRAME_LEN;

   if (!srv_start(&sp, &th))
   {
      printf("    SKIP: server start failed\n");
      return;
   }
   if (!run_transfer(sp.port, &r, 0))
   {
      printf("    SKIP: client setup failed\n");
      pthread_join(th, NULL);
      close(sp.listen_fd);
      return;
   }

   CHECK(r.len == sp.body, "transfer after re-init got %lu of %lu bytes",
         (unsigned long)r.len, (unsigned long)sp.body);

   free(r.data);
   pthread_join(th, NULL);
   close(sp.listen_fd);

   /* Final teardown; anything still held now is a genuine leak. */
   net_http_deinit();
}

/* ================================================================= */

int main(void)
{
   size_t chunk_sizes[] = { 1, 3, 511, 1024, 8192, 65536 };
   size_t i;

   real_recv   = (ssize_t(*)(int, void*, size_t, int))dlsym(RTLD_NEXT, "recv");
   real_socket = (int(*)(int, int, int))dlsym(RTLD_NEXT, "socket");
   if (!real_recv || !real_socket)
   {
      printf("SKIP: cannot resolve libc recv/socket via RTLD_NEXT\n");
      return 0;
   }

   network_init();
   /* Explicit init, so the DNS cache and connection pool locks exist
    * before any thread can reach net_http_update().  They used to be
    * created lazily on first use, unsynchronised. */
   net_http_init();
   pattern_init(BODY_LARGE);

   printf("net_http transfer stress tests "
          "(SO_RCVBUF pinned to %d B)\n\n", TEST_RCVBUF);

   printf("[framing]\n");
   test_framing(FRAME_LEN,     256 * 1024, 0, 0);
   test_framing(FRAME_EOF,     256 * 1024, 0, 0);
   for (i = 0; i < sizeof(chunk_sizes) / sizeof(chunk_sizes[0]); i++)
      test_framing(FRAME_CHUNKED, 256 * 1024, chunk_sizes[i], 0);

   printf("\n[framing, torn across reads]\n");
   test_framing(FRAME_LEN,     32 * 1024, 0,   7);
   test_framing(FRAME_EOF,     32 * 1024, 0,   3);
   test_framing(FRAME_CHUNKED, 32 * 1024, 1,   1);
   test_framing(FRAME_CHUNKED, 32 * 1024, 511, 5);

   printf("\n[tick cost]\n");
   test_tick_cost(FRAME_LEN,     0);
   test_tick_cost(FRAME_CHUNKED, 8192);
   test_tick_cost(FRAME_EOF,     0);

   printf("\n[failure teardown]\n");
   test_truncated();
   test_malformed_head("not HTTP at all",
         "GARBAGE\r\n\r\n");
   test_malformed_head("non-digit status",
         "HTTP/1.1 2X0 OK\r\n\r\n");
   test_malformed_head("absurd content-length",
         "HTTP/1.1 200 OK\r\nContent-Length: 99999999999999999999\r\n\r\n");
   test_malformed_head("content-length and chunked together",
         "HTTP/1.1 200 OK\r\nContent-Length: 4096\r\n"
         "Transfer-Encoding: chunked\r\n\r\n");

   printf("\n[streaming sink]\n");
   test_sink(FRAME_LEN,     BODY_LARGE,  0,    0);
   test_sink(FRAME_EOF,     BODY_LARGE,  0,    0);
   test_sink(FRAME_CHUNKED, BODY_LARGE,  8192, 0);
   test_sink(FRAME_CHUNKED, 64 * 1024,   1,    1);
   test_sink(FRAME_CHUNKED, 64 * 1024,   511,  5);
   test_sink(FRAME_LEN,     32 * 1024,   0,    7);
   test_no_sink_buffers_whole_body();
   test_sink_write_failure();

   printf("\n[concurrency]\n");
   test_concurrent();

   printf("\n[teardown]\n");
   test_deinit_releases_globals();

   free((void*)g_pattern);

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
