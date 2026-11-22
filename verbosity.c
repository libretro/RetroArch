/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _XBOX1
#include <xtl.h>
#endif

#if defined(__PSL1GHT__) || defined(__PS3__)
#include <defines/ps3_defines.h>
#endif

#ifdef __MACH__
#include <TargetConditionals.h>
#include <Availability.h>
#if TARGET_IPHONE_SIMULATOR
#include <stdio.h>
#else
/* The same test as the os_log call site below: a 10.0 floor logs through
 * os_log, anything lower (or an SDK without os/log.h) through asl. */
#if (defined(__IPHONE_10_0) && (__IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0)) \
   || (defined(__TV_OS_VERSION_MIN_REQUIRED) && defined(__TVOS_10_0) \
      && (__TV_OS_VERSION_MIN_REQUIRED >= __TVOS_10_0))
#include <os/log.h>
#else
#include <asl.h>
#endif
#endif
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif
#include <compat/strl.h>

#ifdef ANDROID
#include <android/log.h>
#endif

#if defined(_WIN32)

#if defined(_XBOX)
#include <Xtl.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#endif

#include <file/file_path.h>
#include <streams/file_stream.h>
#include <compat/fopen_utf8.h>
#include <time/rtime.h>
#include <retro_miscellaneous.h>
#include <retro_atomic.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef RARCH_INTERNAL
#include "frontend/frontend_driver.h"
#endif

#include "verbosity.h"
#include "file_path_special.h"

#ifdef RARCH_INTERNAL
/* Defines HAVE_COMPANION_WIMP when a desktop companion (Qt, native
 * Win32, native Cocoa) is in the build; its log view mirrors the log. */
#include "ui/ui_companion_driver.h"
#endif

/* The companion copy of a log line is formatted from a copy of the
 * argument list so the platform sink below still sees an untouched one.
 * C89 has no va_copy; gcc/clang spell the same thing __va_copy. */
#ifndef va_copy
# ifdef __va_copy
#  define va_copy(dst, src) __va_copy(dst, src)
# else
#  define va_copy(dst, src) ((dst) = (src))
# endif
#endif

#ifdef RARCH_INTERNAL
#include "config.def.h"
#else
#define DEFAULT_FRONTEND_LOG_LEVEL 1
#endif

#if defined(IS_SALAMANDER)
#define FILE_PATH_PROGRAM_NAME "RetroArch Salamander"
#else
#define FILE_PATH_PROGRAM_NAME "RetroArch"
#endif

typedef struct verbosity_state
{
   /* The main thread's view of where lines go: the log file, stderr,
    * or NULL. Logging threads read the published copy, sink. */
   FILE *fp;   /* pointer-sized: keep near top */

   /* Files taken out of sink while a logging thread may still hold
    * them; main thread only, closed by log_sink_drain(). */
   FILE **retired;

   /* sink is fp as a size_t, published with release; sink_users
    * counts the logging threads between log_sink_hold() and
    * log_sink_release(). */
   retro_atomic_size_t sink;
   size_t retired_count;
   size_t retired_cap;
   retro_atomic_int_t sink_users;

   /* Read by every thread that logs */
   retro_atomic_int_t verbosity;

   /* Booleans grouped together to avoid padding holes */
   bool verbosity_setting; /* verbosity_get_ptr()'s copy */
   bool initialized;
   bool override_active;

   /* Large array last: avoids padding before it */
   char override_path[PATH_MAX_LENGTH];
} verbosity_state_t;

/* TODO/FIXME - static public global variables */
static verbosity_state_t main_verbosity_st;
static retro_atomic_int_t verbosity_log_level =
RETRO_ATOMIC_INT_INITIALIZER(DEFAULT_FRONTEND_LOG_LEVEL);

#ifdef HAVE_LIBNX
#ifdef NXLINK
/* TODO/FIXME - global referenced in platform_switch.c - not
 * thread-safe */
bool nxlink_connected = false;
#endif /* NXLINK */

#endif /* HAVE_LIBNX */

void verbosity_set_log_level(unsigned level)
{
   retro_atomic_store_relaxed_int(&verbosity_log_level, (int)level);
}

unsigned verbosity_get_log_level(void)
{
   return (unsigned)retro_atomic_load_relaxed_int(&verbosity_log_level);
}

void verbosity_enable(void)
{
   main_verbosity_st.verbosity_setting = true;
   retro_atomic_store_relaxed_int(&main_verbosity_st.verbosity, 1);
#ifdef RARCH_INTERNAL
   if (!main_verbosity_st.initialized)
      frontend_driver_attach_console();
#endif
}

void verbosity_disable(void)
{
   main_verbosity_st.verbosity_setting = false;
   retro_atomic_store_relaxed_int(&main_verbosity_st.verbosity, 0);
#ifdef RARCH_INTERNAL
   if (!main_verbosity_st.initialized)
      frontend_driver_detach_console();
#endif
}

bool verbosity_is_enabled(void)
{
   return retro_atomic_load_relaxed_int(&main_verbosity_st.verbosity) != 0;
}

bool is_logging_to_file(void)
{
   return main_verbosity_st.initialized;
}

/* The one consumer is the menu's LOG_VERBOSITY setting, which
 * binds this as its boolean target. The atomic flag cannot be
 * bound, so this is a main-thread copy: the framework writes
 * through it, and the setting's change handler immediately
 * re-drives verbosity_enable()/verbosity_disable() from the written
 * value, which carry it to the flag and attach or detach the
 * console. Nothing else may write through this. */
bool *verbosity_get_ptr(void)
{
   return &main_verbosity_st.verbosity_setting;
}

/* Where a logging thread writes. A thread holds the sink from
 * log_sink_hold() to log_sink_release(); the main thread swaps it
 * with log_sink_publish() and closes a file it took out only once
 * no thread can still hold it.
 *
 * Why the count settles it: a holder's increment either comes before
 * the main thread's read of the count (an RMW, ordered after the
 * swap), and is seen there, or after it, and then that increment
 * acquires the release sequence the RMW heads, so the holder's load
 * sees the swap and never the file taken out. A zero count after a
 * swap therefore means no thread holds any file swapped out before
 * it. Nothing waits: a file still held is kept on the retired list
 * and closed by a later drain on the main thread. */
#define log_sink_hold() \
   (retro_atomic_fetch_add_int(&main_verbosity_st.sink_users, 1), \
    (FILE*)retro_atomic_load_acquire_size(&main_verbosity_st.sink))

#define log_sink_release() \
   retro_atomic_fetch_sub_int(&main_verbosity_st.sink_users, 1)

static void log_sink_publish(FILE *fp)
{
   main_verbosity_st.fp = fp;
   retro_atomic_store_release_size(&main_verbosity_st.sink, (size_t)fp);
}

/* True when no logging thread holds a file swapped out before now */
#define log_sink_quiet() \
   (retro_atomic_fetch_add_int(&main_verbosity_st.sink_users, 0) == 0)

static void log_sink_drain(void)
{
   size_t i;
   if (!main_verbosity_st.retired_count || !log_sink_quiet())
      return;
   for (i = 0; i < main_verbosity_st.retired_count; i++)
      fclose(main_verbosity_st.retired[i]);
   main_verbosity_st.retired_count = 0;
}

/* Close a file already swapped out of the sink, now or at a later
 * drain. If the list cannot grow the file stays open: a leaked
 * handle over a close under a writer. */
static void log_sink_retire(FILE *fp)
{
   if (log_sink_quiet())
   {
      log_sink_drain();
      fclose(fp);
      return;
   }
   if (main_verbosity_st.retired_count == main_verbosity_st.retired_cap)
   {
      size_t cap = main_verbosity_st.retired_cap
         ? main_verbosity_st.retired_cap * 2 : 4;
      FILE **nl  = (FILE**)realloc(main_verbosity_st.retired,
            cap * sizeof(*nl));
      if (!nl)
         return;
      main_verbosity_st.retired     = nl;
      main_verbosity_st.retired_cap = cap;
   }
   main_verbosity_st.retired[main_verbosity_st.retired_count++] = fp;
}

void retro_main_log_file_init(const char *path, bool append)
{
   FILE *tmp;

   log_sink_drain();

   if (main_verbosity_st.initialized)
      return;

   /* Default to stderr; only override when a valid path is given */
   log_sink_publish(stderr);

   if (!path)
      return;

   tmp = (FILE*)fopen_utf8(path, append ? "ab" : "wb");

   if (!tmp)
   {
      RARCH_ERR("Failed to open system event log file: \"%s\".\n", path);
      return;
   }

   log_sink_publish(tmp);
   main_verbosity_st.initialized = true;

   /* No setvbuf here, deliberately.
    *
    * There used to be a 16 KiB buffer installed at this point, with a
    * note wondering which platforms it helped.  The answer is none, and
    * the reason is a few lines further down: every path that writes a
    * log line calls fflush immediately afterwards, so the buffer never
    * holds more than the line just written and the write reaches the
    * operating system either way.  Measured at twenty thousand lines,
    * a 16 KiB buffer and no buffer at all are the same speed, and both
    * cost exactly one write per line.
    *
    * The flushing is the part worth keeping.  A log is read after a
    * crash, so a line that is still sitting in a buffer when the
    * process dies is a line that was not worth writing.  Given that,
    * the buffer only added an allocation and a copy per line. */
}

void retro_main_log_file_deinit(void)
{
   if (main_verbosity_st.fp && main_verbosity_st.initialized)
   {
      FILE *fp = main_verbosity_st.fp;
      log_sink_publish(NULL);
      log_sink_retire(fp);
   }
   else
      log_sink_drain();

   main_verbosity_st.initialized = false;
}

#if !defined(HAVE_LOGGER)
/* Compiled for every consumer past the Xbox/WinRT head branch,
 * which formats into its own bounded buffer: the Android file arm,
 * the Apple arms (one format feeding printf/os_log/asl and the
 * file), the desktop companion mirror, and the generic branch. */
#if defined(ANDROID) \
 || (!defined(_XBOX1) && !defined(__WINRT__))
/* Format tag and message into one buffer and write it with a single
 * stdio call: the per-call lock then keeps concurrent lines whole.
 * Lines wider than the stack buffer take an exact-sized heap detour
 * rather than truncating; if that allocation fails, the truncated
 * stack line ships - a shortened message over a dropped one. */
/* Format "tag body" once into the caller's stack line, taking an
 * exact-sized heap detour for wider lines rather than truncating
 * (allocation failure ships the truncated stack line - a shortened
 * message over a dropped one). Returns the buffer to hand to every
 * sink; *heap is owned by the caller and freed after the sinks are
 * done. Consumes one formatting pass of ap (plus one of a private
 * copy for the measuring pass). */
static const char *rarch_log_line_format(char *line, size_t line_size,
      char **heap, const char *tag_v, const char *fmt, va_list ap)
{
   size_t t_len = strlen(tag_v);
   int b_len;
   va_list ap_cp;

   *heap = NULL;

   if (line_size < 2 || t_len >= line_size - 1)
      t_len = 0;
   else
   {
      /* Snapshot before formatting: %n may modify the caller's tag. */
      memcpy(line, tag_v, t_len);
      line[t_len++] = ' ';
   }

   va_copy(ap_cp, ap);
   b_len = vsnprintf(line + t_len, line_size - t_len, fmt, ap_cp);
   va_end(ap_cp);

   if (b_len >= (int)(line_size - t_len))
   {
      size_t need = t_len + (size_t)b_len + 1;
      if ((*heap = (char*)malloc(need)))
      {
         memcpy(*heap, line, t_len);
         vsnprintf(*heap + t_len, need - t_len, fmt, ap);
         return *heap;
      }
   }
   else if (b_len < 0)
      line[t_len] = '\0';

   return line;
}

/* One write, one flush.
 *
 * The flush is unconditional, for every tag. Buffering informational
 * lines was tried and taken back out: a log is read after a crash, and
 * the lines that explain it are usually the INFO/DEBUG ones written
 * just before it - exactly the tail a buffer loses - and anything
 * following the log live (tail -f, a monitoring script) sees nothing
 * until the buffer happens to fill. */
static void rarch_log_line_emit(FILE *fp, const char *out)
{
   fputs(out, fp);
   fflush(fp);
}

#if defined(ANDROID)
/* The Android file arm is the one caller that still wants
 * format-and-emit as a unit; the generic and Apple region formats
 * once up front and hands the buffer to each sink itself. */
static void rarch_log_line_write(FILE *fp, const char *tag_v,
      const char *fmt, va_list ap)
{
   char line[1024];
   char *heap      = NULL;
   const char *out = rarch_log_line_format(line, sizeof(line), &heap,
         tag_v, fmt, ap);

   rarch_log_line_emit(fp, out);
   free(heap);
}
#endif
#endif

#ifdef HAVE_MCP
#include <string.h>

/* The last lines logged, for GET_LOG, filled by whatever thread logs
 * and read on the main thread without a lock: a writer claims a line
 * number, marks the slot busy, writes the text as atomic words and
 * stamps the slot with its number; a reader takes a slot's text only
 * when the stamp is the line it wants before and after the copy. */
#define LOG_RING_SLOTS  256
#define LOG_RING_WORDS  (256 / sizeof(size_t))

struct log_ring_slot
{
   retro_atomic_size_t stamp; /* line number + 1, 0 while written */
   retro_atomic_size_t text[LOG_RING_WORDS];
};

static struct
{
   struct log_ring_slot slots[LOG_RING_SLOTS];
   retro_atomic_size_t  head;
   retro_atomic_int_t   armed;
} log_ring;

void verbosity_ring_arm(void)
{
   retro_atomic_store_release_int(&log_ring.armed, 1);
}

/* The line goes from the caller's buffer straight into the slot, a
 * word at a time, up to the word holding its terminator; the words
 * after it keep an older line's bytes, which no reader looks at. */
static void verbosity_ring_put(const char *line)
{
   size_t i, n, last, seq, w;
   const char *nul;
   struct log_ring_slot *slot;

   if (!retro_atomic_load_relaxed_int(&log_ring.armed))
      return;
   nul  = (const char*)memchr(line, '\0', LOG_RING_WORDS * sizeof(size_t) - 1);
   n    = nul ? (size_t)(nul - line) : LOG_RING_WORDS * sizeof(size_t) - 1;
   last = n / sizeof(size_t);
   seq  = retro_atomic_fetch_add_size(&log_ring.head, 1);
   slot = &log_ring.slots[seq % LOG_RING_SLOTS];
   retro_atomic_store_relaxed_size(&slot->stamp, 0);
   retro_atomic_thread_fence_release();
   for (i = 0; i < last; i++)
   {
      memcpy(&w, line + i * sizeof(size_t), sizeof(w));
      retro_atomic_store_relaxed_size(&slot->text[i], w);
   }
   w = 0;
   memcpy(&w, line + last * sizeof(size_t), n - last * sizeof(size_t));
   retro_atomic_store_relaxed_size(&slot->text[last], w);
   retro_atomic_store_release_size(&slot->stamp, seq + 1);
}

size_t verbosity_ring_read(size_t since, char *s, size_t len,
      size_t *first, size_t *next)
{
   size_t _len = 0;
   size_t head = retro_atomic_load_acquire_size(&log_ring.head);
   size_t seq  = since;

   if (head > LOG_RING_SLOTS && seq < head - LOG_RING_SLOTS)
      seq = head - LOG_RING_SLOTS;
   if (seq > head)
      seq = head;
   *first = seq;
   /* Each slot's text is copied straight to s past what is already
    * there and counted only once the stamp still holds after the
    * copy; a slot rewritten meanwhile leaves its bytes behind for the
    * next line to overwrite. */
   for (; seq < head; seq++)
   {
      size_t i, n = 0;
      bool room   = true;
      struct log_ring_slot *slot = &log_ring.slots[seq % LOG_RING_SLOTS];
      size_t stamp = retro_atomic_load_acquire_size(&slot->stamp);
      /* still being written, or already a later line: skipped */
      if (stamp != seq + 1)
         continue;
      for (i = 0; i < LOG_RING_WORDS; i++)
      {
         size_t k;
         size_t w      = retro_atomic_load_relaxed_size(&slot->text[i]);
         const char *b = (const char*)&w;
         /* the slot's last byte is its terminator */
         size_t lim    = (i == LOG_RING_WORDS - 1)
            ? sizeof(w) - 1 : sizeof(w);
         for (k = 0; k < lim && b[k]; k++);
         if (_len + n + k + 2 > len)
         {
            room = false;
            break;
         }
         memcpy(s + _len + n, b, k);
         n += k;
         if (k < sizeof(w))
            break;
      }
      retro_atomic_thread_fence_acquire();
      if (retro_atomic_load_relaxed_size(&slot->stamp) != stamp)
         continue;
      if (!room)
         break;
      _len += n;
      if (!n || s[_len - 1] != '\n')
         s[_len++] = '\n';
   }
   if (len)
      s[_len < len ? _len : len - 1] = '\0';
   *next = seq;
   return _len;
}
#else
#define verbosity_ring_put(line) ((void)0)
#endif

void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
#if defined(_XBOX1) || defined(__WINRT__)
   /* wvsprintf takes no size and writes past any small buffer on a
    * long expansion; vsnprintf is bounded on both targets (the msvc
    * compat header maps it for the Xbox toolchain), and 1024 matches
    * the line budget the generic branch uses. Truncation stays the
    * policy here - no heap on this target. */
   char buffer[1024];
   int _len;
   const char *tag_v = tag ? tag : FILE_PATH_LOG_INFO;
   buffer[0] = '\0';
   _len = snprintf(buffer, sizeof(buffer),
         "%s: %s ", FILE_PATH_PROGRAM_NAME, tag_v);

   if (_len > 0 && _len < (int)sizeof(buffer))
      vsnprintf(buffer + _len, sizeof(buffer) - (size_t)_len, fmt, ap);
#ifdef _DEBUG
   OutputDebugStringA(buffer);
#endif
   {
      /* stderr is the console default, not a log file */
      FILE *fp = log_sink_hold();
      if (fp && fp != stderr)
      {
         fputs(buffer, fp);
         fflush(fp);
      }
      log_sink_release();
   }

#elif defined(ANDROID)
   {
      FILE *fp = log_sink_hold();
      if (fp && fp != stderr)
      {
         /* Logging to file: one formatted write, tag included -
          * the file carries the same line format as every other
          * platform (logcat gets the tag as a priority instead). */
         const char *tag_v = tag ? tag : FILE_PATH_LOG_INFO;
         rarch_log_line_write(fp, tag_v, fmt, ap);
         log_sink_release();
      }
      else
      {
         int prio = ANDROID_LOG_INFO;
         log_sink_release();
         if (tag)
         {
            /* strcmp, not memcmp with the pattern's sizeof: a sized
             * compare reads past a shorter tag's terminator. */
            if (strcmp(tag, FILE_PATH_LOG_WARN) == 0)
               prio = ANDROID_LOG_WARN;
            else if (strcmp(tag, FILE_PATH_LOG_ERROR) == 0)
               prio = ANDROID_LOG_ERROR;
         }
         __android_log_vprint(prio, FILE_PATH_PROGRAM_NAME, fmt, ap);
      }
   }

#else
   {
      FILE       *fp;
      const char *tag_v = tag ? tag : FILE_PATH_LOG_INFO;
      /* One format for every consumer in this region - the desktop
       * companion's log view, the Apple sinks, the file - instead of
       * a formatting pass (and on Apple a heap vasprintf, and in the
       * companion a second 1024-byte copy whose overflow went
       * unhandled) per sink. */
      char        line[1024];
      char       *heap  = NULL;
      const char *out   = rarch_log_line_format(line, sizeof(line),
            &heap, tag_v, fmt, ap);

      verbosity_ring_put(out);

#ifdef HAVE_COMPANION_WIMP
      /* Mirror the line into the desktop companion's log view, when
       * one is open. The companion now shows the tag too, and wide
       * lines arrive whole through the shared heap detour. */
      if (ui_companion_driver_log_active())
         ui_companion_driver_log_msg(out);
#endif

#if TARGET_OS_MAC
      {
#if TARGET_OS_MAC && !TARGET_OS_IPHONE
         /* Emit to the terminal unconditionally so Terminal.app and
          * Xcode's console always see output. The file write is gated
          * on `initialized` (= a real log file was opened) because fp
          * defaults to stderr when no file is configured; without the
          * guard, every line would print twice in the no-file case. */
         printf("%s", out);
         fp = log_sink_hold();
         if (fp && fp != stderr)
            rarch_log_line_emit(fp, out);
         log_sink_release();

#else
         {
#if TARGET_OS_SIMULATOR
            fprintf(stderr, "%s", out);
#elif defined(__IPHONE_10_0) && (__IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0)
            os_log(OS_LOG_DEFAULT, "%s", out);
#elif defined(__TV_OS_VERSION_MIN_REQUIRED) && defined(__TVOS_10_0) \
         && (__TV_OS_VERSION_MIN_REQUIRED >= __TVOS_10_0)
            os_log(OS_LOG_DEFAULT, "%s", out);
#else
            {
               static aslclient asl_client      = NULL;
               static int       asl_initialized = 0;
               aslmsg           msg;

               if (!asl_initialized)
               {
                  asl_client      = asl_open(FILE_PATH_PROGRAM_NAME,
                                    "com.apple.console",
                                    ASL_OPT_STDERR | ASL_OPT_NO_DELAY);
                  asl_initialized = 1;
               }
               msg = asl_new(ASL_TYPE_MSG);
               asl_set(msg, ASL_KEY_READ_UID, "-1");
               asl_log(asl_client, msg, ASL_LEVEL_NOTICE, "%s", out);
               asl_free(msg);
            }
#endif

            fp = log_sink_hold();
            if (fp && fp != stderr)
               rarch_log_line_emit(fp, out);
            log_sink_release();
         }
#endif
      }

#else
      /* stdio's per-call lock keeps the single write whole under
       * concurrent writers; the format above already took the heap
       * detour for wide lines. */
      fp = log_sink_hold();
      if (fp)
         rarch_log_line_emit(fp, out);
      log_sink_release();
#endif

      free(heap);
   }
#endif
}

void RARCH_LOG_BUFFER(uint8_t *data, size_t len)
{
   size_t i;

   if (!data && len)
      return;

   RARCH_LOG("== %d-byte buffer ==================\n", (int)len);

   /* Whole rows print from the caller's bytes */
   for (i = 0; i + 16 <= len; i += 16)
   {
      const uint8_t *r = data + i;
      RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x  %02x%02x%02x%02x%02x%02x%02x%02x\n",
         r[0], r[1], r[2],  r[3],  r[4],  r[5],  r[6],  r[7],
         r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
   }

   /* A partial last row is padded with 0xff */
   if (i < len)
   {
      uint8_t buf[16];
      memset(buf, 0xff, sizeof(buf));
      memcpy(buf, data + i, len - i);
      RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x  %02x%02x%02x%02x%02x%02x%02x%02x\n",
         buf[0], buf[1], buf[2],  buf[3],  buf[4],  buf[5],  buf[6],  buf[7],
         buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
   }
   RARCH_LOG("==================================\n");
}

void RARCH_DBG(const char *fmt, ...)
{
   va_list ap;
#ifndef _DEBUG
   if (!verbosity_is_enabled() || verbosity_get_log_level() > 0)
      return;
#endif
   va_start(ap, fmt);
   RARCH_LOG_V(FILE_PATH_LOG_DBG, fmt, ap);
   va_end(ap);
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
#ifndef _DEBUG
   if (!verbosity_is_enabled() || verbosity_get_log_level() > 1)
      return;
#endif
   va_start(ap, fmt);
   RARCH_LOG_V(FILE_PATH_LOG_INFO, fmt, ap);
   va_end(ap);
}

void RARCH_LOG_OUTPUT(const char *msg, ...)
{
   va_list ap;
   va_start(ap, msg);
   RARCH_LOG_OUTPUT_V(FILE_PATH_LOG_INFO, msg, ap);
   va_end(ap);
}

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
#ifndef _DEBUG
   if (!verbosity_is_enabled() || verbosity_get_log_level() > 2)
      return;
#endif
   va_start(ap, fmt);
   RARCH_WARN_V(FILE_PATH_LOG_WARN, fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
#ifndef _DEBUG
   if (!verbosity_is_enabled())
      return;
#endif
   va_start(ap, fmt);
   RARCH_ERR_V(FILE_PATH_LOG_ERROR, fmt, ap);
   va_end(ap);
}
#endif

size_t rarch_log_file_set_override(const char *path)
{
   main_verbosity_st.override_active = true;
   return strlcpy(main_verbosity_st.override_path, path,
         sizeof(main_verbosity_st.override_path));
}

void rarch_log_file_init(
      bool log_to_file,
      bool log_to_file_timestamp,
      const char *log_dir
      )
{
   char log_directory[DIR_MAX_LENGTH] = {0};
   char log_file_path[PATH_MAX_LENGTH];
   static bool log_file_created              = false;
   static char timestamped_log_file_name[64] = {0};
   bool logging_to_file                      = main_verbosity_st.initialized;

   /* The override branch below leaves log_directory untouched
    * when the override path has no directory component
    * (e.g. --log-file retroarch.log); an uninitialised buffer
    * here was passed to path_mkdir(), creating garbage-named
    * directories in the current working directory. */
   log_directory[0] = '\0';
   log_file_path[0] = '\0';

   /* If this is the first run, generate a timestamped log
    * file name (do this even when not outputting timestamped
    * log files, since user may decide to switch at any moment...) */
   if (!timestamped_log_file_name[0])
   {
      struct tm tm_;
      time_t cur_time = time(NULL);

      rtime_localtime(&cur_time, &tm_);
#ifdef DJGPP
      strftime(timestamped_log_file_name,
            sizeof(timestamped_log_file_name),
            "RA%d%H%M.log", &tm_);
#else
      strftime(timestamped_log_file_name,
            sizeof(timestamped_log_file_name),
            "retroarch__%Y_%m_%d__%H_%M_%S.log", &tm_);
#endif
   }

   /* If nothing has changed, do nothing */
   if (  (!log_to_file && !logging_to_file)
       || (log_to_file && logging_to_file))
      return;

   /* If we are currently logging to file and wish to stop,
    * deinitialise existing logger... */
   if (!log_to_file && logging_to_file)
   {
      retro_main_log_file_deinit();
      /* ...and revert to console */
      retro_main_log_file_init(NULL, false);
      return;
   }

   /* If we reach this point, then we are not currently
    * logging to file, and wish to do so */

   /* > Check whether we are already logging to console */
   /* De-initialise existing logger */
   if (main_verbosity_st.fp)
      retro_main_log_file_deinit();

   /* Get directory/file paths */
   if (main_verbosity_st.override_active)
   {
      /* Get log directory */
      const char *override_path = main_verbosity_st.override_path;
      const char *last_slash    = find_last_slash(override_path);

      if (last_slash)
      {
         char tmp_buf[PATH_MAX_LENGTH] = {0};
         size_t __len                  = last_slash + 1 - override_path;

         if ((__len > 1) && (__len < PATH_MAX_LENGTH))
            strlcpy(tmp_buf, override_path, __len * sizeof(char));
         strlcpy(log_directory, tmp_buf, sizeof(log_directory));
      }

      /* Get log file path */
      strlcpy(log_file_path, override_path, sizeof(log_file_path));
   }
   else if (log_dir && *log_dir)
   {
      /* Get log directory */
      strlcpy(log_directory, log_dir, sizeof(log_directory));

      /* Get log file path */
      fill_pathname_join_special(log_file_path,
            log_dir,
            log_to_file_timestamp
            ? timestamped_log_file_name
            : FILE_PATH_DEFAULT_EVENT_LOG,
            sizeof(log_file_path));
   }
   else
       log_file_path[0] = '\0';

   /* Attempt to initialise log file */
   if (log_file_path[0])
   {
      /* Create log directory, if required */
      if (     log_directory[0]
            && !path_is_directory(log_directory)
            && !path_mkdir(log_directory))
      {
         /* Re-enable console logging and output error message */
         retro_main_log_file_init(NULL, false);
         RARCH_ERR("Failed to create system event log directory: %s\n", log_directory);
         return;
      }

      /* When RetroArch is launched, log file is overwritten.
       * On subsequent calls within the same session, it is appended to. */
      retro_main_log_file_init(log_file_path, log_file_created);
      if (main_verbosity_st.initialized)
         log_file_created = true;
      return;
   }

   /* If we reach this point, then something went wrong...
    * Just fall back to console logging */
   retro_main_log_file_init(NULL, false);
   RARCH_ERR("Failed to initialise system event file logging...\n");
}

void rarch_log_file_deinit(void)
{
   /* De-initialise existing logger, if currently logging to file */
   if (main_verbosity_st.initialized)
      retro_main_log_file_deinit();

   /* If logging is currently disabled... */
   if (!main_verbosity_st.fp) /* ...initialise logging to console */
      retro_main_log_file_init(NULL, false);
}
