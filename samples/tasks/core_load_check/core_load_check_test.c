/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#include <compat/strl.h>
#include <dynamic/dylib.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <retro_atomic.h>
#include <retro_dirent.h>
#include <retro_timers.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../core_load_check.h"
#include "../../../core_trust_runtime.h"
#include "../../../runloop.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"

#define TMP_DIR "core_load_check_test_tmp"
#ifdef _WIN32
#define LIB_EXT ".dll"
#define CORE_EXT "dll"
#else
#define LIB_EXT ".so"
#define CORE_EXT "so"
#endif

static unsigned checks;
static unsigned failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* Frontend stand-ins: what the modules call outside themselves */
const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   static char buf[4][32];
   static unsigned n;
   char *s = buf[n++ & 3];
   sprintf(s, "msg %d", (int)msg);
   return s;
}

#define OSD_MAX 16
static char osd[OSD_MAX][160];
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

bool fill_pathname_application_data(char *s, size_t len)
{
   strlcpy(s, TMP_DIR, len);
   return true;
}

/* The shared I/O window: one item per open, so hashing takes a
 * handler call per chunk */
void task_nbio_slice_open(nbio_budget_t *b)
{
   b->floor = 1;
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
   if (!b->floor)
      return false;
   b->floor = 0;
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

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

/* The last warning or error's format, enough to name the line */
static char last_log[256];

static unsigned checking_logs;
static unsigned moved_logs;

void RARCH_LOG(const char *fmt, ...)
{
   if (strstr(fmt, "Checking"))
      checking_logs++;
   if (strstr(fmt, "changed after its check"))
      moved_logs++;
}

void RARCH_WARN(const char *fmt, ...)
{
   strlcpy(last_log, fmt, sizeof(last_log));
}

void RARCH_ERR(const char *fmt, ...)
{
   strlcpy(last_log, fmt, sizeof(last_log));
}

#ifdef COUNT_HASHES
/* Hashes started, in place or in a task */
static unsigned hashes;
static bool hash_init_fails;

bool __real_core_hash_init(struct core_hash *h);

bool __wrap_core_hash_init(struct core_hash *h)
{
   hashes++;
   if (hash_init_fails)
      return false;
   return __real_core_hash_init(h);
}
#endif

static void pump(unsigned n)
{
   while (n--)
   {
      task_queue_check();
      retro_sleep(1);
   }
}

static bool count_task(retro_task_t *task, void *ud)
{
   (void)task;
   (*(unsigned*)ud)++;
   return false;
}

/* Tasks pushed and not yet retired */
static unsigned queued(void)
{
   task_finder_data_t find;
   unsigned n    = 0;
   find.func     = count_task;
   find.userdata = &n;
   task_queue_find(&find);
   return n;
}

static void drain(void)
{
   unsigned i;
   for (i = 0; i < 5000 && queued(); i++)
      pump(1);
   pump(2);
}

/* Test library a, b or c (no test trusts c) as TMP_DIR/cores/<name>, a
 * new file each time.
 * The old file is moved aside, not deleted: a freed inode handed to the
 * new file, with the same size inside one coarse timestamp tick, would
 * give it the old identity. False while a held handle keeps the old
 * file in place (Windows). */
static bool put_core(char *s, const char *name, char which)
{
   static unsigned aside_n;
   char aside[64];
   void *buf   = NULL;
   int64_t len = 0;
   bool ok     = false;
   sprintf(s, TMP_DIR "/cores/%s", name);
   if (!filestream_read_file(which == 'a'
            ? "core_load_check_test_a" LIB_EXT
            : which == 'b'
            ? "core_load_check_test_b" LIB_EXT
            : "core_load_check_test_c" LIB_EXT, &buf, &len))
      return false;
   sprintf(aside, TMP_DIR "/cores/aside_%u", ++aside_n);
   if (!path_is_valid(s) || !filestream_rename(s, aside))
      ok = filestream_write_file(s, buf, len);
   free(buf);
   return ok;
}

#ifdef _WIN32
/* FILE_BASIC_INFO, which older SDKs lack */
struct test_basic_info
{
   LARGE_INTEGER creation;
   LARGE_INTEGER access;
   LARGE_INTEGER write;
   LARGE_INTEGER change;
   DWORD attributes;
};

typedef BOOL (WINAPI *test_file_info_t)(HANDLE, int, LPVOID, DWORD);
#endif

/* Moves only the file's ctime, as real Windows' first load of a DLL
 * does (Code Integrity writes an extended attribute; Wine doesn't).
 * False if it didn't move. */
static bool ctime_move(const char *path)
{
   struct core_identity was, now;
   core_file_t *f = core_file_open(path);
   bool moved     = false;
   unsigned i;
#ifdef _WIN32
   HMODULE k32          = GetModuleHandleA("kernel32.dll");
   test_file_info_t get = (test_file_info_t)GetProcAddress(k32,
         "GetFileInformationByHandleEx");
   test_file_info_t set = (test_file_info_t)GetProcAddress(k32,
         "SetFileInformationByHandle");
   struct test_basic_info b;
   HANDLE h;
#else
   struct stat st;
#endif
   memset(&now, 0, sizeof(now));
   if (!f || !core_file_identity(f, &was))
   {
      core_file_close(f);
      return false;
   }
   /* Retried until a coarse clock ticks */
   for (i = 0; i < 1000 && !moved; i++)
   {
#ifdef _WIN32
      /* Attributes only, which no share mode refuses. NTFS takes the
       * ChangeTime given; Wine reports the Unix ctime, which setting
       * the write time moves. */
      h = CreateFileA(path, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
            OPEN_EXISTING, 0, NULL);
      if (h == INVALID_HANDLE_VALUE)
         break;
      if (get && set && get(h, 0, &b, sizeof(b)))
      {
         b.creation.QuadPart = 0;
         b.access.QuadPart   = 0;
         b.attributes        = 0;
         b.change.QuadPart  += 10000000;
         set(h, 0, &b, sizeof(b));
      }
      CloseHandle(h);
#else
      if (stat(path, &st) != 0 || chmod(path, st.st_mode & 07777) != 0)
         break;
#endif
      if (!core_file_identity(f, &now))
         break;
      moved = now.ctime != was.ctime || now.ctime_nsec != was.ctime_nsec;
      if (!moved)
         retro_sleep(1);
   }
   core_file_close(f);
   return   moved
         && now.size       == was.size
         && now.mtime      == was.mtime
         && now.mtime_nsec == was.mtime_nsec;
}

static int lib_id(dylib_t lib)
{
   int (*fn)(void) = lib
      ? (int (*)(void))dylib_proc(lib, "core_load_test_id") : NULL;
   return fn ? fn() : 0;
}

/* What Always Trust records for path: its hash and identity */
static bool trust(const char *path)
{
   struct core_check c;
   size_t n            = 0;
   const uint8_t *keys = core_sig_builtin_keys(&n);
   core_trust_set_t *s = core_trust_runtime_get();
   bool ok;
   if (!s)
      return false;
   core_check_file(s, keys, n, path, 0, &c);
   core_trust_set_put(s);
   ok = core_trust_runtime_always(&c);
   drain();
   return ok;
}

static unsigned store_n;

static void start(bool threaded)
{
   char store[64];
   task_queue_init(threaded, NULL);
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   path_mkdir(TMP_DIR "/cores");
   CHECK(core_trust_runtime_init(store, NULL, NULL));
}

static void stop(void)
{
   drain();
   core_load_check_deinit();
   core_trust_runtime_deinit();
   task_queue_deinit();
}

static void test_inactive(void)
{
   dylib_t lib = (dylib_t)1;
   char core[256];
   task_queue_init(false, NULL);
   path_mkdir(TMP_DIR "/cores");
   CHECK(put_core(core, "inactive_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OFF);
   CHECK(!lib);
   CHECK(!core_load_check_refused(core));
   task_queue_deinit();
}

static void test_checks_off(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   CHECK(put_core(core, "off_libretro" LIB_EXT, 'a'));
   CHECK(core_trust_runtime_set_checks(false));
   drain();
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   CHECK(queued() == 0);
   if (lib)
      dylib_close(lib);
   stop();
}

static void test_refused_cached(void)
{
   dylib_t lib = NULL;
   char core[256];
   char copy[256];
   start(false);
   CHECK(put_core(core, "unsigned_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   CHECK(core_load_check_refused(core));
   CHECK(!core_load_check_refused_seed(core));
   CHECK(queued() == 1);
   drain();
   /* The quiet check answers the next try: no second hash */
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 0);
   /* A new file under that name is checked again */
   CHECK(put_core(core, "unsigned_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   drain();
   /* The store changed (these bytes trusted under another name), which
    * the quick check can't see: the refusal no longer answers */
   CHECK(put_core(copy, "unsigned_copy_libretro" LIB_EXT, 'b'));
   CHECK(trust(copy));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   drain();
   /* Checks turned off: the quick check answers before a refusal */
   CHECK(put_core(core, "unsigned_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   drain();
   CHECK(core_trust_runtime_set_checks(false));
   drain();
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
   stop();
}

static void test_attach(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   CHECK(put_core(core, "twice_libretro" LIB_EXT, 'a'));
   checking_logs = 0;
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   CHECK(checking_logs == 1);
   stop();
}

static void test_full_check(bool threaded)
{
   dylib_t lib = NULL;
   char core[256];
   start(threaded);
   CHECK(put_core(core, "hash_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   /* The same bytes in a new file: its hash is trusted, not its identity */
   CHECK(put_core(core, "hash_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   drain();
#ifdef _WIN32
   /* No identity binds a quiet pass here, and no handle is kept for
    * it: the file can be replaced, and the next open checks again */
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   drain();
   CHECK(put_core(core, "hash_libretro" LIB_EXT, 'a'));
#else
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
#endif
   stop();
}

static void test_slot_binds_file(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   CHECK(put_core(core, "bind_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "bind_libretro" LIB_EXT, 'a'));
#ifdef _WIN32
   /* Only a waited pass is kept here, with its handle, and nothing can
    * replace the file while it is held */
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_set_startup(false);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   /* As its first load does on NTFS: the held handle still binds it */
   CHECK(ctime_move(core));
   CHECK(!put_core(core, "bind_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
#else
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   drain();
   /* With no held handle, its ctime binds the pass */
   CHECK(ctime_move(core));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   drain();
   /* Replaced after its check: the pass was for the file checked */
   CHECK(put_core(core, "bind_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   CHECK(queued() == 1);
#endif
   stop();
}

#ifdef _WIN32
/* LoadLibraryW loads <name>.dll for a file name with no '.': the check
 * reads that file, and "<name>." for <name> itself */
static void test_dll_name(void)
{
   dylib_t lib = NULL;
   char core[256];
   char dll[256];
   char dot[256];
   start(false);
   path_mkdir(TMP_DIR "/cores/dir.d");
   core_load_check_set_startup(true);
   CHECK(put_core(core, "dir.d/noext_libretro", 'a'));
   CHECK(trust(core));
   CHECK(put_core(dll, "dir.d/noext_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(lib_id(lib) == 0);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   CHECK(core_load_check_refused(dll));
   CHECK(trust(dll));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 2);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   sprintf(dot, "%s.", core);
   CHECK(core_load_check_open(dot, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
   core_load_check_set_startup(false);
   stop();
}
#endif

/* While the seed is pending, a core it covers is refused quietly
 * and not checked; nothing from before the seed answers after it */
static void test_seed_probe(void)
{
   dylib_t lib = NULL;
   char store[64];
   char core[256];
   void *buf   = NULL;
   int64_t len = 0;
   task_queue_init(false, NULL);
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   path_mkdir(TMP_DIR "/seed0");
   strlcpy(core, TMP_DIR "/seed0/probe_libretro." CORE_EXT, sizeof(core));
   CHECK(filestream_read_file("core_load_check_test_b" LIB_EXT, &buf, &len));
   CHECK(buf && filestream_write_file(core, buf, len));
   free(buf);
   CHECK(core_trust_runtime_init(store, TMP_DIR "/seed0", CORE_EXT));
   CHECK(core_trust_runtime_seed_pending());
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_refused(core));
   CHECK(core_load_check_refused_seed(core));
   /* Only the seed's own task */
   CHECK(queued() == 1);
   drain();
   CHECK(!core_trust_runtime_seed_pending());
#ifdef _WIN32
   /* No identity record here: checked again, not answered from before */
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
#else
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 2);
   if (lib)
      dylib_close(lib);
#endif
   stop();
}

/* Cancelled before its hash ended: no verdict, so checked again */
static void test_cancelled(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   CHECK(put_core(core, "cancel_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   task_queue_reset();
   drain();
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   CHECK(queued() == 1);
   if (lib)
      dylib_close(lib);
   stop();
}

/* Unreadable or gone: the caller's error, not a check's refusal */
static void test_unopenable(void)
{
   dylib_t lib = NULL;
   start(false);
   last_log[0] = '\0';
   CHECK(core_load_check_open(TMP_DIR "/cores/missing_libretro" LIB_EXT,
            &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!core_load_check_refused(TMP_DIR "/cores/missing_libretro" LIB_EXT));
   CHECK(strstr(last_log, "Could not open") != NULL);
   CHECK(queued() == 0);
   stop();
}

static void test_startup_inline(void)
{
   dylib_t lib = NULL;
   char good[256];
   char bad[256];
   start(false);
   CHECK(put_core(good, "start_good_libretro" LIB_EXT, 'a'));
   CHECK(trust(good));
   CHECK(put_core(good, "start_good_libretro" LIB_EXT, 'a'));
   CHECK(put_core(bad, "start_bad_libretro" LIB_EXT, 'b'));
   core_load_check_set_startup(true);
   /* Checked whole in place: no frame passes before the answer */
   CHECK(core_load_check_open(good, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   CHECK(core_load_check_open(bad, &lib) == CORE_LOAD_GATE_REFUSED);
   core_load_check_set_startup(false);
   CHECK(core_load_check_refused(bad));
   drain();
   CHECK(core_load_check_open(bad, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 0);
   stop();
}

/* The startup's one wait isn't repeated: the probe after the refused
 * startup load is answered by the refusal kept for that file */
static void test_startup_refused_once(void)
{
   dylib_t lib = NULL;
   char bad[256];
   start(false);
   CHECK(put_core(bad, "start_once_libretro" LIB_EXT, 'b'));
   core_load_check_set_startup(true);
#ifdef COUNT_HASHES
   hashes = 0;
#endif
   CHECK(core_load_check_open(bad, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_open(bad, &lib) == CORE_LOAD_GATE_REFUSED);
   core_load_check_set_startup(false);
   CHECK(!lib);
   CHECK(core_load_check_refused(bad));
#ifdef COUNT_HASHES
   CHECK(hashes == 1);
#endif
   stop();
}

static void test_deinit_drops(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   CHECK(put_core(core, "gen_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   core_load_check_deinit();
   drain();
   /* A result from before the deinit answers nothing */
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   stop();
}

#ifdef __linux__
extern int no_ctime_fs;

/* Open descriptors, to see a held file */
static unsigned open_fds(void)
{
   unsigned n     = 0;
   struct RDIR *d = retro_opendir("/proc/self/fd");
   if (!d)
      return 0;
   while (retro_readdir(d))
      n++;
   retro_closedir(d);
   return n;
}
#endif

#if defined(_WIN32) || defined(__linux__)
/* A load the user waits on (the startup one): its pass keeps its file
 * where no identity binds it */
static void load_waited(const char *path, int id)
{
   dylib_t lib = NULL;
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(path, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_set_startup(false);
   CHECK(lib_id(lib) == id);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
}
#endif

/* Where no identity binds a pass, the core loaded last keeps its file:
 * its automatic probes pass, any other core's don't, until another core
 * loads or a release, which closes every file the slots keep */
static void test_held_loaded(void)
{
#if defined(_WIN32) || defined(__linux__)
   dylib_t lib = NULL;
   char a[256];
   char b[256];
   char c[256];
#ifdef __linux__
   unsigned fds;
   no_ctime_fs = 1;
#endif
   start(false);
   CHECK(put_core(a, "held_a_libretro" LIB_EXT, 'a'));
   CHECK(trust(a));
   CHECK(put_core(a, "held_a_libretro" LIB_EXT, 'a'));
   CHECK(put_core(b, "held_b_libretro" LIB_EXT, 'b'));
   CHECK(trust(b));
   CHECK(put_core(b, "held_b_libretro" LIB_EXT, 'b'));
   CHECK(put_core(c, "held_c_libretro" LIB_EXT, 'c'));
#ifdef __linux__
   fds = open_fds();
#endif
   load_waited(a, 1);
   core_load_check_unloaded();
   /* Close Content's reload, menu or driver init: a probe of the core
    * loaded last passes while its file is held */
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   CHECK(queued() == 0);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
#ifdef _WIN32
   CHECK(!put_core(a, "held_a_libretro" LIB_EXT, 'a'));
#else
   CHECK(open_fds() == fds + 1);
#endif
   /* Any other core: refused quietly, checked in the background */
   CHECK(core_load_check_open(b, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_refused(b));
   CHECK(queued() == 1);
   drain();
   /* Loading another core releases the first one's file */
   load_waited(b, 2);
#ifdef _WIN32
   CHECK(put_core(a, "held_a_libretro" LIB_EXT, 'a'));
#else
   CHECK(open_fds() == fds + 1);
#endif
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   drain();
   /* A refusal the startup load waited on keeps its file in the slot */
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(c, &lib) == CORE_LOAD_GATE_REFUSED);
   core_load_check_set_startup(false);
#ifdef _WIN32
   CHECK(!put_core(c, "held_c_libretro" LIB_EXT, 'c'));
#else
   CHECK(open_fds() == fds + 2);
#endif
   /* Before the Core Updater or Core Manager touches a core, here while
    * b still runs: its record stays, but nothing binds a probe now, and
    * no file stays open */
   core_load_check_release_held();
#ifdef _WIN32
   CHECK(put_core(c, "held_c_libretro" LIB_EXT, 'c'));
#else
   CHECK(open_fds() == fds);
#endif
   CHECK(core_load_check_open(b, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   drain();
#ifdef _WIN32
   CHECK(put_core(b, "held_b_libretro" LIB_EXT, 'b'));
#endif
   core_load_check_unloaded();
   stop();
#ifdef __linux__
   no_ctime_fs = 0;
#endif
#endif
}

/* Loaded by path, the held core passes only while its path still names
 * the held file: a folder above it swapped for another isn't that file */
static void test_held_path_moved(void)
{
#if defined(_WIN32) || defined(__linux__)
   dylib_t lib = NULL;
   char a[256];
   char core[256];
   void *buf   = NULL;
   int64_t len = 0;
#ifdef __linux__
   no_ctime_fs = 1;
#endif
   start(false);
   CHECK(put_core(a, "moved_libretro" LIB_EXT, 'a'));
   CHECK(trust(a));
   strlcpy(core, TMP_DIR "/moved/moved_libretro" LIB_EXT, sizeof(core));
   path_mkdir(TMP_DIR "/moved");
   CHECK(filestream_read_file("core_load_check_test_a" LIB_EXT, &buf, &len));
   CHECK(buf && filestream_write_file(core, buf, len));
   free(buf);
   buf = NULL;
   load_waited(core, 1);
   core_load_check_unloaded();
   /* Windows can refuse to move a folder holding an open file; a
    * junction above it can still be pointed elsewhere */
   if (filestream_rename(TMP_DIR "/moved", TMP_DIR "/moved_aside"))
      printf("note: a folder holding an open file can't be moved here, "
            "not checked\n");
   else
   {
      path_mkdir(TMP_DIR "/moved");
      CHECK(filestream_read_file("core_load_check_test_c" LIB_EXT, &buf,
               &len));
      CHECK(buf && filestream_write_file(core, buf, len));
      free(buf);
      CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
      CHECK(lib_id(lib) == 0);
      CHECK(queued() == 1);
      if (lib)
         dylib_close(lib);
   }
   stop();
#ifdef __linux__
   no_ctime_fs = 0;
#endif
#endif
}

static unsigned osd_checking(void)
{
   char want[32];
   unsigned i;
   unsigned n = 0;
   sprintf(want, "msg %d", (int)MSG_CHECKING_CORE);
   for (i = 0; i < osd_count && i < OSD_MAX; i++)
      if (!strncmp(osd[i], want, strlen(want)))
         n++;
   return n;
}

static bool stage_wait(void)
{
   unsigned i;
   for (i = 0; i < 5000; i++)
   {
      if (core_load_check_stage_ready())
         return true;
      pump(1);
   }
   return false;
}

static void test_stage_quick(void)
{
   dylib_t lib = NULL;
   char core[256];
#ifdef __linux__
   unsigned fds;
#endif
   start(false);
   CHECK(put_core(core, "stage_off_libretro" LIB_EXT, 'a'));
   CHECK(core_trust_runtime_set_checks(false));
   drain();
#ifdef __linux__
   fds = open_fds();
#endif
   CHECK(core_load_check_stage_begin(core));
   /* Settled at once, and no worker to open it on */
   CHECK(core_load_check_stage_ready());
   CHECK(queued() == 0);
   CHECK(!core_load_check_stage_refused());
#ifdef __linux__
   /* Nothing kept for a core stage that opens it itself */
   CHECK(open_fds() == fds);
#endif
   CHECK(!core_load_check_stage_take(core));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
   stop();
}

static void test_stage_check(bool threaded)
{
   dylib_t lib = NULL;
   char core[256];
   unsigned shown;
   start(threaded);
   CHECK(put_core(core, "stage_hash_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "stage_hash_libretro" LIB_EXT, 'a'));
   shown = osd_checking();
   CHECK(core_load_check_stage_begin(core));
   CHECK(osd_checking() == shown + 1);
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   if (threaded)
      CHECK(lib_id(lib) == 1);
   else
   {
      /* No worker: the core stage opens it from the slot */
      CHECK(!lib);
      CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
      CHECK(lib_id(lib) == 1);
   }
   if (lib)
      dylib_close(lib);
   stop();
}

#ifdef COUNT_HASHES
/* Its ctime moved once a check's result is applied, before the pass
 * settles */
static char drift_path[256];
static unsigned drift_again;  /* moves after the first */

bool __real_core_trust_runtime_apply(const struct core_check *c);

bool __wrap_core_trust_runtime_apply(const struct core_check *c)
{
   if (drift_path[0])
   {
      CHECK(ctime_move(drift_path));
      if (drift_again)
         drift_again--;
      else
         drift_path[0] = '\0';
   }
   return __real_core_trust_runtime_apply(c);
}

/* Moved after its check, before the pass settles: checked again and
 * opened, never a pass with nothing to open. A held pass (Windows)
 * opens its handle. */
static void test_stage_drift(void)
{
   dylib_t lib = NULL;
   char core[256];
   unsigned before;
#ifndef _WIN32
   unsigned logs;
#endif
   start(true);
   CHECK(put_core(core, "stage_drift_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "stage_drift_libretro" LIB_EXT, 'a'));
   before = hashes;
#ifndef _WIN32
   logs   = moved_logs;
#endif
   strlcpy(drift_path, core, sizeof(drift_path));
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(!drift_path[0]);
   CHECK(!core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 1);
#ifndef _WIN32
   CHECK(hashes == before + 2);
   CHECK(moved_logs == logs + 1);
#else
   CHECK(hashes == before + 1);
#endif
   if (lib)
      dylib_close(lib);
#ifndef _WIN32
   lib = NULL;
   /* Moved after two checks: said once a stage */
   CHECK(put_core(core, "stage_drift_libretro" LIB_EXT, 'a'));
   before = hashes;
   logs   = moved_logs;
   strlcpy(drift_path, core, sizeof(drift_path));
   drift_again = 1;
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(!drift_path[0]);
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 1);
   CHECK(hashes == before + 3);
   CHECK(moved_logs == logs + 1);
   if (lib)
      dylib_close(lib);
#endif
   drift_path[0] = '\0';
   drift_again   = 0;
   stop();
}
#endif

/* The worker still opens a held pass's core once its first load moved
 * its ChangeTime (real Windows) */
static void test_stage_held_ctime_moved(void)
{
#ifdef _WIN32
   dylib_t lib = NULL;
   char core[256];
   start(true);
   CHECK(put_core(core, "stage_ctime_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "stage_ctime_libretro" LIB_EXT, 'a'));
   /* A waited pass keeps its handle; a probe loads the core first */
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_set_startup(false);
   if (lib)
      dylib_close(lib);
   CHECK(ctime_move(core));
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
   stop();
#endif
}

static void test_stage_refused(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   CHECK(put_core(core, "stage_bad_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(core_load_check_stage_refused());
   CHECK(!core_load_check_stage_take(core));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 0);
   /* Checks turned off: the quick check answers before the refusal */
   CHECK(core_trust_runtime_set_checks(false));
   drain();
   CHECK(core_load_check_stage_begin(core));
   CHECK(core_load_check_stage_ready());
   CHECK(!core_load_check_stage_refused());
   stop();
}

static void test_stage_take_other(void)
{
   char core[256];
   char other[256];
   start(true);
   CHECK(put_core(core, "stage_mine_libretro" LIB_EXT, 'a'));
   CHECK(put_core(other, "stage_other_libretro" LIB_EXT, 'b'));
   CHECK(core_trust_runtime_set_checks(false));
   drain();
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   /* Opened for its own core only */
   CHECK(!core_load_check_stage_take(other));
   stop();
}

/* A core the pending seed covers waits for it, then is checked */
static void test_stage_seed(void)
{
   dylib_t lib = NULL;
   char store[64];
   char dir[64];
   char core[256];
   void *buf   = NULL;
   int64_t len = 0;
   task_queue_init(false, NULL);
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   strlcpy(dir, TMP_DIR "/seed", sizeof(dir));
   path_mkdir(dir);
   strlcpy(core, TMP_DIR "/seed/seeded_libretro." CORE_EXT, sizeof(core));
   CHECK(filestream_read_file("core_load_check_test_b" LIB_EXT, &buf, &len));
   CHECK(buf && filestream_write_file(core, buf, len));
   free(buf);
   CHECK(core_trust_runtime_init(store, dir, CORE_EXT));
   CHECK(core_trust_runtime_seed_pending());
   /* A quiet probe first: refused, nothing checked or kept */
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_stage_begin(core));
   /* Only the seed's own task: no check of a guess */
   CHECK(queued() == 1);
   CHECK(!core_load_check_stage_ready());
   CHECK(stage_wait());
   CHECK(!core_trust_runtime_seed_pending());
   /* The seed trusted it, so the check after it passes */
   CHECK(!core_load_check_stage_refused());
   stop();
}

static void test_stage_cancel(void)
{
   char core[256];
   start(true);
   CHECK(put_core(core, "stage_cancel_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "stage_cancel_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_stage_begin(core));
   core_load_check_stage_cancel();
   drain();
   /* Nothing left over for a later take */
   CHECK(core_load_check_stage_ready());
   CHECK(!core_load_check_stage_take(core));
   stop();
}

/* The check the stage waits on is cancelled: refused, with no verdict
 * kept, so the next open checks it again */
static void test_stage_check_cancelled(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   CHECK(put_core(core, "stage_cut_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "stage_cut_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_stage_begin(core));
   CHECK(queued() == 1);
   task_queue_reset();
   CHECK(stage_wait());
   CHECK(core_load_check_stage_refused());
   CHECK(!core_load_check_stage_take(core));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   CHECK(queued() == 1);
   stop();
}

/* A check that can't start leaves no load waiting */
static void test_stage_no_check(void)
{
#ifdef COUNT_HASHES
   char core[256];
   start(false);
   CHECK(put_core(core, "stage_nohash_libretro" LIB_EXT, 'a'));
   hash_init_fails = true;
   CHECK(core_load_check_stage_begin(core));
   hash_init_fails = false;
   CHECK(queued() == 0);
   CHECK(core_load_check_stage_ready());
   CHECK(core_load_check_stage_refused());
   stop();
#endif
}

#if defined(_WIN32) || defined(__linux__)
/* Where only its file binds a pass, a pass lent to an open the stage
 * then cancels ends with it */
static void test_stage_lent_cancelled(void)
{
   dylib_t lib = NULL;
   char core[256];
#ifdef __linux__
   no_ctime_fs = 1;
#endif
   start(true);
   CHECK(put_core(core, "stage_lent_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "stage_lent_libretro" LIB_EXT, 'a'));
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_set_startup(false);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   CHECK(core_load_check_stage_begin(core));
   /* Its open on the worker */
   CHECK(!core_load_check_stage_ready());
   core_load_check_stage_cancel();
   drain();
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   CHECK(queued() == 1);
   if (lib)
      dylib_close(lib);
   stop();
#ifdef __linux__
   no_ctime_fs = 0;
#endif
}

/* Loaded by path, a pass opens on the worker only while its path still
 * names the file checked */
static void test_stage_path_moved(void)
{
   dylib_t lib = NULL;
   char a[256];
   char core[256];
   void *buf   = NULL;
   int64_t len = 0;
#ifdef __linux__
   no_ctime_fs = 1;
#endif
   start(true);
   CHECK(put_core(a, "stage_moved_libretro" LIB_EXT, 'a'));
   CHECK(trust(a));
   strlcpy(core, TMP_DIR "/stage_moved/stage_moved_libretro" LIB_EXT,
         sizeof(core));
   path_mkdir(TMP_DIR "/stage_moved");
   CHECK(filestream_read_file("core_load_check_test_a" LIB_EXT, &buf, &len));
   CHECK(buf && filestream_write_file(core, buf, len));
   free(buf);
   buf = NULL;
   CHECK(core_load_check_stage_begin(core));
   /* Its folder swapped while the check runs */
   if (filestream_rename(TMP_DIR "/stage_moved", TMP_DIR "/stage_moved_aside"))
      printf("note: a folder holding an open file can't be moved here, "
            "not checked\n");
   else
   {
      path_mkdir(TMP_DIR "/stage_moved");
      CHECK(filestream_read_file("core_load_check_test_c" LIB_EXT, &buf,
               &len));
      CHECK(buf && filestream_write_file(core, buf, len));
      free(buf);
      CHECK(stage_wait());
      /* The new file checked in the stage: refused, so the load parks */
      CHECK(core_load_check_stage_refused());
      lib = core_load_check_stage_take(core);
      CHECK(!lib);
      CHECK(lib_id(lib) == 0);
      if (lib)
         dylib_close(lib);
      lib = NULL;
      CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
      CHECK(!lib);
      CHECK(queued() == 0);
   }
   core_load_check_stage_cancel();
   stop();
#ifdef __linux__
   no_ctime_fs = 0;
#endif
}
#endif

/* One worker runs every task that isn't parallel: a task that holds it
 * keeps the tasks pushed after it waiting until worker_go() */
static retro_atomic_int_t worker_held;
static retro_atomic_int_t worker_free;

static void hold_handler(retro_task_t *task)
{
   retro_atomic_store_release_int(&worker_held, 1);
   while (!retro_atomic_load_acquire_int(&worker_free))
      retro_sleep(1);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static bool worker_hold(void)
{
   unsigned i;
   retro_task_t *task = task_init();
   if (!task)
      return false;
   retro_atomic_store_release_int(&worker_held, 0);
   retro_atomic_store_release_int(&worker_free, 0);
   task->handler = hold_handler;
   task_queue_push(task);
   for (i = 0; i < 5000 && !retro_atomic_load_acquire_int(&worker_held); i++)
      pump(1);
   return retro_atomic_load_acquire_int(&worker_held) != 0;
}

static void worker_go(void)
{
   retro_atomic_store_release_int(&worker_free, 1);
}

/* Loaded by path outside Android: a file swapped in after the check and
 * before the worker's open isn't opened */
static void test_stage_swapped(void)
{
   dylib_t lib = NULL;
   char core[256];
#ifndef _WIN32
   unsigned logs;
#endif
   start(true);
   CHECK(put_core(core, "stage_swap_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(worker_hold());
   CHECK(core_load_check_stage_begin(core));
   CHECK(!core_load_check_stage_ready());
#ifdef _WIN32
   /* The checked file is held: it can't be swapped */
   CHECK(!put_core(core, "stage_swap_libretro" LIB_EXT, 'c'));
   worker_go();
   CHECK(stage_wait());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 1);
#else
   logs = moved_logs;
   CHECK(put_core(core, "stage_swap_libretro" LIB_EXT, 'c'));
   worker_go();
   CHECK(stage_wait());
   CHECK(moved_logs == logs + 1);
   /* Checked again: refused, so the load parks */
   CHECK(core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(!lib);
   CHECK(lib_id(lib) == 0);
#endif
   if (lib)
      dylib_close(lib);
   stop();
}

/* The same swap to a trusted core: checked again, then opened */
static void test_stage_swapped_trusted(void)
{
#ifndef _WIN32
   dylib_t lib = NULL;
   char core[256];
   char b[256];
   unsigned logs;
   start(true);
   CHECK(put_core(b, "swap_b_libretro" LIB_EXT, 'b'));
   CHECK(trust(b));
   CHECK(put_core(core, "stage_swap_ok_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(worker_hold());
   CHECK(core_load_check_stage_begin(core));
   CHECK(!core_load_check_stage_ready());
   CHECK(put_core(core, "stage_swap_ok_libretro" LIB_EXT, 'b'));
   logs = moved_logs;
   worker_go();
   CHECK(stage_wait());
   CHECK(moved_logs == logs + 1);
   CHECK(!core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 2);
   if (lib)
      dylib_close(lib);
   stop();
#endif
}

/* Test library c written over path in place, a byte longer so the
 * identity changes whatever the clock's grain. False while a held
 * handle refuses the write (Windows). */
static bool rewrite_core(const char *path)
{
   void *buf   = NULL;
   int64_t len = 0;
   bool ok     = false;
   RFILE *f    = NULL;
   if (!filestream_read_file("core_load_check_test_c" LIB_EXT, &buf, &len))
      return false;
   if ((f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      ok = filestream_write(f, buf, len) == len
         && filestream_write(f, "", 1) == 1;
      if (filestream_close(f) != 0)
         ok = false;
   }
   free(buf);
   return ok;
}

#ifndef _WIN32
static bool path_identity(const char *path, struct core_identity *id)
{
   core_file_t *f = core_file_open(path);
   bool ok        = f && core_file_identity(f, id);
   core_file_close(f);
   return ok;
}
#endif

/* Nor is a file rewritten in place after the check: same inode, so
 * only the identity checked tells it apart */
static void test_stage_rewritten(void)
{
   dylib_t lib = NULL;
   char core[256];
#ifndef _WIN32
   struct core_identity before;
   struct core_identity after;
#endif
   start(true);
   CHECK(put_core(core, "stage_rewrite_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
#ifndef _WIN32
   CHECK(path_identity(core, &before));
#endif
   CHECK(worker_hold());
   CHECK(core_load_check_stage_begin(core));
   CHECK(!core_load_check_stage_ready());
#ifdef _WIN32
   /* The checked file is held: it can't be written */
   CHECK(!rewrite_core(core));
   worker_go();
   CHECK(stage_wait());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 1);
#else
   CHECK(rewrite_core(core));
   CHECK(path_identity(core, &after));
   CHECK(!memcmp(before.ino, after.ino, sizeof(before.ino)));
   CHECK(!core_identity_equal(&before, &after));
   worker_go();
   CHECK(stage_wait());
   CHECK(core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(!lib);
   CHECK(lib_id(lib) == 0);
#endif
   if (lib)
      dylib_close(lib);
   stop();
}

/* Probes while the stage's worker opens its core grant other cores:
 * the stage's load still ends the pass of the core loaded before */
static void test_stage_stale_grant(void)
{
#if defined(_WIN32) || defined(__linux__)
   dylib_t lib = NULL;
   char p[256];
   char a[256];
   char b[256];
#ifdef __linux__
   unsigned fds;
   no_ctime_fs = 1;
#endif
   start(true);
   CHECK(put_core(p, "stale_p_libretro" LIB_EXT, 'a'));
   CHECK(trust(p));
   CHECK(put_core(p, "stale_p_libretro" LIB_EXT, 'a'));
   CHECK(put_core(a, "stale_a_libretro" LIB_EXT, 'a'));
   CHECK(put_core(b, "stale_b_libretro" LIB_EXT, 'b'));
   load_waited(p, 1);
   core_load_check_unloaded();
#ifdef __linux__
   fds = open_fds();
#endif
   CHECK(core_trust_runtime_set_checks(false));
   drain();
   CHECK(worker_hold());
   CHECK(core_load_check_stage_begin(a));
   CHECK(!core_load_check_stage_ready());
   /* b's quick pass takes a's place, then p passes on its record */
   CHECK(core_load_check_open(b, &lib) == CORE_LOAD_GATE_OPENED);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   CHECK(core_load_check_open(p, &lib) == CORE_LOAD_GATE_OPENED);
   if (lib)
      dylib_close(lib);
   worker_go();
   CHECK(stage_wait());
   lib = core_load_check_stage_take(a);
   CHECK(lib_id(lib) == 1);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
#ifdef __linux__
   CHECK(open_fds() == fds - 1);
#endif
   CHECK(core_trust_runtime_set_checks(true));
   drain();
   CHECK(core_load_check_open(p, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   CHECK(queued() == 1);
   if (lib)
      dylib_close(lib);
   drain();
#ifdef _WIN32
   CHECK(put_core(p, "stale_p_libretro" LIB_EXT, 'a'));
#endif
   stop();
#ifdef __linux__
   no_ctime_fs = 0;
#endif
#endif
}

/* A staged load of the core loaded last passes on its record, as the
 * gate's first branch does: no hash, no "Checking core", and nothing
 * kept beside the record's own file */
static void test_stage_loaded(void)
{
#if defined(_WIN32) || defined(__linux__)
   dylib_t lib = NULL;
   char core[256];
   unsigned shown;
#ifdef __linux__
   unsigned fds;
   no_ctime_fs = 1;
#endif
   start(true);
   CHECK(put_core(core, "stage_held_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "stage_held_libretro" LIB_EXT, 'a'));
#ifdef __linux__
   fds = open_fds();
#endif
   load_waited(core, 1);
   core_load_check_unloaded();
   drain();
   shown = osd_checking();
   CHECK(core_load_check_stage_begin(core));
   CHECK(core_load_check_stage_ready());
   CHECK(queued() == 0);
   CHECK(osd_checking() == shown);
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_refused());
#ifdef __linux__
   CHECK(open_fds() == fds + 1);
#endif
   /* Nothing opened on the worker: the core stage's gate opens it from
    * the record */
   lib = core_load_check_stage_take(core);
   CHECK(!lib);
   if (!lib)
      CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   core_load_check_unloaded();
   /* Again, with a release before the stage ends: no file stays open */
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   core_load_check_release_held();
#ifdef _WIN32
   CHECK(put_core(core, "stage_held_libretro" LIB_EXT, 'a'));
#else
   CHECK(open_fds() == fds);
#endif
   core_load_check_stage_cancel();
   stop();
#ifdef __linux__
   no_ctime_fs = 0;
#endif
#endif
}

#ifdef _WIN32
/* \\?\ + the absolute form of rel, and suffix */
static void verbatim_path(char *s, size_t len, const char *rel,
      const char *suffix)
{
   char abs[512];
   strlcpy(abs, rel, sizeof(abs));
   path_resolve_realpath(abs, sizeof(abs), false);
   strlcpy(s, "\\\\?\\", len);
   strlcat(s, abs, len);
   strlcat(s, suffix, len);
}

/* CreateFileW opens a \\?\ path's trailing '.' as written, and
 * LoadLibraryW may drop it: refused, though trusted, wherever it is
 * asked for. Another \\?\ path loads. */
static void test_verbatim_dot(void)
{
   const char *forms[] = { "\\\\?\\", "//?/", "\\??\\" };
   dylib_t lib = NULL;
   char core[256];
   char path[512];
   void *buf   = NULL;
   int64_t len = 0;
   unsigned i;
   start(true);
   core_load_check_set_startup(true);
   CHECK(put_core(core, "verb_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   verbatim_path(path, sizeof(path), core, "");
   CHECK(core_load_check_open(path, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
   /* The trusted name and a '.', in each form: refused as given,
    * whatever _wfullpath makes of it */
   for (i = 0; i < sizeof(forms) / sizeof(*forms); i++)
   {
      verbatim_path(path, sizeof(path), core, ".");
      memcpy(path, forms[i], 4);
      last_log[0] = '\0';
      CHECK(core_load_check_open(path, &lib) == CORE_LOAD_GATE_REFUSED);
      CHECK(!lib);
      CHECK(strstr(last_log, "ending in '.'") != NULL);
      if (lib)
         dylib_close(lib);
      lib = NULL;
   }
   /* The trusted bytes as "verb_dot_libretro." itself */
   CHECK(put_core(core, "verb_dot_libretro", 'b'));
   verbatim_path(path, sizeof(path), core, ".");
   CHECK(filestream_read_file("core_load_check_test_a" LIB_EXT, &buf, &len));
   CHECK(buf && filestream_write_file(path, buf, len));
   free(buf);
   CHECK(trust(path));
   last_log[0] = '\0';
   CHECK(core_load_check_open(path, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   CHECK(strstr(last_log, "ending in '.'") != NULL);
   CHECK(!core_load_check_refused(path));
   core_load_check_set_startup(false);
   CHECK(core_load_check_stage_begin(path));
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_refused());
   CHECK(!core_load_check_stage_take(path));
   if (lib)
      dylib_close(lib);
   stop();
}
#endif

int main(void)
{
   if (!path_mkdir(TMP_DIR))
      return 1;
   test_inactive();
   test_checks_off();
   test_refused_cached();
   test_attach();
   test_full_check(false);
   test_full_check(true);
   test_slot_binds_file();
#ifdef _WIN32
   test_dll_name();
#endif
   test_startup_inline();
   test_startup_refused_once();
   test_deinit_drops();
   test_cancelled();
   test_seed_probe();
   test_unopenable();
   test_held_loaded();
   test_held_path_moved();
   test_stage_quick();
   test_stage_check(false);
   test_stage_check(true);
#ifdef COUNT_HASHES
   test_stage_drift();
#endif
   test_stage_held_ctime_moved();
   test_stage_refused();
   test_stage_take_other();
   test_stage_seed();
   test_stage_cancel();
   test_stage_check_cancelled();
   test_stage_no_check();
#if defined(_WIN32) || defined(__linux__)
   test_stage_lent_cancelled();
   test_stage_path_moved();
#endif
   test_stage_swapped();
   test_stage_swapped_trusted();
   test_stage_rewritten();
   test_stage_stale_grant();
   test_stage_loaded();
#ifdef _WIN32
   test_verbatim_dot();
#endif
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
