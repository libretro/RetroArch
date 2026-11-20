/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#if defined(__linux__) && !defined(_XOPEN_SOURCE)
/* symlink; strict C89 hides it */
#define _XOPEN_SOURCE 700
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <retro_dirent.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "../../../core_trust_runtime.h"
#include "../../../runloop.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"

#define TMP_DIR "core_trust_runtime_test_tmp"

static unsigned checks;
static unsigned failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* Frontend stand-ins: what the module calls outside itself */
const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   static char buf[4][32];
   static unsigned n;
   char *s = buf[n++ & 3];
   sprintf(s, "msg %d", (int)msg);
   return s;
}

static settings_t test_settings;

settings_t *config_get_ptr(void)
{
   return &test_settings;
}

/* Log lines that say the build trusts a test key */
static unsigned log_test_key;

void RARCH_LOG(const char *fmt, ...)
{
   if (strstr(fmt, "test signing key"))
      log_test_key++;
}

static unsigned warn_count;

void RARCH_WARN(const char *fmt, ...)
{
   (void)fmt;
   warn_count++;
}

#define OSD_MAX 16
static char osd[OSD_MAX][32];
static unsigned osd_dur[OSD_MAX];
static unsigned osd_count;

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush,
      char *title, enum message_queue_icon icon,
      enum message_queue_category category)
{
   (void)len; (void)prio; (void)flush;
   (void)title; (void)icon; (void)category;
   if (osd_count < OSD_MAX)
   {
      strlcpy(osd[osd_count], msg, sizeof(osd[0]));
      osd_dur[osd_count] = duration;
   }
   osd_count++;
}

/* core_trust_default_path's folder, here the test's own; none once
 * when app_data_fails is set */
static bool app_data_fails;

bool fill_pathname_application_data(char *s, size_t len)
{
   if (app_data_fails)
   {
      app_data_fails = false;
      return false;
   }
   strlcpy(s, TMP_DIR, len);
   return true;
}

/* How many times msg went on screen */
static unsigned osd_shown(enum msg_hash_enums msg)
{
   char want[32];
   unsigned i;
   unsigned n = 0;
   sprintf(want, "msg %d", (int)msg);
   for (i = 0; i < osd_count && i < OSD_MAX; i++)
      if (string_is_equal(osd[i], want))
         n++;
   return n;
}

#ifdef HAVE_CORE_SIGNING_TEST_KEY
/* Frames msg's last showing lasts, 0 if it never showed */
static unsigned osd_duration(enum msg_hash_enums msg)
{
   char want[32];
   unsigned i;
   unsigned d = 0;
   sprintf(want, "msg %d", (int)msg);
   for (i = 0; i < osd_count && i < OSD_MAX; i++)
      if (string_is_equal(osd[i], want))
         d = osd_dur[i];
   return d;
}
#endif

/* The shared I/O window: a floor of one item per open, then
 * window_items more; 0 is a window already spent */
static unsigned window_items = 1000000;
static unsigned window_left;
#ifdef HAVE_CORE_LOAD_CHECK
static unsigned window_opens;
#endif

void task_nbio_slice_open(nbio_budget_t *b)
{
   b->floor    = 1;
   window_left = window_items;
#ifdef HAVE_CORE_LOAD_CHECK
   window_opens++;
#endif
}

void task_nbio_slice_close(nbio_budget_t *b)
{
   (void)b;
}

bool task_nbio_slice_within_budget(void *ud, size_t avail, size_t len)
{
   nbio_budget_t *b = (nbio_budget_t*)ud;
   (void)avail;
   (void)len;
   if (b->floor)
   {
      b->floor = 0;
      return true;
   }
   if (!window_left)
      return false;
   window_left--;
   return true;
}

static unsigned menu_opens;
static bool menu_can_open = true;

void menu_core_warning_open(void)
{
   menu_opens++;
}

bool menu_core_warning_can_open(void)
{
   return menu_can_open;
}

#ifdef HAVE_CORE_LOAD_CHECK
static unsigned forget_calls;
static char forget_path[256];

void core_load_check_untrust(const char *path)
{
   forget_calls++;
   strlcpy(forget_path, path, sizeof(forget_path));
}
#endif

struct warn_seen
{
   struct core_check c;
   char path[64];
   void *userdata;
   enum core_warning_choice choice;
   unsigned calls;
   bool push_again;
   bool again_pushed;
};

static struct warn_seen seen;

static void warn_cb(enum core_warning_choice choice,
      const struct core_check *c, const char *core_path, void *userdata)
{
   seen.calls++;
   seen.choice   = choice;
   seen.c        = *c;
   seen.userdata = userdata;
   seen.path[0]  = '\0';
   CHECK(core_path);
   if (core_path)
      strlcpy(seen.path, core_path, sizeof(seen.path));
   if (seen.push_again)
   {
      seen.push_again   = false;
      seen.again_pushed = core_warning_push(c, CORE_WARNING_INSTALL,
            core_path, warn_cb, NULL);
   }
}

static bool choices_are(enum core_warning_choice a, int b, int c)
{
   enum core_warning_choice got[CORE_WARNING_CHOICES_MAX];
   unsigned n    = core_warning_choices(got);
   unsigned want = b < 0 ? 1 : c < 0 ? 2 : 3;
   return   n == want
         && got[0] == a
         && (n < 2 || (int)got[1] == b)
         && (n < 3 || (int)got[2] == c);
}

/* The queue runs on its own thread: tests run again with it, under
 * other names */
static bool task_thread;

static char tmp_path[256];
static const char *tmp(const char *name)
{
   char base[64];
   strlcpy(base, task_thread ? "thread_" : "", sizeof(base));
   strlcat(base, name, sizeof(base));
   fill_pathname_join_special(tmp_path, TMP_DIR, base, sizeof(tmp_path));
   return tmp_path;
}

/* Unthreaded, each check runs every task's handler once and retires
 * what finished; threaded, the wait retires every task */
static void pump(unsigned n)
{
   if (task_thread)
      task_queue_wait(NULL, NULL);
   else
      while (n--)
         task_queue_check();
}

static void fake_check(struct core_check *c, const char *name,
      uint8_t byte, enum core_check_result result)
{
   memset(c, 0, sizeof(*c));
   strlcpy(c->rec.name, name, sizeof(c->rec.name));
   memset(c->rec.sha256, byte, CORE_SHA256_LEN);
   c->result = result;
}

static bool write_store(const char *path, unsigned flags)
{
   core_trust_t *t = core_trust_new();
   size_t len      = 0;
   uint8_t *buf;
   bool ok;
   core_trust_set_flag(t, CORE_TRUST_CHECK_OFF,
         (flags & CORE_TRUST_CHECK_OFF) != 0);
   core_trust_set_flag(t, CORE_TRUST_SEEDED,
         (flags & CORE_TRUST_SEEDED) != 0);
   core_trust_set_flag(t, CORE_TRUST_NOTICE_SHOWN,
         (flags & CORE_TRUST_NOTICE_SHOWN) != 0);
   buf = core_trust_serialize(t, &len);
   ok  = buf && filestream_write_file(path, buf, (int64_t)len);
   free(buf);
   core_trust_free(t);
   return ok;
}

/* The flags of the store on disk, or ~0u when it doesn't load */
static unsigned disk_flags(const char *path)
{
   core_trust_t *u = core_trust_new();
   unsigned flags  = ~0u;
   core_trust_set_t *s;
   if (core_trust_load_file(u, path) == CORE_TRUST_LOAD_OK)
   {
      s     = core_trust_get(u);
      flags = core_trust_flags(s);
      core_trust_set_put(s);
   }
   core_trust_free(u);
   return flags;
}

static bool disk_has_sha(const char *path, const uint8_t *sha256)
{
   core_trust_t *u = core_trust_new();
   bool has        = false;
   core_trust_set_t *s;
   if (core_trust_load_file(u, path) == CORE_TRUST_LOAD_OK)
   {
      s   = core_trust_get(u);
      has = core_trust_has_hash(s, sha256);
      core_trust_set_put(s);
   }
   core_trust_free(u);
   return has;
}

static bool disk_has_hash(const char *path, uint8_t byte)
{
   uint8_t sha256[CORE_SHA256_LEN];
   memset(sha256, byte, sizeof(sha256));
   return disk_has_sha(path, sha256);
}

static int RETRO_CALLCONV refuse_rename(const char *old_path,
      const char *new_path)
{
   (void)old_path;
   (void)new_path;
   return -1;
}

/* Every store write fails at its rename while on */
static void saves_fail(bool on)
{
   static struct retro_vfs_interface iface;
   struct retro_vfs_interface_info info;
   iface.rename                    = refuse_rename;
   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                      = on ? &iface : NULL;
   filestream_vfs_init(&info);
}

static void test_inactive(void)
{
   struct core_check c;
   fake_check(&c, "a_libretro.so", 1, CORE_CHECK_UNSIGNED);
   CHECK(!core_trust_runtime_init(tmp("inactive"), NULL, NULL));
   CHECK(!core_trust_runtime_active());
   CHECK(core_trust_runtime_get() == NULL);
   CHECK(!core_trust_runtime_checks_on());
   CHECK(!core_trust_runtime_set_checks(false));
   CHECK(!core_trust_runtime_always(&c));
   CHECK(!core_trust_runtime_apply(&c));
   pump(4);
   CHECK(!path_is_valid(tmp("inactive")));
   CHECK(!core_warning_push(&c, CORE_WARNING_LOAD, "/c/a_libretro.so",
            warn_cb, NULL));
   CHECK(core_trust_runtime_toggle_checks() == CORE_TRUST_TOGGLE_READ_ONLY);
   CHECK(!core_trust_runtime_forget("/c/a_libretro.so"));
   CHECK(core_trust_runtime_state("/c/a_libretro.so")
         == CORE_TRUST_STATE_UNCHECKED);
   CHECK(!core_trust_runtime_can_save());
   core_trust_runtime_deinit();
}

static void test_load_missing(void)
{
   char path[256];
   strlcpy(path, tmp("missing"), sizeof(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(core_trust_runtime_active());
   CHECK(!core_trust_runtime_read_only());
   CHECK(core_trust_runtime_checks_on());
   /* Nothing is written until something changes */
   pump(4);
   CHECK(!path_is_valid(path));
   CHECK(core_trust_runtime_set_checks(false));
   CHECK(!core_trust_runtime_checks_on());
   pump(4);
   CHECK(disk_flags(path) == CORE_TRUST_CHECK_OFF);
   core_trust_runtime_deinit();
}

static void test_load_good(void)
{
   char path[256];
   strlcpy(path, tmp("good"), sizeof(path));
   CHECK(write_store(path, CORE_TRUST_CHECK_OFF));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(!core_trust_runtime_checks_on());
   CHECK(core_trust_runtime_set_checks(true));
   pump(4);
   CHECK(disk_flags(path) == 0);
   core_trust_runtime_deinit();
}

static void test_load_damaged(void)
{
   static const char junk[] = "not a trust store";
   char path[256];
   char bad[256];
   strlcpy(path, tmp("damaged"), sizeof(path));
   strlcpy(bad, tmp("damaged.bad"), sizeof(bad));
   CHECK(filestream_write_file(path, junk, sizeof(junk)));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(!core_trust_runtime_read_only());
   CHECK(core_trust_runtime_checks_on());
   CHECK(path_is_valid(bad));
   CHECK(core_trust_runtime_set_checks(false));
   pump(4);
   CHECK(disk_flags(path) == CORE_TRUST_CHECK_OFF);
   core_trust_runtime_deinit();
}

static void test_load_unreadable(void)
{
   char path[256];
   /* Anything but a file can't be read, and stays where it is */
   strlcpy(path, tmp("unreadable"), sizeof(path));
   CHECK(path_mkdir(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(core_trust_runtime_active());
   CHECK(core_trust_runtime_read_only());
   CHECK(core_trust_runtime_unreadable());
   CHECK(core_trust_runtime_checks_on());
   CHECK(!core_trust_runtime_set_checks(false));
   CHECK(core_trust_runtime_checks_on());
   pump(4);
   core_trust_runtime_deinit();
   CHECK(path_is_directory(path));
}

/* A newer RetroArch's store: read-only, and left as it is */
static void test_load_newer(void)
{
   core_trust_t *t = core_trust_new();
   uint8_t *buf    = NULL;
   void *got       = NULL;
   int64_t got_len = 0;
   size_t len      = 0;
   char path[256];
   strlcpy(path, tmp("newer"), sizeof(path));
   if (t)
   {
      buf = core_trust_serialize(t, &len);
      core_trust_free(t);
   }
   CHECK(buf != NULL);
   if (!buf)
      return;
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = '2';
   CHECK(filestream_write_file(path, buf, (int64_t)len));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(   core_trust_runtime_read_only()
         && core_trust_runtime_newer()
         && !core_trust_runtime_can_save());
   CHECK(core_trust_runtime_toggle_checks() == CORE_TRUST_TOGGLE_READ_ONLY);
   pump(4);
   core_trust_runtime_deinit();
   CHECK(filestream_read_file(path, &got, &got_len));
   CHECK(got && got_len == (int64_t)len && !memcmp(got, buf, len));
   free(got);
   free(buf);
   /* One that can't be read isn't a newer one's */
   strlcpy(path, tmp("newer_dir"), sizeof(path));
   CHECK(path_mkdir(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(core_trust_runtime_read_only() && !core_trust_runtime_newer());
   core_trust_runtime_deinit();
}

static void test_toggle(void)
{
   char path[256];
   strlcpy(path, tmp("toggle"), sizeof(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(core_trust_runtime_toggle_checks() == CORE_TRUST_TOGGLE_CONFIRM_OFF);
   CHECK(core_trust_runtime_checks_on());
   CHECK(core_trust_runtime_set_checks(false));
   CHECK(core_trust_runtime_toggle_checks() == CORE_TRUST_TOGGLE_DONE);
   CHECK(core_trust_runtime_checks_on());
   pump(4);
   core_trust_runtime_deinit();
   strlcpy(path, tmp("toggle_dir"), sizeof(path));
   CHECK(path_mkdir(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(core_trust_runtime_toggle_checks() == CORE_TRUST_TOGGLE_READ_ONLY);
   CHECK(core_trust_runtime_checks_on());
   core_trust_runtime_deinit();
}

static void test_can_save(void)
{
   char path[256];
   strlcpy(path, tmp("can_save"), sizeof(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(core_trust_runtime_can_save());
   /* From a failed write until one goes through */
   saves_fail(true);
   CHECK(core_trust_runtime_set_checks(false));
   pump(4);
   CHECK(!core_trust_runtime_can_save());
   saves_fail(false);
   CHECK(core_trust_runtime_set_checks(true));
   pump(4);
   CHECK(core_trust_runtime_can_save());
   core_trust_runtime_deinit();
   strlcpy(path, tmp("can_save_dir"), sizeof(path));
   CHECK(path_mkdir(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(!core_trust_runtime_can_save());
   core_trust_runtime_deinit();
}

/* What a save killed before its rename left, once an hour old */
static void test_temp_sweep(void)
{
   int64_t now = (int64_t)time(NULL);
   char store[256];
   char old[256];
   char recent[256];
   char fresh[256];
   char other[256];
   char folder[256];
   strlcpy(store, tmp("tmp_other"), sizeof(store));
   CHECK(core_trust_temp_name(other, sizeof(other), store, 4, 0xd));
   strlcpy(store, tmp("tmp_store"), sizeof(store));
   CHECK(core_trust_temp_name(old, sizeof(old), store, 1, 0xa));
   CHECK(core_trust_temp_name(recent, sizeof(recent), store, 2, 0xb));
   CHECK(core_trust_temp_name(fresh, sizeof(fresh), store, 3, 0xc));
   CHECK(core_trust_temp_name(folder, sizeof(folder), store, 5, 0xe));
   CHECK(filestream_write_file(old, "x", 1));
   CHECK(filestream_write_file(recent, "x", 1));
   CHECK(filestream_write_file(fresh, "x", 1));
   CHECK(filestream_write_file(other, "x", 1));
   CHECK(path_set_mtime(old, now - 7200));
   CHECK(path_set_mtime(recent, now - 3500));
   CHECK(path_set_mtime(other, now - 7200));
   /* Named like one, but a folder */
   CHECK(path_mkdir(folder));
   CHECK(path_set_mtime(folder, now - 7200));
   CHECK(core_trust_runtime_init(store, NULL, NULL));
   CHECK(!path_is_valid(old));
   CHECK(path_is_valid(recent));
   CHECK(path_is_valid(fresh));
   CHECK(path_is_valid(other));
   CHECK(path_is_directory(folder));
   core_trust_runtime_deinit();
   /* Read-only: its folder is left alone */
   strlcpy(store, tmp("tmp_dir_store"), sizeof(store));
   CHECK(path_mkdir(store));
   CHECK(core_trust_temp_name(old, sizeof(old), store, 1, 0xa));
   CHECK(filestream_write_file(old, "x", 1));
   CHECK(path_set_mtime(old, now - 7200));
   CHECK(core_trust_runtime_init(store, NULL, NULL));
   CHECK(core_trust_runtime_read_only());
   CHECK(path_is_valid(old));
   core_trust_runtime_deinit();
}

/* A save of the user's own change that fails says so, once a session */
static void test_save_failed_once(void)
{
   struct core_check c;
   char path[256];
   unsigned warns;
   strlcpy(path, tmp("save_failed"), sizeof(path));
   CHECK(write_store(path, 0));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   osd_count = 0;
   warns     = warn_count;
   saves_fail(true);
   fake_check(&c, "a_libretro.so", 1, CORE_CHECK_UNSIGNED);
   CHECK(core_trust_runtime_always(&c));
   pump(8);
   CHECK(osd_shown(MSG_CORE_TRUST_SAVE_FAILED) == 1);
   CHECK(!core_trust_runtime_can_save());
   CHECK(warn_count > warns);
   fake_check(&c, "b_libretro.so", 2, CORE_CHECK_UNSIGNED);
   CHECK(core_trust_runtime_always(&c));
   CHECK(core_trust_runtime_set_checks(false));
   pump(8);
   CHECK(osd_shown(MSG_CORE_TRUST_SAVE_FAILED) == 1);
   saves_fail(false);
   fake_check(&c, "c_libretro.so", 3, CORE_CHECK_UNSIGNED);
   CHECK(core_trust_runtime_always(&c));
   pump(8);
   CHECK(core_trust_runtime_can_save());
   core_trust_runtime_deinit();
}

/* Nothing on screen for a change the user didn't make, or at exit */
static void test_save_failed_quiet(void)
{
   static const uint8_t folder[CORE_SHA256_LEN] = { 7 };
   struct core_check c;
   char path[256];
   unsigned warns;
   strlcpy(path, tmp("save_failed_quiet"), sizeof(path));
   CHECK(write_store(path, 0));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   osd_count = 0;
   saves_fail(true);
   fake_check(&c, "s_libretro.so", 4, CORE_CHECK_OK);
   memcpy(c.folder, folder, sizeof(folder));
   c.rec.build_time = 5;
   c.raise_newest   = true;
   CHECK(core_trust_runtime_apply(&c));
   pump(8);
   CHECK(osd_shown(MSG_CORE_TRUST_SAVE_FAILED) == 0);
   CHECK(!core_trust_runtime_can_save());
   warns = warn_count;
   core_trust_runtime_deinit();
   CHECK(osd_shown(MSG_CORE_TRUST_SAVE_FAILED) == 0);
   CHECK(warn_count > warns);
   saves_fail(false);
}

/* NULL: the default path, a file of its own with the test key, or
 * read-only when there is none */
static void test_default_path(void)
{
   char path[256];
   char release[256];
   CHECK(core_trust_default_path(release, sizeof(release)));
   strlcpy(path, release, sizeof(path));
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   strlcat(path, ".testkey", sizeof(path));
#endif
   CHECK(core_trust_runtime_init(NULL, NULL, NULL));
   CHECK(!core_trust_runtime_read_only());
   CHECK(core_trust_runtime_set_checks(false));
   pump(4);
   CHECK(disk_flags(path) == CORE_TRUST_CHECK_OFF);
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CHECK(!path_is_valid(release));
#endif
   core_trust_runtime_deinit();
   app_data_fails = true;
   CHECK(core_trust_runtime_init(NULL, NULL, NULL));
   CHECK(core_trust_runtime_active());
   CHECK(core_trust_runtime_read_only());
   /* No file, so none that couldn't be read */
   CHECK(!core_trust_runtime_unreadable() && !core_trust_runtime_newer());
   CHECK(core_trust_runtime_checks_on());
   CHECK(!core_trust_runtime_set_checks(false));
   core_trust_runtime_deinit();
   app_data_fails = false;
}

static void test_saves_chained(void)
{
   char path[256];
   strlcpy(path, tmp("chained"), sizeof(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   /* The second change lands while the first write is out */
   CHECK(core_trust_runtime_set_checks(false));
   CHECK(core_trust_runtime_set_checks(true));
   pump(8);
   CHECK(disk_flags(path) == 0);
   core_trust_runtime_deinit();
}

static void test_deinit_writes(void)
{
   char dir[256];
   char path[256];
   /* Its folder is missing, so the write can't go through until quit */
   strlcpy(dir, tmp("later"), sizeof(dir));
   fill_pathname_join_special(path, dir, "store", sizeof(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(core_trust_runtime_set_checks(false));
   pump(4);
   CHECK(!path_is_valid(path));
   CHECK(path_mkdir(dir));
   core_trust_runtime_deinit();
   CHECK(disk_flags(path) == CORE_TRUST_CHECK_OFF);
}

static void test_apply_always(void)
{
   static const uint8_t folder[CORE_SHA256_LEN] = { 9 };
   uint8_t sha256[CORE_SHA256_LEN];
   struct core_check c;
   core_trust_set_t *s;
   char path[256];
   strlcpy(path, tmp("record"), sizeof(path));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   fake_check(&c, "u_libretro.so", 0x55, CORE_CHECK_UNSIGNED);
   CHECK(core_trust_runtime_always(&c));
   fake_check(&c, "s_libretro.so", 0x66, CORE_CHECK_OK);
   memcpy(c.folder, folder, sizeof(folder));
   c.rec.build_time = 1700000000;
   c.raise_newest   = true;
   CHECK(core_trust_runtime_apply(&c));
   s = core_trust_runtime_get();
   memset(sha256, 0x55, sizeof(sha256));
   CHECK(s && core_trust_has_hash(s, sha256));
   CHECK(s && core_trust_newest(s, folder, "s_libretro.so") == 1700000000);
   core_trust_set_put(s);
   pump(4);
   CHECK(disk_has_hash(path, 0x55));
   core_trust_runtime_deinit();
}

#define SNAPS 200

struct snap_handoff
{
   core_trust_set_t *sets[SNAPS];
   retro_atomic_int_t ready;
};

/* Drops each snapshot the main thread hands over */
static void snap_putter(void *data)
{
   struct snap_handoff *h = (struct snap_handoff*)data;
   int i                  = 0;
   while (i < SNAPS)
   {
      int ready = retro_atomic_load_acquire_int(&h->ready);
      while (i < ready)
         core_trust_set_put(h->sets[i++]);
   }
}

static void test_snapshot_threads(void)
{
   static struct snap_handoff h;
   struct core_check c;
   sthread_t *th;
   int i;
   CHECK(core_trust_runtime_init(tmp("threads"), NULL, NULL));
   retro_atomic_int_init(&h.ready, 0);
   th = sthread_create(snap_putter, &h);
   CHECK(th != NULL);
   for (i = 0; i < SNAPS; i++)
   {
      h.sets[i] = core_trust_runtime_get();
      retro_atomic_store_release_int(&h.ready, i + 1);
      /* A new set each time while the worker drops the old ones */
      fake_check(&c, "t_libretro.so", (uint8_t)(i + 1), CORE_CHECK_UNSIGNED);
      core_trust_runtime_always(&c);
   }
   if (th)
      sthread_join(th);
   else
      for (i = 0; i < SNAPS; i++)
         core_trust_set_put(h.sets[i]);
   pump(8);
   core_trust_runtime_deinit();
}

static void test_test_key_notice(void)
{
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   test_settings.bools.notification_show_core_signing_test_key = true;
   log_test_key = 0;
#endif
   osd_count = 0;
   CHECK(core_trust_runtime_init(tmp("notice"), NULL, NULL));
   /* Nothing before messages can show */
   CHECK(osd_count == 0);
   core_trust_runtime_notify();
   core_trust_runtime_notify();
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CHECK(osd_shown(MSG_CORE_SIGNING_TEST_KEY) == 1);
   CHECK(osd_duration(MSG_CORE_SIGNING_TEST_KEY) == 180);
#else
   CHECK(osd_shown(MSG_CORE_SIGNING_TEST_KEY) == 0);
#endif
   core_trust_runtime_deinit();
   /* Every launch */
   CHECK(core_trust_runtime_init(tmp("notice"), NULL, NULL));
   core_trust_runtime_notify();
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CHECK(osd_shown(MSG_CORE_SIGNING_TEST_KEY) == 2);
   CHECK(log_test_key == 2);
#endif
   core_trust_runtime_deinit();
}

/* Off: no notice, but the log still says so every launch */
static void test_test_key_notice_off(void)
{
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   test_settings.bools.notification_show_core_signing_test_key = false;
   osd_count    = 0;
   log_test_key = 0;
   CHECK(core_trust_runtime_init(tmp("notice-off"), NULL, NULL));
   core_trust_runtime_notify();
   core_trust_runtime_notify();
   core_trust_runtime_deinit();
   CHECK(core_trust_runtime_init(tmp("notice-off"), NULL, NULL));
   core_trust_runtime_notify();
   core_trust_runtime_deinit();
   CHECK(osd_shown(MSG_CORE_SIGNING_TEST_KEY) == 0);
   CHECK(log_test_key == 2);
   test_settings.bools.notification_show_core_signing_test_key = true;
#endif
}

/* A "core": size bytes (at least 2), the first two the tag, so no two
 * tags hash alike */
static bool put_file(const char *path, unsigned tag, size_t size)
{
   uint8_t *buf = (uint8_t*)malloc(size);
   size_t i;
   bool ok;
   if (!buf)
      return false;
   for (i = 0; i < size; i++)
      buf[i] = (uint8_t)(i * 7 + 1);
   buf[0] = (uint8_t)tag;
   buf[1] = (uint8_t)(tag >> 8);
   ok = filestream_write_file(path, buf, (int64_t)size);
   free(buf);
   return ok;
}

static void file_sha(const char *path, uint8_t *sha256)
{
   core_file_t *f = core_file_open(path);
   struct core_hash h;
   memset(sha256, 0, CORE_SHA256_LEN);
   if (f && core_hash_init(&h))
   {
      while (core_hash_step(&h, f) > 0);
      core_hash_final(&h, sha256);
   }
   core_file_close(f);
}

static bool trusted(const char *path)
{
   uint8_t sha256[CORE_SHA256_LEN];
   core_trust_set_t *s = core_trust_runtime_get();
   bool has;
   file_sha(path, sha256);
   has = s && core_trust_has_hash(s, sha256);
   core_trust_set_put(s);
   return has;
}

static unsigned store_flags(void)
{
   core_trust_set_t *s = core_trust_runtime_get();
   unsigned flags      = s ? core_trust_flags(s) : 0;
   core_trust_set_put(s);
   return flags;
}

/* What the Security Warning's Always Trust records for path */
static bool always_trust(const char *path, bool ctime_kept)
{
   struct core_check c;
   size_t num_keys     = 0;
   const uint8_t *keys = core_sig_builtin_keys(&num_keys);
   core_trust_set_t *s = core_trust_runtime_get();
   if (!s)
      return false;
   core_check_file(s, keys, num_keys, path, 0, &c);
   core_trust_set_put(s);
   if (!ctime_kept)
      c.rec.id.ctime_kept = false;
   return core_trust_runtime_always(&c);
}

static enum core_trust_state state_now(const char *path)
{
   core_trust_set_t *s         = core_trust_runtime_get();
   enum core_trust_state state = s ? core_trust_state_file(s, path, NULL)
      : CORE_TRUST_STATE_UNCHECKED;
   core_trust_set_put(s);
   return state;
}

static void test_forget(void)
{
   uint8_t sha256[CORE_SHA256_LEN];
   core_trust_set_t *s;
   char dir[256], a[256], b[256], store[256];
   strlcpy(dir, tmp("forget"), sizeof(dir));
   strlcpy(store, tmp("forget_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(a, dir, "a_libretro.core", sizeof(a));
   fill_pathname_join_special(b, dir, "b_libretro.core", sizeof(b));
   CHECK(put_file(a, 21, 1000));
   CHECK(put_file(b, 23, 1000));
   file_sha(a, sha256);
   CHECK(core_trust_runtime_init(store, NULL, NULL));
   CHECK(always_trust(a, true));
   if (state_now(a) != CORE_TRUST_STATE_TRUSTED)
   {
      printf("note: test_forget skipped, no records on %s's filesystem\n",
            dir);
      pump(8);
      core_trust_runtime_deinit();
      return;
   }
   pump(8);
   CHECK(disk_has_sha(store, sha256));
   CHECK(core_trust_runtime_forget(a));
   CHECK(!trusted(a));
#ifdef HAVE_CORE_LOAD_CHECK
   CHECK(forget_calls == 1);
   CHECK(string_is_equal(forget_path, a));
#endif
   pump(8);
   CHECK(!disk_has_sha(store, sha256));
   CHECK(!core_trust_runtime_forget(a));
#ifdef HAVE_CORE_LOAD_CHECK
   CHECK(forget_calls == 1);
#endif
   /* Trusted by its hash alone: no record says this file is the one */
   CHECK(always_trust(b, false));
   CHECK(!core_trust_runtime_forget(b));
   CHECK(trusted(b));
   /* Replaced since: the new bytes aren't trusted, the old hash stays */
   CHECK(always_trust(a, true));
   CHECK(put_file(a, 22, 1001));
   CHECK(state_now(a) == CORE_TRUST_STATE_UNCHECKED);
   CHECK(!core_trust_runtime_forget(a));
   s = core_trust_runtime_get();
   CHECK(s && core_trust_has_hash(s, sha256));
   core_trust_set_put(s);
   CHECK(!trusted(a));
   pump(8);
   core_trust_runtime_deinit();
   /* Read-only: taken back for this session, nothing written */
   strlcpy(store, tmp("forget_read_only"), sizeof(store));
   CHECK(path_mkdir(store));
   CHECK(core_trust_runtime_init(store, NULL, NULL));
   CHECK(core_trust_runtime_read_only());
   CHECK(always_trust(a, true));
   CHECK(core_trust_runtime_forget(a));
   CHECK(!trusted(a));
   pump(8);
   core_trust_runtime_deinit();
   CHECK(path_is_directory(store));
}

static void test_runtime_state(void)
{
   char dir[256], a[256], missing[256], store[256];
   strlcpy(dir, tmp("state"), sizeof(dir));
   strlcpy(store, tmp("state_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(a, dir, "a_libretro.core", sizeof(a));
   fill_pathname_join_special(missing, dir, "missing_libretro.core",
         sizeof(missing));
   CHECK(put_file(a, 31, 1000));
   CHECK(core_trust_runtime_init(store, NULL, NULL));
   CHECK(core_trust_runtime_state(missing) == CORE_TRUST_STATE_UNCHECKED);
   CHECK(always_trust(a, true));
   if (state_now(a) != CORE_TRUST_STATE_TRUSTED)
   {
      printf("note: test_runtime_state skipped, no records on %s's "
            "filesystem\n", dir);
      pump(8);
      core_trust_runtime_deinit();
      return;
   }
   CHECK(core_trust_runtime_state(a) == CORE_TRUST_STATE_TRUSTED);
   CHECK(core_trust_runtime_forget(a));
   CHECK(core_trust_runtime_state(a) == CORE_TRUST_STATE_UNCHECKED);
   pump(8);
   core_trust_runtime_deinit();
}

/* Untrust alone, and the checks setting alone, are changes the user
 * made: a failed save says so */
static void test_save_failed_user(void)
{
   char path[256], dir[256], a[256];
   strlcpy(path, tmp("save_failed_checks"), sizeof(path));
   CHECK(write_store(path, 0));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   osd_count = 0;
   saves_fail(true);
   CHECK(core_trust_runtime_set_checks(false));
   pump(8);
   CHECK(osd_shown(MSG_CORE_TRUST_SAVE_FAILED) == 1);
   saves_fail(false);
   core_trust_runtime_deinit();
   strlcpy(dir, tmp("save_failed_forget"), sizeof(dir));
   strlcpy(path, tmp("save_failed_forget_store"), sizeof(path));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(a, dir, "a_libretro.core", sizeof(a));
   CHECK(put_file(a, 24, 1000));
   CHECK(core_trust_runtime_init(path, NULL, NULL));
   CHECK(always_trust(a, true));
   pump(8);
   if (state_now(a) != CORE_TRUST_STATE_TRUSTED)
   {
      printf("note: test_save_failed_user's Untrust skipped, no records "
            "on %s's filesystem\n", dir);
      core_trust_runtime_deinit();
      return;
   }
   osd_count = 0;
   saves_fail(true);
   CHECK(core_trust_runtime_forget(a));
   pump(8);
   CHECK(osd_shown(MSG_CORE_TRUST_SAVE_FAILED) == 1);
   saves_fail(false);
   core_trust_runtime_deinit();
}

#ifdef HAVE_CORE_LOAD_CHECK
static bool seed_running(void *data)
{
   (void)data;
   return core_trust_runtime_seed_pending();
}

static void pump_seed(void)
{
   unsigned n = 0;
   if (task_thread)
      task_queue_wait(seed_running, NULL);
   while (core_trust_runtime_seed_pending() && n++ < 100000)
      task_queue_check();
   pump(2);
}
#endif

static void test_seed_walk(void)
{
   char dir[256], a[256], b[256], c[256], other[256], sub[256], store[256];
   char in_sub[256];
   strlcpy(dir, tmp("seed"), sizeof(dir));
   strlcpy(store, tmp("seed_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(a, dir, "a_libretro.core", sizeof(a));
   fill_pathname_join_special(b, dir, "b_libretro.core", sizeof(b));
   fill_pathname_join_special(c, dir, "c_libretro.CORE", sizeof(c));
   fill_pathname_join_special(other, dir, "readme.txt", sizeof(other));
   fill_pathname_join_special(sub, dir, "d_libretro.core", sizeof(sub));
   fill_pathname_join_special(in_sub, sub, "e_libretro.core", sizeof(in_sub));
   CHECK(put_file(a, 1, 1000));
   CHECK(put_file(b, 2, 3 * 65536 + 1));
   CHECK(put_file(c, 3, 10));
   CHECK(put_file(other, 4, 10));
   CHECK(path_mkdir(sub));
   osd_count = 0;
   CHECK(core_trust_runtime_init(store, dir, "core"));
#ifdef HAVE_CORE_LOAD_CHECK
   CHECK(core_trust_runtime_seed_pending());
   CHECK(core_trust_runtime_seed_covers(a));
   CHECK(!core_trust_runtime_seed_covers(store));
   CHECK(!core_trust_runtime_seed_covers(in_sub));
   pump_seed();
   CHECK(!core_trust_runtime_seed_pending());
   CHECK(!core_trust_runtime_seed_covers(a));
   CHECK(trusted(a));
   CHECK(trusted(b));
   CHECK(trusted(c));
   CHECK(!trusted(other));
   CHECK(store_flags() & CORE_TRUST_SEEDED);
   CHECK(store_flags() & CORE_TRUST_NOTICE_SHOWN);
   /* Queued until messages can show, then once */
   CHECK(osd_shown(MSG_CORE_TRUST_SEEDED) == 0);
   core_trust_runtime_notify();
   core_trust_runtime_notify();
   CHECK(osd_shown(MSG_CORE_TRUST_SEEDED) == 1);
   core_trust_runtime_deinit();
   CHECK(disk_flags(store) & CORE_TRUST_SEEDED);
   /* Once only */
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(!core_trust_runtime_seed_pending());
   core_trust_runtime_notify();
   CHECK(osd_shown(MSG_CORE_TRUST_SEEDED) == 1);
#else
   CHECK(!core_trust_runtime_seed_pending());
   CHECK(!core_trust_runtime_seed_covers(a));
   pump(8);
   CHECK(!trusted(a));
   CHECK(!(store_flags() & CORE_TRUST_SEEDED));
#endif
   core_trust_runtime_deinit();
}

#ifdef HAVE_CORE_LOAD_CHECK
static void test_seed_paced(void)
{
   char dir[256], core[256];
   unsigned opens;
   strlcpy(dir, tmp("paced"), sizeof(dir));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(core, dir, "p_libretro.core", sizeof(core));
   CHECK(put_file(core, 5, 3 * 65536 + 1));
   /* A spent window: one step per call, and still a step */
   window_items = 0;
   window_opens = 0;
   CHECK(core_trust_runtime_init(tmp("paced_store"), dir, "core"));
   pump(1);
   CHECK(core_trust_runtime_seed_pending());
   pump_seed();
   opens = window_opens;
   /* opendir, the entry, four reads and the end: at least six calls */
   CHECK(opens >= 6);
   CHECK(trusted(core));
   core_trust_runtime_deinit();
   /* A wide window: done in one or two calls */
   window_items = 1000000;
   window_opens = 0;
   CHECK(core_trust_runtime_init(tmp("paced_store2"), dir, "core"));
   pump_seed();
   CHECK(window_opens <= 2);
   core_trust_runtime_deinit();
}

static void test_seed_cap(void)
{
   char dir[256], core[256], name[32];
   unsigned i, n = 0;
   strlcpy(dir, tmp("cap"), sizeof(dir));
   CHECK(path_mkdir(dir));
   for (i = 0; i <= CORE_TRUST_MAX_HASHES; i++)
   {
      sprintf(name, "%03u_libretro.core", i);
      fill_pathname_join_special(core, dir, name, sizeof(core));
      CHECK(put_file(core, 100 + i, 8));
   }
   CHECK(core_trust_runtime_init(tmp("cap_store"), dir, "core"));
   pump_seed();
   for (i = 0; i <= CORE_TRUST_MAX_HASHES; i++)
   {
      sprintf(name, "%03u_libretro.core", i);
      fill_pathname_join_special(core, dir, name, sizeof(core));
      if (trusted(core))
         n++;
   }
   CHECK(n == CORE_TRUST_MAX_HASHES);
   CHECK(store_flags() & CORE_TRUST_SEEDED);
   core_trust_runtime_deinit();
}

/* A symlink in the folder to a core outside it, or to a folder: passed
 * over, and the walk goes on */
static void test_seed_symlink(void)
{
#ifndef _WIN32
   char dir[256], outside[256], lnk[256], dirlnk[256], own[256];
   strlcpy(dir, tmp("symlink"), sizeof(dir));
   strlcpy(outside, tmp("outside.core"), sizeof(outside));
   CHECK(path_mkdir(dir));
   CHECK(put_file(outside, 7, 4096));
   fill_pathname_join_special(lnk, dir, "link_libretro.core", sizeof(lnk));
   fill_pathname_join_special(dirlnk, dir, "dir_libretro.core",
         sizeof(dirlnk));
   fill_pathname_join_special(own, dir, "own_libretro.core", sizeof(own));
   CHECK(symlink("../outside.core", lnk) == 0);
   CHECK(symlink("..", dirlnk) == 0);
   CHECK(put_file(own, 8, 4096));
   CHECK(core_trust_runtime_init(tmp("symlink_store"), dir, "core"));
   pump_seed();
   CHECK(!trusted(outside));
   CHECK(trusted(own));
   CHECK(store_flags() & CORE_TRUST_SEEDED);
   core_trust_runtime_deinit();
#endif
}

/* SEEDED is on disk before anything is hashed */
static void test_seed_saved_first(void)
{
   uint8_t sha256[CORE_SHA256_LEN];
   char dir[256], core[256], store[256];
   unsigned n = 0;
   strlcpy(dir, tmp("first"), sizeof(dir));
   strlcpy(store, tmp("first_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(core, dir, "f_libretro.core", sizeof(core));
   CHECK(put_file(core, 12, 3 * 65536 + 1));
   file_sha(core, sha256);
   window_items = 0;
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(core_trust_runtime_seed_pending());
   CHECK(core_trust_runtime_seed_covers(core));
   CHECK(!path_is_valid(store));
   while (disk_flags(store) == ~0u && n++ < 64)
      pump(1);
   CHECK(disk_flags(store) == CORE_TRUST_SEEDED);
   CHECK(!disk_has_sha(store, sha256));
   CHECK(core_trust_runtime_seed_pending());
   window_items = 1000000;
   pump_seed();
   CHECK(trusted(core));
   core_trust_runtime_deinit();
}

/* Cut short at quit, before its write ran or after: never again */
static void test_seed_cancel(void)
{
   char dir[256], core[256], store[256];
   unsigned i;
   strlcpy(dir, tmp("cancel"), sizeof(dir));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(core, dir, "x_libretro.core", sizeof(core));
   CHECK(put_file(core, 6, 3 * 65536));
   for (i = 0; i < 2; i++)
   {
      strlcpy(store, tmp(i ? "cancel_store" : "cancel_store_early"),
            sizeof(store));
      window_items = 0;
      CHECK(core_trust_runtime_init(store, dir, "core"));
      pump(i);
      /* As quit does */
      task_queue_reset();
      pump(4);
      window_items = 1000000;
      CHECK(!core_trust_runtime_seed_pending());
      CHECK(store_flags() & CORE_TRUST_SEEDED);
      CHECK(!trusted(core));
      core_trust_runtime_deinit();
      CHECK(disk_flags(store) == CORE_TRUST_SEEDED);
      CHECK(core_trust_runtime_init(store, dir, "core"));
      CHECK(!core_trust_runtime_seed_pending());
      CHECK(!trusted(core));
      core_trust_runtime_deinit();
   }
}

#define KEEPS_N 20

/* What a seed cut short found stays trusted */
static void test_seed_cancel_keeps_found(void)
{
   uint8_t sha256[CORE_SHA256_LEN];
   char dir[256], core[256], store[256], name[32];
   unsigned long found = 0;
   unsigned i, n = 0;
   strlcpy(dir, tmp("keeps"), sizeof(dir));
   strlcpy(store, tmp("keeps_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   for (i = 0; i < KEEPS_N; i++)
   {
      sprintf(name, "%02u_libretro.core", i);
      fill_pathname_join_special(core, dir, name, sizeof(core));
      CHECK(put_file(core, 400 + i, 10));
   }
   window_items = 0;
   CHECK(core_trust_runtime_init(store, dir, "core"));
   for (i = 0; disk_flags(store) == ~0u && i < 64; i++)
      pump(1);
   pump(3 * KEEPS_N);
   task_queue_reset();
   pump(4);
   window_items = 1000000;
   for (i = 0; i < KEEPS_N; i++)
   {
      sprintf(name, "%02u_libretro.core", i);
      fill_pathname_join_special(core, dir, name, sizeof(core));
      if (trusted(core))
      {
         found |= 1UL << i;
         n++;
      }
   }
   CHECK(n >= 1 && n < KEEPS_N);
   core_trust_runtime_deinit();
   for (i = 0; i < KEEPS_N; i++)
   {
      if (!(found & (1UL << i)))
         continue;
      sprintf(name, "%02u_libretro.core", i);
      fill_pathname_join_special(core, dir, name, sizeof(core));
      file_sha(core, sha256);
      CHECK(disk_has_sha(store, sha256));
   }
}

static void test_seed_unreadable_and_off(void)
{
   char dir[256], core[256], store[256];
   strlcpy(dir, tmp("off"), sizeof(dir));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(core, dir, "o_libretro.core", sizeof(core));
   CHECK(put_file(core, 7, 100));
   /* A store that can't be read never seeds */
   strlcpy(store, tmp("off_dir_store"), sizeof(store));
   CHECK(path_mkdir(store));
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(!core_trust_runtime_seed_pending());
   core_trust_runtime_deinit();
   /* Checks off still seeds, so turning them on later finds it trusted */
   strlcpy(store, tmp("off_store"), sizeof(store));
   CHECK(write_store(store, CORE_TRUST_CHECK_OFF));
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(core_trust_runtime_seed_pending());
   pump_seed();
   CHECK(trusted(core));
   core_trust_runtime_deinit();
}

/* A store that already showed the notice seeds without it */
static void test_seed_notice_shown(void)
{
   char dir[256], core[256], store[256];
   strlcpy(dir, tmp("shown"), sizeof(dir));
   strlcpy(store, tmp("shown_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(core, dir, "n_libretro.core", sizeof(core));
   CHECK(put_file(core, 8, 100));
   CHECK(write_store(store, CORE_TRUST_NOTICE_SHOWN));
   osd_count = 0;
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(core_trust_runtime_seed_pending());
   pump_seed();
   core_trust_runtime_notify();
   CHECK(store_flags() & CORE_TRUST_SEEDED);
   CHECK(osd_shown(MSG_CORE_TRUST_SEEDED) == 0);
   CHECK(trusted(core));
   core_trust_runtime_deinit();
}

/* Nothing found, or no folder: seeded, and nothing to tell */
static void test_seed_nothing(void)
{
   char dir[256], store[256];
   unsigned i;
   strlcpy(dir, tmp("empty"), sizeof(dir));
   CHECK(path_mkdir(dir));
   for (i = 0; i < 2; i++)
   {
      strlcpy(store, tmp(i ? "no_folder_store" : "empty_store"),
            sizeof(store));
      osd_count = 0;
      CHECK(core_trust_runtime_init(store,
               i ? tmp("no_folder") : dir, "core"));
      CHECK(core_trust_runtime_seed_pending());
      pump_seed();
      core_trust_runtime_notify();
      CHECK(!core_trust_runtime_seed_pending());
      CHECK(store_flags() & CORE_TRUST_SEEDED);
      CHECK(!(store_flags() & CORE_TRUST_NOTICE_SHOWN));
      CHECK(osd_shown(MSG_CORE_TRUST_SEEDED) == 0);
      core_trust_runtime_deinit();
   }
}

/* A store with no room left trusts nothing new, so says nothing */
static void test_seed_full(void)
{
   char dir[256], core[256], store[256];
   core_trust_t *t = core_trust_new();
   struct core_check c;
   size_t len      = 0;
   uint8_t *buf;
   unsigned i, n = 0;
   strlcpy(dir, tmp("full"), sizeof(dir));
   strlcpy(store, tmp("full_store"), sizeof(store));
   for (i = 0; i < CORE_TRUST_MAX_HASHES; i++)
   {
      fake_check(&c, "f_libretro.core", 0x51, CORE_CHECK_UNSIGNED);
      c.rec.sha256[0] = (uint8_t)i;
      c.rec.sha256[1] = (uint8_t)(i >> 8);
      if (core_trust_always(t, &c))
         n++;
   }
   CHECK(n == CORE_TRUST_MAX_HASHES);
   buf = core_trust_serialize(t, &len);
   CHECK(buf && filestream_write_file(store, buf, (int64_t)len));
   free(buf);
   core_trust_free(t);
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(core, dir, "f_libretro.core", sizeof(core));
   CHECK(put_file(core, 11, 100));
   osd_count = 0;
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(core_trust_runtime_seed_pending());
   pump_seed();
   core_trust_runtime_notify();
   CHECK(!trusted(core));
   CHECK(store_flags() & CORE_TRUST_SEEDED);
   CHECK(!(store_flags() & CORE_TRUST_NOTICE_SHOWN));
   CHECK(osd_shown(MSG_CORE_TRUST_SEEDED) == 0);
   core_trust_runtime_deinit();
}

/* A folder that can't be opened is a seed that found nothing */
static void test_seed_unopened(void)
{
#ifdef _WIN32
   printf("note: test_seed_unopened skipped, no unopenable folder here\n");
#else
   char dir[256], core[256], store[256];
   struct RDIR *probe;
   strlcpy(dir, tmp("locked"), sizeof(dir));
   strlcpy(store, tmp("locked_store"), sizeof(store));
   /* Left at 000 by a run cut short here */
   chmod(dir, 0755);
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(core, dir, "l_libretro.core", sizeof(core));
   CHECK(put_file(core, 9, 100));
   chmod(dir, 0);
   if ((probe = retro_opendir(dir)))
   {
      retro_closedir(probe);
      chmod(dir, 0755);
      printf("note: test_seed_unopened skipped, %s opens anyway\n", dir);
      return;
   }
   osd_count = 0;
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(core_trust_runtime_seed_pending());
   pump_seed();
   core_trust_runtime_notify();
   CHECK(!core_trust_runtime_seed_pending());
   CHECK(store_flags() & CORE_TRUST_SEEDED);
   CHECK(osd_shown(MSG_CORE_TRUST_SEEDED) == 0);
   chmod(dir, 0755);
   CHECK(!trusted(core));
   core_trust_runtime_deinit();
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(!core_trust_runtime_seed_pending());
   CHECK(!trusted(core));
   core_trust_runtime_deinit();
#endif
}

/* A store that can't be saved: no seed and no message, at every start */
static void test_seed_save_fails(void)
{
   char dir[256], core[256], store[256], other[256];
   unsigned i, warns;
   strlcpy(dir, tmp("nosave"), sizeof(dir));
   strlcpy(store, tmp("nosave_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(core, dir, "s_libretro.core", sizeof(core));
   CHECK(put_file(core, 13, 100));
   saves_fail(true);
   osd_count = 0;
   for (i = 0; i < 2; i++)
   {
      if (i)
         core_trust_runtime_deinit();
      warns = warn_count;
      CHECK(core_trust_runtime_init(store, dir, "core"));
      pump_seed();
      core_trust_runtime_notify();
      CHECK(!core_trust_runtime_seed_pending());
      CHECK(!trusted(core));
      CHECK(osd_shown(MSG_CORE_TRUST_SEEDED) == 0);
      CHECK(osd_shown(MSG_CORE_TRUST_SAVE_FAILED) == 0);
      CHECK(!core_trust_runtime_can_save());
      CHECK(warn_count == warns + 1);
      CHECK(!path_is_valid(store));
   }
   /* A write that goes through later this session, and the exit
    * write, keep the store unseeded: the next start seeds */
   strlcpy(other, tmp("nosave_other.core"), sizeof(other));
   CHECK(put_file(other, 14, 100));
   saves_fail(false);
   CHECK(always_trust(other, true));
   pump(8);
   CHECK(path_is_valid(store));
   CHECK(!(disk_flags(store) & CORE_TRUST_SEEDED));
   core_trust_runtime_deinit();
   CHECK(!(disk_flags(store) & CORE_TRUST_SEEDED));
   CHECK(core_trust_runtime_init(store, dir, "core"));
   pump_seed();
   CHECK(trusted(core));
   CHECK(trusted(other));
   core_trust_runtime_deinit();
   CHECK(disk_flags(store) & CORE_TRUST_SEEDED);
}

/* A seed that ends after its store is gone leaves the next store alone */
static void test_seed_gen(void)
{
   char dir[256], core[256], store[256];
   strlcpy(dir, tmp("gen"), sizeof(dir));
   strlcpy(store, tmp("gen_store2"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(core, dir, "g_libretro.core", sizeof(core));
   CHECK(put_file(core, 10, 100));
   window_items = 0;
   CHECK(core_trust_runtime_init(tmp("gen_store1"), dir, "core"));
   pump(1);
   core_trust_runtime_deinit();
   CHECK(core_trust_runtime_init(store, NULL, NULL));
   pump(64);
   window_items = 1000000;
   CHECK(!(store_flags() & CORE_TRUST_SEEDED));
   CHECK(!trusted(core));
   core_trust_runtime_deinit();
}

/* A seeded store of n fake hashes and extra's, when given. Written with
 * no device, so it holds no records. */
static bool write_hashes(const char *path, unsigned n, const char *extra)
{
   struct core_check c;
   core_trust_t *t = core_trust_new();
   size_t len      = 0;
   uint8_t *buf    = NULL;
   unsigned i;
   bool ok         = t && core_trust_set_flag(t, CORE_TRUST_SEEDED, true);
   for (i = 0; ok && i < n; i++)
   {
      fake_check(&c, "fake_libretro.core", 0x5a, CORE_CHECK_UNSIGNED);
      c.rec.sha256[0] = (uint8_t)i;
      c.rec.sha256[1] = (uint8_t)(i >> 8);
      ok = core_trust_always(t, &c);
   }
   if (ok && extra)
   {
      fake_check(&c, "extra_libretro.core", 0, CORE_CHECK_UNSIGNED);
      file_sha(extra, c.rec.sha256);
      ok = core_trust_always(t, &c);
   }
   if (ok)
      buf = core_trust_serialize(t, &len);
   ok = buf && filestream_write_file(path, buf, (int64_t)len);
   free(buf);
   core_trust_free(t);
   return ok;
}

static unsigned hash_count(void)
{
   core_trust_set_t *s = core_trust_runtime_get();
   unsigned n          = s ? core_trust_num_hashes(s) : 0;
   core_trust_set_put(s);
   return n;
}

/* The hashes in the store on disk, ~0u when it doesn't load */
static unsigned disk_hashes(const char *path)
{
   core_trust_t *u = core_trust_new();
   unsigned n      = ~0u;
   core_trust_set_t *s;
   if (core_trust_load_file(u, path) == CORE_TRUST_LOAD_OK)
   {
      s = core_trust_get(u);
      n = core_trust_num_hashes(s);
      core_trust_set_put(s);
   }
   core_trust_free(u);
   return n;
}

static bool any_task(retro_task_t *task, void *ud)
{
   (void)task;
   (void)ud;
   return true;
}

/* Until no task is left, a sweep and its save included */
static void pump_tasks(void)
{
   task_finder_data_t find;
   unsigned n    = 0;
   find.func     = any_task;
   find.userdata = NULL;
   if (task_thread)
      task_queue_wait(NULL, NULL);
   while (task_queue_find(&find) && n++ < 100000)
      task_queue_check();
}

/* A full list: Always Trust waits on a sweep of the cores folder, which
 * drops every hash no core there has */
static void test_sweep_frees(void)
{
   uint8_t sha256[CORE_SHA256_LEN];
   char dir[256], b[256], store[256];
   strlcpy(dir, tmp("sweep_frees"), sizeof(dir));
   strlcpy(store, tmp("sweep_frees_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(b, dir, "b_libretro.core", sizeof(b));
   CHECK(put_file(b, 600, 3 * 65536 + 1));
   file_sha(b, sha256);
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES, NULL));
   osd_count = 0;
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(always_trust(b, true));
   CHECK(!trusted(b));
   pump_tasks();
   CHECK(trusted(b));
   CHECK(hash_count() == 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == 0);
   core_trust_runtime_deinit();
   CHECK(disk_hashes(store) == 1);
   CHECK(disk_has_sha(store, sha256));
}

/* A core still in the folder keeps its hash, and so does one trusted
 * this session from outside it */
static void test_sweep_keeps_live(void)
{
   char dir[256], a[256], b[256], o[256], store[256];
   strlcpy(dir, tmp("sweep_live"), sizeof(dir));
   strlcpy(store, tmp("sweep_live_store"), sizeof(store));
   strlcpy(o, tmp("sweep_outside.core"), sizeof(o));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(a, dir, "a_libretro.core", sizeof(a));
   fill_pathname_join_special(b, dir, "b_libretro.core", sizeof(b));
   CHECK(put_file(a, 601, 1000));
   CHECK(put_file(b, 602, 1000));
   CHECK(put_file(o, 603, 1000));
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES - 2, a));
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(always_trust(o, true));
   CHECK(hash_count() == CORE_TRUST_MAX_HASHES);
   CHECK(always_trust(b, true));
   CHECK(!trusted(b));
   pump_tasks();
   CHECK(trusted(a));
   CHECK(trusted(o));
   CHECK(trusted(b));
   CHECK(hash_count() == 3);
   core_trust_runtime_deinit();
}

/* Every hash still has its core: the waiting trust is refused, once per
 * sweep, and said so */
static void test_sweep_still_full(void)
{
   char dir[256], core[256], u[256], name[32];
   unsigned i;
   strlcpy(dir, tmp("sweep_full"), sizeof(dir));
   CHECK(path_mkdir(dir));
   for (i = 0; i <= CORE_TRUST_MAX_HASHES; i++)
   {
      sprintf(name, "%03u_libretro.core", i);
      fill_pathname_join_special(core, dir, name, sizeof(core));
      CHECK(put_file(core, 700 + i, 8));
   }
   osd_count = 0;
   CHECK(core_trust_runtime_init(tmp("sweep_full_store"), dir, "core"));
   pump_seed();
   u[0] = '\0';
   for (i = 0; i <= CORE_TRUST_MAX_HASHES; i++)
   {
      sprintf(name, "%03u_libretro.core", i);
      fill_pathname_join_special(core, dir, name, sizeof(core));
      if (!trusted(core))
         strlcpy(u, core, sizeof(u));
   }
   CHECK(*u);
   CHECK(always_trust(u, true));
   pump_tasks();
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == 1);
   CHECK(!trusted(u));
   CHECK(hash_count() == CORE_TRUST_MAX_HASHES);
   CHECK(always_trust(u, true));
   pump_tasks();
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == 2);
   core_trust_runtime_deinit();
}

/* A trust refused while a sweep runs waits on that one */
static void test_sweep_joins(void)
{
   char dir[256], b[256], d[256], store[256];
   strlcpy(dir, tmp("sweep_joins"), sizeof(dir));
   strlcpy(store, tmp("sweep_joins_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(b, dir, "b_libretro.core", sizeof(b));
   fill_pathname_join_special(d, dir, "d_libretro.core", sizeof(d));
   CHECK(put_file(b, 604, 1000));
   CHECK(put_file(d, 605, 1000));
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES, NULL));
   osd_count = 0;
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(always_trust(b, true));
   CHECK(always_trust(d, true));
   pump_tasks();
   CHECK(trusted(b));
   CHECK(trusted(d));
   CHECK(hash_count() == 2);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == 0);
   core_trust_runtime_deinit();
}

/* Cut short at quit: nothing dropped, and the waiting trust isn't kept */
static void test_sweep_cancelled(void)
{
   uint8_t sha256[CORE_SHA256_LEN];
   char dir[256], b[256], store[256];
   strlcpy(dir, tmp("sweep_cancel"), sizeof(dir));
   strlcpy(store, tmp("sweep_cancel_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(b, dir, "b_libretro.core", sizeof(b));
   CHECK(put_file(b, 606, 3 * 65536));
   file_sha(b, sha256);
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES, NULL));
   CHECK(core_trust_runtime_init(store, dir, "core"));
   window_items = 0;
   CHECK(always_trust(b, true));
   pump(1);
   /* As quit does */
   task_queue_reset();
   pump(4);
   window_items = 1000000;
   CHECK(!trusted(b));
   CHECK(hash_count() == CORE_TRUST_MAX_HASHES);
   core_trust_runtime_deinit();
   CHECK(disk_hashes(store) == CORE_TRUST_MAX_HASHES);
   CHECK(!disk_has_sha(store, sha256));
}

/* No folder to sweep: a full list refuses at once */
/* <name>.framework folders, as Apple installs cores: the seed, the
 * sweep, the Core Trust row and Untrust all read the binary inside */
static void test_framework_cores(void)
{
#ifndef _WIN32
   char dir[256], fw[256], bin[256], fw2[256], bin2[256], store[256];
   strlcpy(dir, tmp("frameworks"), sizeof(dir));
   strlcpy(store, tmp("frameworks_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(fw, dir, "x_libretro.framework", sizeof(fw));
   fill_pathname_join_special(bin, fw, "x_libretro", sizeof(bin));
   fill_pathname_join_special(fw2, dir, "y_libretro.framework",
         sizeof(fw2));
   fill_pathname_join_special(bin2, fw2, "y_libretro", sizeof(bin2));
   CHECK(path_mkdir(fw));
   CHECK(path_mkdir(fw2));
   CHECK(put_file(bin, 621, 1000));
   CHECK(put_file(bin2, 622, 1000));
   /* The seed trusts both */
   CHECK(core_trust_runtime_init(store, dir, "framework"));
   pump_seed();
   CHECK(trusted(bin));
   CHECK(trusted(bin2));
   if (core_trust_runtime_state(fw) != CORE_TRUST_STATE_TRUSTED)
   {
      printf("note: test_framework_cores skipped, no records on %s's "
            "filesystem\n", dir);
      pump(8);
      core_trust_runtime_deinit();
      return;
   }
   CHECK(core_trust_runtime_forget(fw2));
   CHECK(!trusted(bin2));
   CHECK(trusted(bin));
   CHECK(core_trust_runtime_state(fw2) == CORE_TRUST_STATE_UNCHECKED);
   pump(8);
   core_trust_runtime_deinit();
   /* A full list keeps the installed framework's hash */
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES - 1, bin));
   CHECK(core_trust_runtime_init(store, dir, "framework"));
   CHECK(always_trust(bin2, true));
   pump_tasks();
   CHECK(trusted(bin));
   CHECK(trusted(bin2));
   CHECK(hash_count() == 2);
   core_trust_runtime_deinit();
#endif
}

/* Cores kept in the cores directory setting, not the seed folder: the
 * sweep keeps their hashes; one that doesn't open drops nothing */
static void test_sweep_cores_dir(void)
{
   char seed[256], cores[256], c[256], n[256], gone[256], store[256];
   strlcpy(seed, tmp("sweep_cd_seed"), sizeof(seed));
   strlcpy(cores, tmp("sweep_cd_cores"), sizeof(cores));
   strlcpy(gone, tmp("sweep_cd_gone"), sizeof(gone));
   strlcpy(store, tmp("sweep_cd_store"), sizeof(store));
   CHECK(path_mkdir(seed));
   CHECK(path_mkdir(cores));
   fill_pathname_join_special(c, cores, "c_libretro.core", sizeof(c));
   fill_pathname_join_special(n, cores, "n_libretro.core", sizeof(n));
   CHECK(put_file(c, 611, 1000));
   CHECK(put_file(n, 612, 1000));
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES - 1, c));
   CHECK(core_trust_runtime_init(store, seed, "core"));
   core_trust_runtime_set_cores_dir(cores);
   CHECK(hash_count() == CORE_TRUST_MAX_HASHES);
   CHECK(always_trust(n, true));
   pump_tasks();
   CHECK(trusted(c));
   CHECK(trusted(n));
   CHECK(hash_count() == 2);
   core_trust_runtime_deinit();
   /* The cores directory alone, with no seed folder */
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES - 1, c));
   CHECK(core_trust_runtime_init(store, NULL, "core"));
   core_trust_runtime_set_cores_dir(cores);
   CHECK(always_trust(n, true));
   pump_tasks();
   CHECK(trusted(c));
   CHECK(trusted(n));
   CHECK(hash_count() == 2);
   core_trust_runtime_deinit();
   /* A cores directory that doesn't open proves nothing */
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES - 1, c));
   CHECK(core_trust_runtime_init(store, seed, "core"));
   core_trust_runtime_set_cores_dir(gone);
   CHECK(always_trust(n, true));
   pump_tasks();
   CHECK(trusted(c));
   CHECK(!trusted(n));
   CHECK(hash_count() == CORE_TRUST_MAX_HASHES);
   core_trust_runtime_deinit();
}

static void test_sweep_no_folder(void)
{
   char b[256], store[256];
   strlcpy(b, tmp("sweep_none.core"), sizeof(b));
   strlcpy(store, tmp("sweep_none_store"), sizeof(store));
   CHECK(put_file(b, 607, 1000));
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES, NULL));
   CHECK(core_trust_runtime_init(store, NULL, NULL));
   CHECK(!always_trust(b, true));
   pump_tasks();
   CHECK(!trusted(b));
   core_trust_runtime_deinit();
}

/* At most CORE_TRUST_SWEEP_WAIT_MAX trusts wait on one sweep */
static void test_sweep_wait_cap(void)
{
   struct core_check c;
   char dir[256], store[256];
   unsigned i;
   strlcpy(dir, tmp("sweep_cap"), sizeof(dir));
   strlcpy(store, tmp("sweep_cap_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES, NULL));
   CHECK(core_trust_runtime_init(store, dir, "core"));
   for (i = 0; i <= CORE_TRUST_SWEEP_WAIT_MAX; i++)
   {
      fake_check(&c, "wait_libretro.core", 0x77, CORE_CHECK_UNSIGNED);
      c.rec.sha256[0] = (uint8_t)i;
      CHECK(core_trust_runtime_always(&c)
            == (i < CORE_TRUST_SWEEP_WAIT_MAX));
   }
   pump_tasks();
   CHECK(hash_count() == CORE_TRUST_SWEEP_WAIT_MAX);
   core_trust_runtime_deinit();
}

/* A core the sweep can't read proves nothing; a folder or a symlink
 * named like a core is passed over */
static void test_sweep_unreadable(void)
{
#ifdef _WIN32
   printf("note: test_sweep_unreadable skipped, no unreadable file here\n");
#else
   uint8_t sha256[CORE_SHA256_LEN], out_sha[CORE_SHA256_LEN];
   char dir[256], b[256], x[256], sub[256], lnk[256], out[256], store[256];
   core_trust_set_t *s;
   core_file_t *probe;
   strlcpy(dir, tmp("sweep_unreadable"), sizeof(dir));
   strlcpy(store, tmp("sweep_unreadable_store"), sizeof(store));
   strlcpy(out, tmp("sweep_unreadable_out.core"), sizeof(out));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(b, dir, "b_libretro.core", sizeof(b));
   fill_pathname_join_special(x, dir, "x_libretro.core", sizeof(x));
   fill_pathname_join_special(sub, dir, "dir_libretro.core", sizeof(sub));
   fill_pathname_join_special(lnk, dir, "link_libretro.core", sizeof(lnk));
   CHECK(put_file(b, 608, 1000));
   CHECK(put_file(x, 609, 1000));
   file_sha(x, sha256);
   CHECK(put_file(out, 610, 1000));
   file_sha(out, out_sha);
   CHECK(path_mkdir(sub));
   CHECK(symlink("../sweep_unreadable_out.core", lnk) == 0);
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES - 2, x));
   /* The link's target, trusted in an earlier session */
   CHECK(core_trust_runtime_init(store, NULL, NULL));
   CHECK(always_trust(out, false));
   pump_tasks();
   core_trust_runtime_deinit();
   CHECK(disk_has_sha(store, out_sha));
   chmod(x, 0);
   if ((probe = core_file_open(x)))
   {
      core_file_close(probe);
      chmod(x, 0644);
      printf("note: test_sweep_unreadable skipped, %s opens anyway\n", x);
      return;
   }
   CHECK(core_trust_runtime_init(store, dir, "core"));
   CHECK(always_trust(b, true));
   pump_tasks();
   CHECK(trusted(b));
   s = core_trust_runtime_get();
   CHECK(s && !core_trust_has_hash(s, sha256));
   CHECK(s && !core_trust_has_hash(s, out_sha));
   core_trust_set_put(s);
   CHECK(hash_count() == 1);
   core_trust_runtime_deinit();
   chmod(x, 0644);
#endif
}

/* A folder the sweep can't open proves nothing: nothing is dropped */
static void test_sweep_unopened(void)
{
#ifdef _WIN32
   printf("note: test_sweep_unopened skipped, no unopenable folder here\n");
#else
   struct core_check c;
   char dir[256], store[256];
   struct RDIR *probe;
   strlcpy(dir, tmp("sweep_locked"), sizeof(dir));
   strlcpy(store, tmp("sweep_locked_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES, NULL));
   chmod(dir, 0);
   if ((probe = retro_opendir(dir)))
   {
      retro_closedir(probe);
      chmod(dir, 0755);
      printf("note: test_sweep_unopened skipped, %s opens anyway\n", dir);
      return;
   }
   osd_count = 0;
   CHECK(core_trust_runtime_init(store, dir, "core"));
   fake_check(&c, "locked_libretro.core", 0x78, CORE_CHECK_UNSIGNED);
   CHECK(core_trust_runtime_always(&c));
   pump_tasks();
   chmod(dir, 0755);
   CHECK(hash_count() == CORE_TRUST_MAX_HASHES);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == 1);
   core_trust_runtime_deinit();
#endif
}

/* The list can't be saved and a sweep frees nothing: the core was never
 * trusted, so the list-full message all the same */
static void test_sweep_cant_save(void)
{
   struct core_check c;
   char dir[256], store[256];
   unsigned i;
   strlcpy(dir, tmp("sweep_cant_save"), sizeof(dir));
   strlcpy(store, tmp("sweep_cant_save_store"), sizeof(store));
   CHECK(path_mkdir(dir));
   CHECK(write_hashes(store, 0, NULL));
   CHECK(core_trust_runtime_init(store, dir, "core"));
   saves_fail(true);
   /* Trusted this session: the sweep keeps them all */
   for (i = 0; i < CORE_TRUST_MAX_HASHES; i++)
   {
      fake_check(&c, "live_libretro.core", 0x79, CORE_CHECK_UNSIGNED);
      c.rec.sha256[0] = (uint8_t)i;
      c.rec.sha256[1] = (uint8_t)(i >> 8);
      if (!core_trust_runtime_always(&c))
         break;
   }
   CHECK(i == CORE_TRUST_MAX_HASHES);
   pump_tasks();
   CHECK(!core_trust_runtime_can_save());
   osd_count = 0;
   fake_check(&c, "wait_libretro.core", 0x7a, CORE_CHECK_UNSIGNED);
   CHECK(core_trust_runtime_always(&c));
   pump_tasks();
   CHECK(hash_count() == CORE_TRUST_MAX_HASHES);
   CHECK(osd_count == 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == 1);
   core_trust_runtime_deinit();
   saves_fail(false);
}
#endif

static void test_warning(void)
{
   static int ua, ub;
   struct core_check c, d;
   enum core_warning_kind kind;
   enum core_check_result result;
   unsigned serial;
   CHECK(core_trust_runtime_init(tmp("warning"), NULL, NULL));
   memset(&seen, 0, sizeof(seen));
   menu_opens = 0;
   /* Nothing to warn about */
   fake_check(&c, "w_libretro.so", 0x21, CORE_CHECK_OK);
   CHECK(!core_warning_push(&c, CORE_WARNING_LOAD, "/c/w_libretro.so",
            warn_cb, &ua));
   CHECK(menu_opens == 0);
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(core_warning_serial() == 0);
   CHECK(choices_are(CORE_WARNING_CANCEL, -1, -1));
   fake_check(&c, "w_libretro.so", 0x21, CORE_CHECK_UNSIGNED);
   CHECK(!core_warning_push(&c, CORE_WARNING_LOAD, NULL, warn_cb, &ua));
   CHECK(!core_warning_push(&c, CORE_WARNING_LOAD, "/c/w_libretro.so",
            NULL, &ua));
   CHECK(!core_warning_pending(NULL, NULL));
   /* A menu that can't show it (the null driver) */
   menu_can_open = false;
   CHECK(!core_warning_push(&c, CORE_WARNING_LOAD, "/c/w_libretro.so",
            warn_cb, &ua));
   menu_can_open = true;
   CHECK(menu_opens == 0 && seen.calls == 0);
   CHECK(!core_warning_pending(NULL, NULL));
   /* A load */
   fake_check(&c, "w_libretro.so", 0x21, CORE_CHECK_UNSIGNED);
   CHECK(core_warning_push(&c, CORE_WARNING_LOAD, "/c/w_libretro.so",
            warn_cb, &ua));
   CHECK(menu_opens == 1);
   CHECK(core_warning_pending(&kind, &result));
   CHECK(kind == CORE_WARNING_LOAD && result == CORE_CHECK_UNSIGNED);
   CHECK(choices_are(CORE_WARNING_CANCEL, CORE_WARNING_LOAD_ONCE,
            CORE_WARNING_ALWAYS_TRUST));
   /* Refused, it leaves the pending request alone */
   menu_can_open = false;
   CHECK(!core_warning_push(&c, CORE_WARNING_INSTALL, "/c/x_libretro.so",
            warn_cb, &ub));
   menu_can_open = true;
   CHECK(seen.calls == 0 && core_warning_pending(&kind, NULL));
   CHECK(kind == CORE_WARNING_LOAD);
   CHECK(!core_warning_choose(CORE_WARNING_INSTALL_AND_TRUST));
   CHECK(seen.calls == 0 && core_warning_pending(NULL, NULL));
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   CHECK(seen.calls == 1 && seen.choice == CORE_WARNING_ALWAYS_TRUST);
   CHECK(seen.userdata == &ua);
   CHECK(string_is_equal(seen.path, "/c/w_libretro.so"));
   CHECK(!memcmp(&seen.c, &c, sizeof(c)));
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(!core_warning_choose(CORE_WARNING_CANCEL));
   /* An install: a newer push cancels the older one */
   fake_check(&c, "i_libretro.so", 0x22, CORE_CHECK_MODIFIED);
   CHECK(core_warning_push(&c, CORE_WARNING_INSTALL, "/c/i_libretro.so",
            warn_cb, &ua));
   CHECK(choices_are(CORE_WARNING_CANCEL, CORE_WARNING_INSTALL_AND_TRUST, -1));
   serial = core_warning_serial();
   CHECK(serial != 0);
   fake_check(&d, "j_libretro.so", 0x23, CORE_CHECK_ERROR);
   CHECK(core_warning_push(&d, CORE_WARNING_LOAD, "/c/j_libretro.so",
            warn_cb, &ub));
   CHECK(seen.calls == 2 && seen.choice == CORE_WARNING_CANCEL);
   CHECK(seen.userdata == &ua);
   CHECK(string_is_equal(seen.path, "/c/i_libretro.so"));
   CHECK(seen.c.result == CORE_CHECK_MODIFIED);
   /* The screen's rows for the older request no longer count */
   CHECK(core_warning_serial() != serial && core_warning_serial() != 0);
   /* ERROR: Cancel only */
   CHECK(choices_are(CORE_WARNING_CANCEL, -1, -1));
   CHECK(!core_warning_choose(CORE_WARNING_LOAD_ONCE));
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   CHECK(seen.calls == 3 && seen.userdata == &ub);
   CHECK(core_warning_serial() == 0);
   /* The callback may push again: the slot is free by then */
   CHECK(core_warning_push(&c, CORE_WARNING_INSTALL, "/c/i_libretro.so",
            warn_cb, &ua));
   seen.push_again = true;
   CHECK(core_warning_choose(CORE_WARNING_INSTALL_AND_TRUST));
   CHECK(seen.calls == 4 && core_warning_pending(&kind, NULL));
   CHECK(kind == CORE_WARNING_INSTALL);
   CHECK(seen.again_pushed);
   /* Quit cancels what is still pending, and takes no new request */
   seen.push_again = true;
   menu_opens      = 0;
   core_trust_runtime_deinit();
   CHECK(seen.calls == 5 && seen.choice == CORE_WARNING_CANCEL);
   CHECK(!seen.again_pushed);
   CHECK(menu_opens == 0);
   CHECK(!core_warning_pending(NULL, NULL));
}

/* A CANCEL that chooses, from inside the push that replaced it */
static unsigned cancel_choose_calls;
static bool cancel_chose;

static void cancel_choose_cb(enum core_warning_choice choice,
      const struct core_check *c, const char *core_path, void *userdata)
{
   (void)c;
   (void)core_path;
   (void)userdata;
   cancel_choose_calls++;
   if (choice == CORE_WARNING_CANCEL)
      cancel_chose = core_warning_choose(CORE_WARNING_CANCEL);
}

static void test_warning_choose_from_cancel(void)
{
   struct core_check a, b;
   CHECK(core_trust_runtime_init(tmp("cancel_choose"), NULL, NULL));
   memset(&seen, 0, sizeof(seen));
   cancel_choose_calls = 0;
   cancel_chose        = true;
   fake_check(&a, "a_libretro.so", 0x41, CORE_CHECK_UNSIGNED);
   fake_check(&b, "b_libretro.so", 0x42, CORE_CHECK_UNSIGNED);
   CHECK(core_warning_push(&a, CORE_WARNING_LOAD, "/c/a_libretro.so",
            cancel_choose_cb, NULL));
   CHECK(core_warning_push(&b, CORE_WARNING_LOAD, "/c/b_libretro.so",
            warn_cb, NULL));
   CHECK(cancel_choose_calls == 1);
   CHECK(!cancel_chose);
   /* B is still the push's to answer */
   CHECK(seen.calls == 0);
   CHECK(core_warning_pending(NULL, NULL));
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   CHECK(seen.calls == 1 && seen.choice == CORE_WARNING_LOAD_ONCE);
   core_trust_runtime_deinit();
}

static void test_warning_text(void)
{
   struct core_check a, b;
   CHECK(core_warning_reason(CORE_CHECK_UNSIGNED) == MSG_CORE_WARNING_UNSIGNED);
   CHECK(core_warning_reason(CORE_CHECK_MODIFIED) == MSG_CORE_WARNING_MODIFIED);
   CHECK(core_warning_reason(CORE_CHECK_OLDER) == MSG_CORE_WARNING_OLDER);
   CHECK(core_warning_reason(CORE_CHECK_UNKNOWN_KEY)
         == MSG_CORE_WARNING_UNKNOWN_KEY);
   CHECK(core_warning_reason(CORE_CHECK_ERROR) == MSG_CORE_WARNING_ERROR);
   fake_check(&a, "s_libretro.so", 0x31, CORE_CHECK_UNSIGNED);
   fake_check(&b, "s_libretro.so", 0x31, CORE_CHECK_UNSIGNED);
   CHECK(core_warning_same_core(&a, &b));
   b.rec.sha256[31] ^= 1;
   CHECK(!core_warning_same_core(&a, &b));
   fake_check(&b, "t_libretro.so", 0x31, CORE_CHECK_UNSIGNED);
   CHECK(!core_warning_same_core(&a, &b));
   /* A check that never hashed matches nothing */
   fake_check(&a, "s_libretro.so", 0, CORE_CHECK_ERROR);
   fake_check(&b, "s_libretro.so", 0, CORE_CHECK_ERROR);
   CHECK(!core_warning_same_core(&a, &b));
}

int main(void)
{
   size_t num_keys = 0;
   setvbuf(stdout, NULL, _IONBF, 0);
   if (!path_mkdir(TMP_DIR))
   {
      printf("FAIL: cannot make %s\n", TMP_DIR);
      return 1;
   }
   task_queue_init(false, NULL);
   if (!core_sig_builtin_keys(&num_keys))
      test_inactive();
   else
   {
      test_load_missing();
      test_load_good();
      test_load_damaged();
      test_load_unreadable();
      test_load_newer();
      test_toggle();
      test_can_save();
      test_temp_sweep();
      test_save_failed_once();
      test_save_failed_quiet();
      test_default_path();
      test_saves_chained();
      test_deinit_writes();
      test_apply_always();
      test_forget();
      test_runtime_state();
      test_save_failed_user();
      test_snapshot_threads();
      test_test_key_notice();
      test_test_key_notice_off();
      test_seed_walk();
#ifdef HAVE_CORE_LOAD_CHECK
      test_seed_paced();
      test_seed_cap();
      test_seed_symlink();
      test_seed_saved_first();
      test_seed_cancel();
      test_seed_cancel_keeps_found();
      test_seed_unreadable_and_off();
      test_seed_notice_shown();
      test_seed_nothing();
      test_seed_full();
      test_seed_unopened();
      test_seed_save_fails();
      test_seed_gen();
      test_sweep_frees();
      test_sweep_keeps_live();
      test_sweep_still_full();
      test_sweep_joins();
      test_sweep_cancelled();
      test_sweep_no_folder();
      test_sweep_cores_dir();
      test_framework_cores();
      test_sweep_wait_cap();
      test_sweep_unreadable();
      test_sweep_unopened();
      test_sweep_cant_save();
#endif
      test_warning();
      test_warning_choose_from_cancel();
      test_warning_text();
      /* The worker's side of a save, the seed and a sweep */
      task_queue_deinit();
      task_queue_init(true, NULL);
      task_thread = true;
      test_saves_chained();
      test_seed_walk();
#ifdef HAVE_CORE_LOAD_CHECK
      test_sweep_frees();
      test_sweep_cores_dir();
#endif
      task_thread = false;
   }
   task_queue_deinit();
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
