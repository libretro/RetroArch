/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (update_installed_scan_test.c).
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

/* Regression test for the scan in task_update_installed_cores_handler
 * (tasks/task_core_updater.c), against the shipping task, list parser,
 * HTTP task and task queue.
 *
 * Update Installed Cores walks the whole buildbot list - hundreds of
 * entries, most of them not installed - and hashes every installed
 * core.  Each list entry and each CRC chunk is a work item of the
 * shared per-frame I/O window (task_nbio_slice_*), so one handler call
 * covers as much of the list as the window allows.  With the
 * unthreaded task queue that is the difference between a scan that
 * finishes in a few dozen frames and one that takes a frame per list
 * entry: the lane asserts the number of task_queue_check() passes
 * stays well under the number of entries.
 *
 * The second property is that a CRC resumed on a later tick does not
 * redo the per-core checks: the lane counts path_is_valid() calls at
 * the link boundary and asserts at most one per list entry plus one
 * per installed core, with cores large enough to span several ticks.
 *
 * Gathers are paced 17 ms apart, as frames are: the window refills
 * once per 16.67 ms period, and pumping faster would only measure the
 * one-item floor. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <queues/task_queue.h>
#include <net/net_http.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <streams/interface_stream.h>
#include <file/file_path.h>
#include <compat/strl.h>

#include "../../../configuration.h"
#include "../../../tasks/tasks_internal.h"

#define NUM_ENTRIES   512
#define NUM_INSTALLED 2
/* Several 4 ms windows' worth of hashing each on any runner */
#define CORE_SIZE     (96 * 1024 * 1024)

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

/* ---------------- path_is_valid() at the link boundary ----------- */

static retro_atomic_int_t n_path_is_valid;

bool __real_path_is_valid(const char *path);
bool __wrap_path_is_valid(const char *path)
{
   retro_atomic_fetch_add_int(&n_path_is_valid, 1);
   return __real_path_is_valid(path);
}

/* ---------------- loopback server: serves .index-extended -------- */

static char *srv_body       = NULL;
static size_t srv_body_len  = 0;
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
            "Connection: close\r\n\r\n", (unsigned)srv_body_len);
      send(cfd, head, strlen(head), 0);
      send(cfd, srv_body, srv_body_len, 0);
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

/* ---------------- fixture ---------------------------------------- */

static char g_dir[] = "/tmp/update_installed_scan_XXXXXX";

/* Installed cores are sparse files: CRC work without disk use. */
static bool make_core(const char *path, uint32_t *crc)
{
   intfstream_t *s;
   FILE *f = fopen(path, "wb");
   bool ok;
   if (!f)
      return false;
   ok = (ftruncate(fileno(f), CORE_SIZE) == 0);
   if (fputc(0x5a, f) == EOF)
      ok = false;
   fclose(f);
   if (!ok)
      return false;
   if (!(s = intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return false;
   ok = intfstream_get_crc(s, crc);
   intfstream_close(s);
   free(s);
   return ok && *crc != 0;
}

static bool build_fixture(void)
{
   size_t cap = NUM_ENTRIES * 96, len = 0;
   int i;

   if (!(srv_body = (char*)malloc(cap)))
      return false;

   for (i = 0; i < NUM_ENTRIES; i++)
   {
      uint32_t crc = 0x1234abcdu;
      char name[64];

      snprintf(name, sizeof(name), "scan%03d_libretro.so", i);

      /* Spread the installed cores through the list */
      if ((i % (NUM_ENTRIES / NUM_INSTALLED)) == NUM_ENTRIES / (2 * NUM_INSTALLED))
      {
         char path[PATH_MAX_LENGTH];
         fill_pathname_join_special(path, g_dir, name, sizeof(path));
         if (!make_core(path, &crc))
            return false;
      }

      len += snprintf(srv_body + len, cap - len,
            "2026-10-01 %08x %s.zip\n", (unsigned)crc, name);
   }
   srv_body_len = len;
   return true;
}

/* ---------------- one lane --------------------------------------- */

static bool find_any(retro_task_t *task, void *user_data)
{
   (void)task;
   (void)user_data;
   return true;
}

static void run_lane(bool threaded)
{
   char url[128];
   char name[128];
   task_finder_data_t find_data;
   settings_t *settings = config_get_ptr();
   long gathers         = 0;
   int stats;

   printf("[lane threaded=%d]\n", (int)threaded);

   task_queue_init(threaded, NULL);

   snprintf(url, sizeof(url), "http://127.0.0.1:%d", srv_port);
   get_list_test_set_buildbot_url(url);
   strlcpy(settings->paths.directory_libretro, g_dir,
         sizeof(settings->paths.directory_libretro));
   strlcpy(settings->paths.path_libretro_info, g_dir,
         sizeof(settings->paths.path_libretro_info));

   retro_atomic_store_release_int(&n_path_is_valid, 0);
   task_push_update_installed_cores(false, 0, g_dir, NULL);

   find_data.func     = find_any;
   find_data.userdata = NULL;
   for (gathers = 0; gathers < 4 * NUM_ENTRIES; gathers++)
   {
      task_queue_check();
      if (!task_queue_find(&find_data))
         break;
      retro_sleep(17);
   }

   stats = retro_atomic_load_acquire_int(&n_path_is_valid);
   printf("  %ld gathers, %d path_is_valid() for %d entries, %d installed\n",
         gathers, stats, NUM_ENTRIES, NUM_INSTALLED);

   CHECK(gathers < 4 * NUM_ENTRIES, "update installed cores completed");
   if (!threaded)
   {
      snprintf(name, sizeof(name),
            "scan finished in fewer than %d gathers", NUM_ENTRIES / 4);
      CHECK(gathers < NUM_ENTRIES / 4, name);
   }
   CHECK(stats <= NUM_ENTRIES + NUM_INSTALLED,
         "at most one stat per entry plus one per installed core");

   task_queue_deinit();
   task_queue_unset_threaded();
}

int main(void)
{
   char cmd[128];

   setvbuf(stdout, NULL, _IOLBF, 0);
   printf("core updater update-installed scan test\n\n");

   if (!mkdtemp(g_dir))
   {
      printf("SKIP: could not create temp dir\n");
      return 0;
   }

   network_init();
   net_http_init();

   if (!build_fixture())
      printf("SKIP: could not create fixture\n");
   else if (!server_start())
      printf("SKIP: could not start loopback server\n");
   else
   {
      run_lane(false);
      run_lane(true);
      server_stop();
   }

   net_http_deinit();
   free(srv_body);
   snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
   if (system(cmd)) { /* best effort */ }

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
