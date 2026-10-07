/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (download_backup_wait_test.c).
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

/* Regression test for the core download task's wait on its automatic
 * backup (CORE_UPDATER_DOWNLOAD_WAIT_BACKUP in tasks/task_core_updater.c),
 * against the shipping task, list parser, HTTP task and task queue.
 * The backup task is the stand-in in stubs_retroarch.c, which keeps
 * the shipping contract: a real task whose callback reaches the
 * pusher at retirement.
 *
 * The threaded queue frees a retired task on the main thread while
 * the download handler runs on the worker, so the handler may not
 * read the backup task to learn that it finished: completion comes
 * from the callback, through a record both sides hold a reference to.
 *
 * Lane 1 parks the download (task_set_flags is wrapped, as in
 * get_list_refresh_flags_test) the moment the backup finishes, retires
 * and frees the backup on this thread, then lets the download tick:
 * a handler that reads the task is a heap-use-after-free under ASan.
 *
 * Lane 2 cancels the download while the backup is still running, so
 * the download's handle is gone when the backup's callback fires: a
 * record owned by the handle alone is written after free.  LeakSan
 * covers the other direction - a reference nobody drops. */

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
#include <file/file_path.h>
#include <compat/strl.h>

#include "../../../configuration.h"
#include "../../../core_updater_list.h"
#include "../../../tasks/tasks_internal.h"

#define CORE_NAME "bk_libretro.so"

void get_list_test_set_buildbot_url(const char *url);
extern retro_atomic_int_t stub_backup_hold;
extern retro_atomic_int_t stub_backup_pushes;
extern retro_task_t *volatile stub_backup_task;

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

/* ---------------- loopback server -------------------------------- */

/* Serves the index for any request; the 'core archive' the download
 * fetches is the same bytes, which is all the transfer needs. */
static const char srv_body[] =
   "2026-10-01 1234abcd " CORE_NAME ".zip\n";
static retro_atomic_int_t srv_fd;
static sthread_t *srv_thread = NULL;
static volatile int srv_port = 0;

static void server_thread(void *unused)
{
   char head[256];
   char rbuf[4096];
   (void)unused;
   for (;;)
   {
      int fd      = retro_atomic_load_acquire_int(&srv_fd);
      int cfd     = fd >= 0 ? accept(fd, NULL, NULL) : -1;
      size_t have = 0;
      if (cfd < 0)
         return;
      while (have < sizeof(rbuf) - 1)
      {
         ssize_t n = recv(cfd, rbuf + have, sizeof(rbuf) - 1 - have, 0);
         if (n <= 0)
            break;
         have      += (size_t)n;
         rbuf[have] = '\0';
         if (strstr(rbuf, "\r\n\r\n"))
            break;
      }
      snprintf(head, sizeof(head),
            "HTTP/1.1 200 OK\r\nContent-Length: %u\r\n"
            "Connection: close\r\n\r\n", (unsigned)(sizeof(srv_body) - 1));
      send(cfd, head, strlen(head), 0);
      send(cfd, srv_body, sizeof(srv_body) - 1, 0);
      socket_close(cfd);
   }
}

static bool server_start(void)
{
   struct sockaddr_in addr;
   socklen_t alen = sizeof(addr);
   int fd         = socket(AF_INET, SOCK_STREAM, 0);
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
   retro_atomic_store_release_int(&srv_fd, fd);
   srv_thread = sthread_create(server_thread, NULL);
   return srv_thread != NULL;
}

static void server_stop(void)
{
   int fd = retro_atomic_load_acquire_int(&srv_fd);
   retro_atomic_store_release_int(&srv_fd, -1);
   if (fd >= 0)
   {
      shutdown(fd, SHUT_RDWR);
      socket_close(fd);
   }
   if (srv_thread)
   {
      sthread_join(srv_thread);
      srv_thread = NULL;
   }
}

/* ---------------- parking the download behind the backup --------- */

/* When the stand-in backup marks itself finished - on the worker,
 * inside its own tick - the wrapper pushes every other unfinished
 * task's 'when' into the future.  The only one is the download, so
 * the worker sleeps while this thread retires and frees the backup,
 * and the download's next tick comes after the free.  'when' is read
 * by the worker under running_lock and written here on that same
 * thread. */
enum { PARK_OFF = 0, PARK_ARMED, PARK_PARKED };
static retro_atomic_int_t park_state;
static retro_time_t park_until = 0;
#define PARK_USEC (2 * 1000 * 1000)

static bool park_other_finder(retro_task_t *task, void *user_data)
{
   if (     task != stub_backup_task
         && !(task_get_flags(task) & RETRO_TASK_FLG_FINISHED))
      task->when = *(retro_time_t*)user_data;
   return false; /* visit every task */
}

void __real_task_set_flags(retro_task_t *task, uint8_t flags, bool set);
void __wrap_task_set_flags(retro_task_t *task, uint8_t flags, bool set)
{
   __real_task_set_flags(task, flags, set);

   if (     set
         && (flags & RETRO_TASK_FLG_FINISHED)
         && task == stub_backup_task
         && retro_atomic_load_acquire_int(&park_state) == PARK_ARMED)
   {
      task_finder_data_t find_data;
      park_until         = cpu_features_get_time_usec() + PARK_USEC;
      find_data.func     = park_other_finder;
      find_data.userdata = &park_until;
      task_queue_find(&find_data);
      retro_atomic_store_release_int(&park_state, PARK_PARKED);
   }
}

/* ---------------- helpers ---------------------------------------- */

static char g_dir[] = "/tmp/download_backup_wait_XXXXXX";

static bool is_task(retro_task_t *task, void *user_data)
{
   return task == (retro_task_t*)user_data;
}

static bool any_task(retro_task_t *task, void *user_data)
{
   (void)task;
   (void)user_data;
   return true;
}

static bool pump_until_idle(int max_ms)
{
   task_finder_data_t find_data;
   int i;
   find_data.func     = any_task;
   find_data.userdata = NULL;
   for (i = 0; i < max_ms; i++)
   {
      task_queue_check();
      if (!task_queue_find(&find_data))
         return true;
      retro_sleep(1);
   }
   return false;
}

static bool write_core(void)
{
   char path[PATH_MAX_LENGTH];
   FILE *f;
   fill_pathname_join_special(path, g_dir, CORE_NAME, sizeof(path));
   if (!(f = fopen(path, "wb")))
      return false;
   fputs("not the buildbot's core", f);
   fclose(f);
   return true;
}

static core_updater_list_t *fetch_list(void)
{
   char url[128];
   settings_t *settings      = config_get_ptr();
   core_updater_list_t *list = core_updater_list_init();

   snprintf(url, sizeof(url), "http://127.0.0.1:%d", srv_port);
   get_list_test_set_buildbot_url(url);
   strlcpy(settings->paths.directory_libretro, g_dir,
         sizeof(settings->paths.directory_libretro));
   strlcpy(settings->paths.path_libretro_info, g_dir,
         sizeof(settings->paths.path_libretro_info));

   if (!list || !task_push_get_core_updater_list(list, true, false))
      return list;
   pump_until_idle(5000);
   return list;
}

/* ---------------- lanes ------------------------------------------ */

static void lane_backup_retired_before_next_tick(void)
{
   task_finder_data_t find_data;
   core_updater_list_t *list;
   void *download;
   int i;

   printf("[lane: backup retired before the download's next tick]\n");

   task_queue_init(true, NULL);
   list = fetch_list();
   CHECK(list && core_updater_list_size(list) == 1, "index parsed into list");

   stub_backup_task = NULL;
   retro_atomic_store_release_int(&stub_backup_hold, 0);
   retro_atomic_store_release_int(&park_state, PARK_ARMED);

   download = task_push_core_updater_download(list, CORE_NAME ".zip",
         0, true, true, 1, g_dir, g_dir);
   CHECK(download != NULL, "download pushed");

   for (i = 0; i < 5000
         && retro_atomic_load_acquire_int(&park_state) != PARK_PARKED; i++)
      retro_sleep(1);
   CHECK(i < 5000, "backup finished and the download was parked");

   /* Retire the backup while the download sleeps: once it is no
    * longer findable the queue has freed it. */
   find_data.func     = is_task;
   find_data.userdata = stub_backup_task;
   for (i = 0; i < 1000; i++)
   {
      task_queue_check();
      if (!task_queue_find(&find_data))
         break;
      retro_sleep(1);
   }
   CHECK(i < 1000, "backup task retired while the download was parked");
   CHECK(cpu_features_get_time_usec() < park_until,
         "backup freed before the download's next tick");

   retro_atomic_store_release_int(&park_state, PARK_OFF);
   CHECK(pump_until_idle(10000), "download completed after the backup");

   task_queue_deinit();
   task_queue_unset_threaded();
   core_updater_list_free(list);
}

static void lane_download_cancelled_during_backup(void)
{
   task_finder_data_t find_data;
   core_updater_list_t *list;
   void *download;
   int i;

   printf("[lane: download cancelled while its backup runs]\n");

   task_queue_init(true, NULL);
   list = fetch_list();
   CHECK(list && core_updater_list_size(list) == 1, "index parsed into list");

   retro_atomic_store_release_int(&stub_backup_pushes, 0);
   retro_atomic_store_release_int(&stub_backup_hold, 1);
   retro_atomic_store_release_int(&park_state, PARK_OFF);

   download = task_push_core_updater_download(list, CORE_NAME ".zip",
         0, true, true, 1, g_dir, g_dir);
   CHECK(download != NULL, "download pushed");

   for (i = 0; i < 5000
         && !retro_atomic_load_acquire_int(&stub_backup_pushes); i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(i < 5000, "backup pushed");

   /* Retire the download with its backup still running */
   task_queue_cancel_task(download);
   find_data.func     = is_task;
   find_data.userdata = download;
   for (i = 0; i < 5000; i++)
   {
      task_queue_check();
      if (!task_queue_find(&find_data))
         break;
      retro_sleep(1);
   }
   CHECK(i < 5000, "cancelled download retired first");

   /* Now let the backup finish; its callback runs after the
    * download's handle is gone. */
   retro_atomic_store_release_int(&stub_backup_hold, 0);
   CHECK(pump_until_idle(5000), "backup completed after the download");

   task_queue_deinit();
   task_queue_unset_threaded();
   core_updater_list_free(list);
}

int main(void)
{
   char cmd[128];

   setvbuf(stdout, NULL, _IOLBF, 0);
   printf("core updater download / backup wait test\n\n");

   if (!mkdtemp(g_dir))
   {
      printf("SKIP: could not create temp dir\n");
      return 0;
   }

   network_init();
   net_http_init();

   if (!write_core())
      printf("SKIP: could not create fixture\n");
   else if (!server_start())
      printf("SKIP: could not start loopback server\n");
   else
   {
      lane_backup_retired_before_next_tick();
      if (!write_core())
         printf("SKIP: could not recreate fixture\n");
      else
         lane_download_cancelled_during_backup();
      server_stop();
   }

   net_http_deinit();
   snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
   if (system(cmd)) { /* best effort */ }

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
