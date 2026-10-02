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
 * one-item floor.
 *
 * The cancel lanes retire the parent task while a child it waits on
 * is still pending - the loopback server holds the child's HTTP
 * response - and only then let the child finish:
 *
 *   - reset in WAIT_LIST: task_queue_reset(), as the exit path's
 *     retroarch_drain_tasks_for_exit() does, with the list fetch in
 *     flight;
 *   - cancel in WAIT_LIST: the parent alone, so the list task goes on
 *     to publish into the core list the parent was waiting for;
 *   - cancel in WAIT_DOWNLOAD: the parent alone, with a core download
 *     in flight.
 *
 * A child callback or list parse that reaches the parent's freed
 * handle is a heap-use-after-free under ASan; LeakSan covers the
 * other direction - a completion record or list nobody frees.
 *
 * The get-list lane covers the fetch that precedes the scan.  The
 * listing is parsed one line per work item of the same window into a
 * list private to the task, and published into the caller's list by
 * the task's callback on the main thread.  With the window forced to
 * its one-item floor (task_nbio_slice_within_budget is wrapped) the
 * parse must take a gather per line, and the caller's list - which
 * the Core Downloader's menu callbacks read every frame - must read
 * either its old contents or the complete new ones after every
 * gather, never an emptied or half-built list.
 *
 * Update Installed Cores fetches the listing for its installed cores
 * alone: the parse leaves out every core that is not installed before
 * reading its info file.  The scan lanes count core info reads at the
 * link boundary and assert one per installed core.  The status lanes
 * pin what that must not change: a fetch that finds no installed
 * cores still ends in 'all cores updated', and a fetch that fails
 * still ends in 'core list failed'.
 *
 * The refused-download lane covers a core download whose transfer
 * cannot be started (task_push_http_download_file() is wrapped to
 * refuse it): the download must fail, and Update Installed Cores
 * finish, rather than wait for an extraction that never starts.
 *
 * The CRC cache lane covers the hash itself.  Each installed core's
 * CRC is cached against the size and mtime the libretro directory walk
 * reports, so a repeat run reads no unchanged core.  Bytes hashed are
 * counted at intfstream_crc_step() and core downloads refused as
 * above: a first run hashes every core, a repeat hashes none, a
 * touched core is hashed again, a core whose contents changed is
 * hashed again *and* downloaded - the cache must never hide an
 * update - and a corrupt cache falls back to hashing everything. */

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
#include <sys/stat.h>
#include <utime.h>

#include "../../../configuration.h"
#include "../../../core_updater_list.h"
#include "../../../msg_hash.h"
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

/* ---------------- core info reads and final status --------------- */

static retro_atomic_int_t n_info_reads;
/* 0 none yet, 1 all cores updated, 2 core list failed */
static retro_atomic_int_t final_status;

core_updater_info_t *__real_core_info_get_core_updater_info(
      const char *info_path);
core_updater_info_t *__wrap_core_info_get_core_updater_info(
      const char *info_path)
{
   retro_atomic_fetch_add_int(&n_info_reads, 1);
   return __real_core_info_get_core_updater_info(info_path);
}

const char *__real_msg_hash_to_str(enum msg_hash_enums msg);
const char *__wrap_msg_hash_to_str(enum msg_hash_enums msg)
{
   if (msg == MSG_ALL_CORES_UPDATED)
      return "<all cores updated>";
   if (msg == MSG_CORE_LIST_FAILED)
      return "<core list failed>";
   return __real_msg_hash_to_str(msg);
}

void __real_task_set_title(retro_task_t *task, char *title);
void __wrap_task_set_title(retro_task_t *task, char *title)
{
   if (title && !strncmp(title, "<all cores updated>", 19))
      retro_atomic_store_release_int(&final_status, 1);
   else if (title && !strcmp(title, "<core list failed>"))
      retro_atomic_store_release_int(&final_status, 2);
   __real_task_set_title(task, title);
}

/* ---------------- bytes hashed, core downloads ------------------- */


static retro_atomic_int_t hashed_mb;     /* in units of 64 KiB */
static retro_atomic_int_t core_downloads;
/* Set for the lanes that only need the download decision */
static retro_atomic_int_t refuse_downloads;

void *__real_task_push_http_download_file(const char *url, const char *path,
      bool mute, const char *title, retro_task_callback_t cb, void *user_data);
void *__wrap_task_push_http_download_file(const char *url, const char *path,
      bool mute, const char *title, retro_task_callback_t cb, void *user_data)
{
   /* Counted and refused where a lane needs the decision, not the
    * core; the cancel lanes need the real transfer */
   if (!retro_atomic_load_acquire_int(&refuse_downloads))
      return __real_task_push_http_download_file(url, path, mute, title,
            cb, user_data);
   retro_atomic_fetch_add_int(&core_downloads, 1);
   return NULL;
}

int64_t __real_intfstream_crc_step(intfstream_t *s, uint32_t *acc, size_t c);
int64_t __wrap_intfstream_crc_step(intfstream_t *s, uint32_t *acc, size_t c)
{
   int64_t r = __real_intfstream_crc_step(s, acc, c);
   if (r > 0)
      retro_atomic_fetch_add_int(&hashed_mb, (int)(r >> 16));
   return r;
}

/* ---------------- the I/O window, forced to its floor ------------ */

static retro_atomic_int_t force_floor;

bool __real_task_nbio_slice_within_budget(void *ud, size_t avail, size_t len);
bool __wrap_task_nbio_slice_within_budget(void *ud, size_t avail, size_t len)
{
   bool floor = ((nbio_budget_t*)ud)->floor != 0;
   bool ret   = __real_task_nbio_slice_within_budget(ud, avail, len);
   if (retro_atomic_load_acquire_int(&force_floor))
      return floor;
   return ret;
}

/* ---------------- loopback server: serves .index-extended -------- */

static char *srv_body       = NULL;
static size_t srv_body_len  = 0;
/* The cancel lanes' index: one installed core whose CRC does not
 * match, so the scan pushes a download.  The download fetches the
 * same bytes, which is all the transfer needs. */
#define CANCEL_CORE "cancel_libretro.so"
static const char srv_small_body[] =
   "2026-10-01 1234abcd " CANCEL_CORE ".zip\n";
static retro_atomic_int_t srv_use_small;
/* The request numbered srv_hold_at (from 1) is answered only once
 * srv_hold clears; 0 holds nothing. */
static retro_atomic_int_t srv_requests;
static retro_atomic_int_t srv_answered;
static retro_atomic_int_t srv_hold_at;
static retro_atomic_int_t srv_hold;
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
      if (     retro_atomic_fetch_add_int(&srv_requests, 1) + 1
            == retro_atomic_load_acquire_int(&srv_hold_at))
         while (     retro_atomic_load_acquire_int(&srv_hold)
                  && retro_atomic_load_acquire_int(&srv_fd) >= 0)
            retro_sleep(1);
      {
         const char *body = srv_body;
         size_t body_len  = srv_body_len;
         if (retro_atomic_load_acquire_int(&srv_use_small))
         {
            body     = srv_small_body;
            body_len = sizeof(srv_small_body) - 1;
         }
         snprintf(head, sizeof(head),
               "HTTP/1.1 200 OK\r\nContent-Length: %u\r\n"
               "Connection: close\r\n\r\n", (unsigned)body_len);
         /* A cancelled transfer has closed its end by now */
         send(cfd, head, strlen(head), MSG_NOSIGNAL);
         send(cfd, body, body_len, MSG_NOSIGNAL);
      }
      socket_close(cfd);
      retro_atomic_fetch_add_int(&srv_answered, 1);
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
/* The first installed core, which the refused-download and CRC
 * cache lanes edit */
static char g_core_a[PATH_MAX_LENGTH];

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
         if (!*g_core_a)
            strlcpy(g_core_a, path, sizeof(g_core_a));
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
   retro_atomic_store_release_int(&n_info_reads, 0);
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
   printf("  %d core info reads\n",
         retro_atomic_load_acquire_int(&n_info_reads));
   CHECK(retro_atomic_load_acquire_int(&n_info_reads) == NUM_INSTALLED,
         "core info read for the installed cores alone");

   task_queue_deinit();
   task_queue_unset_threaded();
}

/* ---------------- refused-download lane --------------------------- */

/* A same-size edit within the second the CRC cache last saw is, by
 * design, invisible to it; the lanes move the mtime on explicitly */
static void set_mtime(const char *path, time_t t)
{
   struct utimbuf ut;
   ut.actime  = t;
   ut.modtime = t;
   utime(path, &ut);
}


static void run_refused_download_lane(void)
{
   char url[128];
   task_finder_data_t find_data;
   settings_t *settings = config_get_ptr();
   long gathers;
   FILE *f;

   printf("[lane: a core download that cannot start]\n");

   /* Same size, new contents: the CRC no longer matches */
   if (!(f = fopen(g_core_a, "r+b")))
   {
      printf("  SKIP: could not open the fixture core\n");
      return;
   }
   fputc(0xa5, f);
   fclose(f);
   {
      struct stat st;
      if (!stat(g_core_a, &st))
         set_mtime(g_core_a, st.st_mtime + 5);
   }

   task_queue_init(false, NULL);
   snprintf(url, sizeof(url), "http://127.0.0.1:%d", srv_port);
   get_list_test_set_buildbot_url(url);
   strlcpy(settings->paths.directory_libretro, g_dir,
         sizeof(settings->paths.directory_libretro));
   strlcpy(settings->paths.path_libretro_info, g_dir,
         sizeof(settings->paths.path_libretro_info));

   retro_atomic_store_release_int(&core_downloads, 0);
   retro_atomic_store_release_int(&refuse_downloads, 1);
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
   retro_atomic_store_release_int(&refuse_downloads, 0);

   printf("  %ld gathers, %d core download(s) requested\n", gathers,
         retro_atomic_load_acquire_int(&core_downloads));
   CHECK(retro_atomic_load_acquire_int(&core_downloads) == 1,
         "the changed core's download was requested");
   CHECK(gathers < 4 * NUM_ENTRIES,
         "update installed cores completed with the download refused");

   task_queue_deinit();
   task_queue_unset_threaded();

   /* Back to the listed contents (make_core's first byte) */
   if ((f = fopen(g_core_a, "r+b")))
   {
      fputc(0x5a, f);
      fclose(f);
   }
}

/* ---------------- CRC cache lane --------------------------------- */

static char g_cache_path[PATH_MAX_LENGTH];

/* One unthreaded Update Installed Cores run; reports what it hashed,
 * in 64 KiB units, and the core downloads it asked for */
static void cache_run(int *hashed, int *downloads)
{
   char url[128];
   task_finder_data_t find_data;
   settings_t *settings = config_get_ptr();
   long gathers;

   task_queue_init(false, NULL);
   snprintf(url, sizeof(url), "http://127.0.0.1:%d", srv_port);
   get_list_test_set_buildbot_url(url);
   strlcpy(settings->paths.directory_libretro, g_dir,
         sizeof(settings->paths.directory_libretro));
   strlcpy(settings->paths.path_libretro_info, g_dir,
         sizeof(settings->paths.path_libretro_info));

   retro_atomic_store_release_int(&hashed_mb, 0);
   retro_atomic_store_release_int(&core_downloads, 0);
   retro_atomic_store_release_int(&refuse_downloads, 1);
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

   retro_atomic_store_release_int(&refuse_downloads, 0);
   *hashed    = retro_atomic_load_acquire_int(&hashed_mb);
   *downloads = retro_atomic_load_acquire_int(&core_downloads);
   printf("    hashed %d MiB, %d core download(s), %ld gathers%s\n",
         *hashed / 16, *downloads, gathers,
         gathers < 4 * NUM_ENTRIES ? "" : " (did not complete)");
   CHECK(gathers < 4 * NUM_ENTRIES, "the run completed");

   task_queue_deinit();
   task_queue_unset_threaded();
}

static void run_cache_lane(void)
{
   const int one = CORE_SIZE >> 16;
   const int all = NUM_INSTALLED * one;
   int hashed, downloads;
   struct stat st;
   FILE *f;

   printf("[lane: installed core CRC cache]\n");

   fill_pathname_join_special(g_cache_path, g_dir,
         "core_backups/core_updater.crc", sizeof(g_cache_path));
   remove(g_cache_path);

   printf("  first run\n");
   cache_run(&hashed, &downloads);
   CHECK(hashed == all, "a first run hashes every installed core");
   CHECK(downloads == 0, "matching cores are not downloaded");
   CHECK(path_is_valid(g_cache_path), "the cache was written");

   printf("  repeat run\n");
   cache_run(&hashed, &downloads);
   CHECK(hashed == 0, "a repeat run hashes no unchanged core");
   CHECK(downloads == 0, "unchanged cores are not downloaded");

   /* Same contents, newer mtime */
   stat(g_core_a, &st);
   set_mtime(g_core_a, st.st_mtime + 10);
   printf("  one core touched\n");
   cache_run(&hashed, &downloads);
   CHECK(hashed == one, "a touched core is hashed again, and only it");
   CHECK(downloads == 0, "a touched but unchanged core is not downloaded");

   /* Same size, new contents, newer mtime */
   if ((f = fopen(g_core_a, "r+b")))
   {
      fputc(0xa5, f);
      fclose(f);
   }
   stat(g_core_a, &st);
   set_mtime(g_core_a, st.st_mtime + 20);
   printf("  one core changed\n");
   cache_run(&hashed, &downloads);
   CHECK(hashed == one, "a changed core is hashed again");
   CHECK(downloads == 1, "a changed core is downloaded: the cache hid an update");

   /* Back to the listed contents (make_core's first byte) */
   if ((f = fopen(g_core_a, "r+b")))
   {
      fputc(0x5a, f);
      fclose(f);
   }
   stat(g_core_a, &st);
   set_mtime(g_core_a, st.st_mtime + 30);

   if ((f = fopen(g_cache_path, "wb")))
   {
      fputs("not a cache\n", f);
      fclose(f);
   }
   printf("  corrupt cache\n");
   cache_run(&hashed, &downloads);
   CHECK(hashed == all, "a corrupt cache falls back to hashing every core");
   CHECK(downloads == 0, "restored cores are not downloaded");
}

/* ---------------- status lanes ----------------------------------- */

/* Update Installed Cores against @dir_libretro and @url, to its final
 * title */
static void status_lane(bool threaded, const char *label,
      const char *dir_libretro, const char *url, int expected)
{
   task_finder_data_t find_data;
   settings_t *settings = config_get_ptr();
   long gathers;

   printf("[lane threaded=%d: %s]\n", (int)threaded, label);

   task_queue_init(threaded, NULL);

   get_list_test_set_buildbot_url(url);
   strlcpy(settings->paths.directory_libretro, dir_libretro,
         sizeof(settings->paths.directory_libretro));
   strlcpy(settings->paths.path_libretro_info, g_dir,
         sizeof(settings->paths.path_libretro_info));

   retro_atomic_store_release_int(&final_status, 0);
   retro_atomic_store_release_int(&n_info_reads, 0);
   task_push_update_installed_cores(false, 0, dir_libretro, NULL);

   find_data.func     = find_any;
   find_data.userdata = NULL;
   for (gathers = 0; gathers < 4 * NUM_ENTRIES; gathers++)
   {
      task_queue_check();
      if (!task_queue_find(&find_data))
         break;
      retro_sleep(17);
   }

   CHECK(gathers < 4 * NUM_ENTRIES, "update installed cores completed");
   CHECK(retro_atomic_load_acquire_int(&final_status) == expected,
         expected == 1
         ? "ended in 'all cores updated'"
         : "ended in 'core list failed'");
   CHECK(retro_atomic_load_acquire_int(&n_info_reads) == 0,
         "no core info read");

   task_queue_deinit();
   task_queue_unset_threaded();
}

/* ---------------- cancel lanes ----------------------------------- */

enum cancel_mode
{
   RESET_IN_WAIT_LIST = 0,
   CANCEL_IN_WAIT_LIST,
   CANCEL_IN_WAIT_DOWNLOAD
};

static const char *cancel_mode_name[] =
{
   "reset in WAIT_LIST",
   "cancel in WAIT_LIST",
   "cancel in WAIT_DOWNLOAD"
};

/* The parent is the one task the core updater pushes unmuted: the
 * list fetch, the downloads and their HTTP transfers are all muted. */
static bool find_parent(retro_task_t *task, void *user_data)
{
   if (task_get_flags(task) & RETRO_TASK_FLG_MUTE)
      return false;
   *(retro_task_t**)user_data = task;
   return true;
}

static bool find_task(retro_task_t *task, void *user_data)
{
   return task == (retro_task_t*)user_data;
}

static bool write_cancel_core(void)
{
   char path[PATH_MAX_LENGTH];
   FILE *f;
   fill_pathname_join_special(path, g_dir, CANCEL_CORE, sizeof(path));
   if (!(f = fopen(path, "wb")))
      return false;
   fputs("not the buildbot's core", f);
   fclose(f);
   return true;
}

static void cancel_lane(bool threaded, enum cancel_mode mode)
{
   char url[128];
   task_finder_data_t find_data;
   settings_t *settings  = config_get_ptr();
   retro_task_t *parent  = NULL;
   int hold_at           = (mode == CANCEL_IN_WAIT_DOWNLOAD) ? 2 : 1;
   bool child_pending    = false;
   int i;

   printf("[lane threaded=%d: %s]\n", (int)threaded,
         cancel_mode_name[mode]);

   if (!write_cancel_core())
   {
      printf("  SKIP: could not create fixture\n");
      return;
   }

   retro_atomic_store_release_int(&srv_use_small, 1);
   retro_atomic_store_release_int(&srv_requests, 0);
   retro_atomic_store_release_int(&srv_answered, 0);
   retro_atomic_store_release_int(&srv_hold, 1);
   retro_atomic_store_release_int(&srv_hold_at, hold_at);

   task_queue_init(threaded, NULL);

   snprintf(url, sizeof(url), "http://127.0.0.1:%d", srv_port);
   get_list_test_set_buildbot_url(url);
   strlcpy(settings->paths.directory_libretro, g_dir,
         sizeof(settings->paths.directory_libretro));
   strlcpy(settings->paths.path_libretro_info, g_dir,
         sizeof(settings->paths.path_libretro_info));

   task_push_update_installed_cores(false, 0, g_dir, NULL);

   /* Run until the server holds the child's request: the list
    * fetch's for WAIT_LIST, the download's for WAIT_DOWNLOAD */
   for (i = 0; i < 5000
         && retro_atomic_load_acquire_int(&srv_requests) < hold_at; i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(i < 5000, "child request held by the server");

   find_data.func     = find_parent;
   find_data.userdata = &parent;
   task_queue_find(&find_data);
   CHECK(parent != NULL, "parent task found");

   if (mode == RESET_IN_WAIT_LIST)
      task_queue_reset();
   else if (parent)
      task_queue_cancel_task(parent);

   /* Retire the parent; once it is no longer findable the queue has
    * freed it, and its handle with it */
   find_data.func     = find_task;
   find_data.userdata = parent;
   for (i = 0; i < 5000; i++)
   {
      task_queue_check();
      if (!parent || !task_queue_find(&find_data))
         break;
      retro_sleep(1);
   }
   CHECK(i < 5000, "cancelled parent retired");

   find_data.func     = find_any;
   find_data.userdata = NULL;
   child_pending      = task_queue_find(&find_data);
   if (mode != RESET_IN_WAIT_LIST)
      CHECK(child_pending, "child still pending after the parent retired");

   /* Now let the child finish: its parse and its callback run after
    * the parent's handle is gone */
   retro_atomic_store_release_int(&srv_hold, 0);
   for (i = 0; i < 10000; i++)
   {
      task_queue_check();
      if (!task_queue_find(&find_data))
         break;
      retro_sleep(1);
   }
   CHECK(i < 10000, "children completed after the parent");

   task_queue_deinit();
   task_queue_unset_threaded();

   /* The server answers the held request only once it sees the
    * release; the next lane must not re-arm the hold before that */
   for (i = 0; i < 5000
         &&    retro_atomic_load_acquire_int(&srv_answered)
             < retro_atomic_load_acquire_int(&srv_requests); i++)
      retro_sleep(1);
   CHECK(i < 5000, "server answered every request");

   retro_atomic_store_release_int(&srv_hold_at, 0);
   retro_atomic_store_release_int(&srv_use_small, 0);
}

static bool find_any_task(retro_task_t *task, void *user_data)
{
   (void)task;
   (void)user_data;
   return true;
}

static void run_get_list_lane(void)
{
   static const char old_listing[] =
      "2026-09-01 1234abcd old0_libretro.so.zip\n"
      "2026-09-01 1234abcd old1_libretro.so.zip\n";
   char url[128];
   char name[128];
   task_finder_data_t find_data;
   settings_t *settings      = config_get_ptr();
   core_updater_list_t *list = core_updater_list_init();
   long gathers              = 0;
   bool torn                 = false;

   printf("[lane get-list: one line per gather, published whole]\n");

   snprintf(url, sizeof(url), "http://127.0.0.1:%d", srv_port);
   get_list_test_set_buildbot_url(url);
   strlcpy(settings->paths.directory_libretro, g_dir,
         sizeof(settings->paths.directory_libretro));
   strlcpy(settings->paths.path_libretro_info, g_dir,
         sizeof(settings->paths.path_libretro_info));

   core_updater_list_parse_network_data(list, g_dir, g_dir, url,
         old_listing, sizeof(old_listing) - 1);
   CHECK(core_updater_list_size(list) == 2, "old list in place");

   task_queue_init(false, NULL);
   retro_atomic_store_release_int(&force_floor, 1);

   task_push_get_core_updater_list(list, true, false);

   find_data.func     = find_any_task;
   find_data.userdata = NULL;
   for (gathers = 0; gathers < 8 * NUM_ENTRIES; gathers++)
   {
      size_t n;
      task_queue_check();
      n = core_updater_list_size(list);
      if (n != 2 && n != NUM_ENTRIES)
         torn = true;
      if (!task_queue_find(&find_data))
         break;
      /* The loopback transfer runs in real time on the server
       * thread */
      retro_sleep(1);
   }

   retro_atomic_store_release_int(&force_floor, 0);

   printf("  %ld gathers for %d listing lines\n", gathers, NUM_ENTRIES);
   CHECK(gathers < 8 * NUM_ENTRIES, "get-list completed");
   CHECK(!torn, "the caller's list was never seen emptied or half-built");
   CHECK(core_updater_list_size(list) == NUM_ENTRIES,
         "the fetched list replaced the old one");
   snprintf(name, sizeof(name),
         "the parse took a gather per line with the window at its floor "
         "(at least %d)", NUM_ENTRIES);
   CHECK(gathers >= NUM_ENTRIES, name);

   task_queue_deinit();
   task_queue_unset_threaded();
   core_updater_list_free(list);
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
      int mode;
      run_get_list_lane();
      char url[128];
      char empty_dir[] = "/tmp/update_installed_empty_XXXXXX";

      run_lane(false);
      run_lane(true);
      run_refused_download_lane();
      run_cache_lane();

      snprintf(url, sizeof(url), "http://127.0.0.1:%d", srv_port);
      if (mkdtemp(empty_dir))
      {
         status_lane(false, "no installed cores", empty_dir, url, 1);
         status_lane(true,  "no installed cores", empty_dir, url, 1);
         rmdir(empty_dir);
      }
      /* Nothing listens on port 1 */
      status_lane(false, "fetch fails", g_dir, "http://127.0.0.1:1", 2);
      status_lane(true,  "fetch fails", g_dir, "http://127.0.0.1:1", 2);
      for (mode = RESET_IN_WAIT_LIST; mode <= CANCEL_IN_WAIT_DOWNLOAD; mode++)
      {
         cancel_lane(false, (enum cancel_mode)mode);
         cancel_lane(true,  (enum cancel_mode)mode);
      }
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
