/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* Core backups with core signatures (tasks/task_core_backup.c): a
 * backup keeps the core's .sig beside it, and pruning drops it. Against
 * the shipping backup task. Linux and mingw-w64. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <retro_dirent.h>
#include <retro_timers.h>
#include <streams/file_stream.h>
#include <encodings/crc32.h>

#include "../../../command.h"
#include "../../../core_backup.h"
#include "../../../core_info.h"
#include "../../../menu/menu_driver.h"
#include "../../../msg_hash.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"
#include "../core_signature/core_signature_fixtures.h"

#define TMP_DIR "core_backup_sig_test_tmp"

static int checks   = 0;
static int failures = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---- what the backup task and the trust store call ---- */

static int  g_osd_count;
static char g_osd_last[256];
/* Errors are counted; printed only when no lane expects them */
static int  g_err_count;
static char g_err_last[256];
static bool g_err_expected;

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   g_err_count++;
   strlcpy(g_err_last, fmt, sizeof(g_err_last));
   if (g_err_expected)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "msg";
}

bool command_event(enum event_command cmd, void *data)
{
   (void)cmd;
   (void)data;
   return true;
}

bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{
   (void)state;
   (void)data;
   return false;
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)len; (void)prio; (void)duration; (void)flush; (void)title;
   (void)icon; (void)category;
   g_osd_count++;
   strlcpy(g_osd_last, msg, sizeof(g_osd_last));
}

bool core_info_find(const char *core_path, core_info_t **core_info)
{
   (void)core_path;
   (void)core_info;
   return false;
}

bool core_info_get_core_lock(const char *core_path, bool validate_path)
{
   (void)core_path;
   (void)validate_path;
   return false;
}

size_t frontend_driver_get_core_extension(char *s, size_t len)
{
   return strlcpy(s, "so", len);
}

void task_window_progress_cb(retro_task_t *task) { (void)task; }

struct menu_state *menu_state_get_ptr(void)
{
   static struct menu_state st;
   return &st;
}

void menu_contentless_cores_free(void) { }

/* core_trust_default_path's folder, here the test's own */
bool fill_pathname_application_data(char *s, size_t len)
{
   strlcpy(s, TMP_DIR, len);
   return true;
}

/* ---- helpers ---- */

static uint8_t *fx_core;
static char g_cores[256];
static char g_assets[256];
static char g_core[300];
static char g_core_sig[300];
static char g_core_old[310];
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

static void empty_backups(void)
{
   char p[300];
   bool deleted = true;
   while (deleted && count(g_backups, "", p, sizeof(p)))
      deleted = filestream_delete(p) == 0;
   CHECK(deleted);
}

/* The installed core: fx_core with byte i flipped, and sig beside it */
static void install(unsigned i, const uint8_t *sig, size_t sig_len)
{
   fx_core[i] ^= 0x01;
   put(g_core, fx_core, FX_CORE_SIZE);
   fx_core[i] ^= 0x01;
   if (path_is_valid(g_core_sig))
      filestream_delete(g_core_sig);
   if (sig)
      put(g_core_sig, sig, sig_len);
}

static void backup(enum core_backup_mode mode, size_t history)
{
   CHECK(task_push_core_backup(g_core, "Test Core", 0, mode, history,
            g_assets, true, NULL, NULL) != NULL);
   pump();
}

/* ---- backups ---- */

static void test_backup_keeps_sig(void)
{
   char b[300];
   char bsig[300];
   printf("[backup keeps .sig]\n");
   empty_backups();
   install(0, fx_sig_good, sizeof(fx_sig_good));
   backup(CORE_BACKUP_MODE_MANUAL, 0);
   CHECK(count(g_backups, ".lcbk", b, sizeof(b)) == 1);
   sig_of(b, bsig, sizeof(bsig));
   CHECK(same(bsig, fx_sig_good, sizeof(fx_sig_good)));
}

static void test_backup_without_sig(void)
{
   printf("[backup without .sig]\n");
   empty_backups();
   install(1, NULL, 0);
   backup(CORE_BACKUP_MODE_MANUAL, 0);
   CHECK(count(g_backups, ".lcbk", NULL, 0) == 1);
   CHECK(count(g_backups, ".sig", NULL, 0) == 0);
}

/* History 1: the older automatic backup goes, and its .sig with it */
static void test_prune_drops_sig(void)
{
   char b[300];
   char bsig[300];
   char crc[16];
   printf("[prune drops .sig]\n");
   empty_backups();
   install(2, fx_sig_good, sizeof(fx_sig_good));
   backup(CORE_BACKUP_MODE_AUTO, 1);
   install(3, fx_sig_older, sizeof(fx_sig_older));
   backup(CORE_BACKUP_MODE_AUTO, 1);
   CHECK(count(g_backups, ".lcbk", b, sizeof(b)) == 1);
   CHECK(count(g_backups, ".lcbk.sig", NULL, 0) == 1);
   /* The one left is the kept backup's own, whichever was kept (both
    * may carry the same second) */
   fx_core[3] ^= 0x01;
   sprintf(crc, ".%08lx.", (unsigned long)encoding_crc32(0, fx_core,
         FX_CORE_SIZE));
   fx_core[3] ^= 0x01;
   sig_of(b, bsig, sizeof(bsig));
   if (strstr(b, crc))
      CHECK(same(bsig, fx_sig_older, sizeof(fx_sig_older)));
   else
      CHECK(same(bsig, fx_sig_good, sizeof(fx_sig_good)));
}

/* An update's move: the replaced core goes into the backups as it is,
 * with a copy of its .sig, which isn't left beside the new core */
static void test_move_keeps_sig(void)
{
   char staging[300];
   char staged[300];
   char b[300];
   char bsig[300];
   printf("[install move keeps .sig]\n");
   empty_backups();
   install(4, fx_sig_older, sizeof(fx_sig_older));
   put(g_core_old, "stray", 5);
   fill_pathname_join_special(staging, g_cores, "." FX_NAME ".staging",
         sizeof(staging));
   fill_pathname_join_special(staged, staging, FX_NAME, sizeof(staged));
   path_mkdir(staging);
   put(staged, fx_core, FX_CORE_SIZE);
   CHECK(task_push_core_backup_install(g_core, staged, "Test Core", 0, 1,
            g_assets, true, NULL, NULL) != NULL);
   pump();
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(!path_is_valid(g_core_sig) && !path_is_valid(g_core_old));
   CHECK(count(g_backups, ".lcbk", b, sizeof(b)) == 1);
   sig_of(b, bsig, sizeof(bsig));
   CHECK(same(bsig, fx_sig_older, sizeof(fx_sig_older)));
   path_rmdir(staging);
}

/* The new core can't go in: the old one stays with its .sig */
static void test_move_fails_keeps_sig(void)
{
   char staging[300];
   char staged[300];
   char b[300];
   char bsig[300];
   printf("[install move fails]\n");
   empty_backups();
   install(5, fx_sig_older, sizeof(fx_sig_older));
   fill_pathname_join_special(staging, g_cores, "." FX_NAME ".staging",
         sizeof(staging));
   fill_pathname_join_special(staged, staging, FX_NAME, sizeof(staged));
   path_mkdir(staging);
   put(staged, fx_core, FX_CORE_SIZE);
   g_err_expected = true;
   CHECK(task_push_core_backup_install(g_core, staged, "Test Core", 0, 1,
            g_assets, true, NULL, NULL) != NULL);
   filestream_delete(staged);
   pump();
   g_err_expected = false;
   fx_core[5] ^= 0x01;
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   fx_core[5] ^= 0x01;
   CHECK(same(g_core_sig, fx_sig_older, sizeof(fx_sig_older)));
   CHECK(!path_is_valid(g_core_old));
   CHECK(count(g_backups, ".lcbk", b, sizeof(b)) == 1);
   sig_of(b, bsig, sizeof(bsig));
   CHECK(same(bsig, fx_sig_older, sizeof(fx_sig_older)));
   path_rmdir(staging);
}

/* The move fails and the copy that follows is cancelled: no backup .sig
 * is left without its backup */
static void test_move_fails_cancelled(void)
{
   char staging[300];
   char staged[300];
   retro_task_t *task;
   int i;
   printf("[install move fails, copy cancelled]\n");
   empty_backups();
   install(8, fx_sig_older, sizeof(fx_sig_older));
   fill_pathname_join_special(staging, g_cores, "." FX_NAME ".staging",
         sizeof(staging));
   fill_pathname_join_special(staged, staging, FX_NAME, sizeof(staged));
   path_mkdir(staging);
   put(staged, fx_core, FX_CORE_SIZE);
   CHECK((task = (retro_task_t*)task_push_core_backup_install(g_core,
            staged, "Test Core", 0, 1, g_assets, true, NULL, NULL)) != NULL);
   filestream_delete(staged);
   /* The copy opens the backup once the move is rolled back */
   for (i = 0; i < 1000 && !count(g_backups, ".lcbk", NULL, 0); i++)
      task_queue_check();
   CHECK(i < 1000);
   if (task)
      task_queue_cancel_task(task);
   pump();
   fx_core[8] ^= 0x01;
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   fx_core[8] ^= 0x01;
   CHECK(same(g_core_sig, fx_sig_older, sizeof(fx_sig_older)));
   CHECK(count(g_backups, ".sig", NULL, 0) == 0);
   path_rmdir(staging);
}

/* A .sig.old a crash left beside a core with none never comes back as
 * its .sig when the new core can't go in */
static void test_move_fails_stray(void)
{
   char staging[300];
   char staged[300];
   printf("[install move fails, stray .sig.old]\n");
   empty_backups();
   install(7, NULL, 0);
   put(g_core_old, "stray", 5);
   fill_pathname_join_special(staging, g_cores, "." FX_NAME ".staging",
         sizeof(staging));
   fill_pathname_join_special(staged, staging, FX_NAME, sizeof(staged));
   path_mkdir(staging);
   put(staged, fx_core, FX_CORE_SIZE);
   g_err_expected = true;
   CHECK(task_push_core_backup_install(g_core, staged, "Test Core", 0, 1,
            g_assets, true, NULL, NULL) != NULL);
   filestream_delete(staged);
   pump();
   g_err_expected = false;
   fx_core[7] ^= 0x01;
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   fx_core[7] ^= 0x01;
   CHECK(!path_is_valid(g_core_sig) && !path_is_valid(g_core_old));
   path_rmdir(staging);
}

/* Already backed up: the new core goes in at the end, and the old .sig
 * goes before it */
static void test_backed_up_install_drops_sig(void)
{
   char staging[300];
   char staged[300];
   printf("[install over a core already backed up]\n");
   empty_backups();
   install(6, fx_sig_older, sizeof(fx_sig_older));
   backup(CORE_BACKUP_MODE_AUTO, 2);
   CHECK(count(g_backups, ".lcbk", NULL, 0) == 1);
   put(g_core_old, "stray", 5);
   fill_pathname_join_special(staging, g_cores, "." FX_NAME ".staging",
         sizeof(staging));
   fill_pathname_join_special(staged, staging, FX_NAME, sizeof(staged));
   path_mkdir(staging);
   put(staged, fx_core, FX_CORE_SIZE);
   CHECK(task_push_core_backup_install(g_core, staged, "Test Core", 0, 2,
            g_assets, true, NULL, NULL) != NULL);
   pump();
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(!path_is_valid(g_core_sig) && !path_is_valid(g_core_old));
   CHECK(count(g_backups, ".lcbk", NULL, 0) == 1);
   path_rmdir(staging);
}

int main(void)
{
   setvbuf(stdout, NULL, _IONBF, 0);
   path_mkdir(TMP_DIR);
   fill_pathname_join_special(g_cores, TMP_DIR, "cores", sizeof(g_cores));
   fill_pathname_join_special(g_assets, TMP_DIR, "assets", sizeof(g_assets));
   fill_pathname_join_special(g_core, g_cores, FX_NAME, sizeof(g_core));
   sig_of(g_core, g_core_sig, sizeof(g_core_sig));
   strlcpy(g_core_old, g_core_sig, sizeof(g_core_old));
   strlcat(g_core_old, ".old", sizeof(g_core_old));
   fill_pathname_join_special(g_backups, g_assets,
         "core_backups/test_libretro", sizeof(g_backups));
   path_mkdir(g_cores);
   path_mkdir(g_backups);
   fx_make_core();

   task_queue_init(false, NULL);
   test_backup_keeps_sig();
   test_backup_without_sig();
   test_prune_drops_sig();
   test_move_keeps_sig();
   test_move_fails_keeps_sig();
   test_move_fails_stray();
   test_move_fails_cancelled();
   test_backed_up_install_drops_sig();
   pump();
   task_queue_deinit();

   /* A backup's .sig copy on the task thread */
   task_queue_init(true, NULL);
   test_backup_keeps_sig();
   pump();
   task_queue_deinit();
   task_queue_unset_threaded();

   free(fx_core);
   printf("%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
