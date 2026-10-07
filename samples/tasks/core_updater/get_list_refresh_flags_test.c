/* Regression test for the menu refresh flags around the core updater
 * list task, against the shipping tasks/task_core_updater.c,
 * tasks/task_http.c, libretro-common/queues/task_queue.c and
 * libretro-common/net/net_http.c.
 *
 * The Core Downloader entry point (action_ok_core_updater_list) sets
 * MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH before pushing the get-list
 * task with refresh_menu=true, and the menu can only rebuild the
 * pending displaylist after the task clears that flag again at
 * retrieval.  If it never does, that entry into the menu never
 * populates and, because MENU_ENTRIES_NEEDS_REFRESH() stays false
 * while the flag is set, every later dirwalk refresh in the session
 * is swallowed with it: one visit to Core Downloader wedges all
 * deeper menu navigation until restart.
 *
 * The test drives the real task end to end against a loopback server
 * serving an .index-extended body and asserts, for both values of
 * refresh_menu, that the corresponding flag is cleared once the task
 * has retired and that the fetched list parsed.
 *
 * The parked threaded lane also pins when the updater may read its
 * HTTP task against the queue freeing that task; see run_lane. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <queues/task_queue.h>
#include <net/net_http.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

#include "../../../core_updater_list.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../menu/menu_driver.h"

void get_list_test_set_buildbot_url(const char *url);

static int checks   = 0;
static int failures = 0;

#define CHECK(cond, name) \
   do \
   { \
      checks++; \
      if (cond) \
         printf("  ok   %s\n", name); \
      else \
      { \
         failures++; \
         printf("  FAIL %s\n", name); \
      } \
   } while (0)

/* ---------------- loopback server: serves .index-extended -------- */

/* The listening socket is read by the accept loop on the server
 * thread and retired by server_stop on the main thread: an atomic,
 * published once before the thread starts and replaced once to stop
 * it. The accept loop reads it fresh each lap so the -1 lands. */
static retro_atomic_int_t srv_fd;
static sthread_t *srv_thread  = NULL;
static volatile int srv_port  = 0;

static void server_thread(void *unused)
{
   static const char body[] =
      "2026-09-15 abcdef01 gambatte_libretro.so.zip\n"
      "2026-09-15 deadbeef mgba_libretro.so.zip\n";
   char head[256];
   char rbuf[4096];
   (void)unused;
   for (;;)
   {
      struct sockaddr_in cli;
      socklen_t clen = sizeof(cli);
      int fd         = retro_atomic_load_acquire_int(&srv_fd);
      int cfd        = fd >= 0
            ? accept(fd, (struct sockaddr*)&cli, &clen) : -1;
      size_t have    = 0;
      if (cfd < 0)
         return;
      /* Read the whole request head before answering; answering
       * after one partial read and closing races the client's next
       * send into EPIPE. */
      while (have < sizeof(rbuf) - 1)
      {
         ssize_t n = recv(cfd, rbuf + have, sizeof(rbuf) - 1 - have, 0);
         if (n <= 0)
            break;
         have += (size_t)n;
         rbuf[have] = '\0';
         if (strstr(rbuf, "\r\n\r\n"))
            break;
      }
      snprintf(head, sizeof(head),
            "HTTP/1.1 200 OK\r\nContent-Length: %u\r\n"
            "Connection: close\r\n\r\n",
            (unsigned)(sizeof(body) - 1));
      send(cfd, head, strlen(head), 0);
      send(cfd, body, sizeof(body) - 1, 0);
      socket_close(cfd);
   }
}

static bool server_start(void)
{
   struct sockaddr_in addr;
   socklen_t alen = sizeof(addr);
   int fd = socket(AF_INET, SOCK_STREAM, 0);
   if (fd < 0)
      return false;
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
      return false;
   if (listen(fd, 4) < 0)
      return false;
   getsockname(fd, (struct sockaddr*)&addr, &alen);
   srv_port   = ntohs(addr.sin_port);
   /* Published before the thread exists; sthread_create orders it. */
   retro_atomic_store_release_int(&srv_fd, fd);
   srv_thread = sthread_create(server_thread, NULL);
   return srv_thread != NULL;
}

static void server_stop(void)
{
   {
      int fd = retro_atomic_load_acquire_int(&srv_fd);
      retro_atomic_store_release_int(&srv_fd, -1);
      if (fd >= 0)
      {
         /* Unblock the accept loop before closing: a close alone does
          * not wake a thread parked in accept() on every libc. */
         shutdown(fd, SHUT_RDWR);
         socket_close(fd);
      }
   }
   if (srv_thread)
   {
      sthread_join(srv_thread);
      srv_thread = NULL;
   }
}

/* ---------------- parking the worker behind the HTTP task -------- */

/* The threaded queue frees a finished task on the main thread, in
 * task_queue_check(), right after running its callback.  The
 * updater's handler runs on the worker.  With one worker the HTTP
 * task and the updater alternate, so the free can only land between
 * the HTTP task's last tick and the updater's next one - a window
 * the test cannot hit by timing.  It is made deterministic instead:
 * task_set_flags() is wrapped at link time (-Wl,--wrap), and when
 * the HTTP task marks itself finished - on the worker, inside its
 * own last tick - the wrapper pushes the updater's 'when' into the
 * future.  The worker only ever looks at the task at the front of
 * the running list, which is now the updater, so it sleeps; the
 * main thread retires and frees the HTTP task meanwhile; then the
 * updater ticks with a pointer to freed memory in hand.  'when' is
 * read by the worker under running_lock and written here on that
 * same thread, so no lock is needed.  An updater that reads the
 * task there is a heap-use-after-free under ASan; one that checks
 * the callback's complete flag first never touches it. */
enum { PARK_OFF = 0, PARK_ARMED, PARK_PARKED };
static retro_atomic_int_t park_state;
static retro_task_t *park_updater = NULL;
#define PARK_USEC (2 * 1000 * 1000)

void __real_task_set_flags(retro_task_t *task, uint8_t flags, bool set);
void __wrap_task_set_flags(retro_task_t *task, uint8_t flags, bool set)
{
   __real_task_set_flags(task, flags, set);

   if (     set
         && (flags & RETRO_TASK_FLG_FINISHED)
         && task != park_updater
         && retro_atomic_load_acquire_int(&park_state) == PARK_ARMED)
   {
      park_updater->when = cpu_features_get_time_usec() + PARK_USEC;
      retro_atomic_store_release_int(&park_state, PARK_PARKED);
   }
}

static bool find_other_than_updater(retro_task_t *task, void *user_data)
{
   return task != (retro_task_t*)user_data;
}

/* ---------------- one lane: push, retire, inspect flags ---------- */

/* park: threaded queue only - hold the updater as described above so
 * the HTTP task is certainly freed before the updater's next tick. */
static void run_lane(bool refresh_menu, bool park)
{
   char url[128];
   core_updater_list_t *list  = core_updater_list_init();
   struct menu_state *menu_st = menu_state_get_ptr();
   void *task;
   int i;

   printf("[lane refresh_menu=%d park=%d]\n", (int)refresh_menu, (int)park);

   /* The caller's side of the contract, as in
    * action_ok_core_updater_list. */
   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH
                   | MENU_ST_FLAG_ENTRIES_NEED_REFRESH;

   snprintf(url, sizeof(url), "http://127.0.0.1:%d", srv_port);
   get_list_test_set_buildbot_url(url);
   task = task_push_get_core_updater_list(list, true, refresh_menu);
   CHECK(task != NULL, "task pushed");

   park_updater = (retro_task_t*)task;
   retro_atomic_store_release_int(&park_state, park ? PARK_ARMED : PARK_OFF);

   /* Pump retrieval on this (the main) thread, as the runloop does
    * once a frame, until the task's retirement callback has cleared
    * the flag under test - that clear IS the contract being checked,
    * and it doubles as the completion signal: on a tree where it
    * never happens the loop runs out and the CHECK below goes red.
    * A few grace pumps follow so cleanup has certainly run too;
    * task_queue_check() orders this thread behind everything the
    * worker published, so the list read below is race-free. */
   {
      unsigned flag = refresh_menu
            ? MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH
            : MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      for (i = 0; i < 1000 && (menu_st->flags & flag); i++)
      {
         if (retro_atomic_load_acquire_int(&park_state) == PARK_PARKED)
         {
            /* The worker is asleep behind the updater's 'when'.
             * Retire the HTTP task now: check until nothing but the
             * updater is findable, which is the point at which the
             * queue has freed it (find() stays truthful until the
             * free).  That must happen before the updater's tick
             * comes due, or the lane proves nothing. */
            task_finder_data_t find_data;
            int j;

            find_data.func     = find_other_than_updater;
            find_data.userdata = task;
            for (j = 0; j < 1000; j++)
            {
               task_queue_check();
               if (!task_queue_find(&find_data))
                  break;
               retro_sleep(1);
            }
            CHECK(j < 1000, "HTTP task retired while the updater was parked");
            CHECK(cpu_features_get_time_usec() < park_updater->when,
                  "HTTP task freed before the updater's next tick");
            retro_atomic_store_release_int(&park_state, PARK_OFF);
         }
         task_queue_check();
         retro_sleep(10);
      }
   }
   for (i = 0; i < 5; i++)
   {
      task_queue_check();
      retro_sleep(10);
   }

   CHECK(core_updater_list_size(list) == 2, "index parsed into list");

   if (refresh_menu)
      CHECK(!(menu_st->flags & MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH),
            "NONBLOCKING_REFRESH cleared at retrieval");
   else
      CHECK(!(menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH),
            "NEED_REFRESH cleared at retrieval");

   /* With the flags back in their resting state the menu's refresh
    * gate works again; while the regression held NONBLOCKING_REFRESH
    * set, this stayed false forever and no later menu could
    * repopulate. */
   if (refresh_menu)
   {
      menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      CHECK(MENU_ENTRIES_NEEDS_REFRESH(menu_st),
            "refresh gate live again after retire");
   }

   retro_atomic_store_release_int(&park_state, PARK_OFF);
   park_updater   = NULL;
   menu_st->flags = 0;
   core_updater_list_free(list);
}

int main(void)
{
   setvbuf(stdout, NULL, _IOLBF, 0);
   printf("core updater get-list menu refresh flags test\n\n");

   network_init();
   net_http_init();

   if (!server_start())
   {
      printf("SKIP: could not start loopback server\n");
      return 0;
   }

   task_queue_init(true, NULL); /* threaded, as in the app */

   run_lane(true, false);
   run_lane(false, false);
   run_lane(true, true);

   task_queue_deinit();

   /* The unthreaded queue runs the HTTP task ahead of the updater
    * every pass and retires it only after the pass, so the updater
    * reads it directly there; run that path under the sanitizers
    * too. */
   task_queue_unset_threaded();
   task_queue_init(false, NULL);

   run_lane(true, false);

   task_queue_deinit();
   server_stop();
   net_http_deinit();

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
