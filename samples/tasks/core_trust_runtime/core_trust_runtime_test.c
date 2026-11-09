/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../core_trust_runtime.h"
#include "../../../runloop.h"

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

#define OSD_MAX 16
static char osd[OSD_MAX][32];
static unsigned osd_count;

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush,
      char *title, enum message_queue_icon icon,
      enum message_queue_category category)
{
   (void)len; (void)prio; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category;
   if (osd_count < OSD_MAX)
      strlcpy(osd[osd_count], msg, sizeof(osd[0]));
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

static char tmp_path[256];
static const char *tmp(const char *name)
{
   fill_pathname_join_special(tmp_path, TMP_DIR, name, sizeof(tmp_path));
   return tmp_path;
}

/* The queue is unthreaded: each check runs every task's handler once
 * and retires what finished */
static void pump(unsigned n)
{
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

static bool disk_has_hash(const char *path, uint8_t byte)
{
   uint8_t sha256[CORE_SHA256_LEN];
   core_trust_t *u = core_trust_new();
   bool has        = false;
   core_trust_set_t *s;
   memset(sha256, byte, sizeof(sha256));
   if (core_trust_load_file(u, path) == CORE_TRUST_LOAD_OK)
   {
      s   = core_trust_get(u);
      has = core_trust_has_hash(s, sha256);
      core_trust_set_put(s);
   }
   core_trust_free(u);
   return has;
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
   CHECK(core_trust_runtime_checks_on());
   CHECK(!core_trust_runtime_set_checks(false));
   CHECK(core_trust_runtime_checks_on());
   pump(4);
   core_trust_runtime_deinit();
   CHECK(path_is_directory(path));
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
   osd_count = 0;
   CHECK(core_trust_runtime_init(tmp("notice"), NULL, NULL));
   /* Nothing before messages can show */
   CHECK(osd_count == 0);
   core_trust_runtime_notify();
   core_trust_runtime_notify();
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CHECK(osd_shown(MSG_CORE_SIGNING_TEST_KEY) == 1);
#else
   CHECK(osd_shown(MSG_CORE_SIGNING_TEST_KEY) == 0);
#endif
   core_trust_runtime_deinit();
   /* Every launch */
   CHECK(core_trust_runtime_init(tmp("notice"), NULL, NULL));
   core_trust_runtime_notify();
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CHECK(osd_shown(MSG_CORE_SIGNING_TEST_KEY) == 2);
#endif
   core_trust_runtime_deinit();
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
      test_default_path();
      test_saves_chained();
      test_deinit_writes();
      test_apply_always();
      test_snapshot_threads();
      test_test_key_notice();
   }
   task_queue_deinit();
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
