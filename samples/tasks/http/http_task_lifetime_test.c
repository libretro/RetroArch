/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (http_task_lifetime_test.c).
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

/* Regression test for the task->state lifetime contract between
 * tasks/task_http.c and libretro-common/queues/task_queue.c.
 *
 * WHY THIS EXISTS
 *
 * The other HTTP samples drive net_http.c directly.  That covers the
 * protocol engine well and covers the task layer not at all -- and
 * the task layer is where this bug lived:
 *
 *   Thread 8 received signal SIGSEGV
 *   #0  strcmp ()
 *   #1  task_http_finder ()
 *   #2  retro_task_threaded_find ()
 *   #3  task_push_http_transfer_generic_titled ()
 *   #4  task_push_http_download_file ()
 *   #5  download_pl_thumbnail ()
 *
 * task_http_transfer_handler() used to free its http_handle_t as soon
 * as it set RETRO_TASK_FLG_FINISHED.  But a task stays *findable*
 * long after its handler is done: retro_task_threaded_find() scans
 * the running, finished and retiring lists, and task_http_finder()
 * dereferences task->state on every candidate it is handed.  So
 * task->state dangled for the entire window between the last handler
 * tick and full retirement, and any concurrent push racing a
 * finishing download read freed memory.
 *
 * It survived for years because connection_url was an inline
 * char[NAME_MAX_LENGTH]: reading a freed-but-still-mapped array
 * yields stale bytes and strcmp() plods through them harmlessly.
 * Making connection_url a pointer -- so that long URLs stopped
 * defeating the duplicate-download guard -- changed the same read
 * into "load the freed chunk's tombstone and dereference it", which
 * is the SIGSEGV above.  The pointer change did not introduce the
 * defect; it removed the padding that was hiding it.
 *
 * WHAT IS EXERCISED
 *
 * The real tasks/task_http.c and the real
 * libretro-common/queues/task_queue.c, threaded, against a loopback
 * server.  Only msg_hash_to_str() and task_window_progress_cb() are
 * stubbed, since neither participates in lifetime.
 *
 * The shape is the one from the backtrace: a pool of threads pushes
 * GETs for a small set of URLs while the queue is finishing and
 * retiring earlier ones.  Every push runs task_queue_find() ->
 * task_http_finder() over the running, finished and retiring lists,
 * so the pushes and the retirements collide continuously.
 *
 * Run it under AddressSanitizer.  Without ASan a use-after-free on a
 * recently freed chunk usually reads plausible bytes and the test
 * passes; ASan turns it into a hard, immediate report.  ThreadSanitizer
 * matters just as much here, since the race is what makes the window
 * reachable in the first place.
 *
 *   make -f Makefile.task_lifetime check SANITIZER=address
 *   make -f Makefile.task_lifetime check SANITIZER=thread
 *
 * To confirm this test still bites, revert the teardown in
 * task_http.c from task_http_transfer_cleanup() back into
 * task_http_transfer_handler() and rerun under ASan.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/net_http.h>
#include <net/net_compat.h>
#include <queues/task_queue.h>

#include "../../../tasks/tasks_internal.h"
#include "../../../tasks/task_file_transfer.h"

/* Distinct URLs in flight.  Small, so pushes collide on the finder
 * constantly instead of walking past each other. */
#define URL_SLOTS      16
/* Pusher threads, i.e. how many callers race the retirement path. */
#define PUSHER_THREADS 4
/* Pushes attempted per thread. */
#define PUSHES_EACH    400

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

/* task_http.c uses these for task titles and progress reporting only;
 * neither takes part in the lifetime of task->state. */
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
/* Loopback server                                                   */
/* ================================================================= */

#define BODY_BYTES 2048

static int  g_listen_fd = -1;
static int  g_port;
static int  g_server_stop;
static pthread_t g_server_th;

static void *server_thread(void *unused)
{
   (void)unused;
   for (;;)
   {
      int    cs = accept(g_listen_fd, NULL, NULL);
      char   req[2048];
      char   head[128];
      char  *body;
      size_t got = 0;
      int    n;

      if (cs < 0)
         break;
      if (g_server_stop)
      {
         close(cs);
         break;
      }

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

      n = snprintf(head, sizeof(head),
            "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n", BODY_BYTES);
      if ((body = (char*)malloc(BODY_BYTES)))
      {
         memset(body, 'z', BODY_BYTES);
         if (send(cs, head, (size_t)n, MSG_NOSIGNAL) > 0)
            send(cs, body, BODY_BYTES, MSG_NOSIGNAL);
         free(body);
      }
      shutdown(cs, SHUT_RDWR);
      close(cs);
   }
   return NULL;
}

static int server_start(void)
{
   struct sockaddr_in sa;
   socklen_t sl = sizeof(sa);
   int one      = 1;

   if ((g_listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      return 0;
   setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
   memset(&sa, 0, sizeof(sa));
   sa.sin_family      = AF_INET;
   sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   sa.sin_port        = 0;
   if (bind(g_listen_fd, (struct sockaddr*)&sa, sizeof(sa)) < 0)
      return 0;
   if (listen(g_listen_fd, 64) < 0)
      return 0;
   if (getsockname(g_listen_fd, (struct sockaddr*)&sa, &sl) < 0)
      return 0;
   g_port = ntohs(sa.sin_port);
   return pthread_create(&g_server_th, NULL, server_thread, NULL) == 0;
}

static void server_stop(void)
{
   g_server_stop = 1;
   shutdown(g_listen_fd, SHUT_RDWR);
   close(g_listen_fd);
   pthread_join(g_server_th, NULL);
}

/* ================================================================= */
/* Pushers                                                           */
/* ================================================================= */

static pthread_mutex_t g_count_lock = PTHREAD_MUTEX_INITIALIZER;
static int             g_completed;

static void transfer_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   (void)task; (void)user_data; (void)err;

   /* Touch the payload so a botched teardown of the response shows up
    * here rather than silently. */
   if (data && data->data && data->len)
   {
      volatile char c = data->data[0];
      (void)c;
   }

   pthread_mutex_lock(&g_count_lock);
   g_completed++;
   pthread_mutex_unlock(&g_count_lock);
}

static void *pusher_thread(void *arg)
{
   long id = (long)(intptr_t)arg;
   int  i;

   for (i = 0; i < PUSHES_EACH; i++)
   {
      char url[128];
      /* Cycle a small set of URLs: pushes for a URL already in flight
       * take the dedup path, pushes for one that just finished walk
       * the finished and retiring lists -- which is the window that
       * crashed. */
      snprintf(url, sizeof(url), "http://127.0.0.1:%d/obj%ld",
            g_port, (id + i) % URL_SLOTS);

      /* Return value intentionally ignored: NULL just means the URL
       * was already in flight, which is a valid outcome here. */
      task_push_http_transfer(url, true, NULL, transfer_cb, NULL);

      /* A short yield, so in-flight transfers actually complete and
       * retire between pushes.  Without it the duplicate-download
       * guard short-circuits nearly every push and almost nothing
       * ever reaches the retirement path this test is aimed at. */
      usleep(300);
   }
   return NULL;
}

/* ================================================================= */

static void test_push_races_retirement(void)
{
   pthread_t th[PUSHER_THREADS];
   long i;
   int  started = 0;
   int  spins   = 0;

   printf("  %d threads x %d pushes over %d URLs, racing retirement\n",
         PUSHER_THREADS, PUSHES_EACH, URL_SLOTS);

   for (i = 0; i < PUSHER_THREADS; i++)
   {
      if (pthread_create(&th[i], NULL, pusher_thread,
               (void*)(intptr_t)i) != 0)
         break;
      started++;
   }

   CHECK(started == PUSHER_THREADS, "only started %d of %d pushers",
         started, PUSHER_THREADS);

   /* Drive the gather from this thread, which is what retires tasks
    * (callback, prune, cleanup, free) concurrently with the pushes. */
   for (i = 0; i < started; i++)
   {
      while (pthread_tryjoin_np(th[i], NULL) != 0)
      {
         task_queue_check();
         usleep(200);
      }
   }

   /* Let everything still in flight drain. */
   while (spins++ < 20000)
   {
      task_queue_check();
      usleep(500);
      pthread_mutex_lock(&g_count_lock);
      if (g_completed >= URL_SLOTS * 4)
      {
         pthread_mutex_unlock(&g_count_lock);
         break;
      }
      pthread_mutex_unlock(&g_count_lock);
   }

   task_queue_wait(NULL, NULL);
   task_queue_check();

   CHECK(g_completed > 0,
         "no transfer completed at all -- the test did not exercise "
         "the retirement path it claims to");
   printf("    %d transfers completed\n", g_completed);
}

int main(void)
{
   printf("task_http / task_queue state lifetime regression test\n\n");

   network_init();
   net_http_init();

   if (!server_start())
   {
      printf("SKIP: could not start loopback server\n");
      return 0;
   }

   /* Threaded, which is the configuration the crash came from: the
    * worker retires tasks while other threads push. */
   task_queue_init(true, NULL);

   test_push_races_retirement();

   task_queue_deinit();
   server_stop();
   net_http_deinit();

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
