/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (staged_install_test.c).
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

/* End-to-end test of the staged core install in tasks/task_core_updater.c
 * and the install mode of tasks/task_core_backup.c, against the shipping
 * updater, HTTP, decompress, archive, rzip and core backup code.
 *
 * With 'Compress Automatic Core Backups' off, a core update downloads
 * and extracts the new core beside the cores, then the backup task
 * moves the replaced core into the backups as it is and the new one
 * into place.  The lanes pin:
 *
 *   move     - the new core is installed, the replaced one is the
 *              backup byte for byte (uncompressed, its CRC in the
 *              name), and the core's staging directory is gone.
 *   history  - a second update keeps the history size: the older
 *              backup is pruned, the newer one is the replaced core.
 *   fallback - when the replaced core cannot be moved into the
 *              backups (filestream_rename refused at the link
 *              boundary), it is backed up as a compressed copy and the
 *              new core is still installed.
 *   compress - with the setting on, the backup is compressed as before.
 *   failed   - when the new core cannot be moved into place, the
 *              installed core is left as it was and the staging
 *              directory is gone.
 *   threaded - the move lane on the threaded task queue. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <queues/task_queue.h>
#include <net/net_http.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <retro_dirent.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <compat/strl.h>

#include "../../../configuration.h"
#include "../../../core_updater_list.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../core_info.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../retroarch.h"

#define CORE_NAME  "st_libretro.so"
#define CORE_SIZE  (600 * 1024)   /* several 256 KiB output windows */

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

/* ---------------- stubs core_backup.c needs ---------------------- */

bool core_info_find(const char *core_path, core_info_t **core_info)
{
   (void)core_path;
   (void)core_info;
   return false;
}

size_t frontend_driver_get_core_extension(char *s, size_t len)
{
   return strlcpy(s, "so", len);
}

bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{
   (void)state;
   (void)data;
   return false;
}

/* ---------------- refusing a rename at the link boundary --------- */

/* 0 nothing refused; 1 moves into the backups; 2 moves out of staging */
static retro_atomic_int_t refuse_rename;

int __real_filestream_rename(const char *old_path, const char *new_path);
int __wrap_filestream_rename(const char *old_path, const char *new_path)
{
   int mode = retro_atomic_load_acquire_int(&refuse_rename);
   if (mode == 1 && strstr(new_path, "core_backups"))
      return -1;
   if (mode == 2 && strstr(old_path, ".staging"))
      return -1;
   return __real_filestream_rename(old_path, new_path);
}

/* ---------------- fixture data ----------------------------------- */

static uint8_t *make_core(uint32_t seed)
{
   size_t i;
   uint8_t *b = (uint8_t*)malloc(CORE_SIZE);
   uint32_t x = seed * 2654435761u + 1;
   for (i = 0; i < CORE_SIZE; i++)
   {
      x    = x * 1103515245u + 12345u;
      b[i] = (uint8_t)("ABCDEFGHIJKLMNOP"[(x >> 16) & 15]);
   }
   return b;
}

static void put16(uint8_t **p, unsigned v)
{
   (*p)[0] = v & 0xff;
   (*p)[1] = (v >> 8) & 0xff;
   *p     += 2;
}

static void put32(uint8_t **p, uint32_t v)
{
   put16(p, v & 0xffff);
   put16(p, v >> 16);
}

/* One deflated member called CORE_NAME holding @data */
static uint8_t *make_zip(const uint8_t *data, size_t *zip_len)
{
   z_stream zs;
   uLong bound;
   uint8_t *cbuf, *zip, *p;
   size_t clen, nlen = strlen(CORE_NAME);
   uint32_t crc     = (uint32_t)crc32(0, data, CORE_SIZE);

   memset(&zs, 0, sizeof(zs));
   deflateInit2(&zs, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
   bound = deflateBound(&zs, CORE_SIZE);
   cbuf  = (uint8_t*)malloc(bound);
   zs.next_in   = (Bytef*)data;
   zs.avail_in  = CORE_SIZE;
   zs.next_out  = cbuf;
   zs.avail_out = (uInt)bound;
   deflate(&zs, Z_FINISH);
   clen = zs.total_out;
   deflateEnd(&zs);

   zip = (uint8_t*)malloc(clen + 2 * nlen + 200);
   p   = zip;
   /* local header */
   put32(&p, 0x04034b50); put16(&p, 20); put16(&p, 0); put16(&p, 8);
   put16(&p, 0); put16(&p, 0); put32(&p, crc); put32(&p, (uint32_t)clen);
   put32(&p, CORE_SIZE); put16(&p, (unsigned)nlen); put16(&p, 0);
   memcpy(p, CORE_NAME, nlen); p += nlen;
   memcpy(p, cbuf, clen);      p += clen;
   {
      uint32_t cd_off = (uint32_t)(p - zip);
      uint8_t *cd     = p;
      put32(&p, 0x02014b50); put16(&p, 20); put16(&p, 20); put16(&p, 0);
      put16(&p, 8); put16(&p, 0); put16(&p, 0); put32(&p, crc);
      put32(&p, (uint32_t)clen); put32(&p, CORE_SIZE);
      put16(&p, (unsigned)nlen); put16(&p, 0); put16(&p, 0); put16(&p, 0);
      put16(&p, 0); put32(&p, 0); put32(&p, 0);
      memcpy(p, CORE_NAME, nlen); p += nlen;
      {
         uint32_t cd_len = (uint32_t)(p - cd);
         put32(&p, 0x06054b50); put16(&p, 0); put16(&p, 0); put16(&p, 1);
         put16(&p, 1); put32(&p, cd_len); put32(&p, cd_off); put16(&p, 0);
      }
   }
   free(cbuf);
   *zip_len = (size_t)(p - zip);
   return zip;
}

/* ---------------- loopback server -------------------------------- */

/* The payload each lane serves, swapped by the main thread between
 * lanes and read by the server thread: both under srv_lock */
static slock_t *srv_lock  = NULL;
static char srv_index[256];
static uint8_t *srv_zip   = NULL;
static size_t srv_zip_len = 0;
static retro_atomic_int_t srv_fd;
static sthread_t *srv_thread = NULL;
static volatile int srv_port = 0;

static void send_all(int fd, const void *data, size_t len)
{
   const char *p = (const char*)data;
   while (len)
   {
      ssize_t n = send(fd, p, len, 0);
      if (n <= 0)
         return;
      p   += n;
      len -= (size_t)n;
   }
}

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
      bool is_zip;
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
      is_zip = (strstr(rbuf, ".zip HTTP/") != NULL);
      slock_lock(srv_lock);
      snprintf(head, sizeof(head),
            "HTTP/1.1 200 OK\r\nContent-Length: %u\r\n"
            "Connection: close\r\n\r\n",
            (unsigned)(is_zip ? srv_zip_len : strlen(srv_index)));
      send_all(cfd, head, strlen(head));
      if (is_zip)
         send_all(cfd, srv_zip, srv_zip_len);
      else
         send_all(cfd, srv_index, strlen(srv_index));
      slock_unlock(srv_lock);
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

/* ---------------- helpers ---------------------------------------- */

static char g_root[] = "/tmp/staged_install_XXXXXX";
static char g_cores[PATH_MAX_LENGTH];
static char g_assets[PATH_MAX_LENGTH];
static char g_core[PATH_MAX_LENGTH];
static char g_staging[PATH_MAX_LENGTH];
static char g_backups[PATH_MAX_LENGTH];

static bool file_equals(const char *path, const uint8_t *data, size_t len)
{
   void *buf = NULL;
   int64_t n = 0;
   bool ok;
   if (!filestream_read_file(path, &buf, &n))
      return false;
   ok = (n == (int64_t)len && !memcmp(buf, data, len));
   free(buf);
   return ok;
}

/* Files in @dir; the first one's path in @first */
static unsigned count_files(const char *dir, char *first, size_t len)
{
   unsigned n = 0;
   struct RDIR *rdir = retro_opendir(dir);
   if (first)
      *first = '\0';
   if (!rdir)
      return 0;
   while (retro_readdir(rdir))
   {
      const char *name = retro_dirent_get_name(rdir);
      if (!name || name[0] == '.' || retro_dirent_is_dir(rdir, NULL))
         continue;
      if (first && !*first)
         fill_pathname_join_special(first, dir, name, len);
      n++;
   }
   retro_closedir(rdir);
   return n;
}

static bool any_task(retro_task_t *task, void *user_data)
{
   (void)task;
   (void)user_data;
   return true;
}

static void pump(void)
{
   task_finder_data_t find_data;
   int i;
   find_data.func     = any_task;
   find_data.userdata = NULL;
   for (i = 0; i < 20000; i++)
   {
      task_queue_check();
      if (!task_queue_find(&find_data))
         return;
      retro_sleep(1);
   }
   CHECK(false, "tasks completed");
}

/* Serves @next as the listed core and updates to it */
static void update_to(const uint8_t *next, bool compress, bool threaded)
{
   char url[128];
   settings_t *settings      = config_get_ptr();
   core_updater_list_t *list = core_updater_list_init();

   slock_lock(srv_lock);
   free(srv_zip);
   srv_zip = make_zip(next, &srv_zip_len);
   snprintf(srv_index, sizeof(srv_index), "2026-10-01 %08x %s.zip\n",
         (unsigned)crc32(0, next, CORE_SIZE), CORE_NAME);
   slock_unlock(srv_lock);

   settings->bools.core_updater_auto_backup_compress = compress;
   snprintf(url, sizeof(url), "http://127.0.0.1:%d", srv_port);
   get_list_test_set_buildbot_url(url);
   strlcpy(settings->paths.directory_libretro, g_cores,
         sizeof(settings->paths.directory_libretro));
   strlcpy(settings->paths.path_libretro_info, g_cores,
         sizeof(settings->paths.path_libretro_info));

   task_queue_init(threaded, NULL);
   task_push_get_core_updater_list(list, true, false);
   pump();
   CHECK(core_updater_list_size(list) == 1, "listing parsed");
   CHECK(task_push_core_updater_download(list, CORE_NAME ".zip", 0, true,
            true, 1, g_cores, g_assets) != NULL, "download pushed");
   pump();
   task_queue_deinit();
   task_queue_unset_threaded();
   core_updater_list_free(list);
}

static bool is_rzip(const char *path)
{
   char head[8];
   bool ok   = false;
   RFILE *f  = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f)
      return false;
   ok = filestream_read(f, head, 6) == 6 && !memcmp(head, "#RZIPv", 6);
   filestream_close(f);
   return ok;
}

/* ---------------- lanes ------------------------------------------ */

int main(void)
{
   uint8_t *core[7];
   char backup[PATH_MAX_LENGTH];
   char crc_hex[16];
   int i;

   setvbuf(stdout, NULL, _IOLBF, 0);
   printf("core updater staged install test\n\n");

   if (!mkdtemp(g_root))
   {
      printf("SKIP: could not create temp dir\n");
      return 0;
   }
   fill_pathname_join_special(g_cores,   g_root,  "cores",  sizeof(g_cores));
   fill_pathname_join_special(g_assets,  g_root,  "assets", sizeof(g_assets));
   fill_pathname_join_special(g_core,    g_cores, CORE_NAME, sizeof(g_core));
   fill_pathname_join_special(g_staging, g_cores, "." CORE_NAME ".staging",
         sizeof(g_staging));
   fill_pathname_join_special(g_backups, g_assets,
         "core_backups/st_libretro", sizeof(g_backups));
   path_mkdir(g_cores);
   path_mkdir(g_assets);

   for (i = 0; i < 7; i++)
      core[i] = make_core((uint32_t)i + 1);
   filestream_write_file(g_core, core[0], CORE_SIZE);

   network_init();
   net_http_init();
   if (!(srv_lock = slock_new()) || !server_start())
   {
      printf("SKIP: could not start loopback server\n");
      return 0;
   }

   printf("[move]\n");
   update_to(core[1], false, false);
   CHECK(file_equals(g_core, core[1], CORE_SIZE), "new core installed");
   CHECK(count_files(g_backups, backup, sizeof(backup)) == 1, "one backup");
   CHECK(file_equals(backup, core[0], CORE_SIZE),
         "the backup is the replaced core, uncompressed");
   snprintf(crc_hex, sizeof(crc_hex), "%08x",
         (unsigned)crc32(0, core[0], CORE_SIZE));
   CHECK(strstr(backup, crc_hex) != NULL, "the backup names the replaced core's CRC");
   CHECK(!path_is_directory(g_staging), "the staging directory is gone");

   printf("[history]\n");
   update_to(core[2], false, false);
   CHECK(file_equals(g_core, core[2], CORE_SIZE), "new core installed");
   CHECK(count_files(g_backups, backup, sizeof(backup)) == 1,
         "the history keeps one backup");
   CHECK(file_equals(backup, core[1], CORE_SIZE),
         "the kept backup is the core just replaced");
   CHECK(!path_is_directory(g_staging), "the staging directory is gone");

   printf("[fallback]\n");
   retro_atomic_store_release_int(&refuse_rename, 1);
   update_to(core[3], false, false);
   retro_atomic_store_release_int(&refuse_rename, 0);
   CHECK(file_equals(g_core, core[3], CORE_SIZE), "new core installed");
   CHECK(count_files(g_backups, backup, sizeof(backup)) == 1, "one backup");
   CHECK(is_rzip(backup), "the backup is a compressed copy");
   CHECK(!path_is_directory(g_staging), "the staging directory is gone");

   printf("[compress]\n");
   update_to(core[4], true, false);
   CHECK(file_equals(g_core, core[4], CORE_SIZE), "new core installed");
   CHECK(count_files(g_backups, backup, sizeof(backup)) == 1, "one backup");
   CHECK(is_rzip(backup), "the backup is compressed");

   printf("[failed]\n");
   retro_atomic_store_release_int(&refuse_rename, 2);
   update_to(core[5], false, false);
   retro_atomic_store_release_int(&refuse_rename, 0);
   CHECK(file_equals(g_core, core[4], CORE_SIZE),
         "the installed core is left as it was");
   CHECK(!path_is_directory(g_staging), "the staging directory is gone");

   /* The failed lane left a compressed backup of the installed core,
    * which an update would reuse as an identical backup; start the
    * threaded lane from an empty history instead */
   while (count_files(g_backups, backup, sizeof(backup)))
      filestream_delete(backup);

   printf("[threaded]\n");
   update_to(core[6], false, true);
   CHECK(file_equals(g_core, core[6], CORE_SIZE), "new core installed");
   CHECK(count_files(g_backups, backup, sizeof(backup)) == 1, "one backup");
   CHECK(file_equals(backup, core[4], CORE_SIZE),
         "the backup is the replaced core, uncompressed");
   CHECK(!path_is_directory(g_staging), "the staging directory is gone");

   server_stop();
   net_http_deinit();
   slock_free(srv_lock);
   free(srv_zip);
   for (i = 0; i < 7; i++)
      free(core[i]);
   {
      char cmd[128];
      snprintf(cmd, sizeof(cmd), "rm -rf %s", g_root);
      if (system(cmd)) { /* best effort */ }
   }

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
