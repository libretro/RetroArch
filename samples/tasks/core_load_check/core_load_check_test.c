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
#ifdef VERITY_STANDIN
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "../../../tools/platform_stubs/android/android/dlext.h"
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
#ifdef VERITY_STANDIN
#include "../core_signature/core_signature_fixtures.h"
#endif

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

/* The frame loop polls the park only while its frame_work bit is up */
static unsigned frame_work;

void runloop_frame_work_set(unsigned bit, bool on)
{
   if (on)
      frame_work |= bit;
   else
      frame_work &= ~bit;
}

static void frame_iterate(void)
{
   if (frame_work & RUNLOOP_WORK_CORE_PARK)
      core_load_check_iterate();
}

#define OSD_MAX 256
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

static bool load_pending;

bool task_content_load_pending(void)
{
   return load_pending;
}

/* Quits asked for as a failed startup's */
static unsigned quits;

void retroarch_main_quit_failure(void)
{
   quits++;
}

#ifdef COUNT_HASHES
/* Hashes started, in place or in a task */
static unsigned hashes;
static bool hash_init_fails;
/* hash_moves's ctime moved as hash number hash_move_at starts */
static char hash_moves[256];
static unsigned hash_move_at;
static bool hash_moved;

static bool ctime_move(const char *path);

bool __real_core_hash_init(struct core_hash *h);

bool __wrap_core_hash_init(struct core_hash *h)
{
   if (++hashes == hash_move_at)
      hash_moved = ctime_move(hash_moves);
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

/* What Always Trust records for path: its hash and identity. Its save
 * is left queued. */
static bool trust_queued(const char *path)
{
   struct core_check c;
   size_t n            = 0;
   const uint8_t *keys = core_sig_builtin_keys(&n);
   core_trust_set_t *s = core_trust_runtime_get();
   if (!s)
      return false;
   core_check_file(s, keys, n, path, 0, &c);
   core_trust_set_put(s);
   return core_trust_runtime_always(&c);
}

static bool trust(const char *path)
{
   bool ok = trust_queued(path);
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

#ifdef VERITY_STANDIN
/* The kernel's fs-verity: 0 can't (as on this host), 1 on, 2 on after
 * verity_write is written over, as by a writer between the check's
 * hash and the enable, 3 refused (EPERM: require_signatures, a core
 * with no kernel signature) by a rollback that moves verity_write's
 * ctime, as f2fs's truncate does */
static int verity_mode;
static char verity_write[256];
static unsigned verity_enables;
static bool verity_moved;

/* As the app's: the real path, here wherever it is */
bool android_core_path_resolve(const char *path, char *s, size_t len)
{
   if (     !path || !*path || strlcpy(s, path, len) >= len
         || !path_resolve_realpath(s, len, true))
   {
      if (len)
         *s = '\0';
      return false;
   }
   return true;
}

static unsigned dlext_loads;
static unsigned dlext_moved;  /* loads from an fd not at offset 0 */
static uint64_t dlext_flags;  /* the last load's */

/* Android 5.0's linker, which core_load_file.c finds by dlsym: it
 * read()s the ELF header from the fd's offset (later ones pread at 0),
 * and maps the fd's file, whatever name names now */
void *android_dlopen_ext(const char *name, int flags,
      const android_dlextinfo *info)
{
   unsigned char head[64];
   char fd_path[32];
   (void)name;
   dlext_loads++;
   dlext_flags = info->flags;
   if (lseek(info->library_fd, 0, SEEK_CUR) != 0)
      dlext_moved++;
   if (     read(info->library_fd, head, sizeof(head)) != (ssize_t)sizeof(head)
         || memcmp(head, "\177ELF", 4))
      return NULL;
   sprintf(fd_path, "/proc/self/fd/%d", info->library_fd);
   return dlopen(fd_path, flags);
}

int __wrap_core_file_verity_enable(core_file_t *f)
{
   (void)f;
   verity_enables++;
   if (verity_mode == 2 && !rewrite_core(verity_write))
      return -1;
   if (verity_mode == 3)
   {
      verity_moved = ctime_move(verity_write);
      return 0;
   }
   return verity_mode ? 1 : 0;
}

static void verity_digest(struct core_identity *id)
{
   memset(id->verity, 0x5a, 32);
   id->verity_len = 32;
}

bool __wrap_core_file_verity_measure(core_file_t *f,
      struct core_identity *id)
{
   (void)f;
   verity_digest(id);
   return true;
}

/* The quick check's measure, made inside core_signature_io.c, reaches
 * the kernel through ioctl: while fs-verity is on, every file measures
 * as verity_digest() */
struct verity_head
{
   uint16_t alg;
   uint16_t size;
};

int __real_ioctl(int fd, unsigned long req, ...);

int __wrap_ioctl(int fd, unsigned long req, ...)
{
   va_list ap;
   void *arg;
   va_start(ap, req);
   arg = va_arg(ap, void*);
   va_end(ap);
   if (     (verity_mode == 1 || verity_mode == 2)
         && req == (unsigned long)_IOWR('f', 134,
            struct verity_head))
   {
      struct verity_head *h = (struct verity_head*)arg;
      h->alg  = 1;
      h->size = 32;
      memset(h + 1, 0x5a, 32);
      return 0;
   }
   return __real_ioctl(fd, req, arg);
}

/* The build time of what Always Trust records */
static uint64_t always_build_time;

bool __real_core_trust_runtime_always(const struct core_check *c);

bool __wrap_core_trust_runtime_always(const struct core_check *c)
{
   always_build_time = c->rec.build_time;
   return __real_core_trust_runtime_always(c);
}

/* The core_signature harness's signed core and its .sig, as FX_NAME */
static bool put_signed_core(char *s, size_t len)
{
   char sig[280];
   unsigned long x = FX_CORE_SEED;
   uint8_t *buf    = (uint8_t*)malloc(FX_CORE_SIZE);
   bool ok         = false;
   size_t i;
   if (!buf)
      return false;
   for (i = 0; i < FX_CORE_SIZE; i++)
   {
      x      = (x * 1103515245UL + 12345UL) & 0xffffffffUL;
      buf[i] = (uint8_t)(x >> 24);
   }
   strlcpy(s, TMP_DIR "/cores/" FX_NAME, len);
   strlcpy(sig, s, sizeof(sig));
   strlcat(sig, ".sig", sizeof(sig));
   ok = filestream_write_file(s, buf, FX_CORE_SIZE)
      && filestream_write_file(sig, fx_sig_good, sizeof(fx_sig_good));
   free(buf);
   return ok;
}

/* Not threaded: the check's handler runs on the main thread, where the
 * enable (a read of the whole core) must never run */
static void test_verity_main_thread(void)
{
   dylib_t lib = NULL;
   char core[256];
   struct core_identity id;
   core_trust_set_t *s;
   start(false);
   CHECK(put_core(core, "verity_main_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "verity_main_libretro" LIB_EXT, 'a'));
   CHECK(path_identity(core, &id));
   verity_mode    = 1;
   verity_enables = 0;
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_refused());
   CHECK(!core_load_check_stage_take(core));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   CHECK(verity_enables == 0);
   CHECK((s = core_trust_runtime_get()) != NULL);
   if (s)
   {
      CHECK(core_trust_has_identity(s, &id, "verity_main_libretro" LIB_EXT));
      verity_digest(&id);
      CHECK(!core_trust_has_identity(s, &id,
            "verity_main_libretro" LIB_EXT));
      core_trust_set_put(s);
   }
   if (lib)
      dylib_close(lib);
   verity_mode = 0;
   stop();
}

/* Hashed the same with fs-verity on: recorded with its digest */
static void test_verity_unchanged(void)
{
   dylib_t lib = NULL;
   char core[256];
   struct core_identity id;
   core_trust_set_t *s;
   start(true);
   CHECK(put_core(core, "verity_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "verity_libretro" LIB_EXT, 'a'));
   CHECK(path_identity(core, &id));
   verity_mode = 1;
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 1);
   CHECK((s = core_trust_runtime_get()) != NULL);
   if (s)
   {
      CHECK(!core_trust_has_identity(s, &id, "verity_libretro" LIB_EXT));
      verity_digest(&id);
      CHECK(core_trust_has_identity(s, &id, "verity_libretro" LIB_EXT));
      core_trust_set_put(s);
   }
   if (lib)
      dylib_close(lib);
   verity_mode = 0;
   stop();
}

/* The refused enable moved the file: recorded as it is after the
 * attempt, so loading it again needs no second check */
static void test_verity_refused_moved(void)
{
   dylib_t lib = NULL;
   char core[256];
   struct core_identity after;
   core_trust_set_t *s;
   unsigned before;
   start(true);
   CHECK(put_core(core, "verity_eperm_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "verity_eperm_libretro" LIB_EXT, 'a'));
   strlcpy(verity_write, core, sizeof(verity_write));
   verity_mode    = 3;
   verity_enables = 0;
   before         = hashes;
   /* Loaded */
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_loaded();
   core_load_check_unloaded();
   CHECK(verity_moved);
   CHECK(verity_enables == 1);
   CHECK(hashes == before + 2);
   CHECK(path_identity(core, &after));
   CHECK((s = core_trust_runtime_get()) != NULL);
   if (s)
   {
      CHECK(core_trust_has_identity(s, &after,
            "verity_eperm_libretro" LIB_EXT));
      core_trust_set_put(s);
   }
   /* Started: no second check */
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_refused());
   CHECK(!core_load_check_stage_take(core));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   CHECK(queued() == 0);
   CHECK(hashes == before + 2);
   CHECK(verity_enables == 1);
   if (lib)
      dylib_close(lib);
   verity_mode = 0;
   stop();
}

/* Moved again while hashed again: nothing held it, so it isn't
 * recorded so and the load checks it once more */
static void test_verity_refused_moved_again(void)
{
   dylib_t lib = NULL;
   char core[256];
   unsigned before;
   start(true);
   CHECK(put_core(core, "verity_eperm2_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "verity_eperm2_libretro" LIB_EXT, 'a'));
   strlcpy(verity_write, core, sizeof(verity_write));
   strlcpy(hash_moves, core, sizeof(hash_moves));
   verity_mode    = 3;
   verity_enables = 0;
   before         = hashes;
   hash_moved     = false;
   hash_move_at   = before + 2;
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(hash_moved);
   CHECK(!core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 1);
   CHECK(verity_enables == 2);
   CHECK(hashes == before + 4);
   if (lib)
      dylib_close(lib);
   hash_move_at = 0;
   verity_mode  = 0;
   stop();
}

/* Written over between its hash and the enable: the kernel keeps what
 * was written, so the check fails and records nothing */
static void test_verity_rewritten(void)
{
   dylib_t lib = NULL;
   char core[256];
   struct core_identity before;
   struct core_identity after;
   core_trust_set_t *s;
   start(true);
   CHECK(put_core(core, "verity_rw_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "verity_rw_libretro" LIB_EXT, 'a'));
   CHECK(path_identity(core, &before));
   strlcpy(verity_write, core, sizeof(verity_write));
   verity_mode = 2;
   last_log[0] = '\0';
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(strstr(last_log, "changed while it was checked") != NULL);
   CHECK(core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(!lib);
   CHECK(path_identity(core, &after));
   verity_digest(&after);
   CHECK((s = core_trust_runtime_get()) != NULL);
   if (s)
   {
      CHECK(!core_trust_has_identity(s, &before,
            "verity_rw_libretro" LIB_EXT));
      CHECK(!core_trust_has_identity(s, &after,
            "verity_rw_libretro" LIB_EXT));
      core_trust_set_put(s);
   }
   if (lib)
      dylib_close(lib);
   verity_mode = 0;
   stop();
}
#endif

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

static unsigned reruns;
static unsigned frees;
static int rerun_id;
static bool rerun_loads;      /* the re-run's open goes on to load */
static unsigned rerun_frees;  /* frees once the re-run's core loaded */

/* The re-run opens the core as the load it stands for would */
static void rerun_fn(void *ud)
{
   dylib_t lib = NULL;
   reruns++;
   rerun_id    = 0;
   if (core_load_check_open((const char*)ud, &lib) == CORE_LOAD_GATE_OPENED)
      rerun_id = lib_id(lib);
   if (lib && rerun_loads)
   {
      core_load_check_loaded();
      rerun_frees = frees;
      core_load_check_unloaded();
   }
   if (lib)
      dylib_close(lib);
}

static void free_fn(void *ud)
{
   frees++;
   free(ud);
}

static bool park(const char *path)
{
   size_t len = strlen(path) + 1;
   char *ud   = (char*)malloc(len);
   if (!ud)
      return false;
   memcpy(ud, path, len);
   if (core_load_check_park(path, rerun_fn, free_fn, ud))
      return true;
   free(ud);
   return false;
}

/* Frames until the park re-runs or gives up */
static void park_wait(unsigned want_reruns)
{
   unsigned i;
   for (i = 0; i < 5000 && reruns < want_reruns; i++)
   {
      frame_iterate();
      pump(1);
   }
}

/* A few frames: enough for a park to re-run if it is going to */
static void park_frames(unsigned n)
{
   while (n--)
   {
      frame_iterate();
      pump(1);
   }
}

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

static unsigned osd_refused(void)
{
   return osd_shown(MSG_CORE_LOAD_REFUSED_NO_MENU);
}

static void park_reset(void)
{
   reruns      = 0;
   frees       = 0;
   rerun_id    = 0;
   rerun_loads = false;
   rerun_frees = 0;
}

static void test_park_load_once(void)
{
   dylib_t lib = NULL;
   char core[256];
   unsigned opens;
   start(false);
   park_reset();
   CHECK(put_core(core, "once_libretro" LIB_EXT, 'a'));
   /* A refused probe, then its standalone entry point parks it */
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   opens = menu_opens;
   CHECK(park(core));
   CHECK(frame_work & RUNLOOP_WORK_CORE_PARK);
   CHECK(queued() == 1);
   drain();
   CHECK(menu_opens == opens + 1);
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   /* Not while a load is still on its way to the dummy core */
   load_pending = true;
   frame_iterate();
   CHECK(reruns == 0);
   load_pending = false;
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 1);
   CHECK(frees == 1);
   /* Re-run and dropped: the frame loop stops polling it */
   CHECK(!(frame_work & RUNLOOP_WORK_CORE_PARK));
   /* The pass is for the file the user saw, never one put there since */
#ifdef _WIN32
   /* Its held handle is the binding: nothing replaces the file */
   CHECK(!put_core(core, "once_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
#else
   CHECK(put_core(core, "once_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
#endif
   stop();
}

static void test_park_always(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   park_reset();
   CHECK(put_core(core, "always_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   park_wait(1);
   CHECK(rerun_id == 1);
   drain();
   core_load_check_deinit();
#ifdef _WIN32
   /* No identity record here: the hash Always Trust recorded passes the
    * core when a load checks it again */
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_refused());
#else
   /* Recorded: with the slot gone, the quick check passes it */
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   if (lib)
      dylib_close(lib);
#endif
   stop();
}

/* Always Trust that can't be recorded (the list is full): loaded once,
 * and the OSD and the log say so */
static void test_park_always_full(void)
{
   struct core_check fill;
   struct core_check c;
   char core[256];
   unsigned i;
   unsigned shown;
   size_t n            = 0;
   const uint8_t *keys = core_sig_builtin_keys(&n);
   core_trust_set_t *s = NULL;
   start(false);
   park_reset();
   memset(&fill, 0, sizeof(fill));
   fill.result = CORE_CHECK_UNSIGNED;
   strlcpy(fill.rec.name, "fill_libretro" LIB_EXT, sizeof(fill.rec.name));
   fill.rec.sha256[2] = 0xa5;
   for (i = 0; i < CORE_TRUST_MAX_HASHES; i++)
   {
      fill.rec.sha256[0] = (uint8_t)i;
      fill.rec.sha256[1] = (uint8_t)(i >> 8);
      if (!core_trust_runtime_always(&fill))
         break;
   }
   CHECK(i == CORE_TRUST_MAX_HASHES);
   drain();
   CHECK(put_core(core, "full_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   last_log[0] = '\0';
   shown       = osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL);
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   CHECK(strstr(last_log, "Could not trust") != NULL);
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 1);
   CHECK(frees == 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == shown + 1);
   CHECK((s = core_trust_runtime_get()) != NULL);
   if (s)
   {
      core_check_file(s, keys, n, core, 0, &c);
      CHECK(core_trust_has_hash(s, fill.rec.sha256));
      CHECK(!core_trust_has_hash(s, c.rec.sha256));
      core_trust_set_put(s);
   }
   stop();
}

/* A seeded store of n fake hashes and nothing else */
static bool write_hashes(const char *path, unsigned n)
{
   struct core_check c;
   core_trust_t *t = core_trust_new();
   size_t len      = 0;
   uint8_t *buf    = NULL;
   unsigned i;
   bool ok         = t && core_trust_set_flag(t, CORE_TRUST_SEEDED, true);
   memset(&c, 0, sizeof(c));
   c.result = CORE_CHECK_UNSIGNED;
   strlcpy(c.rec.name, "fill_libretro" LIB_EXT, sizeof(c.rec.name));
   c.rec.sha256[2] = 0xa6;
   for (i = 0; ok && i < n; i++)
   {
      c.rec.sha256[0] = (uint8_t)i;
      c.rec.sha256[1] = (uint8_t)(i >> 8);
      ok = core_trust_always(t, &c);
   }
   if (ok)
      buf = core_trust_serialize(t, &len);
   ok = buf && filestream_write_file(path, buf, (int64_t)len);
   free(buf);
   core_trust_free(t);
   return ok;
}

/* start() on a store already written, with a seed folder */
static void start_with(const char *store, const char *seed_dir)
{
   task_queue_init(false, NULL);
   path_mkdir(TMP_DIR "/cores");
   CHECK(core_trust_runtime_init(store, seed_dir, CORE_EXT));
}

static bool copy_core(const char *from, const char *to);

/* Always Trust with the list full loads the core at once and waits on a
 * sweep of the cores folder, which makes room: nothing to say */
static void test_park_always_sweeps(void)
{
   struct core_check c;
   char store[64];
   char core[256];
   unsigned shown;
   size_t n            = 0;
   const uint8_t *keys = core_sig_builtin_keys(&n);
   core_trust_set_t *s = NULL;
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES));
   path_mkdir(TMP_DIR "/sweep_cores");
   strlcpy(core, TMP_DIR "/sweep_cores/sweep_libretro." CORE_EXT,
         sizeof(core));
   CHECK(copy_core("core_load_check_test_a" LIB_EXT, core));
   start_with(store, TMP_DIR "/sweep_cores");
   park_reset();
   CHECK(park(core));
   drain();
   last_log[0] = '\0';
   shown       = osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL);
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == shown);
   drain();
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == shown);
   CHECK(!strstr(last_log, "Could not trust"));
   CHECK((s = core_trust_runtime_get()) != NULL);
   if (s)
   {
      CHECK(core_check_file(s, keys, n, core, 0, &c) == CORE_CHECK_OK);
      core_trust_set_put(s);
   }
   stop();
}

/* Always Trust while the trusted cores list can't be saved: loaded
 * this once, and the OSD and the log say so */
static void test_park_always_cant_save(void)
{
   char store[64];
   char core[256];
   unsigned full;
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   CHECK(path_mkdir(store));
   start_with(store, NULL);
   CHECK(core_trust_runtime_read_only());
   park_reset();
   CHECK(put_core(core, "cant_save_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   last_log[0] = '\0';
   full        = osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL);
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   CHECK(strstr(last_log, "Could not trust") != NULL);
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_CANT_SAVE) == 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == full);
   stop();
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

/* The seed's save failed, then Always Trust's own does too: the user
 * hears it once, from the can't-save message */
static void test_park_always_save_fails(void)
{
   char store[64];
   char core[256];
   unsigned osd, cant, failed;
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   path_mkdir(TMP_DIR "/seed_fails");
   saves_fail(true);
   start_with(store, TMP_DIR "/seed_fails");
   drain();
   CHECK(!core_trust_runtime_read_only() && !core_trust_runtime_can_save());
   park_reset();
   CHECK(put_core(core, "save_fails_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   osd    = osd_count;
   cant   = osd_shown(MSG_CORE_LOAD_TRUST_CANT_SAVE);
   failed = osd_shown(MSG_CORE_TRUST_SAVE_FAILED);
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   park_wait(1);
   drain();
   CHECK(reruns == 1);
   CHECK(osd_count == osd + 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_CANT_SAVE) == cant + 1);
   CHECK(osd_shown(MSG_CORE_TRUST_SAVE_FAILED) == failed);
   saves_fail(false);
   stop();
}

/* Always Trust with the list full and not saveable, and a sweep that
 * frees nothing: never trusted, so only the list-full message */
static void test_park_always_full_cant_save(void)
{
   struct core_check fill;
   char store[64];
   char core[256];
   unsigned i;
   unsigned osd, full, cant;
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   CHECK(write_hashes(store, 0));
   path_mkdir(TMP_DIR "/no_cores");
   start_with(store, TMP_DIR "/no_cores");
   saves_fail(true);
   /* Trusted this session: the sweep keeps them all */
   memset(&fill, 0, sizeof(fill));
   fill.result = CORE_CHECK_UNSIGNED;
   strlcpy(fill.rec.name, "fill_libretro" LIB_EXT, sizeof(fill.rec.name));
   fill.rec.sha256[2] = 0xa8;
   for (i = 0; i < CORE_TRUST_MAX_HASHES; i++)
   {
      fill.rec.sha256[0] = (uint8_t)i;
      fill.rec.sha256[1] = (uint8_t)(i >> 8);
      if (!core_trust_runtime_always(&fill))
         break;
   }
   CHECK(i == CORE_TRUST_MAX_HASHES);
   drain();
   CHECK(!core_trust_runtime_can_save());
   park_reset();
   CHECK(put_core(core, "full_cant_save_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   osd  = osd_count;
   full = osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL);
   cant = osd_shown(MSG_CORE_LOAD_TRUST_CANT_SAVE);
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   CHECK(osd_count == osd);
   park_wait(1);
   drain();
   CHECK(reruns == 1);
   CHECK(osd_count == osd + 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_LIST_FULL) == full + 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_CANT_SAVE) == cant);
   saves_fail(false);
   stop();
}

/* Always Trust with the list full and not saveable, and a sweep that
 * makes room: trusted for this session, said once the sweep trusted it */
static void test_park_always_sweeps_cant_save(void)
{
   struct core_check c;
   char store[64];
   char core[256];
   unsigned osd, cant;
   size_t n            = 0;
   const uint8_t *keys = core_sig_builtin_keys(&n);
   core_trust_set_t *s = NULL;
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES));
   path_mkdir(TMP_DIR "/no_cores");
   start_with(store, TMP_DIR "/no_cores");
   saves_fail(true);
   /* A failed save of no change of the user's says nothing */
   memset(&c, 0, sizeof(c));
   strlcpy(c.rec.name, "newest_libretro" LIB_EXT, sizeof(c.rec.name));
   c.result         = CORE_CHECK_OK;
   c.folder[0]      = 7;
   c.rec.build_time = 5;
   c.raise_newest   = true;
   CHECK(core_trust_runtime_apply(&c));
   drain();
   CHECK(!core_trust_runtime_read_only() && !core_trust_runtime_can_save());
   park_reset();
   CHECK(put_core(core, "sweeps_cant_save_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   osd  = osd_count;
   cant = osd_shown(MSG_CORE_LOAD_TRUST_CANT_SAVE);
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   CHECK(osd_count == osd);
   park_wait(1);
   drain();
   CHECK(reruns == 1);
   CHECK(osd_count == osd + 1);
   CHECK(osd_shown(MSG_CORE_LOAD_TRUST_CANT_SAVE) == cant + 1);
   CHECK((s = core_trust_runtime_get()) != NULL);
   if (s)
   {
      CHECK(core_check_file(s, keys, n, core, 0, &c) == CORE_CHECK_OK);
      core_trust_set_put(s);
   }
   saves_fail(false);
   stop();
}

#ifdef COUNT_HASHES
static enum core_trust_state state_now(const char *path)
{
   core_trust_set_t *s         = core_trust_runtime_get();
   enum core_trust_state state = s ? core_trust_state_file(s, path, NULL)
      : CORE_TRUST_STATE_UNCHECKED;
   core_trust_set_put(s);
   return state;
}

/* A sweep takes a recorded core's hash from its record, unread */
static void test_sweep_uses_records(void)
{
   struct core_check c;
   char store[64];
   char x[256], y[256];
   unsigned n;
   size_t num_keys     = 0;
   const uint8_t *keys = core_sig_builtin_keys(&num_keys);
   core_trust_set_t *s;
   memset(&c, 0, sizeof(c));
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   CHECK(write_hashes(store, CORE_TRUST_MAX_HASHES - 1));
   path_mkdir(TMP_DIR "/sweep_records");
   strlcpy(x, TMP_DIR "/sweep_records/x_libretro." CORE_EXT, sizeof(x));
   strlcpy(y, TMP_DIR "/sweep_records/y_libretro." CORE_EXT, sizeof(y));
   CHECK(copy_core("core_load_check_test_a" LIB_EXT, x));
   CHECK(copy_core("core_load_check_test_b" LIB_EXT, y));
   start_with(store, NULL);
   CHECK(trust(x));
   if (state_now(x) != CORE_TRUST_STATE_TRUSTED)
   {
      printf("note: test_sweep_uses_records skipped, no records on this filesystem\n");
      stop();
      return;
   }
   stop();
   start_with(store, TMP_DIR "/sweep_records");
   s = core_trust_runtime_get();
   CHECK(s && core_check_file(s, keys, num_keys, y, 0, &c)
         == CORE_CHECK_UNSIGNED);
   core_trust_set_put(s);
   n = hashes;
   CHECK(core_trust_runtime_always(&c));
   CHECK(state_now(y) != CORE_TRUST_STATE_TRUSTED);
   drain();
   CHECK(hashes - n == 1);
   CHECK(state_now(x) == CORE_TRUST_STATE_TRUSTED);
   CHECK(state_now(y) == CORE_TRUST_STATE_TRUSTED);
   stop();
}
#endif

static void test_park_cancel(void)
{
   char core[256];
   start(false);
   park_reset();
   CHECK(put_core(core, "cancel_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   last_log[0] = '\0';
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   CHECK(strstr(last_log, "replaced its warning") == NULL);
   park_wait(1);
   CHECK(reruns == 0);
   CHECK(frees == 1);
   stop();
}

static unsigned other_cancels;

static void other_choice(enum core_warning_choice choice,
      const struct core_check *c, const char *core_path, void *userdata)
{
   (void)c;
   (void)core_path;
   (void)userdata;
   if (choice == CORE_WARNING_CANCEL)
      other_cancels++;
}

/* Another Security Warning replaces a park's: the park ends, and the
 * log says why */
static void test_park_replaced_other(void)
{
   char core[256];
   struct core_check c;
   start(false);
   park_reset();
   other_cancels = 0;
   CHECK(put_core(core, "other_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   memset(&c, 0, sizeof(c));
   c.result    = CORE_CHECK_UNSIGNED;
   last_log[0] = '\0';
   CHECK(core_warning_push(&c, CORE_WARNING_INSTALL, core,
            other_choice, NULL));
   CHECK(frees == 1);
   CHECK(strstr(last_log, "replaced its warning") != NULL);
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   CHECK(other_cancels == 1);
   park_frames(3);
   CHECK(reruns == 0);
   stop();
}

static void test_park_pass(void)
{
   char core[256];
   unsigned opens;
   start(false);
   park_reset();
   CHECK(put_core(core, "pass_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "pass_libretro" LIB_EXT, 'a'));
   opens = menu_opens;
   CHECK(park(core));
   park_wait(1);
   /* Passed its check: re-run with no question asked */
   CHECK(reruns == 1);
   CHECK(rerun_id == 1);
   CHECK(menu_opens == opens);
   stop();
}

/* A park and a stage share a check whose core moved before its pass
 * settled: the park waits for the stage's new check, never answers
 * from the slot that check cleared */
static void test_park_stage_drift(void)
{
#if defined(COUNT_HASHES) && !defined(_WIN32)
   dylib_t lib = NULL;
   char core[256];
   unsigned i;
   unsigned before;
   start(true);
   park_reset();
   CHECK(put_core(core, "park_drift_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "park_drift_libretro" LIB_EXT, 'a'));
   before = hashes;
   CHECK(park(core));
   CHECK(core_load_check_stage_begin(core));
   CHECK(queued() == 1);
   strlcpy(drift_path, core, sizeof(drift_path));
   for (i = 0; i < 5000 && drift_path[0]; i++)
      pump(1);
   CHECK(!drift_path[0]);
   /* The new check's callback can't have run yet */
   frame_iterate();
   CHECK(reruns == 0);
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 1);
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 1);
   /* The park joined the stage's new check */
   CHECK(hashes == before + 2);
   if (lib)
      dylib_close(lib);
   drift_path[0] = '\0';
   stop();
#endif
}

/* No menu to ask on: Cancel, a log line and an OSD that waits for the
 * drivers */
static void test_park_no_menu(void)
{
   char core[256];
   unsigned shown;
   start(false);
   park_reset();
   menu_can_open = false;
   last_log[0]   = '\0';
   shown         = osd_refused();
   quits         = 0;
   CHECK(put_core(core, "nomenu_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   CHECK(frees == 1);
   CHECK(reruns == 0);
   CHECK(strstr(last_log, "Always Trust") != NULL);
   /* Not the startup load's: the user goes on with what runs */
   CHECK(quits == 0);
   CHECK(osd_refused() == shown);
   core_load_check_notify();
   CHECK(osd_refused() == shown + 1);
   /* With the drivers up it shows at once; the refusal kept for the
    * file answers, so the park ends at once. Windows keeps no file or
    * ctime to bind it: checked again. */
#ifdef _WIN32
   CHECK(park(core));
   drain();
   CHECK(frees == 2);
#else
   CHECK(!park(core));
   CHECK(frees == 1);
#endif
   CHECK(osd_refused() == shown + 2);
   /* After a deinit it waits for the drivers again */
   core_load_check_deinit();
   CHECK(park(core));
   drain();
#ifdef _WIN32
   CHECK(frees == 3);
#else
   CHECK(frees == 2);
#endif
   CHECK(osd_refused() == shown + 2);
   /* and what it held is gone with the next one */
   core_load_check_deinit();
   core_load_check_notify();
   CHECK(osd_refused() == shown + 2);
   menu_can_open = true;
   stop();
}

static void test_park_replaced(void)
{
   char a[256];
   char b[256];
   start(false);
   park_reset();
   CHECK(put_core(a, "first_libretro" LIB_EXT, 'a'));
   CHECK(put_core(b, "second_libretro" LIB_EXT, 'b'));
   CHECK(park(a));
   drain();
   /* A newer request replaces the parked one */
   CHECK(park(b));
   CHECK(frees == 1);
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 2);
   stop();
}

/* A newer park while the older one's warning is up, asking at once
 * (its refusal is kept for this file): the older callback gets CANCEL
 * inside the newer push, the newer park survives, and nothing pushes
 * from that CANCEL */
static void test_park_replaced_up(void)
{
   char core[256];
   unsigned opens;
   start(false);
   park_reset();
   CHECK(put_core(core, "again_libretro" LIB_EXT, 'b'));
   CHECK(park(core));
   drain();
   opens = menu_opens;
   CHECK(park(core));
   CHECK(frees == 1);
#ifdef _WIN32
   /* Checked again first: nothing binds the refusal's file */
   drain();
#endif
   /* One push: the newer request's */
   CHECK(menu_opens == opens + 1);
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 2);
   CHECK(frees == 2);
   stop();
}

/* A quiet check's refusal keeps no file. With no kept ctime to bind it,
 * a park checks again rather than answer on a file opened after the
 * hash. */
static void test_park_quiet_refused(bool ctime_kept)
{
   dylib_t lib = NULL;
   char core[256];
   unsigned opens;
#ifdef __linux__
   no_ctime_fs = !ctime_kept;
#endif
   start(false);
   park_reset();
   CHECK(put_core(core, "quiet_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   drain();
   opens = menu_opens;
   CHECK(park(core));
   CHECK(queued() == (ctime_kept ? 0 : 1));
   drain();
   CHECK(menu_opens == opens + 1);
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(rerun_id == 2);
   stop();
#ifdef __linux__
   no_ctime_fs = 0;
#endif
}

/* A park's check is the user's: a pass the slot holds for another core
 * doesn't turn its answer away */
static void test_park_other_pass(void)
{
   char a[256];
   char b[256];
   unsigned opens;
   start(false);
   park_reset();
   CHECK(put_core(a, "other_a_libretro" LIB_EXT, 'a'));
   CHECK(trust(a));
   CHECK(put_core(a, "other_a_libretro" LIB_EXT, 'a'));
   CHECK(put_core(b, "other_b_libretro" LIB_EXT, 'b'));
   CHECK(park(a));
   park_wait(1);
   CHECK(rerun_id == 1);
   opens = menu_opens;
   CHECK(park(b));
   drain();
   CHECK(menu_opens == opens + 1);
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(2);
   CHECK(rerun_id == 2);
   stop();
}

/* The seed first, then the check. A quiet probe before it leaves
 * nothing that answers for the seed. */
static void test_park_seed(void)
{
   dylib_t lib = NULL;
   char store[64];
   char core[256];
   void *buf   = NULL;
   int64_t len = 0;
   task_queue_init(false, NULL);
   park_reset();
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   path_mkdir(TMP_DIR "/seed2");
   strlcpy(core, TMP_DIR "/seed2/parked_libretro." CORE_EXT, sizeof(core));
   CHECK(filestream_read_file("core_load_check_test_a" LIB_EXT, &buf, &len));
   CHECK(buf && filestream_write_file(core, buf, len));
   free(buf);
   CHECK(core_trust_runtime_init(store, TMP_DIR "/seed2", CORE_EXT));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   CHECK(park(core));
   CHECK(queued() == 1);
   frame_iterate();
   CHECK(reruns == 0);
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 1);
   stop();
}

/* A core that can't be opened is the caller's error */
static void test_park_unopenable(void)
{
   start(false);
   park_reset();
   CHECK(!park(TMP_DIR "/cores/missing_libretro" LIB_EXT));
   CHECK(frees == 0);
   CHECK(queued() == 0);
   stop();
}

/* The check a park waits on is cancelled: no verdict, so the load ends
 * as a Cancel would, and nothing stays parked */
static void test_park_check_cancelled(void)
{
   char core[256];
   unsigned opens;
   start(false);
   park_reset();
   CHECK(put_core(core, "park_cut_libretro" LIB_EXT, 'b'));
   opens = menu_opens;
   CHECK(park(core));
   CHECK(queued() == 1);
   task_queue_reset();
   drain();
   CHECK(frees == 1);
   CHECK(menu_opens == opens);
   park_wait(1);
   CHECK(reruns == 0);
   /* Nothing kept: a new park checks it again */
   CHECK(park(core));
   CHECK(queued() == 1);
   stop();
}

/* A check that can't start leaves nothing parked */
static void test_park_no_check(void)
{
#ifdef COUNT_HASHES
   char core[256];
   start(false);
   park_reset();
   CHECK(put_core(core, "park_nohash_libretro" LIB_EXT, 'b'));
   hash_init_fails = true;
   CHECK(!park(core));
   hash_init_fails = false;
   CHECK(queued() == 0);
   CHECK(frees == 0);
   park_wait(1);
   CHECK(reruns == 0);
   stop();
#endif
}

/* A deinit drops the park: its warning answered after that re-runs
 * nothing */
static void test_park_deinit(void)
{
   char core[256];
   start(false);
   park_reset();
   CHECK(put_core(core, "park_deinit_libretro" LIB_EXT, 'b'));
   CHECK(park(core));
   drain();
   last_log[0] = '\0';
   core_load_check_deinit();
   CHECK(frees == 1);
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(reruns == 0);
   CHECK(frees == 1);
   CHECK(strstr(last_log, "replaced its warning") == NULL);
   stop();
}

/* A core loads, opened as the startup load opens it (no task to wait
 * on) */
static void load_other(const char *path, int id)
{
   dylib_t lib = NULL;
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(path, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_set_startup(false);
   CHECK(lib_id(lib) == id);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   core_load_check_unloaded();
}

enum park_wait_kind
{
   PARK_IN_CHECK = 0,
   PARK_IN_USER,
   PARK_IN_IDLE,
   PARK_IN_SEED
};

/* Another core, or the parked one by another route, loads while a
 * park waits: the park ends as a Cancel would, and nothing re-runs
 * over that session */
static void park_other_load(enum park_wait_kind kind, bool same)
{
   char store[64];
   char a[256];
   char b[256];
   unsigned opens;
   dylib_t lib = NULL;
   void *buf   = NULL;
   int64_t len = 0;
   park_reset();
   if (kind == PARK_IN_SEED)
   {
      task_queue_init(false, NULL);
      sprintf(store, TMP_DIR "/trust_%u", ++store_n);
      path_mkdir(TMP_DIR "/seed4");
      path_mkdir(TMP_DIR "/cores");
      strlcpy(a, TMP_DIR "/seed4/waits_libretro." CORE_EXT, sizeof(a));
      CHECK(filestream_read_file("core_load_check_test_a" LIB_EXT,
               &buf, &len));
      CHECK(buf && filestream_write_file(a, buf, len));
      free(buf);
      CHECK(core_trust_runtime_init(store, TMP_DIR "/seed4", CORE_EXT));
   }
   else
   {
      start(false);
      CHECK(put_core(a, "waits_a_libretro" LIB_EXT, 'a'));
   }
   CHECK(put_core(b, "loads_b_libretro" LIB_EXT, 'b'));
   /* Running the queue would end the seed */
   if (!same)
      CHECK(kind == PARK_IN_SEED ? trust_queued(b) : trust(b));
   opens = menu_opens;
   CHECK(park(a));
   switch (kind)
   {
      case PARK_IN_CHECK:
         CHECK(queued() == 1);
         break;
      case PARK_IN_SEED:
         CHECK(core_trust_runtime_seed_pending());
         break;
      case PARK_IN_USER:
         drain();
         CHECK(core_warning_pending(NULL, NULL));
         break;
      case PARK_IN_IDLE:
         drain();
         CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
         /* Waits for a load on its way */
         load_pending = true;
         frame_iterate();
         CHECK(reruns == 0);
         break;
   }
   last_log[0] = '\0';
   if (same)
   {
      /* Passing now, as after an Always Trust from elsewhere */
      CHECK(trust_queued(a));
      load_other(a, 1);
   }
   else
      load_other(b, 2);
   CHECK(strstr(last_log, "replaced its warning") == NULL);
   CHECK(frees == 1);
   /* A warning up for it closed through its Cancel: nothing to answer */
   CHECK(!core_warning_pending(NULL, NULL));
   CHECK(!core_warning_choose(CORE_WARNING_LOAD_ONCE));
   load_pending = false;
   drain();
   park_frames(20);
   CHECK(reruns == 0);
   CHECK(frees == 1);
   /* Its check, if one ran, ended quietly: no warning came of it */
   if (kind != PARK_IN_USER && kind != PARK_IN_IDLE)
      CHECK(menu_opens == opens);
   /* Its Load Once went with it */
   if (kind == PARK_IN_IDLE && !same)
   {
      CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_REFUSED);
      CHECK(!lib);
      if (lib)
         dylib_close(lib);
   }
   stop();
}

static void test_park_other_load(void)
{
   park_other_load(PARK_IN_CHECK, false);
   park_other_load(PARK_IN_USER, false);
   park_other_load(PARK_IN_IDLE, false);
   park_other_load(PARK_IN_SEED, false);
}

static unsigned ends_cancelled;
static unsigned ends_dropped;
static unsigned ends_rerun;
static unsigned ends_reran;
static unsigned rerun_at_rerun; /* reruns when RERUN came */
static unsigned rerun_at_reran; /* reruns when RERAN came */

static void end_fn(enum core_load_park_end end)
{
   switch (end)
   {
      case CORE_LOAD_PARK_END_CANCELLED:
         ends_cancelled++;
         break;
      case CORE_LOAD_PARK_END_DROPPED:
         ends_dropped++;
         break;
      case CORE_LOAD_PARK_END_RERUN:
         ends_rerun++;
         rerun_at_rerun = reruns;
         break;
      case CORE_LOAD_PARK_END_RERAN:
         ends_reran++;
         rerun_at_reran = reruns;
         break;
   }
}

/* A park with its end callback, its warning up */
static void park_end_start(char *core, const char *name)
{
   start(false);
   park_reset();
   ends_cancelled = 0;
   ends_dropped   = 0;
   ends_rerun     = 0;
   ends_reran     = 0;
   rerun_at_rerun = 0;
   rerun_at_reran = 0;
   CHECK(put_core(core, name, 'a'));
   CHECK(park(core));
   core_load_check_park_on_end(end_fn);
   drain();
}

/* Cancel ends the park from the frame loop, not inside the warning */
static void test_park_end_cancel(void)
{
   char core[256];
   park_end_start(core, "end_cancel_libretro" LIB_EXT);
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   CHECK(ends_cancelled == 0);
   park_frames(1);
   CHECK(ends_cancelled == 1);
   CHECK(ends_dropped == 0);
   CHECK(reruns == 0);
   CHECK(frees == 1);
   park_frames(3);
   CHECK(ends_cancelled == 1);
   stop();
}

/* A load on its way ends first */
static void test_park_end_waits_for_load(void)
{
   char core[256];
   park_end_start(core, "end_waits_libretro" LIB_EXT);
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   load_pending = true;
   park_frames(3);
   CHECK(ends_cancelled == 0);
   load_pending = false;
   park_frames(1);
   CHECK(ends_cancelled == 1);
   CHECK(ends_dropped == 0);
   stop();
}

/* A newer warning over the park's ends it as a Cancel does */
static void test_park_end_replaced_warning(void)
{
   char core[256];
   struct core_check c;
   park_end_start(core, "end_replaced_libretro" LIB_EXT);
   memset(&c, 0, sizeof(c));
   c.result = CORE_CHECK_UNSIGNED;
   CHECK(core_warning_push(&c, CORE_WARNING_INSTALL, core,
            other_choice, NULL));
   CHECK(ends_cancelled == 0);
   park_frames(1);
   CHECK(ends_cancelled == 1);
   CHECK(ends_dropped == 0);
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   stop();
}

/* Another core's load takes over: nothing is put back over it */
static void test_park_end_other_load(void)
{
   char core[256];
   char other[256];
   park_end_start(core, "end_other_a_libretro" LIB_EXT);
   CHECK(put_core(other, "end_other_b_libretro" LIB_EXT, 'b'));
   CHECK(trust(other));
   load_other(other, 2);
   CHECK(ends_dropped == 1);
   CHECK(ends_cancelled == 0);
   CHECK(frees == 1);
   park_frames(3);
   CHECK(ends_dropped == 1);
   CHECK(ends_cancelled == 0);
   stop();
}

/* A re-run ends its park with RERUN before it and RERAN after it */
static void test_park_end_rerun(void)
{
   char core[256];
   park_end_start(core, "end_rerun_libretro" LIB_EXT);
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(ends_cancelled == 0);
   CHECK(ends_dropped == 0);
   CHECK(ends_rerun == 1);
   CHECK(ends_reran == 1);
   CHECK(rerun_at_rerun == 0);
   CHECK(rerun_at_reran == 1);
   stop();
}

static void test_park_end_deinit(void)
{
   char core[256];
   park_end_start(core, "end_deinit_libretro" LIB_EXT);
   core_load_check_deinit();
   CHECK(ends_dropped == 1);
   CHECK(ends_cancelled == 0);
   stop();
}

/* A built-in core's start is another core's load: it ends a waiting
 * park and the Load Once of the core loaded last */
static void test_builtin_load(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   park_reset();
   CHECK(put_core(core, "builtin_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   core_load_check_builtin_loaded();
   CHECK(frees == 1);
   CHECK(reruns == 0);
   /* Load Once, its load, Close Content and its reload */
   CHECK(park(core));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   /* The reload's grant is no built-in core's */
   core_load_check_builtin_loaded();
   core_load_check_unloaded();
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   stop();
}

static void test_park_same_load(void)
{
   park_other_load(PARK_IN_CHECK, true);
   park_other_load(PARK_IN_USER, true);
   park_other_load(PARK_IN_IDLE, true);
#ifndef _WIN32
   /* No kept ctime binds a record there: nothing passes it before the
    * seed is done */
   park_other_load(PARK_IN_SEED, true);
#endif
}

/* The park's own re-run is no other load: its core loading ends
 * nothing, and free_cb runs once, after the re-run */
static void test_park_own_rerun(void)
{
   char a[256];
   start(false);
   park_reset();
   CHECK(put_core(a, "own_libretro" LIB_EXT, 'a'));
   CHECK(park(a));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   rerun_loads = true;
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 1);
   CHECK(rerun_frees == 0);
   CHECK(frees == 1);
   park_frames(20);
   CHECK(reruns == 1);
   CHECK(frees == 1);
   stop();
}

static bool rerun_saw_refused;

/* A re-run whose load opens nothing (a cached probe) */
static void rerun_no_open_fn(void *ud)
{
   reruns++;
   rerun_saw_refused = core_load_check_refused((const char*)ud);
}

/* The refusal that parked the load is not the re-run's: one that opens
 * nothing must not find it and park again */
static void test_park_rerun_no_open(void)
{
   dylib_t lib = NULL;
   char a[256];
   char *ud    = (char*)malloc(sizeof(a));
   start(false);
   park_reset();
   CHECK(put_core(a, "noopen_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_refused(a));
   CHECK(ud != NULL);
   if (ud)
      memcpy(ud, a, sizeof(a));
   CHECK(ud && core_load_check_park(a, rerun_no_open_fn, free_fn, ud));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   rerun_saw_refused = true;
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(!rerun_saw_refused);
   CHECK(frees == 1);
   stop();
}

/* The refused load's fallback to the dummy core (the unload, and the
 * dummy's probe of the core it stands for) leaves the park asking */
static void test_park_fallback(void)
{
   dylib_t lib = NULL;
   char a[256];
   start(false);
   park_reset();
   CHECK(put_core(a, "fallback_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(park(a));
   drain();
   CHECK(core_warning_pending(NULL, NULL));
   core_load_check_unloaded();
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(frees == 0);
   CHECK(core_warning_pending(NULL, NULL));
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 1);
   CHECK(frees == 1);
   stop();
}

/* Load Once lasts until another core loads or a release. Its probes
 * pass after the unload where its identity binds it, as where only its
 * held file does (Windows), never for a file put there since */
static void test_load_once_lasts(void)
{
   dylib_t lib = NULL;
   char a[256];
   char b[256];
   start(false);
   park_reset();
   CHECK(put_core(a, "lasts_a_libretro" LIB_EXT, 'a'));
   CHECK(put_core(b, "lasts_b_libretro" LIB_EXT, 'b'));
   CHECK(trust(b));
   /* Load Once; the re-run (Load Core) only probes */
   CHECK(park(a));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(rerun_id == 1);
   /* Its load, then Close Content */
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
   /* Close Content's reload, menu or driver init: no check, no warning */
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   CHECK(queued() == 0);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
   /* A release while it runs (the Core Updater from the menu) ends it */
   core_load_check_release_held();
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   drain();
   core_load_check_unloaded();
   /* Load Once again, then another core loads */
   CHECK(park(a));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(2);
   CHECK(rerun_id == 1);
#ifdef _WIN32
   load_waited(b, 2);
#else
   /* b's identity record passes it: the slot is left alone */
   CHECK(core_load_check_open(b, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 2);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
#endif
   core_load_check_unloaded();
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
   drain();
   /* Load Once again, loaded and unloaded, then a file put there */
   CHECK(park(a));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(3);
   CHECK(rerun_id == 1);
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
#ifdef _WIN32
   /* Its held handle is the binding: nothing replaces the file */
   CHECK(!put_core(a, "lasts_a_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
#else
   CHECK(put_core(a, "lasts_a_libretro" LIB_EXT, 'b'));
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   CHECK(queued() == 1);
#endif
   stop();
}

/* Untrust ends the passes kept for the core, however its path is
 * spelled; the core running keeps its run-ahead source */
static void test_forget_rechecks(void)
{
   struct core_load_source src;
   dylib_t lib = NULL;
   char core[256];
   core_trust_set_t *s;
   enum core_trust_state state;
#ifdef COUNT_HASHES
   unsigned n;
#endif
   start(false);
   CHECK(put_core(core, "forget_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   s     = core_trust_runtime_get();
   state = s ? core_trust_state_file(s, core, NULL)
      : CORE_TRUST_STATE_UNCHECKED;
   core_trust_set_put(s);
   if (state != CORE_TRUST_STATE_TRUSTED)
   {
      printf("note: test_forget_rechecks skipped, no records on this filesystem\n");
      stop();
      return;
   }
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   core_load_check_loaded();
#ifdef COUNT_HASHES
   n = hashes;
#endif
   CHECK(core_trust_runtime_forget(
         TMP_DIR "/cores/../cores/forget_libretro" LIB_EXT));
   /* Its save */
   drain();
#ifdef COUNT_HASHES
   CHECK(hashes == n);
#endif
   CHECK(core_load_check_copy_source(core, &src) == CORE_LOAD_COPY_CHECK);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(!lib);
   CHECK(queued() == 1);
   drain();
   CHECK(core_load_check_refused(core));
   stop();
}

/* On real Windows a core's first load moves its ChangeTime: a Load
 * Once core whose re-run loaded it first still loads, and still passes
 * Close Content's reload, while its handle is held. On Linux nothing
 * pins a held file, so its ctime binds it. */
static void test_held_ctime_moved(void)
{
#if defined(_WIN32) || defined(__linux__)
   dylib_t lib = NULL;
   char a[256];
#ifdef __linux__
   no_ctime_fs = 1;
#endif
   start(false);
   park_reset();
   CHECK(put_core(a, "ctime_libretro" LIB_EXT, 'a'));
   CHECK(park(a));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(rerun_id == 1);
   CHECK(ctime_move(a));
#ifdef _WIN32
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   CHECK(queued() == 0);
   if (lib)
      dylib_close(lib);
#else
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(queued() == 1);
#endif
   stop();
#ifdef __linux__
   no_ctime_fs = 0;
#endif
#endif
}

#ifdef VERITY_STANDIN
/* A signed core written over between its hash and the enable, then
 * trusted: Always Trust records the file as the kernel keeps it, with
 * its digest and no signed build time, so later loads pass the quick
 * check with no hash */
static void test_verity_rewritten_always(void)
{
   dylib_t lib                   = NULL;
   enum core_check_result result = CORE_CHECK_OK;
   char core[256];
   struct core_identity id;
   core_trust_set_t *s;
   unsigned n;
   start(true);
   park_reset();
   CHECK(put_signed_core(core, sizeof(core)));
   strlcpy(verity_write, core, sizeof(verity_write));
   verity_mode = 2;
   CHECK(park(core));
   drain();
   CHECK(core_warning_pending(NULL, &result));
   CHECK(result == CORE_CHECK_MODIFIED);
   CHECK(path_identity(core, &id));
   verity_mode       = 1;
   n                 = hashes;
   always_build_time = FX_TIME;
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   CHECK(always_build_time == 0);
   park_wait(1);
   CHECK(rerun_id == 3);
   drain();
   CHECK((s = core_trust_runtime_get()) != NULL);
   if (s)
   {
      CHECK(!core_trust_has_identity(s, &id, FX_NAME));
      verity_digest(&id);
      CHECK(core_trust_has_identity(s, &id, FX_NAME));
      core_trust_set_put(s);
   }
   core_load_check_deinit();
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 3);
   CHECK(hashes == n);
   if (lib)
      dylib_close(lib);
   verity_mode = 0;
   stop();
}

/* The linker gets the fd at offset 0 after a full check's hash, and
 * again after its own read on an earlier load from the held file */
static void test_android_fd_offset(void)
{
   dylib_t lib = NULL;
   char core[256];
   int i;
   dlext_loads = 0;
   dlext_moved = 0;
   /* The startup load, checked in place */
   start(false);
   CHECK(put_core(core, "fd_start_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   CHECK(put_core(core, "fd_start_libretro" LIB_EXT, 'a'));
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_set_startup(false);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   stop();
   /* Load Core: the check task, then the open on the worker, from the
    * file checked where no ctime binds the pass */
   for (i = 0; i < 2; i++)
   {
      no_ctime_fs = i;
      start(true);
      CHECK(put_core(core, "fd_stage_libretro" LIB_EXT, 'b'));
      CHECK(trust(core));
      CHECK(put_core(core, "fd_stage_libretro" LIB_EXT, 'b'));
      CHECK(core_load_check_stage_begin(core));
      CHECK(stage_wait());
      CHECK(!core_load_check_stage_refused());
      lib = core_load_check_stage_take(core);
      CHECK(lib_id(lib) == 2);
      if (lib)
         dylib_close(lib);
      lib = NULL;
      stop();
   }
   no_ctime_fs = 0;
   /* Load Once: the re-run's probe, its load, Close Content's reload,
    * all from the file the check held */
   start(false);
   park_reset();
   CHECK(put_core(core, "fd_once_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(rerun_id == 1);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   core_load_check_loaded();
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   if (lib)
      dylib_close(lib);
   stop();
   CHECK(dlext_loads >= 6);
   CHECK(dlext_moved == 0);
}

/* which's bytes written over path's file in place, until its identity
 * moves (same size inside one coarse clock tick) */
static bool write_in_place(const char *path, char which)
{
   struct core_identity was, now;
   void *buf   = NULL;
   int64_t len = 0;
   bool moved  = false;
   unsigned i;
   if (     !path_identity(path, &was)
         || !filestream_read_file(which == 'a'
            ? "core_load_check_test_a" LIB_EXT
            : which == 'b'
            ? "core_load_check_test_b" LIB_EXT
            : "core_load_check_test_c" LIB_EXT, &buf, &len))
      return false;
   for (i = 0; i < 1000 && !moved; i++)
   {
      RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      bool ok  = f && filestream_write(f, buf, len) == len;
      if (f && filestream_close(f) != 0)
         ok = false;
      if (!ok || !path_identity(path, &now))
         break;
      moved = !core_identity_equal(&was, &now);
      if (!moved)
         retro_sleep(1);
   }
   free(buf);
   return moved && !memcmp(was.ino, now.ino, sizeof(was.ino));
}

/* path's file moved aside, and on until its identity moves (a rename
 * inside one coarse clock tick leaves its ctime) */
static bool move_aside(const char *path)
{
   static unsigned moved_n;
   struct core_identity was, now;
   char from[256];
   char to[256];
   bool moved = false;
   unsigned i;
   if (!path_identity(path, &was))
      return false;
   strlcpy(from, path, sizeof(from));
   for (i = 0; i < 1000 && !moved; i++)
   {
      sprintf(to, TMP_DIR "/cores/moved_%u", ++moved_n);
      if (filestream_rename(from, to) != 0 || !path_identity(to, &now))
         return false;
      strlcpy(from, to, sizeof(from));
      moved = !core_identity_equal(&was, &now);
      if (!moved)
         retro_sleep(1);
   }
   return moved;
}

/* which's bytes renamed over path once a ctime set now differs from its
 * file's, so the rename moves the replaced file's ctime */
static bool rename_over(const char *path, char which)
{
   struct core_identity was, now;
   char tmp[256];
   unsigned i;
   if (!path_identity(path, &was))
      return false;
   for (i = 0; i < 1000; i++)
   {
      if (     !put_core(tmp, "over_new" LIB_EXT, which)
            || !path_identity(tmp, &now))
         return false;
      if (now.ctime != was.ctime || now.ctime_nsec != was.ctime_nsec)
         return filestream_rename(tmp, path) == 0;
      retro_sleep(1);
   }
   return false;
}

/* Load Once loads from the file held since its check: written in place
 * while its warning is up, it is asked about again, once per answer.
 * A file moved aside or renamed over its path is asked about again. */
static void test_android_held_rewritten(void)
{
   dylib_t lib = NULL;
   char core[256];
   core_trust_set_t *s;
   struct core_check c;
   size_t n            = 0;
   const uint8_t *keys = core_sig_builtin_keys(&n);
   unsigned opens;
   start(false);
   park_reset();
   CHECK(put_core(core, "held_rw_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   CHECK(write_in_place(core, 'c'));
   opens = menu_opens;
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(reruns == 1);
   CHECK(rerun_id == 0);
   /* Refused: its caller parks it again, asking about the new bytes */
   CHECK(core_load_check_refused(core));
   CHECK(park(core));
   drain();
   CHECK(core_warning_pending(NULL, NULL));
   CHECK(menu_opens == opens + 1);
   /* Written again while asked: one re-run, refused, no more */
   CHECK(write_in_place(core, 'b'));
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(2);
   park_frames(20);
   CHECK(reruns == 2);
   CHECK(rerun_id == 0);
   CHECK(core_load_check_refused(core));
   CHECK(!core_warning_pending(NULL, NULL));
   /* Asked about as it is: Load Once loads it */
   CHECK(park(core));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(3);
   CHECK(rerun_id == 2);
   stop();
   /* Always Trust records only the bytes it asked about */
   start(false);
   park_reset();
   CHECK(put_core(core, "held_at_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   CHECK(write_in_place(core, 'c'));
   CHECK(core_warning_choose(CORE_WARNING_ALWAYS_TRUST));
   park_wait(1);
   CHECK(rerun_id == 0);
   drain();
   CHECK((s = core_trust_runtime_get()) != NULL);
   if (s)
   {
      core_check_file(s, keys, n, core, 0, &c);
      CHECK(c.result != CORE_CHECK_OK);
      core_trust_set_put(s);
   }
   stop();
   /* Moved aside or renamed over: either moves the held file's ctime,
    * so the file now there is asked about */
   start(false);
   park_reset();
   CHECK(put_core(core, "held_mv_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   CHECK(move_aside(core));
   CHECK(put_core(core, "held_mv_libretro" LIB_EXT, 'b'));
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(rerun_id == 0);
   CHECK(park(core));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(2);
   CHECK(rerun_id == 2);
   stop();
   start(false);
   park_reset();
   CHECK(put_core(core, "held_ro_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   CHECK(rename_over(core, 'b'));
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(rerun_id == 0);
   CHECK(park(core));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(2);
   CHECK(rerun_id == 2);
   stop();
   /* A content load's core stage after the re-run's probe */
   start(true);
   park_reset();
   CHECK(put_core(core, "held_st_libretro" LIB_EXT, 'a'));
   CHECK(park(core));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(rerun_id == 1);
   CHECK(write_in_place(core, 'c'));
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(core_load_check_stage_refused());
   lib = core_load_check_stage_take(core);
   CHECK(lib_id(lib) == 0);
   if (lib)
      dylib_close(lib);
   stop();
}

/* Refused and gone: the open that fails next never reached the gate,
 * so it isn't the check's refusal */
/* Built only into the Android objects (core_load_check_android.o,
 * core_load_file_android.o): weak, so the desktop test links */
__attribute__((weak)) core_file_t *core_load_check_instance_file(
      const char *path, char **load_path);
__attribute__((weak)) dylib_t core_load_file_instance(core_file_t *f,
      const char *path);

/* core_load_check_instance_file(), with the path it gives checked:
 * path's real path, as the running core was loaded by */
static core_file_t *instance_file(const char *path)
{
   char real[256];
   char *load_path = NULL;
   core_file_t *f  = core_load_check_instance_file(path, &load_path);
   CHECK(!f == !load_path);
   if (load_path)
   {
      strlcpy(real, path, sizeof(real));
      CHECK(path_resolve_realpath(real, sizeof(real), true)
            && string_is_equal(load_path, real));
      free(load_path);
   }
   return f;
}

/* fd's file is path's */
static bool fd_is_path(int fd, const char *path)
{
   struct stat a, b;
   return   fstat(fd, &a) == 0 && stat(path, &b) == 0
         && a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

/* Run-ahead's second instance on Android 6+: the running core's file,
 * the one it passed from and unchanged, as a file of its own, loaded
 * again as a new instance; none once the file is written, for another
 * core, or with nothing running. Checks off: the path's file. */
static void test_android_instance(void)
{
   dylib_t lib = NULL;
   core_file_t *f;
   core_file_t *g;
   char core[256];
   char other[256];
   int i;
   CHECK(core_load_check_instance_file && core_load_file_instance);
   if (!core_load_check_instance_file || !core_load_file_instance)
      return;
   for (i = 0; i < 2; i++)
   {
      no_ctime_fs = i;
      start(false);
      CHECK(put_core(core, "inst_libretro" LIB_EXT, 'a'));
      CHECK(put_core(other, "inst_other_libretro" LIB_EXT, 'b'));
      CHECK(trust(core));
      CHECK(!instance_file(core));
      /* Hashed at its load, as a startup load is */
      CHECK(put_core(core, "inst_libretro" LIB_EXT, 'a'));
      core_load_check_set_startup(true);
      CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
      core_load_check_set_startup(false);
      core_load_check_loaded();
      f = instance_file(core);
      g = instance_file(core);
      CHECK(f && g && core_file_fd(f) != core_file_fd(g));
      CHECK(f && fd_is_path(core_file_fd(f), core));
      /* An offset of its own: moving one moves no other */
      if (f && g)
      {
         CHECK(lseek(core_file_fd(f), 7, SEEK_SET) == 7);
         CHECK(lseek(core_file_fd(g), 0, SEEK_CUR) == 0);
      }
      dlext_flags = 0;
      if (f)
      {
         dylib_t again = core_load_file_instance(f, core);
         CHECK(again != NULL);
         CHECK(dlext_flags == (ANDROID_DLEXT_USE_LIBRARY_FD | 0x40));
         CHECK(lseek(core_file_fd(f), 0, SEEK_CUR) != 7);
         if (again)
            dylib_close(again);
      }
      core_file_close(f);
      core_file_close(g);
      CHECK(!instance_file(other));
      if (lib)
         dylib_close(lib);
      lib = NULL;
      CHECK(write_in_place(core, 'c'));
      CHECK(!instance_file(core));
      core_load_check_unloaded();
      CHECK(!instance_file(core));
      stop();
   }
   no_ctime_fs = 0;
   start(false);
   CHECK(put_core(core, "inst_off_libretro" LIB_EXT, 'a'));
   CHECK(core_trust_runtime_set_checks(false));
   drain();
   f = instance_file(core);
   CHECK(f && fd_is_path(core_file_fd(f), core));
   core_file_close(f);
   stop();
}

static void test_android_refused_gone(void)
{
   dylib_t lib = NULL;
   char core[256];
   start(false);
   park_reset();
   CHECK(put_core(core, "gone_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(park(core));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   CHECK(core_load_check_refused(core));
   CHECK(filestream_delete(core) == 0);
   CHECK(!core_load_check_refused(core));
   stop();
}
#endif

/* A first run's startup load of a core the pending seed covers */
static void test_startup_seed(void)
{
   dylib_t lib = NULL;
   char store[64];
   char core[256];
   void *buf   = NULL;
   int64_t len = 0;
   task_queue_init(false, NULL);
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   path_mkdir(TMP_DIR "/seed3");
   strlcpy(core, TMP_DIR "/seed3/first_libretro." CORE_EXT, sizeof(core));
   CHECK(filestream_read_file("core_load_check_test_b" LIB_EXT, &buf, &len));
   CHECK(buf && filestream_write_file(core, buf, len));
   free(buf);
   CHECK(core_trust_runtime_init(store, TMP_DIR "/seed3", CORE_EXT));
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   /* Not hashed in place: the seed decides first */
   CHECK(core_load_check_startup_take() == CORE_LOAD_STARTUP_SEED);
   CHECK(core_load_check_startup_take() == CORE_LOAD_STARTUP_NONE);
   core_load_check_set_startup(false);
   stop();
}

/* Refused at startup: asked from the menu, or with none, told how to
 * approve it */
static void test_startup_refused(void)
{
   dylib_t lib = NULL;
   char core[256];
   unsigned opens;
   start(false);
   park_reset();
   CHECK(put_core(core, "start_refused_libretro" LIB_EXT, 'b'));
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_startup_take() == CORE_LOAD_STARTUP_REFUSED);
   core_load_check_set_startup(false);
   /* Its park asks at once, about the file checked in place */
   opens = menu_opens;
   CHECK(park(core));
   CHECK(menu_opens == opens + 1);
   CHECK(queued() == 0);
   CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   CHECK(frees == 1);
   menu_can_open = false;
   last_log[0]   = '\0';
   CHECK(put_core(core, "start_refused_libretro" LIB_EXT, 'b'));
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_startup_take() == CORE_LOAD_STARTUP_FAILED);
   CHECK(strstr(last_log, "Always Trust") != NULL);
   core_load_check_set_startup(false);
   menu_can_open = true;
   stop();
}

enum seed_park_change
{
   SEED_PARK_SAME = 0,
   SEED_PARK_CHANGED,
   SEED_PARK_CUT,     /* changed, and quitting cancels its check */
   SEED_PARK_GONE
};

/* A first run's startup load held for the seed, with no menu to fall
 * back to: run again once the seed trusts it; refused after the seed,
 * it says how to approve the core and quits as a failed startup does.
 * With a menu it asks there instead. Its check cut off by quitting, it
 * is only dropped; not run for any other reason, it quits with no menu
 * and leaves the menu up with one. */
static void startup_seed_park(bool menu, enum seed_park_change change)
{
   dylib_t lib = NULL;
   char store[64];
   char core[256];
   unsigned opens;
   void *buf   = NULL;
   int64_t len = 0;
   task_queue_init(false, NULL);
   park_reset();
   quits         = 0;
   menu_can_open = menu;
   last_log[0]   = '\0';
   sprintf(store, TMP_DIR "/trust_%u", ++store_n);
   sprintf(core, TMP_DIR "/seed5_%u", store_n);
   path_mkdir(core);
   strlcat(core, "/held_libretro." CORE_EXT, sizeof(core));
   CHECK(filestream_read_file("core_load_check_test_a" LIB_EXT, &buf, &len));
   CHECK(buf && filestream_write_file(core, buf, len));
   free(buf);
   *strrchr(core, '/') = '\0';
   CHECK(core_trust_runtime_init(store, core, CORE_EXT));
   strlcat(core, "/held_libretro." CORE_EXT, sizeof(core));
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_REFUSED);
   CHECK(core_load_check_startup_take() == CORE_LOAD_STARTUP_SEED);
   CHECK(park(core));
   core_load_check_set_startup(false);
   /* The seed walks the folder first; a file put there after it isn't
    * trusted */
   while (core_trust_runtime_seed_pending())
      pump(1);
   if (change == SEED_PARK_GONE)
      CHECK(filestream_delete(core) == 0);
   else if (change != SEED_PARK_SAME)
      CHECK(rewrite_core(core));
   opens = menu_opens;
   if (change == SEED_PARK_CUT)
   {
      drain();
      frame_iterate();
      CHECK(queued() == 1);
      task_queue_reset();
   }
   park_frames(2);
   drain();
   park_frames(20);
   if (change == SEED_PARK_SAME)
   {
      CHECK(reruns == 1);
      CHECK(rerun_id == 1);
   }
   else if (change != SEED_PARK_CHANGED)
   {
      CHECK(reruns == 0);
      CHECK(menu_opens == opens);
   }
   else if (menu)
   {
      CHECK(reruns == 0);
      CHECK(menu_opens == opens + 1);
      CHECK(core_warning_choose(CORE_WARNING_CANCEL));
   }
   else
   {
      CHECK(reruns == 0);
      CHECK(strstr(last_log, "Always Trust") != NULL);
   }
   CHECK(frees == 1);
   CHECK(quits == ((!menu && (change == SEED_PARK_CHANGED
               || change == SEED_PARK_GONE)) ? 1u : 0u));
   menu_can_open = true;
   stop();
}

static void test_startup_seed_park(void)
{
   startup_seed_park(false, SEED_PARK_SAME);
   startup_seed_park(false, SEED_PARK_CHANGED);
   startup_seed_park(true, SEED_PARK_CHANGED);
   startup_seed_park(false, SEED_PARK_CUT);
   startup_seed_park(true, SEED_PARK_CUT);
   startup_seed_park(false, SEED_PARK_GONE);
   startup_seed_park(true, SEED_PARK_GONE);
}

/* A copy of path as run-ahead makes one, a new file */
static bool copy_core(const char *from, const char *to)
{
   void *buf   = NULL;
   int64_t len = 0;
   bool ok;
   if (!filestream_read_file(from, &buf, &len))
      return false;
   filestream_delete(to);
   ok = filestream_write_file(to, buf, len);
   free(buf);
   return ok;
}

static int match_all(core_load_match_t *m)
{
   int st = 1;
   while (st > 0)
      st = core_load_match_step(m);
   return st;
}

static void test_copy(void)
{
   dylib_t lib  = NULL;
   dylib_t copy = NULL;
   struct core_load_source src;
   core_load_match_t *m = NULL;
   char core[256];
   char tmp[256];
   start(false);
   CHECK(put_core(core, "ra_libretro" LIB_EXT, 'a'));
   strlcpy(tmp, TMP_DIR "/ra_copy" LIB_EXT, sizeof(tmp));
   /* Not the running core: no copy */
   CHECK(core_load_check_copy_source(core, &src) == CORE_LOAD_COPY_NONE);
   /* Loaded through a full check: its hash is known */
   CHECK(trust(core));
   CHECK(put_core(core, "ra_libretro" LIB_EXT, 'a'));
   /* A staged load waits on its check, so the pass is kept on every
    * platform (a quiet one isn't on Windows) */
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_take(core));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_loaded();
   CHECK(core_load_check_copy_source(core, &src) == CORE_LOAD_COPY_CHECK);
   CHECK(copy_core(core, tmp));
   CHECK((m = core_load_match_new(tmp, core, &src)) != NULL);
   CHECK(m && match_all(m) == 0);
   CHECK(m && lib_id(copy = core_load_match_load(m)) == 1);
   if (copy)
      dylib_close(copy);
   core_load_match_free(m);
   /* A copy with other bytes */
   CHECK(copy_core("core_load_check_test_b" LIB_EXT, tmp));
   CHECK((m = core_load_match_new(tmp, core, &src)) != NULL);
   CHECK(m && match_all(m) == -1);
   core_load_match_free(m);
#ifndef _WIN32
   /* Unknown hash, a quick pass's (Windows has none with checks on):
    * the source is hashed too, if it is the file loaded */
   src.hashed = false;
   CHECK(copy_core(core, tmp));
   CHECK((m = core_load_match_new(tmp, core, &src)) != NULL);
   CHECK(m && match_all(m) == 0);
   core_load_match_free(m);
   /* Its record gained an fs-verity digest (Android), which reading the
    * file doesn't show: still the core loaded */
   src.id.verity_len = 32;
   memset(src.id.verity, 0x5a, 32);
   CHECK((m = core_load_match_new(tmp, core, &src)) != NULL);
   CHECK(m && match_all(m) == 0);
   core_load_match_free(m);
   /* Replaced since its load (Windows' held file can't be) */
   CHECK(put_core(core, "ra_libretro" LIB_EXT, 'a'));
   CHECK(core_load_match_new(tmp, core, &src) == NULL);
#endif
   if (lib)
      dylib_close(lib);
   core_load_check_unloaded();
   CHECK(core_load_check_copy_source(core, &src) == CORE_LOAD_COPY_NONE);
   stop();
}

#ifdef _WIN32
/* A copy named with no '.' would load <name>.dll */
static void test_copy_dll_name(void)
{
   dylib_t lib  = NULL;
   dylib_t copy = NULL;
   struct core_load_source src;
   core_load_match_t *m = NULL;
   char core[256];
   start(false);
   CHECK(put_core(core, "ra_name_libretro" LIB_EXT, 'b'));
   CHECK(trust(core));
   core_load_check_set_startup(true);
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_set_startup(false);
   core_load_check_loaded();
   CHECK(core_load_check_copy_source(core, &src) == CORE_LOAD_COPY_CHECK);
   CHECK(copy_core(core, TMP_DIR "/ra_name"));
   CHECK(copy_core("core_load_check_test_a" LIB_EXT,
            TMP_DIR "/ra_name" LIB_EXT));
   CHECK((m = core_load_match_new(TMP_DIR "/ra_name", core, &src)) != NULL);
   CHECK(m && match_all(m) == 0);
   CHECK(m && lib_id(copy = core_load_match_load(m)) == 0);
   if (copy)
      dylib_close(copy);
   core_load_match_free(m);
   if (lib)
      dylib_close(lib);
   core_load_check_unloaded();
   stop();
}

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
   dylib_t lib  = NULL;
   dylib_t copy = NULL;
   struct core_load_source src;
   core_load_match_t *m = NULL;
   char core[256];
   char path[512];
   unsigned i;
   start(true);
   core_load_check_set_startup(true);
   CHECK(put_core(core, "verb_libretro" LIB_EXT, 'a'));
   CHECK(trust(core));
   verbatim_path(path, sizeof(path), core, "");
   CHECK(core_load_check_open(path, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   /* Run-ahead's copy, named so, isn't loaded either */
   core_load_check_loaded();
   CHECK(core_load_check_copy_source(path, &src) == CORE_LOAD_COPY_CHECK);
   verbatim_path(path, sizeof(path), TMP_DIR "/ra_verb", ".");
   CHECK(copy_core(core, path));
   CHECK(copy_core("core_load_check_test_b" LIB_EXT, TMP_DIR "/ra_verb"));
   CHECK((m = core_load_match_new(path, core, &src)) != NULL);
   CHECK(m && match_all(m) == 0);
   CHECK(m && lib_id(copy = core_load_match_load(m)) == 0);
   if (copy)
      dylib_close(copy);
   core_load_match_free(m);
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
   CHECK(copy_core("core_load_check_test_a" LIB_EXT, path));
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
   park_reset();
   CHECK(!park(path));
   CHECK(queued() == 0);
   if (lib)
      dylib_close(lib);
   stop();
}
#endif

/* Run-ahead's copy of the running core is no load: a park waiting on
 * the user, the running core's Load Once and its held file all stay */
static void test_copy_not_a_load(void)
{
   dylib_t lib  = NULL;
   dylib_t copy = NULL;
   struct core_load_source src;
   core_load_match_t *m = NULL;
   char a[256];
   char b[256];
   char tmp[256];
   start(false);
   park_reset();
   CHECK(put_core(a, "copy_once_libretro" LIB_EXT, 'a'));
   CHECK(put_core(b, "copy_waits_libretro" LIB_EXT, 'b'));
   strlcpy(tmp, TMP_DIR "/copy_once" LIB_EXT, sizeof(tmp));
   /* a runs on Load Once */
   CHECK(park(a));
   drain();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(1);
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_loaded();
   /* b waits on the user */
   CHECK(park(b));
   drain();
   CHECK(core_warning_pending(NULL, NULL));
   CHECK(core_load_check_copy_source(a, &src) == CORE_LOAD_COPY_CHECK);
   CHECK(copy_core(a, tmp));
   CHECK((m = core_load_match_new(tmp, a, &src)) != NULL);
   CHECK(m && match_all(m) == 0);
   CHECK(m && lib_id(copy = core_load_match_load(m)) == 1);
   core_load_match_free(m);
   if (copy)
      dylib_close(copy);
   CHECK(core_warning_pending(NULL, NULL));
   CHECK(frees == 1);
   if (lib)
      dylib_close(lib);
   lib = NULL;
   core_load_check_unloaded();
#ifdef _WIN32
   CHECK(!put_core(a, "copy_once_libretro" LIB_EXT, 'a'));
#endif
   /* a's probes still pass on its Load Once */
   CHECK(core_load_check_open(a, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 1);
   CHECK(queued() == 0);
   if (lib)
      dylib_close(lib);
   /* b's park still re-runs on the user's answer */
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE));
   park_wait(2);
   CHECK(reruns == 2);
   CHECK(rerun_id == 2);
   CHECK(frees == 2);
   stop();
}

#ifndef _WIN32
/* Files written in place while the match runs (Windows: its open files
 * can't be): the copy after it matched isn't loaded, and a running
 * core and copy rewritten alike don't match */
static void test_copy_rewritten(void)
{
   dylib_t lib = NULL;
   struct core_load_source src;
   core_load_match_t *m = NULL;
   char core[256];
   char tmp[256];
   start(false);
   CHECK(put_core(core, "rw_libretro" LIB_EXT, 'a'));
   strlcpy(tmp, TMP_DIR "/rw_copy" LIB_EXT, sizeof(tmp));
   CHECK(trust(core));
   CHECK(put_core(core, "rw_libretro" LIB_EXT, 'a'));
   CHECK(core_load_check_stage_begin(core));
   CHECK(stage_wait());
   CHECK(!core_load_check_stage_take(core));
   CHECK(core_load_check_open(core, &lib) == CORE_LOAD_GATE_OPENED);
   core_load_check_loaded();
   /* Unmapped before its file is written */
   if (lib)
      dylib_close(lib);
   CHECK(core_load_check_copy_source(core, &src) == CORE_LOAD_COPY_CHECK);
   CHECK(copy_core(core, tmp));
   CHECK((m = core_load_match_new(tmp, core, &src)) != NULL);
   CHECK(m && match_all(m) == 0);
   CHECK(rewrite_core(tmp));
   CHECK(m && !core_load_match_load(m));
   core_load_match_free(m);
   src.hashed = false;
   CHECK(copy_core(core, tmp));
   CHECK((m = core_load_match_new(tmp, core, &src)) != NULL);
   CHECK(rewrite_core(core));
   CHECK(rewrite_core(tmp));
   CHECK(m && match_all(m) == -1);
   core_load_match_free(m);
   core_load_check_unloaded();
   stop();
}

/* A probe's quick pass while the stage's worker opens its core takes
 * the stage's place: the stage's own pass is still the one granted, so
 * run-ahead has its source (Windows: no record passes a core unhashed) */
static void test_stage_stale_quick(void)
{
   dylib_t lib = NULL;
   struct core_load_source src;
   char a[256];
   char b[256];
   start(true);
   CHECK(put_core(a, "squick_a_libretro" LIB_EXT, 'a'));
   CHECK(put_core(b, "squick_b_libretro" LIB_EXT, 'b'));
   CHECK(trust(a));
   CHECK(trust(b));
   CHECK(worker_hold());
   CHECK(core_load_check_stage_begin(a));
   CHECK(!core_load_check_stage_ready());
   CHECK(core_load_check_open(b, &lib) == CORE_LOAD_GATE_OPENED);
   CHECK(lib_id(lib) == 2);
   if (lib)
      dylib_close(lib);
   worker_go();
   CHECK(stage_wait());
   lib = core_load_check_stage_take(a);
   CHECK(lib_id(lib) == 1);
   core_load_check_loaded();
   CHECK(core_load_check_copy_source(a, &src) == CORE_LOAD_COPY_CHECK);
   CHECK(core_load_check_copy_source(b, &src) == CORE_LOAD_COPY_NONE);
   if (lib)
      dylib_close(lib);
   core_load_check_unloaded();
   stop();
}
#endif

int main(int argc, char *argv[])
{
   if (!path_mkdir(TMP_DIR))
      return 1;
#ifdef VERITY_STANDIN
   /* core_load_check.c built as for Android (Makefile) */
   if (argc > 1 && string_is_equal(argv[1], "android"))
   {
      test_verity_unchanged();
      test_verity_refused_moved();
      test_verity_refused_moved_again();
      test_verity_rewritten();
      test_verity_main_thread();
      test_verity_rewritten_always();
      test_android_fd_offset();
      test_stage_rewritten();
      test_android_held_rewritten();
      test_android_refused_gone();
      test_android_instance();
      printf("%u checks, %u failures\n", checks, failures);
      return failures ? 1 : 0;
   }
#endif
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
   test_park_load_once();
   test_park_always();
   test_park_always_full();
   test_park_always_sweeps();
   test_park_always_cant_save();
   test_park_always_save_fails();
   test_park_always_full_cant_save();
   test_park_always_sweeps_cant_save();
#ifdef COUNT_HASHES
   test_sweep_uses_records();
#endif
   test_park_cancel();
   test_park_end_cancel();
   test_park_end_waits_for_load();
   test_park_end_replaced_warning();
   test_park_end_other_load();
   test_park_end_rerun();
   test_park_end_deinit();
   test_park_replaced_other();
   test_park_pass();
   test_park_stage_drift();
   test_park_no_menu();
   test_park_replaced();
   test_park_replaced_up();
#ifdef _WIN32
   test_park_quiet_refused(false);
#else
   test_park_quiet_refused(true);
#ifdef __linux__
   test_park_quiet_refused(false);
#endif
#endif
   test_park_other_pass();
   test_park_seed();
   test_park_unopenable();
   test_park_check_cancelled();
   test_park_no_check();
   test_park_deinit();
   test_park_other_load();
   test_builtin_load();
   test_park_same_load();
   test_park_own_rerun();
   test_park_rerun_no_open();
   test_park_fallback();
   test_load_once_lasts();
   test_forget_rechecks();
   test_held_ctime_moved();
   test_startup_seed();
   test_startup_refused();
   test_startup_seed_park();
   test_copy();
#ifdef __linux__
   /* No ctime binds the record: its held file does */
   no_ctime_fs = 1;
   test_copy();
   no_ctime_fs = 0;
#endif
   test_copy_not_a_load();
#ifdef _WIN32
   test_copy_dll_name();
   test_verbatim_dot();
#endif
#ifndef _WIN32
   test_copy_rewritten();
   test_stage_stale_quick();
#endif
   /* Messages past OSD_MAX aren't kept for the counts above */
   CHECK(osd_count <= OSD_MAX);
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
