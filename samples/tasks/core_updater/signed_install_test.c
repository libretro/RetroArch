/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* The core updater checks every download (tasks/task_core_updater.c):
 * <core>.sig first, then the core, staged and checked under its install
 * name, then installed with its .sig beside it and recorded; Update
 * Installed Cores tells of each refused core and carries on. Against
 * the shipping updater, HTTP, backup and trust store code, with a
 * loopback server as the buildbot. COMPRESSION=1 also serves a zip.
 * Linux and mingw-w64 (COMPRESSION=0). */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <encodings/crc32.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <net/net_compat.h>
#include <net/net_http.h>
#include <net/net_socket.h>
#include <queues/task_queue.h>
#include <retro_atomic.h>
#include <retro_dirent.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#ifdef HAVE_COMPRESSION
#include <zlib.h>
#endif

#include "../../../configuration.h"
#include "../../../core_info.h"
#include "../../../core_trust_runtime.h"
#include "../../../core_updater_list.h"
#include "../../../retroarch.h"
#include "../../../tasks/tasks_internal.h"
#include "../core_signature/core_signature_fixtures.h"

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

#define TMP_DIR    "signed_install_test_tmp"
#define OTHER_NAME "other_libretro.so"
#define OLD_SIZE   4096

void get_list_test_set_buildbot_url(const char *url);
extern int  stub_osd_count;
extern char stub_osd_last[256];
extern bool stub_err_expected;

static int checks   = 0;
static int failures = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---- what core_backup.c and the trust store call ---- */

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

/* core_trust_default_path's folder, here the test's own */
bool fill_pathname_application_data(char *s, size_t len)
{
   strlcpy(s, TMP_DIR, len);
   return true;
}

/* The updater never opens the Security Warning */
void menu_core_warning_open(void) { }
bool menu_core_warning_can_open(void) { return false; }
bool menu_core_warning_user_only(void) { return false; }

/* ---- fixture data ---- */

static uint8_t *fx_core;
static uint8_t old_core[OLD_SIZE];

static char g_cores[256];
static char g_assets[256];
static char g_core[300];
static char g_core_sig[300];
static char g_other[300];
static char g_other_sig[300];
static char g_staging[300];
static char g_backups[300];

static void fx_make_core(void)
{
   unsigned long x = FX_CORE_SEED;
   size_t i;
   fx_core = (uint8_t*)malloc(FX_CORE_SIZE);
   for (i = 0; i < FX_CORE_SIZE; i++)
   {
      x          = (x * 1103515245UL + 12345UL) & 0xffffffffUL;
      fx_core[i] = (uint8_t)(x >> 24);
   }
}

static bool put(const char *path, const void *data, size_t len)
{
   return filestream_write_file(path, data, (int64_t)len);
}

static bool same(const char *path, const void *data, size_t len)
{
   void *buf = NULL;
   int64_t n = 0;
   bool ok;
   if (!filestream_read_file(path, &buf, &n))
      return false;
   ok = n == (int64_t)len && !memcmp(buf, data, len);
   free(buf);
   return ok;
}

static void sig_of(const char *path, char *s, size_t len)
{
   strlcpy(s, path, len);
   strlcat(s, ".sig", len);
}

#ifdef HAVE_COMPRESSION
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

/* Deflated members: name holding data, then extra (NULL: none)
 * holding extra_data */
static uint8_t *make_zip(const char *name, const uint8_t *data,
      size_t size, const char *extra, const uint8_t *extra_data,
      size_t extra_size, size_t *zip_len)
{
   const char *names[2];
   const uint8_t *datas[2];
   size_t sizes[2];
   uint32_t crcs[2], clens[2], offs[2];
   uint8_t *cbufs[2];
   uint8_t *zip, *p, *cd;
   unsigned i, n = extra ? 2 : 1;
   size_t total  = 200;
   names[0] = name;
   datas[0] = data;
   sizes[0] = size;
   names[1] = extra;
   datas[1] = extra_data;
   sizes[1] = extra_size;
   for (i = 0; i < n; i++)
   {
      z_stream zs;
      uLong bound;
      memset(&zs, 0, sizeof(zs));
      deflateInit2(&zs, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
      bound        = deflateBound(&zs, (uLong)sizes[i]);
      cbufs[i]     = (uint8_t*)malloc(bound);
      zs.next_in   = (Bytef*)datas[i];
      zs.avail_in  = (uInt)sizes[i];
      zs.next_out  = cbufs[i];
      zs.avail_out = (uInt)bound;
      deflate(&zs, Z_FINISH);
      clens[i]     = (uint32_t)zs.total_out;
      deflateEnd(&zs);
      crcs[i]      = (uint32_t)crc32(0, datas[i], (uInt)sizes[i]);
      total       += clens[i] + 2 * strlen(names[i]) + 100;
   }

   zip = (uint8_t*)malloc(total);
   p   = zip;
   /* local headers */
   for (i = 0; i < n; i++)
   {
      size_t nlen = strlen(names[i]);
      offs[i]     = (uint32_t)(p - zip);
      put32(&p, 0x04034b50); put16(&p, 20); put16(&p, 0); put16(&p, 8);
      put16(&p, 0); put16(&p, 0); put32(&p, crcs[i]); put32(&p, clens[i]);
      put32(&p, (uint32_t)sizes[i]); put16(&p, (unsigned)nlen); put16(&p, 0);
      memcpy(p, names[i], nlen); p += nlen;
      memcpy(p, cbufs[i], clens[i]); p += clens[i];
      free(cbufs[i]);
   }
   cd = p;
   for (i = 0; i < n; i++)
   {
      size_t nlen = strlen(names[i]);
      put32(&p, 0x02014b50); put16(&p, 20); put16(&p, 20); put16(&p, 0);
      put16(&p, 8); put16(&p, 0); put16(&p, 0); put32(&p, crcs[i]);
      put32(&p, clens[i]); put32(&p, (uint32_t)sizes[i]);
      put16(&p, (unsigned)nlen); put16(&p, 0); put16(&p, 0); put16(&p, 0);
      put16(&p, 0); put32(&p, 0); put32(&p, offs[i]);
      memcpy(p, names[i], nlen); p += nlen;
   }
   {
      uint32_t cd_len = (uint32_t)(p - cd);
      uint32_t cd_off = (uint32_t)(cd - zip);
      put32(&p, 0x06054b50); put16(&p, 0); put16(&p, 0); put16(&p, n);
      put16(&p, n); put32(&p, cd_len); put32(&p, cd_off); put16(&p, 0);
   }
   *zip_len = (size_t)(p - zip);
   return zip;
}
#endif

/* ---- loopback buildbot ---- */

/* What each listed core serves; the main thread sets it between lanes
 * and the server thread reads it, both under srv_lock */
struct srv_core
{
   const char *name;
   const uint8_t *core;  /* its CRC is listed */
   size_t core_len;
   uint8_t *served;      /* the core, or its zip */
   size_t served_len;
   const uint8_t *sig;   /* NULL: 404 */
   size_t sig_len;
   const char *sig_status; /* non-NULL: the .sig's answer instead */
   bool sig_drop;        /* closed without an answer */
   bool zip;
   unsigned core_hits;
   unsigned sig_hits;
};

static struct srv_core srv[2];
static unsigned srv_n;
static slock_t *srv_lock;
static retro_atomic_int_t srv_fd;
static sthread_t *srv_thread;
static int srv_port;

static void send_all(int fd, const void *data, size_t len)
{
   const char *p = (const char*)data;
   while (len)
   {
      int n = (int)send(fd, p, (int)len, 0);
      if (n <= 0)
         return;
      p   += n;
      len -= (size_t)n;
   }
}

static void reply(int fd, const char *status, const void *body, size_t len)
{
   char head[160];
   sprintf(head, "HTTP/1.1 %s\r\nContent-Length: %u\r\n"
         "Connection: close\r\n\r\n", status, (unsigned)len);
   send_all(fd, head, strlen(head));
   if (len)
      send_all(fd, body, len);
}

/* path: the request's, without its leading '/' */
static void serve(int fd, const char *path)
{
   char index[512];
   unsigned i;
   for (i = 0; i < srv_n; i++)
   {
      struct srv_core *c = &srv[i];
      size_t nlen        = strlen(c->name);
      if (strncmp(path, c->name, nlen))
         continue;
      if (!strcmp(path + nlen, ".sig"))
      {
         c->sig_hits++;
         if (c->sig_drop)
            return;
         if (c->sig_status)
            reply(fd, c->sig_status, NULL, 0);
         else if (c->sig)
            reply(fd, "200 OK", c->sig, c->sig_len);
         else
            reply(fd, "404 Not Found", NULL, 0);
         return;
      }
      if (!strcmp(path + nlen, c->zip ? ".zip" : ""))
      {
         c->core_hits++;
         reply(fd, "200 OK", c->served, c->served_len);
         return;
      }
   }
   /* Anything else is the listing */
   index[0] = '\0';
   for (i = 0; i < srv_n; i++)
      sprintf(index + strlen(index), "2026-10-01 %08lx %s%s\n",
            (unsigned long)encoding_crc32(0, srv[i].core, srv[i].core_len),
            srv[i].name, srv[i].zip ? ".zip" : "");
   reply(fd, "200 OK", index, strlen(index));
}

static void server_thread(void *unused)
{
   char rbuf[2048];
   (void)unused;
   for (;;)
   {
      int fd      = retro_atomic_load_acquire_int(&srv_fd);
      int cfd     = fd >= 0 ? (int)accept(fd, NULL, NULL) : -1;
      size_t have = 0;
      char *p;
      char *e;
      if (cfd < 0)
         return;
      while (have < sizeof(rbuf) - 1)
      {
         int n = (int)recv(cfd, rbuf + have,
               (int)(sizeof(rbuf) - 1 - have), 0);
         if (n <= 0)
            break;
         have      += (size_t)n;
         rbuf[have] = '\0';
         if (strstr(rbuf, "\r\n\r\n"))
            break;
      }
      rbuf[have] = '\0';
      /* "GET /<path> HTTP/1.1" */
      if ((p = strchr(rbuf, '/')) && (e = strchr(p, ' ')))
      {
         *e = '\0';
         slock_lock(srv_lock);
         serve(cfd, p + 1);
         slock_unlock(srv_lock);
      }
      socket_close(cfd);
   }
}

static bool server_start(void)
{
   struct sockaddr_in addr;
   socklen_t alen = sizeof(addr);
   int fd         = (int)socket(AF_INET, SOCK_STREAM, 0);
   if (fd < 0)
      return false;
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (     bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0
         || listen(fd, 8) < 0)
      return false;
   getsockname(fd, (struct sockaddr*)&addr, &alen);
   srv_port = ntohs(addr.sin_port);
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

static void srv_use(unsigned n)
{
   slock_lock(srv_lock);
   srv_n = n;
   slock_unlock(srv_lock);
}

static void srv_set(unsigned i, const char *name, const uint8_t *core,
      size_t core_len, const uint8_t *sig, size_t sig_len, bool zip)
{
   struct srv_core *c = &srv[i];
   slock_lock(srv_lock);
   free(c->served);
   c->name      = name;
   c->core      = core;
   c->core_len  = core_len;
   c->sig       = sig;
   c->sig_len   = sig_len;
   c->sig_status = NULL;
   c->sig_drop  = false;
   c->zip       = zip;
   c->core_hits = 0;
   c->sig_hits  = 0;
#ifdef HAVE_COMPRESSION
   if (zip)
      c->served = make_zip(name, core, core_len, NULL, NULL, 0,
            &c->served_len);
   else
#endif
   {
      c->served     = (uint8_t*)malloc(core_len);
      c->served_len = core_len;
      memcpy(c->served, core, core_len);
   }
   slock_unlock(srv_lock);
}

#ifdef HAVE_CORE_SIGNING_TEST_KEY
static void srv_drop_sig(unsigned i)
{
   slock_lock(srv_lock);
   srv[i].sig_drop = true;
   slock_unlock(srv_lock);
}

static void srv_sig_status(unsigned i, const char *status)
{
   slock_lock(srv_lock);
   srv[i].sig_status = status;
   slock_unlock(srv_lock);
}

#ifdef HAVE_COMPRESSION
/* The zip holds member instead of the core's own name */
static void srv_zip_member(unsigned i, const char *member)
{
   struct srv_core *c = &srv[i];
   slock_lock(srv_lock);
   free(c->served);
   c->served = make_zip(member, c->core, c->core_len, NULL, NULL, 0,
         &c->served_len);
   slock_unlock(srv_lock);
}

/* The zip holds extra besides the core */
static void srv_zip_extra(unsigned i, const char *extra,
      const uint8_t *data, size_t len)
{
   struct srv_core *c = &srv[i];
   slock_lock(srv_lock);
   free(c->served);
   c->served = make_zip(c->name, c->core, c->core_len, extra, data, len,
         &c->served_len);
   slock_unlock(srv_lock);
}
#endif
#endif

static unsigned srv_hits(unsigned i, bool sig)
{
   unsigned n;
   slock_lock(srv_lock);
   n = sig ? srv[i].sig_hits : srv[i].core_hits;
   slock_unlock(srv_lock);
   return n;
}

/* ---- helpers ---- */

/* An unmuted task's last title, as the non-threaded queue shows it */
static char g_title[256];

static void queue_msg(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   (void)task;
   (void)prio;
   (void)duration;
   (void)flush;
   if (!strncmp(msg, "100%: ", 6))
      strlcpy(g_title, msg + 6, sizeof(g_title));
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
   CHECK(!"tasks completed");
}

#ifdef HAVE_CORE_SIGNING_TEST_KEY
/* Files in dir whose names end in suffix; the first one's path in first */
static unsigned count(const char *dir, const char *suffix,
      char *first, size_t len)
{
   unsigned n        = 0;
   size_t slen       = strlen(suffix);
   struct RDIR *rdir = retro_opendir(dir);
   if (first)
      *first = '\0';
   if (!rdir)
      return 0;
   while (retro_readdir(rdir))
   {
      const char *name = retro_dirent_get_name(rdir);
      size_t nlen      = name ? strlen(name) : 0;
      if (     nlen < slen
            || retro_dirent_is_dir(rdir, NULL)
            || strcmp(name + nlen - slen, suffix))
         continue;
      if (first && !*first)
         fill_pathname_join_special(first, dir, name, len);
      n++;
   }
   retro_closedir(rdir);
   return n;
}

static void empty(const char *dir)
{
   char p[300];
   while (count(dir, "", p, sizeof(p)))
      filestream_delete(p);
}
#endif

/* What is installed before an update: old bytes, no .sig */
static void install_old(const char *path)
{
   char s[300];
   put(path, old_core, OLD_SIZE);
   sig_of(path, s, sizeof(s));
   if (path_is_valid(s))
      filestream_delete(s);
}

#ifdef HAVE_CORE_SIGNING_TEST_KEY
static uint64_t newest(const char *path, const char *name)
{
   uint8_t key[CORE_SHA256_LEN];
   uint64_t t          = 0;
   core_trust_set_t *s = core_trust_runtime_get();
   if (s && core_folder_key(path, key))
      t = core_trust_newest(s, key, name);
   if (s)
      core_trust_set_put(s);
   return t;
}
#endif

#ifdef HAVE_CORE_SIGNING_TEST_KEY
/* The installed file's identity is in the store, where one can be: no
 * record is made without a kept ctime (Windows) */
static bool recorded(const char *path)
{
   struct core_identity id;
   bool ok             = false;
   core_file_t *f      = core_file_open(path);
   core_trust_set_t *s = core_trust_runtime_get();
   if (f && s && core_file_identity(f, &id))
      ok = !id.ctime_kept || core_trust_has_identity(s, &id, FX_NAME);
   core_file_close(f);
   if (s)
      core_trust_set_put(s);
   return ok;
}

static unsigned g_tasks;

static bool count_task(retro_task_t *task, void *user_data)
{
   (void)task;
   (void)user_data;
   g_tasks++;
   return false;
}

/* The tasks in the queue */
static unsigned tasks(void)
{
   task_finder_data_t find_data;
   find_data.func     = count_task;
   find_data.userdata = NULL;
   g_tasks            = 0;
   task_queue_find(&find_data);
   return g_tasks;
}
#endif

static bool g_backup_compress = false;

static void settings_point_at_server(void)
{
   char url[64];
   settings_t *settings = config_get_ptr();
   sprintf(url, "http://127.0.0.1:%d", srv_port);
   get_list_test_set_buildbot_url(url);
   settings->bools.core_updater_auto_backup_compress = g_backup_compress;
   strlcpy(settings->paths.directory_libretro, g_cores,
         sizeof(settings->paths.directory_libretro));
   strlcpy(settings->paths.path_libretro_info, g_cores,
         sizeof(settings->paths.path_libretro_info));
}

/* The Core Downloader, for srv[0] */
static void download(bool auto_backup, bool mute)
{
   char filename[64];
   core_updater_list_t *list = core_updater_list_init();
   settings_point_at_server();
   task_push_get_core_updater_list(list, true, false);
   pump();
   strlcpy(filename, srv[0].name, sizeof(filename));
   if (srv[0].zip)
      strlcat(filename, ".zip", sizeof(filename));
   g_title[0] = '\0';
   CHECK(task_push_core_updater_download(list, filename, 0, mute,
            auto_backup, 1, g_cores, g_assets) != NULL);
   pump();
   core_updater_list_free(list);
}

/* A muted download, as Update Installed Cores pushes them, so
 * refusals show as messages */
static void update(bool auto_backup)
{
   download(auto_backup, true);
}

/* ---- lanes ---- */

#ifdef HAVE_CORE_SIGNING_TEST_KEY
static void test_signed(void)
{
   printf("[signed]\n");
   install_old(g_core);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   update(false);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(!path_is_directory(g_staging));
   CHECK(newest(g_core, FX_NAME) == FX_TIME);
}

/* The Core Downloader's commonest case: nothing installed yet */
static void test_first_install(void)
{
   printf("[first install]\n");
   filestream_delete(g_core);
   filestream_delete(g_core_sig);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   update(false);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(!path_is_directory(g_staging));
}

/* What an interrupted run left in staging goes before the download */
static void test_stale_staging(void)
{
   char junk[300];
   printf("[stale staging]\n");
   install_old(g_core);
   path_mkdir(g_staging);
   fill_pathname_join_special(junk, g_staging, OTHER_NAME, sizeof(junk));
   put(junk, "x", 1);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   update(false);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(!path_is_directory(g_staging));
}

/* Pushes a download of srv[0] and runs it until its .sig is staged,
 * then puts n files beside it, as a hostile archive's members */
static void *download_with_junk(core_updater_list_t *list, int n)
{
   char junk[300];
   char ssig[300];
   void *task;
   int i;
   settings_point_at_server();
   task_push_get_core_updater_list(list, true, false);
   pump();
   fill_pathname_join_special(ssig, g_staging, FX_NAME ".sig", sizeof(ssig));
   CHECK((task = task_push_core_updater_download(list, FX_NAME, 0, true,
            false, 1, g_cores, g_assets)) != NULL);
   for (i = 0; i < 20000 && !path_is_valid(ssig); i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(i < 20000);
   for (i = 0; i < n; i++)
   {
      char name[32];
      sprintf(name, "junk%03d", i);
      fill_pathname_join_special(junk, g_staging, name, sizeof(junk));
      put(junk, "x", 1);
   }
   return task;
}

/* More in staging than one removal takes: a refused download clears it
 * a few at a time before it ends; a cancelled one leaves the rest, which
 * the next download of the core clears before it installs */
static void test_staging_past_cap(void)
{
   unsigned left;
   void *task;
   core_updater_list_t *list = core_updater_list_init();
   printf("[staging past the removal cap]\n");
   install_old(g_core);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   download_with_junk(list, 100);
   stub_err_expected = true;
   pump();
   stub_err_expected = false;
   core_updater_list_free(list);
   CHECK(same(g_core, old_core, OLD_SIZE));
   CHECK(!path_is_valid(g_core_sig));
   CHECK(!path_is_directory(g_staging));

   printf("[staging past the removal cap, cancelled]\n");
   list = core_updater_list_init();
   task = download_with_junk(list, 100);
   if (task)
      task_queue_cancel_task(task);
   pump();
   core_updater_list_free(list);
   CHECK(same(g_core, old_core, OLD_SIZE));
   left = count(g_staging, "", NULL, 0);
   CHECK(left > 0 && left < 100);
   update(false);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(!path_is_directory(g_staging));
}

/* Refused: the old core kept with its .sig (NULL: none), staging gone,
 * the core asked for only when its .sig came, and msg shown (NULL: no
 * message) */
static void refused(const char *msg, bool core_asked,
      const uint8_t *old_sig, size_t old_sig_len)
{
   int osd = stub_osd_count;
   stub_err_expected = true;
   update(false);
   stub_err_expected = false;
   CHECK(same(g_core, old_core, OLD_SIZE));
   if (old_sig)
      CHECK(same(g_core_sig, old_sig, old_sig_len));
   else
      CHECK(!path_is_valid(g_core_sig));
   CHECK(!path_is_directory(g_staging));
   CHECK((srv_hits(0, false) > 0) == core_asked);
   if (msg)
   {
      CHECK(stub_osd_count == osd + 1);
      CHECK(strstr(stub_osd_last, FX_NAME) && strstr(stub_osd_last, msg));
   }
   else
      CHECK(stub_osd_count == osd);
}

/* Unmuted, as the Core Downloader pushes it: no message, the outcome
 * is the task's title */
static void titled(const char *title)
{
   int osd = stub_osd_count;
   stub_err_expected = true;
   download(false, false);
   stub_err_expected = false;
   CHECK(!strcmp(g_title, title));
   CHECK(stub_osd_count == osd);
}

static void test_refusals(void)
{
   printf("[unsigned]\n");
   install_old(g_core);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE, NULL, 0, false);
   refused("failed the signature check", false, NULL, 0);

   printf("[tampered]\n");
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   refused("failed the signature check", true, NULL, 0);

   printf("[unknown key]\n");
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_key2, sizeof(fx_sig_key2), false);
   refused("failed the signature check", true, NULL, 0);

   /* test_signed made FX_TIME the folder's newest build */
   printf("[older]\n");
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_older, sizeof(fx_sig_older), false);
   refused("older than one already used", true, NULL, 0);

   /* No answer: a failed download, not a bad signature */
   printf("[.sig not delivered]\n");
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   srv_drop_sig(0);
   refused(NULL, false, NULL, 0);

   /* The old core's own .sig stays with it */
   printf("[refused, old .sig kept]\n");
   put(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2));
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   refused("failed the signature check", true,
         fx_sig_key2, sizeof(fx_sig_key2));

   printf("[unmuted refusal]\n");
   titled(FX_NAME ": failed the signature check");
   CHECK(same(g_core, old_core, OLD_SIZE));
   CHECK(same(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2)));

   /* A server fault is a failed download, not a bad signature */
   printf("[.sig 503]\n");
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   srv_sig_status(0, "503 Service Unavailable");
   refused(NULL, false, fx_sig_key2, sizeof(fx_sig_key2));
   titled("Failed to install core: " FX_NAME);
   CHECK(srv_hits(0, false) == 0);
   CHECK(same(g_core, old_core, OLD_SIZE));
   CHECK(same(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2)));
   filestream_delete(g_core_sig);
}

#ifdef HAVE_COMPRESSION
/* A check that can't run (the zip doesn't hold the core) is a failed
 * install, not a bad signature */
static void test_check_error(void)
{
   printf("[check error]\n");
   install_old(g_core);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), true);
   srv_zip_member(0, OTHER_NAME);
   titled("Failed to install core: " FX_NAME ".zip");
   CHECK(same(g_core, old_core, OLD_SIZE));
   CHECK(!path_is_valid(g_core_sig));
   CHECK(!path_is_directory(g_staging));
   CHECK(!path_is_valid(g_other));
}

/* A member besides the core is never left anywhere, whether the core
 * would pass or not: the install is refused and staging goes */
static void test_zip_extra(void)
{
   static const uint8_t extra[16] = { 0x7f, 'E', 'L', 'F' };
   char sub[300];
   fill_pathname_join_special(sub, g_cores, "sub", sizeof(sub));
   printf("[zip, extra member]\n");
   install_old(g_core);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), true);
   srv_zip_extra(0, "sub/" OTHER_NAME, extra, sizeof(extra));
   titled("Failed to install core: " FX_NAME ".zip");
   CHECK(same(g_core, old_core, OLD_SIZE));
   CHECK(!path_is_valid(g_core_sig));
   CHECK(!path_is_directory(g_staging));
   CHECK(!path_is_directory(sub));

   printf("[zip, extra member, refused core]\n");
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), true);
   srv_zip_extra(0, "sub/" OTHER_NAME, extra, sizeof(extra));
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   titled("Failed to install core: " FX_NAME ".zip");
   CHECK(same(g_core, old_core, OLD_SIZE));
   CHECK(!path_is_directory(g_staging));
   CHECK(!path_is_directory(sub));
}
#endif

static void test_checks_off(void)
{
   static uint8_t unsigned_core[3000];
   int osd;
   printf("[checks off]\n");
   memset(unsigned_core, 0x5a, sizeof(unsigned_core));
   CHECK(core_trust_runtime_set_checks(false));
   install_old(g_core);
   /* The old core's .sig would call the new one changed */
   put(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2));
   srv_set(0, FX_NAME, unsigned_core, sizeof(unsigned_core), NULL, 0,
         false);
   update(false);
   CHECK(same(g_core, unsigned_core, sizeof(unsigned_core)));
   CHECK(!path_is_valid(g_core_sig));
   /* A .sig that exists still goes beside the core */
   install_old(g_core);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_older, sizeof(fx_sig_older), false);
   update(false);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_older, sizeof(fx_sig_older)));
   /* A .sig that didn't come: installed unchecked, the old .sig gone */
   install_old(g_core);
   put(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2));
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   srv_sig_status(0, "503 Service Unavailable");
   osd = stub_osd_count;
   update(false);
   CHECK(srv_hits(0, true) == 1);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(!path_is_valid(g_core_sig));
   CHECK(stub_osd_count == osd);
   CHECK(core_trust_runtime_set_checks(true));
}

/* 'Compress Automatic Core Backups' off: the replaced core is moved into
 * the backups with a copy of its .sig, then the new one goes in */
static void test_move_backup(void)
{
   char b[300];
   char bsig[300];
   printf("[move backup]\n");
   empty(g_backups);
   install_old(g_core);
   put(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2));
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   update(true);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(count(g_backups, ".lcbk", b, sizeof(b)) == 1);
   CHECK(same(b, old_core, OLD_SIZE));
   sig_of(b, bsig, sizeof(bsig));
   CHECK(same(bsig, fx_sig_key2, sizeof(fx_sig_key2)));
   CHECK(!path_is_directory(g_staging));
}

/* 'Compress Automatic Core Backups' on: the backup, with a copy of the
 * old .sig, is written after the .sig is fetched and before the
 * transfer; then the core is checked as without one */
static void test_compressed_backup(void)
{
   char b[300];
   char bsig[300];
   printf("[compressed backup]\n");
   empty(g_backups);
   install_old(g_core);
   put(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2));
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   g_backup_compress = true;
   update(true);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(recorded(g_core));
   CHECK(count(g_backups, ".lcbk", b, sizeof(b)) == 1);
   sig_of(b, bsig, sizeof(bsig));
   CHECK(same(bsig, fx_sig_key2, sizeof(fx_sig_key2)));
   CHECK(!path_is_directory(g_staging));

   printf("[compressed backup, refused]\n");
   empty(g_backups);
   install_old(g_core);
   put(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2));
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   stub_err_expected = true;
   update(true);
   stub_err_expected = false;
   g_backup_compress = false;
   CHECK(same(g_core, old_core, OLD_SIZE));
   CHECK(same(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2)));
   CHECK(count(g_backups, ".lcbk", NULL, 0) == 1);
   CHECK(!path_is_directory(g_staging));
}

/* Cancelled after the backup task moved the new core in: its .sig and
 * record follow all the same */
static void test_cancel_after_move(void)
{
   int i;
   void *task;
   core_updater_list_t *list = core_updater_list_init();
   printf("[cancelled after the move]\n");
   install_old(g_core);
   put(g_core_sig, fx_sig_key2, sizeof(fx_sig_key2));
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   settings_point_at_server();
   task_push_get_core_updater_list(list, true, false);
   pump();
   CHECK((task = task_push_core_updater_download(list, FX_NAME, 0, true,
            true, 1, g_cores, g_assets)) != NULL);
   /* The backup task's callback ran; the download hasn't seen it */
   for (i = 0; i < 20000; i++)
   {
      task_queue_check();
      if (tasks() == 1 && same(g_core, fx_core, FX_CORE_SIZE))
         break;
      retro_sleep(1);
   }
   CHECK(i < 20000);
   task_queue_cancel_task(task);
   pump();
   core_updater_list_free(list);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(recorded(g_core));
   CHECK(!path_is_directory(g_staging));
}

#ifdef HAVE_COMPRESSION
/* The zip's .sig is beside it without .zip; the unzipped core is hashed */
static void test_zip(void)
{
   printf("[zip]\n");
   install_old(g_core);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), true);
   update(false);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(srv_hits(0, true) == 1 && srv_hits(0, false) == 1);
   CHECK(!path_is_directory(g_staging));
}
#endif

static void test_threaded(void)
{
   printf("[threaded]\n");
   install_old(g_core);
   srv_set(0, FX_NAME, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good), false);
   update(false);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(!path_is_directory(g_staging));
}
#else
/* No keys: today's download, and no .sig asked for */
static void test_inactive(void)
{
   static uint8_t unsigned_core[3000];
   printf("[no keys]\n");
   memset(unsigned_core, 0x5a, sizeof(unsigned_core));
   install_old(g_core);
   srv_set(0, FX_NAME, unsigned_core, sizeof(unsigned_core), NULL, 0,
         false);
   update(false);
   CHECK(same(g_core, unsigned_core, sizeof(unsigned_core)));
   CHECK(srv_hits(0, true) == 0);
}
#endif

int main(void)
{
   char store[300];
   unsigned i;
   setvbuf(stdout, NULL, _IONBF, 0);
   path_mkdir(TMP_DIR);
   fill_pathname_join_special(g_cores, TMP_DIR, "cores", sizeof(g_cores));
   fill_pathname_join_special(g_assets, TMP_DIR, "assets", sizeof(g_assets));
   fill_pathname_join_special(g_core, g_cores, FX_NAME, sizeof(g_core));
   sig_of(g_core, g_core_sig, sizeof(g_core_sig));
   fill_pathname_join_special(g_other, g_cores, OTHER_NAME, sizeof(g_other));
   sig_of(g_other, g_other_sig, sizeof(g_other_sig));
   fill_pathname_join_special(g_staging, g_cores, "." FX_NAME ".staging",
         sizeof(g_staging));
   fill_pathname_join_special(g_backups, g_assets,
         "core_backups/test_libretro", sizeof(g_backups));
   fill_pathname_join_special(store, TMP_DIR, "store", sizeof(store));
   path_mkdir(g_cores);
   path_mkdir(g_assets);
   memset(old_core, 0x11, OLD_SIZE);
   fx_make_core();

   network_init();
   net_http_init();
   if (!(srv_lock = slock_new()) || !server_start())
   {
      printf("FAIL: could not start the loopback server\n");
      return 1;
   }
   srv_use(1);

   task_queue_init(false, queue_msg);
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CHECK(core_trust_runtime_init(store, g_cores, "so"));
   test_signed();
   test_first_install();
   test_stale_staging();
   test_staging_past_cap();
   test_refusals();
   test_checks_off();
   test_move_backup();
   test_cancel_after_move();
   test_compressed_backup();
#ifdef HAVE_COMPRESSION
   test_zip();
   test_check_error();
   test_zip_extra();
#endif
   pump();
   task_queue_deinit();
   task_queue_init(true, NULL);
   test_threaded();
#else
   CHECK(!core_trust_runtime_init(store, g_cores, "so"));
   test_inactive();
#endif
   core_trust_runtime_deinit();
   pump();
   task_queue_deinit();
   task_queue_unset_threaded();

   server_stop();
   net_http_deinit();
   slock_free(srv_lock);
   for (i = 0; i < 2; i++)
      free(srv[i].served);
   free(fx_core);
   printf("%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
