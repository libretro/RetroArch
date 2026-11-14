/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* Core backups and restores with core signatures
 * (tasks/task_core_backup.c): a backup keeps the core's .sig beside
 * it; a restore or Install or Restore a Core is checked before it
 * replaces a core, and anything but a pass goes to the Security
 * Warning, whose Install and Trust installs only the file it showed.
 * Against the shipping backup task and trust store. Linux and
 * mingw-w64; TEST_KEY=0 has no keys, so today's restore; PLAY=1
 * (Linux) is a Play Store build. */

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
#include <lrc_hash.h>

#include "../../../command.h"
#include "../../../core_backup.h"
#include "../../../core_info.h"
#include "../../../core_trust_runtime.h"
#include "../../../menu/menu_driver.h"
#include "../../../msg_hash.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"
#include "../core_signature/core_signature_fixtures.h"

#if defined(ANDROID)
#include <unistd.h>
#endif

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

/* The slice clock, wrapped (Makefile.sig): a step of 0 is the real
 * clock; any other spends a slice's budget at its first look, so a
 * task does one work item per tick */
static retro_time_t g_clock_now;
static retro_time_t g_clock_step;

retro_time_t __real_cpu_features_get_time_usec(void);

retro_time_t __wrap_cpu_features_get_time_usec(void)
{
   if (!g_clock_step)
      return __real_cpu_features_get_time_usec();
   return g_clock_now += g_clock_step;
}

/* core_trust_default_path's folder, here the test's own */
bool fill_pathname_application_data(char *s, size_t len)
{
   strlcpy(s, TMP_DIR, len);
   return true;
}

/* The menu's side of the Security Warning */
static bool g_can_open = true;
void menu_core_warning_open(void) { }
bool menu_core_warning_can_open(void) { return g_can_open; }
bool menu_core_warning_user_only(void) { return false; }

/* ---- helpers ---- */

static uint8_t *fx_core;
static char g_cores[256];
static char g_assets[256];
static char g_core[300];
static char g_core_sig[300];
static char g_core_old[310];
static char g_backups[300];
static char g_downloads[256];
static char g_restore_dir[300];
/* A core installed before each restore lane */
static uint8_t g_old[4096];

#if defined(ANDROID)
/* Play feature delivery; deleting a core removes its link */
static char g_apk_core[300];
static bool g_pfd_enabled;
static bool g_pfd_installed;
static int  g_pfd_deletes;
bool play_feature_delivery_enabled(void) { return g_pfd_enabled; }
bool play_feature_delivery_core_installed(const char *core_file)
{
   return g_pfd_installed && !strcmp(core_file, FX_NAME);
}
bool play_feature_delivery_delete(const char *core_file)
{
   g_pfd_deletes++;
   if (!play_feature_delivery_core_installed(core_file) || unlink(g_core))
      return false;
   g_pfd_installed = false;
   return true;
}
#endif

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

#ifdef HAVE_CORE_SIGNING_TEST_KEY
static void sha(const uint8_t *data, size_t len, uint8_t *out)
{
   struct sha256_state st;
   sha256_stream_init(&st, 0);
   sha256_stream_update(&st, data, len);
   sha256_stream_final(&st, out);
}
#endif

/* A backup as RetroArch names it, holding data, sig beside it */
static void make_backup(char *path, size_t len, const char *dir,
      const uint8_t *data, size_t size, const uint8_t *sig, size_t sig_len)
{
   static unsigned stamp = 0;
   char name[128];
   char s[300];
   sprintf(name, "%s.20261109T1200%02u.%08lx.0.lcbk", FX_NAME,
         stamp++ % 60, (unsigned long)encoding_crc32(0, data, size));
   fill_pathname_join_special(path, dir, name, len);
   put(path, data, size);
   sig_of(path, s, sizeof(s));
   if (path_is_valid(s))
      filestream_delete(s);
   if (sig)
      put(s, sig, sig_len);
}

static void install_old(void)
{
   put(g_core, g_old, sizeof(g_old));
   if (path_is_valid(g_core_sig))
      filestream_delete(g_core_sig);
}

static bool restore(const char *b, bool own_backup)
{
   bool loaded = false;
   bool pushed = task_push_core_restore(b, g_cores, &loaded, own_backup,
         NULL);
   pump();
   return pushed;
}

#ifdef HAVE_CORE_SIGNING_TEST_KEY
static uint64_t newest(void)
{
   uint8_t key[CORE_SHA256_LEN];
   uint64_t t          = 0;
   core_trust_set_t *s = core_trust_runtime_get();
   if (s && core_folder_key(g_core, key))
      t = core_trust_newest(s, key, FX_NAME);
   if (s)
      core_trust_set_put(s);
   return t;
}

static bool trusted(const uint8_t *data, size_t len)
{
   uint8_t digest[CORE_SHA256_LEN];
   bool ok             = false;
   core_trust_set_t *s = core_trust_runtime_get();
   sha(data, len, digest);
   if (s)
   {
      ok = core_trust_has_hash(s, digest);
      core_trust_set_put(s);
   }
   return ok;
}

static bool warning(enum core_check_result want)
{
   enum core_warning_kind kind;
   enum core_check_result result;
   return core_warning_pending(&kind, &result)
      && kind == CORE_WARNING_INSTALL && result == want;
}
#endif

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

/* ---- restores ---- */

#ifdef HAVE_CORE_SIGNING_TEST_KEY
static void test_restore_signed(void)
{
   char b[300];
   printf("[restore a signed backup]\n");
   install_old();
   make_backup(b, sizeof(b), g_backups, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good));
   CHECK(restore(b, true));
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(!path_is_directory(g_restore_dir));
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(newest() == FX_TIME);
}

/* RetroArch's own backup may be older than the newest build seen */
static void test_restore_own_older(void)
{
   char b[300];
   printf("[restore an older own backup]\n");
   install_old();
   make_backup(b, sizeof(b), g_backups, fx_core, FX_CORE_SIZE,
         fx_sig_older, sizeof(fx_sig_older));
   CHECK(restore(b, true));
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_older, sizeof(fx_sig_older)));
   CHECK(!core_warning_pending(NULL, NULL));
   /* The newest build seen stays */
   CHECK(newest() == FX_TIME);
}

static bool first_task(retro_task_t *task, void *user_data)
{
   *(retro_task_t**)user_data = task;
   return true;
}

/* Cancelled while the staged copy is hashed: the hash file is closed
 * before the staged files go (Windows), and the core is kept */
static void test_cancel_during_check(void)
{
   task_finder_data_t find_data;
   retro_task_t *task = NULL;
   bool loaded        = false;
   char b[300];
   char bsig[300];
   char tmp_sig[300];
   int i;
   printf("[restore cancelled during its check]\n");
   install_old();
   put(g_core_sig, fx_sig_older, sizeof(fx_sig_older));
   make_backup(b, sizeof(b), g_backups, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good));
   fill_pathname_join_special(tmp_sig, g_restore_dir, FX_NAME ".sig",
         sizeof(tmp_sig));
   g_clock_step = 1000000;
   CHECK(task_push_core_restore(b, g_cores, &loaded, true, NULL));
   /* Copied beside the staged core as the check is entered */
   for (i = 0; i < 1000 && !path_is_valid(tmp_sig); i++)
      task_queue_check();
   /* Opens the staged core and hashes one chunk of several */
   task_queue_check();
   find_data.func     = first_task;
   find_data.userdata = &task;
   task_queue_find(&find_data);
   CHECK(i < 1000 && task);
   CHECK(same(g_core, g_old, sizeof(g_old)));
   if (task)
      task_queue_cancel_task(task);
   g_clock_step = 0;
   pump();
   CHECK(same(g_core, g_old, sizeof(g_old)));
   CHECK(same(g_core_sig, fx_sig_older, sizeof(fx_sig_older)));
   CHECK(!path_is_directory(g_restore_dir));
   CHECK(!core_warning_pending(NULL, NULL));
   sig_of(b, bsig, sizeof(bsig));
   filestream_delete(bsig);
   filestream_delete(b);
}

/* Install or Restore a Core: a picked .lcbk isn't RetroArch's own */
static void test_install_older_warns(void)
{
   char b[300];
   printf("[install an older .lcbk: warning, Cancel]\n");
   install_old();
   make_backup(b, sizeof(b), g_downloads, fx_core, FX_CORE_SIZE,
         fx_sig_older, sizeof(fx_sig_older));
   CHECK(restore(b, false));
   CHECK(warning(CORE_CHECK_OLDER));
   CHECK(same(g_core, g_old, sizeof(g_old)));
   CHECK(!path_is_directory(g_restore_dir));
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   pump();
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(same(g_core, g_old, sizeof(g_old)));
}

/* A backup made before core signatures has no .sig */
static void test_unsigned_install_and_trust(void)
{
   static uint8_t data[3000];
   char b[300];
   printf("[unsigned backup: Install and Trust]\n");
   memset(data, 0x22, sizeof(data));
   install_old();
   put(g_core_sig, fx_sig_good, sizeof(fx_sig_good));
   make_backup(b, sizeof(b), g_backups, data, sizeof(data), NULL, 0);
   CHECK(restore(b, true));
   CHECK(warning(CORE_CHECK_UNSIGNED));
   CHECK(core_warning_choose(CORE_WARNING_INSTALL_AND_TRUST));
   pump();
   CHECK(same(g_core, data, sizeof(data)));
   CHECK(!path_is_valid(g_core_sig));
   CHECK(trusted(data, sizeof(data)));
   CHECK(!core_warning_pending(NULL, NULL));
}

/* Install and Trust installs only the file the warning showed */
static void test_changed_after_warning(void)
{
   static uint8_t data[3000];
   static uint8_t swapped[3000];
   char b[300];
   int errs = g_err_count;
   printf("[changed after the warning]\n");
   memset(data, 0x33, sizeof(data));
   memset(swapped, 0x44, sizeof(swapped));
   install_old();
   make_backup(b, sizeof(b), g_downloads, data, sizeof(data), NULL, 0);
   CHECK(restore(b, false));
   CHECK(warning(CORE_CHECK_UNSIGNED));
   put(b, swapped, sizeof(swapped));
   g_err_expected = true;
   CHECK(core_warning_choose(CORE_WARNING_INSTALL_AND_TRUST));
   pump();
   g_err_expected = false;
   CHECK(g_err_count == errs + 1);
   CHECK(strstr(g_err_last, "Not the core the warning was about"));
   CHECK(same(g_core, g_old, sizeof(g_old)));
   CHECK(!trusted(data, sizeof(data)));
   CHECK(!trusted(swapped, sizeof(swapped)));
   CHECK(!path_is_directory(g_restore_dir));
}

/* Trusted by its hash: the .sig that failed isn't placed beside it */
static void test_trust_drops_failed_sig(void)
{
   char b[300];
   printf("[unknown key: Install and Trust]\n");
   install_old();
   make_backup(b, sizeof(b), g_downloads, fx_core, FX_CORE_SIZE,
         fx_sig_key2, sizeof(fx_sig_key2));
   CHECK(restore(b, false));
   CHECK(warning(CORE_CHECK_UNKNOWN_KEY));
   CHECK(core_warning_choose(CORE_WARNING_INSTALL_AND_TRUST));
   pump();
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(!path_is_valid(g_core_sig));
   CHECK(trusted(fx_core, FX_CORE_SIZE));
   CHECK(!path_is_directory(g_restore_dir));
}

static void test_checks_off(void)
{
   static uint8_t data[3000];
   char b[300];
   printf("[checks off]\n");
   memset(data, 0x55, sizeof(data));
   CHECK(core_trust_runtime_set_checks(false));
   install_old();
   make_backup(b, sizeof(b), g_downloads, data, sizeof(data), NULL, 0);
   CHECK(restore(b, false));
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(same(g_core, data, sizeof(data)));
   CHECK(core_trust_runtime_set_checks(true));
}

/* No menu to ask on: nothing installed, and the user is told */
static void test_no_screen(void)
{
   static uint8_t data[3000];
   char b[300];
   int osd = g_osd_count;
   printf("[no screen to ask on]\n");
   memset(data, 0x66, sizeof(data));
   g_can_open = false;
   install_old();
   make_backup(b, sizeof(b), g_backups, data, sizeof(data), NULL, 0);
   CHECK(restore(b, true));
   g_can_open = true;
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(same(g_core, g_old, sizeof(g_old)));
   CHECK(g_osd_count == osd + 1);
}

/* Install and Trust that can't run the restore again says so */
static void test_trust_backup_gone(void)
{
   static uint8_t data[3000];
   char b[300];
   int osd;
   printf("[Install and Trust, backup gone]\n");
   memset(data, 0x99, sizeof(data));
   install_old();
   make_backup(b, sizeof(b), g_downloads, data, sizeof(data), NULL, 0);
   CHECK(restore(b, false));
   CHECK(warning(CORE_CHECK_UNSIGNED));
   filestream_delete(b);
   osd = g_osd_count;
   CHECK(core_warning_choose(CORE_WARNING_INSTALL_AND_TRUST));
   pump();
   CHECK(g_osd_count == osd + 1);
   CHECK(same(g_core, g_old, sizeof(g_old)));
   CHECK(!trusted(data, sizeof(data)));
}

/* Trusted by its hash (test_trust_drops_failed_sig): a .sig placed
 * beside a restored core must still verify on its own */
static void test_hash_pass_restores(void)
{
   char b[300];
   printf("[trusted hash: restore, bad .sig]\n");
   install_old();
   make_backup(b, sizeof(b), g_backups, fx_core, FX_CORE_SIZE,
         fx_sig_key2, sizeof(fx_sig_key2));
   CHECK(restore(b, true));
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(!path_is_valid(g_core_sig));
   CHECK(!path_is_directory(g_restore_dir));

   printf("[trusted hash: Install or Restore a Core, bad .sig]\n");
   install_old();
   make_backup(b, sizeof(b), g_downloads, fx_core, FX_CORE_SIZE,
         fx_sig_key2, sizeof(fx_sig_key2));
   CHECK(restore(b, false));
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(!path_is_valid(g_core_sig));

   printf("[trusted hash: Install or Restore a Core, good .sig]\n");
   install_old();
   make_backup(b, sizeof(b), g_downloads, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good));
   CHECK(restore(b, false));
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
}

/* Install and Trust on OLDER: that .sig verified, so it stays. Needs a
 * store where fx_core isn't trusted by its hash */
static void test_install_older_trust(void)
{
   char b[300];
   printf("[install an older .lcbk: Install and Trust]\n");
   install_old();
   make_backup(b, sizeof(b), g_downloads, fx_core, FX_CORE_SIZE,
         fx_sig_older, sizeof(fx_sig_older));
   CHECK(restore(b, false));
   CHECK(warning(CORE_CHECK_OLDER));
   CHECK(core_warning_choose(CORE_WARNING_INSTALL_AND_TRUST));
   pump();
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_older, sizeof(fx_sig_older)));
   CHECK(trusted(fx_core, FX_CORE_SIZE));
   CHECK(newest() == FX_TIME);
}

#if defined(ANDROID)
/* ---- a Play Store build ---- */

/* A core play feature delivery installed: a link to g_old in the APK's
 * read-only lib folder, under another name */
static void install_play(void)
{
   char lib[300];
   fill_pathname_join_special(lib, TMP_DIR, "lib", sizeof(lib));
   path_mkdir(lib);
   fill_pathname_join_special(g_apk_core, lib, "libtest.so",
         sizeof(g_apk_core));
   put(g_apk_core, g_old, sizeof(g_old));
   unlink(g_core);
   if (path_is_valid(g_core_sig))
      filestream_delete(g_core_sig);
   CHECK(symlink("../lib/libtest.so", g_core) == 0);
   g_pfd_enabled   = true;
   g_pfd_installed = true;
}

static void uninstall_play(void)
{
   unlink(g_core);
   g_pfd_enabled   = false;
   g_pfd_installed = false;
}

/* Staged beside the link, which goes only once the core passed */
static void test_play_restore(void)
{
   char b[300];
   int deletes = g_pfd_deletes;
   printf("[Play-installed core: restore a signed backup]\n");
   install_play();
   make_backup(b, sizeof(b), g_backups, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good));
   CHECK(restore(b, true));
   CHECK(g_pfd_deletes == deletes + 1);
   CHECK(same(g_core, fx_core, FX_CORE_SIZE));
   CHECK(same(g_core_sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(same(g_apk_core, g_old, sizeof(g_old)));
   CHECK(!path_is_directory(g_restore_dir));
   CHECK(!core_warning_pending(NULL, NULL));
   uninstall_play();
}

static void test_play_cancel_keeps_core(void)
{
   static uint8_t data[3000];
   char b[300];
   int deletes = g_pfd_deletes;
   printf("[Play-installed core: warning, Cancel]\n");
   memset(data, 0x88, sizeof(data));
   install_play();
   make_backup(b, sizeof(b), g_downloads, data, sizeof(data), NULL, 0);
   CHECK(restore(b, false));
   CHECK(warning(CORE_CHECK_UNSIGNED));
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   pump();
   CHECK(g_pfd_deletes == deletes);
   CHECK(same(g_core, g_old, sizeof(g_old)));
   CHECK(!path_is_directory(g_restore_dir));
   uninstall_play();
}

/* A link play feature delivery didn't make is still refused */
static void test_other_link_refused(void)
{
   char b[300];
   int errs    = g_err_count;
   int deletes = g_pfd_deletes;
   printf("[link to a file of another name: refused]\n");
   install_play();
   g_pfd_installed = false;
   make_backup(b, sizeof(b), g_backups, fx_core, FX_CORE_SIZE,
         fx_sig_good, sizeof(fx_sig_good));
   g_err_expected = true;
   CHECK(restore(b, true));
   g_err_expected = false;
   CHECK(g_err_count > errs);
   CHECK(g_pfd_deletes == deletes);
   CHECK(same(g_core, g_old, sizeof(g_old)));
   CHECK(same(g_apk_core, g_old, sizeof(g_old)));
   CHECK(!core_warning_pending(NULL, NULL));
   uninstall_play();
}
#endif
#else
/* No keys: today's restore, nothing checked */
static void test_inactive(void)
{
   static uint8_t data[3000];
   char b[300];
   char tmp[300];
   printf("[no keys: today's restore]\n");
   memset(data, 0x77, sizeof(data));
   install_old();
   make_backup(b, sizeof(b), g_backups, data, sizeof(data), NULL, 0);
   CHECK(restore(b, true));
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(same(g_core, data, sizeof(data)));
   CHECK(!path_is_directory(g_restore_dir));
   strlcpy(tmp, g_core, sizeof(tmp));
   strlcat(tmp, ".tmp", sizeof(tmp));
   CHECK(!path_is_valid(tmp));
}
#endif

int main(void)
{
   char store[300];
   setvbuf(stdout, NULL, _IONBF, 0);
   path_mkdir(TMP_DIR);
   fill_pathname_join_special(g_cores, TMP_DIR, "cores", sizeof(g_cores));
   fill_pathname_join_special(g_assets, TMP_DIR, "assets", sizeof(g_assets));
   fill_pathname_join_special(g_downloads, TMP_DIR, "downloads",
         sizeof(g_downloads));
   fill_pathname_join_special(g_core, g_cores, FX_NAME, sizeof(g_core));
   sig_of(g_core, g_core_sig, sizeof(g_core_sig));
   strlcpy(g_core_old, g_core_sig, sizeof(g_core_old));
   strlcat(g_core_old, ".old", sizeof(g_core_old));
   fill_pathname_join_special(g_backups, g_assets,
         "core_backups/test_libretro", sizeof(g_backups));
   fill_pathname_join_special(g_restore_dir, g_cores,
         "." FX_NAME ".restore", sizeof(g_restore_dir));
   fill_pathname_join_special(store, TMP_DIR, "store", sizeof(store));
   path_mkdir(g_cores);
   path_mkdir(g_backups);
   path_mkdir(g_downloads);
   memset(g_old, 0x11, sizeof(g_old));
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
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CHECK(core_trust_runtime_init(store, g_cores, "so"));
   test_restore_signed();
   test_restore_own_older();
   test_cancel_during_check();
   test_install_older_warns();
   test_unsigned_install_and_trust();
   test_changed_after_warning();
   test_trust_drops_failed_sig();
   test_checks_off();
   test_no_screen();
   test_trust_backup_gone();
   test_hash_pass_restores();
   /* A new store: fx_core trusted by nothing but its .sig again */
   install_old();
   core_trust_runtime_deinit();
   fill_pathname_join_special(store, TMP_DIR, "store2", sizeof(store));
   CHECK(core_trust_runtime_init(store, g_cores, "so"));
   pump();
   test_restore_signed();
   test_install_older_trust();
#if defined(ANDROID)
   test_play_restore();
   test_play_cancel_keeps_core();
   test_other_link_refused();
#endif
#else
   CHECK(!core_trust_runtime_init(store, g_cores, "so"));
   test_inactive();
#endif
   pump();
   task_queue_deinit();

   /* The .sig copy and the restore's check on the task thread */
   task_queue_init(true, NULL);
   test_backup_keeps_sig();
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   test_restore_signed();
#endif
   core_trust_runtime_deinit();
   pump();
   task_queue_deinit();
   task_queue_unset_threaded();

   free(fx_core);
   printf("%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
