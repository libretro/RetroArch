/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (filestream_rbuf_fault_test.c).
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

/* filestream_rbuf_fill() has two ways to give up, and neither is
 * reachable from a normal run: the allocation of the lookahead buffer
 * fails, or the handle cannot report a position.  Both hand the caller
 * back to the byte-at-a-time path, and until something can drive them
 * that path is untested code with a comment on it.  Every other sample
 * in this directory reads ordinary files through an ordinary allocator
 * and therefore never enters either arm.
 *
 * Neither arm is a byte-content property, which is why comparing
 * output against fgets() cannot express what is being asserted here.
 * The claims are about how often the implementation asks the VFS and
 * the allocator for something:
 *
 *   - a handle whose tell() fails must be asked exactly ONCE.  The
 *     failure is a property of the backend (a pipe, a FIFO, a socket,
 *     a frontend VFS with read but no tell), so nothing between two
 *     fills can change the answer, and re-asking costs a failed tell()
 *     per byte on top of the one-byte read - strictly more work than
 *     the unbuffered code the lookahead replaced.
 *
 *   - a handle whose allocation fails must NOT be latched.  That
 *     failure is usually a moment, not a property, and it is likeliest
 *     on exactly the memory-constrained targets the lookahead exists
 *     for.  It must be retried, and it must be retried at a smaller
 *     size, so a handle that can have 1 KiB is buffered instead of
 *     degraded.
 *
 * Two seams drive them.  The tell arm needs nothing from the
 * implementation: filestream_vfs_init() already accepts a caller-
 * supplied VFS, so this installs a pass-through one whose tell() can
 * be made to fail and whose calls are counted.  Installing it also
 * takes the memory-mapped fast path out of filestream_gets() - a
 * caller-supplied VFS has no mapping this side of its callbacks - so
 * every case below goes through the lookahead deterministically.
 * The allocation arm has no such seam, so this target links its own
 * file_stream.o built with FILESTREAM_RBUF_MALLOC defined to the
 * allocator below.  Undefined everywhere else the macro is malloc().
 *
 * All fixtures are generated ASCII. Nothing here reads, produces or
 * requires any real data.
 *
 * Build:  make filestream_rbuf_fault_test
 *         (SANITIZER=address,undefined for a checked run)
 */

/* The VFS interface's callbacks take struct retro_vfs_file_handle*,
 * the *_impl functions take libretro_vfs_implementation_file*, and the
 * two are the same type only under VFS_FRONTEND - which is what
 * file_stream.c itself compiles with.  Defined before any include so
 * nothing can pull in vfs/vfs.h with the other typedef first. */
#define VFS_FRONTEND

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <libretro.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

#include "rbuf_fault_hooks.h"

/* Must match file_stream.c.  Not included from anywhere: the constant
 * is an implementation detail there, and a test that redefined it
 * would be asserting against itself rather than against the code. */
#define RBUF_LEN 16384

#define FIXTURE_PATH "rbuf_fault.txt"
#define LINE_BUF     64

/* Fills that return bytes, plus the two that return none: the fixture
 * ends without a newline, so the gets() that consumes the tail keeps
 * going until something reports end of file, and the gets() after that
 * asks once more before returning NULL.  Both are ordinary behaviour,
 * not an artefact of the faults below - the baseline case pins the
 * same number. */
#define EXPECT_FILLS(cap) \
   ((unsigned long)((fixture_len + (cap) - 1) / (cap)) + 2)

/* The unbuffered fallback costs one 1-byte read per byte, and the same
 * two end-of-file probes. */
#define EXPECT_BYTE_READS ((unsigned long)fixture_len + 2)

static int test_fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         test_fails++; \
      } \
      else \
         printf("ok:   %s\n", msg); \
   } while (0)

/* ---- the allocation seam ------------------------------------------ */

/* Non-static and named in rbuf_fault_hooks.h: this target's private
 * file_stream.o is built with FILESTREAM_RBUF_MALLOC defined to it. */
static unsigned long alloc_calls;
static unsigned long alloc_refusals;
static size_t        alloc_last_granted;
static size_t        alloc_refuse_at_least;  /* 0 = refuse nothing */
static unsigned long alloc_refuse_first_n;   /* transient failure    */

void *filestream_rbuf_fault_malloc(size_t len)
{
   alloc_calls++;

   if (alloc_refuse_first_n != 0)
   {
      alloc_refuse_first_n--;
      alloc_refusals++;
      return NULL;
   }

   if (alloc_refuse_at_least != 0 && len >= alloc_refuse_at_least)
   {
      alloc_refusals++;
      return NULL;
   }

   alloc_last_granted = len;
   return malloc(len);
}

/* ---- the pass-through VFS ----------------------------------------- */

static unsigned long gets_calls;

static unsigned long vfs_tell_calls;
static unsigned long vfs_read_calls;
static bool          vfs_tell_fails;

static int64_t shim_tell(libretro_vfs_implementation_file *stream)
{
   vfs_tell_calls++;
   if (vfs_tell_fails)
      return -1;
   return retro_vfs_file_tell_impl(stream);
}

static int64_t shim_read(libretro_vfs_implementation_file *stream,
      void *s, uint64_t len)
{
   vfs_read_calls++;
   return retro_vfs_file_read_impl(stream, s, len);
}

static struct retro_vfs_interface shim_iface;

static void install_shim_vfs(void)
{
   struct retro_vfs_interface_info info;

   memset(&shim_iface, 0, sizeof(shim_iface));
   shim_iface.get_path = retro_vfs_file_get_path_impl;
   shim_iface.open     = retro_vfs_file_open_impl;
   shim_iface.close    = retro_vfs_file_close_impl;
   shim_iface.size     = retro_vfs_file_size_impl;
   shim_iface.tell     = shim_tell;
   shim_iface.seek     = retro_vfs_file_seek_impl;
   shim_iface.read     = shim_read;
   shim_iface.write    = retro_vfs_file_write_impl;
   shim_iface.flush    = retro_vfs_file_flush_impl;
   shim_iface.remove   = retro_vfs_file_remove_impl;
   shim_iface.rename   = retro_vfs_file_rename_impl;
   shim_iface.truncate = retro_vfs_file_truncate_impl;

   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                      = &shim_iface;
   filestream_vfs_init(&info);
}

static void reset_counters(void)
{
   alloc_calls           = 0;
   alloc_refusals        = 0;
   alloc_last_granted    = 0;
   alloc_refuse_at_least = 0;
   alloc_refuse_first_n  = 0;
   vfs_tell_calls        = 0;
   vfs_read_calls        = 0;
   vfs_tell_fails        = false;
   gets_calls            = 0;
}

/* ---- fixture ------------------------------------------------------- */

/* Ragged ASCII, written through stdio rather than through the code
 * under test, and kept in memory so the scan below has something to be
 * exact against.  Lines run from 1 to 96 characters so that LINE_BUF
 * splits some of them and not others, and the file ends without a
 * final newline. */
static char  *fixture;
static size_t fixture_len;

static bool make_fixture(void)
{
   FILE  *fp;
   size_t cap = 40000;
   size_t n   = 0;
   unsigned line;

   if (!(fixture = (char*)malloc(cap)))
      return false;

   for (line = 0; n + 128 < cap; line++)
   {
      size_t width = 1 + (line * 37) % 96;
      size_t i;

      for (i = 0; i < width; i++)
         fixture[n++] = (char)('a' + ((line + i) % 26));
      fixture[n++] = '\n';
   }

   /* Tail with no newline: the last gets() must still return it. */
   for (line = 0; n < cap; line++)
      fixture[n++] = (char)('0' + (line % 10));

   fixture_len = n;

   if (!(fp = fopen(FIXTURE_PATH, "wb")))
      return false;
   if (fwrite(fixture, 1, fixture_len, fp) != fixture_len)
   {
      fclose(fp);
      return false;
   }
   fclose(fp);
   return true;
}

/* ---- the scan ------------------------------------------------------ */

/* Walk the whole file with filestream_gets() and compare every byte it
 * hands back against the fixture at the offset it should have come
 * from.  Returns the number of bytes consumed, or (size_t)-1 on a
 * mismatch, so a caller can assert both content and length. */
static size_t scan_file(bool *err_flag_out)
{
   char   line[LINE_BUF];
   RFILE *fp   = filestream_open(FIXTURE_PATH,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   size_t off  = 0;

   if (err_flag_out)
      *err_flag_out = false;

   if (!fp)
      return (size_t)-1;

   for (;;)
   {
      size_t n;

      gets_calls++;
      if (!filestream_gets(fp, line, sizeof(line)))
         break;
      n = strlen(line);

      if (n == 0 || n > sizeof(line) - 1)
      {
         off = (size_t)-1;
         break;
      }
      if (off + n > fixture_len || memcmp(fixture + off, line, n) != 0)
      {
         off = (size_t)-1;
         break;
      }
      off += n;
   }

   if (err_flag_out && off != (size_t)-1)
      *err_flag_out = filestream_error(fp) ? true : false;

   filestream_close(fp);
   return off;
}

/* ---- cases --------------------------------------------------------- */

/* Baseline.  Nothing is faulted; this is what the shim on its own
 * costs, and it is what the faulted runs below are read against.  It
 * is also the control that keeps them honest: if the shim itself
 * degraded the handle, every "the handle is buffered" assertion below
 * would pass vacuously. */
static void case_baseline(void)
{
   size_t        got;
   unsigned long expect_fills = EXPECT_FILLS(RBUF_LEN);

   reset_counters();
   got = scan_file(NULL);

   CHECK(got == fixture_len, "baseline: whole file read back byte-exact");
   CHECK(alloc_last_granted == RBUF_LEN,
         "baseline: lookahead allocated at the nominal 16 KiB");
   CHECK(alloc_refusals == 0, "baseline: no allocation refused");
   CHECK(vfs_tell_calls == expect_fills,
         "baseline: one tell() per fill, not per byte");
   CHECK(vfs_read_calls == expect_fills,
         "baseline: one read() per fill, not per byte");
   printf("info: baseline %lu tell, %lu read, %lu alloc for %lu bytes\n",
         vfs_tell_calls, vfs_read_calls, alloc_calls,
         (unsigned long)fixture_len);
}

/* The latch.  tell() fails on this handle from the first call.  Two
 * unrelated things produce that: a backend with no tell callback at
 * all - a pipe, a FIFO, a socket - and an ordinary seekable file whose
 * position has passed what ftell() can return on a build without
 * 64-bit offsets.  This case covers the first only; the second is
 * covered by case_tell_latch_clears_on_seek below, and the two must
 * not be conflated, because the latch is permanent for one and must
 * not be for the other.  The scan must still be byte-exact - the
 * fallback path is the pre-lookahead code and has to keep behaving
 * like it - and the implementation must ask exactly once. */
static void case_tell_failure_is_latched(void)
{
   size_t got;
   bool   err = false;

   reset_counters();
   vfs_tell_fails = true;
   got            = scan_file(&err);

   CHECK(got == fixture_len,
         "sticky tell: degraded path returns the file byte-exact");
   CHECK(vfs_tell_calls == 1,
         "sticky tell: tell() attempted exactly once for the handle");
   CHECK(vfs_read_calls == EXPECT_BYTE_READS,
         "sticky tell: fallback is one 1-byte read per byte, and nothing else");
   CHECK(alloc_calls == 1,
         "sticky tell: the lookahead is allocated once and not reattempted");
   printf("info: sticky tell %lu tell, %lu read for %lu bytes\n",
         vfs_tell_calls, vfs_read_calls, (unsigned long)fixture_len);
   /* Not asserted: filestream_raw_tell() sets err_flag on failure, so a
    * successful scan of a non-seekable handle leaves filestream_error()
    * true.  The latch reduces that from once per byte to once per
    * handle but does not remove it.  It is a separate defect from the
    * one this file covers and is deliberately left alone here. */
   printf("info: filestream_error() after the scan = %s\n",
         err ? "true" : "false");
}

/* The other arm, and the one that must NOT latch.  Every allocation is
 * refused, so every fill gives up and every byte goes through the
 * one-byte path.  The implementation is required to keep asking: the
 * handle must not be written off. */
/* The clear, and it is the reason the latch is not permanent.  A
 * failed tell() is not always a fact about the handle: retro_vfs_fp_tell64()
 * falls through to ftell() on a build without 64-bit offsets, and that
 * returns -1 once the position passes LONG_MAX.  Seeking back makes it
 * work again.  A latch that never cleared would pin an ordinary
 * seekable file to per-byte reads for the rest of its life because it
 * once crossed 2 GiB.
 *
 * The failure is simulated rather than reproduced - a 2 GiB fixture is
 * not something a unit test should write - but the code path is the
 * real one: the same tell() returning the same -1 through the same
 * VFS callback.
 *
 * What is asserted is the cost bound the struct comment claims. After
 * the seek the latch is gone, so ONE fill() asks tell() again; if it
 * failed again the latch would re-arm on that single call rather than
 * re-arming per byte.  Two tell() calls across two scans is that bound
 * observed; anything higher would mean the clear re-opened the
 * per-byte cost the latch exists to close. */
static void case_tell_latch_clears_on_seek(void)
{
   char          line[LINE_BUF];
   RFILE        *fp;
   unsigned long tell_after_first;

   reset_counters();
   vfs_tell_fails = true;

   fp = filestream_open(FIXTURE_PATH,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(fp != NULL, "latch clear: fixture opened");
   if (!fp)
      return;

   CHECK(filestream_gets(fp, line, sizeof(line)) != NULL,
         "latch clear: the degraded first read still returns a line");
   tell_after_first = vfs_tell_calls;
   CHECK(tell_after_first == 1,
         "latch clear: the first fill asks tell() once and latches on the failure");

   /* The position was the problem, not the handle. */
   vfs_tell_fails = false;
   /* -1 rather than VFS_ERROR_RETURN_VALUE: that macro is private to
    * file_stream.c and the public header does not export it. */
   CHECK(filestream_seek(fp, 0, RETRO_VFS_SEEK_POSITION_START) != -1,
         "latch clear: the rewind succeeds");

   CHECK(filestream_gets(fp, line, sizeof(line)) != NULL,
         "latch clear: the rewound read returns a line");
   CHECK(vfs_tell_calls == tell_after_first + 1,
         "latch clear: the seek re-armed exactly one tell(), not one per byte");
   CHECK(strlen(line) != 0 && memcmp(fixture, line, strlen(line)) == 0,
         "latch clear: the rewound read returns the head of the file");

   printf("info: latch clear %lu tell across two scans\n", vfs_tell_calls);

   filestream_close(fp);
}

/* The other half of the clear, and without it the suite cannot tell a
 * correct implementation from one that clears unconditionally.  Only a
 * SUCCESSFUL seek is evidence the position may have moved; a seek that
 * fails leaves the handle exactly where it was, so the latch must
 * survive it.  A pipe is the case that matters - it can neither tell
 * nor seek - and if a failed seek cleared the latch, every seek attempt
 * on such a handle would re-arm a tell() the code already knows will
 * fail.
 *
 * Seeking to a negative offset is used rather than a fault hook: it
 * fails through the real code path with no injection at all, which is
 * one less thing the test has to be trusted about. */
static void case_failed_seek_keeps_the_latch(void)
{
   char          line[LINE_BUF];
   RFILE        *fp;
   unsigned long tell_after_first;

   reset_counters();
   vfs_tell_fails = true;

   fp = filestream_open(FIXTURE_PATH,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(fp != NULL, "failed seek: fixture opened");
   if (!fp)
      return;

   CHECK(filestream_gets(fp, line, sizeof(line)) != NULL,
         "failed seek: the degraded first read still returns a line");
   tell_after_first = vfs_tell_calls;
   CHECK(tell_after_first == 1,
         "failed seek: the first fill asks tell() once and latches");

   CHECK(filestream_seek(fp, -1, RETRO_VFS_SEEK_POSITION_START) == -1,
         "failed seek: seeking to a negative offset does fail");

   CHECK(filestream_gets(fp, line, sizeof(line)) != NULL,
         "failed seek: the next read still returns a line");
   CHECK(vfs_tell_calls == tell_after_first,
         "failed seek: a failed seek does not re-arm tell()");

   printf("info: failed seek %lu tell, latch held\n", vfs_tell_calls);

   filestream_close(fp);
}

static void case_alloc_failure_is_not_latched(void)
{
   size_t got;

   reset_counters();
   alloc_refuse_at_least = 1;      /* refuse every size */
   got                   = scan_file(NULL);

   CHECK(got == fixture_len,
         "alloc refused: degraded path returns the file byte-exact");
   CHECK(vfs_tell_calls == 0,
         "alloc refused: no tell() is attempted when there is no buffer");
   CHECK(vfs_read_calls == EXPECT_BYTE_READS,
         "alloc refused: fallback is one 1-byte read per byte");
   /* Pinned rather than bounded: every fill walks the whole ladder,
    * and a fill happens once per gets() plus once per byte the getc()
    * fallback consumes.  A latch on this arm would collapse it to 3. */
   CHECK(alloc_calls == 3 * (gets_calls + EXPECT_BYTE_READS),
         "alloc refused: the handle is not latched - the full ladder is retried per fill");
   printf("info: alloc refused %lu allocation attempts for %lu bytes\n",
         alloc_calls, (unsigned long)fixture_len);
}

/* Refuse the nominal size but allow anything smaller.  This is the
 * shape a fragmented or nearly-full heap actually has, and the handle
 * is supposed to come out of it buffered rather than degraded. */
static void case_alloc_falls_back_to_smaller(void)
{
   size_t got;

   reset_counters();
   alloc_refuse_at_least = RBUF_LEN;
   got                   = scan_file(NULL);

   CHECK(got == fixture_len,
         "smaller buffer: whole file read back byte-exact");
   CHECK(alloc_last_granted == RBUF_LEN / 4,
         "smaller buffer: the next size down is taken, not the byte path");
   CHECK(alloc_refusals == 1,
         "smaller buffer: the nominal size is refused once, then left alone");
   CHECK(vfs_read_calls == EXPECT_FILLS(RBUF_LEN / 4),
         "smaller buffer: read count is the 4 KiB one, not the per-byte one");
   printf("info: smaller buffer granted %lu bytes, %lu read calls\n",
         (unsigned long)alloc_last_granted, vfs_read_calls);
}

/* A bad moment rather than a bad handle: the first ladder's worth of
 * requests is refused and everything after it is granted.  The handle
 * must recover to the nominal buffer on its own - which is the whole
 * reason this arm is not latched. */
static void case_transient_alloc_failure_recovers(void)
{
   size_t got;

   reset_counters();
   alloc_refuse_first_n = 3;       /* 16 KiB, 4 KiB, 1 KiB */
   got                  = scan_file(NULL);

   CHECK(got == fixture_len,
         "transient: whole file read back byte-exact");
   CHECK(alloc_refusals == 3,
         "transient: the first fill exhausted the ladder and gave up");
   CHECK(alloc_last_granted == RBUF_LEN,
         "transient: a later fill got the nominal size back");
   CHECK(vfs_read_calls == EXPECT_FILLS(RBUF_LEN),
         "transient: the handle went back to the 16 KiB read count");
   printf("info: transient %lu refusals then %lu bytes, %lu read calls\n",
         alloc_refusals, (unsigned long)alloc_last_granted, vfs_read_calls);
}

/* The cost bound the seek clear rests on, observed rather than argued.
 * case_tell_latch_clears_on_seek lets tell() start working before the
 * seek, so it only shows the clear happening.  This is the case the
 * clear could be wrong about: a backend that implements seek but not
 * tell, where clearing buys nothing.  The whole file is scanned after
 * the seek, and the re-arm must cost ONE tell() for the scan, not one
 * per byte - two across the run, before and after.  Remove the guard
 * on the clear in filestream_seek() and this is the case that moves. */
static void case_latch_rearms_once_after_seek(void)
{
   char   line[LINE_BUF];
   RFILE *fp;
   size_t off = 0;

   reset_counters();
   vfs_tell_fails = true;

   fp = filestream_open(FIXTURE_PATH,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(fp != NULL, "re-arm: fixture opened");
   if (!fp)
      return;

   CHECK(filestream_gets(fp, line, sizeof(line)) != NULL,
         "re-arm: the degraded first read still returns a line");
   CHECK(vfs_tell_calls == 1, "re-arm: the first fill latches");
   CHECK(alloc_calls == 1,
         "re-arm: the lookahead is allocated once for the first fill");

   /* tell() is still failing.  This is the seek the clear may be
    * wrong about, and the assertion below is what wrong costs. */
   CHECK(filestream_seek(fp, 0, RETRO_VFS_SEEK_POSITION_START) != -1,
         "re-arm: the rewind succeeds even though tell() does not");

   while (filestream_gets(fp, line, sizeof(line)))
   {
      size_t n = strlen(line);
      if (n == 0 || off + n > fixture_len
            || memcmp(fixture + off, line, n) != 0)
      {
         off = (size_t)-1;
         break;
      }
      off += n;
   }

   CHECK(off == fixture_len,
         "re-arm: the whole file still comes back byte-exact");
   CHECK(vfs_tell_calls == 2,
         "re-arm: the cleared latch costs one tell() for the scan, not one per byte");
   CHECK(alloc_calls == 2,
         "re-arm: the buffer is released on the latch and taken again on the clear");

   printf("info: re-arm %lu tell, %lu alloc across a full scan after the seek\n",
         vfs_tell_calls, alloc_calls);

   filestream_close(fp);
}

/* The limitation the ladder comment states, asserted so it stays a
 * decision rather than drifting into a surprise.  A handle that took a
 * smaller buffer while the heap was tight keeps it once the pressure
 * lifts: rbuf != NULL short-circuits the ladder and there is no path
 * back up.  1 KiB is still a thousandth of the byte path's VFS
 * traffic, which is why this is left alone. */
static void case_smaller_buffer_is_kept(void)
{
   char   line[LINE_BUF];
   RFILE *fp;
   size_t off = 0;

   reset_counters();
   alloc_refuse_at_least = RBUF_LEN;

   fp = filestream_open(FIXTURE_PATH,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(fp != NULL, "kept size: fixture opened");
   if (!fp)
      return;

   CHECK(filestream_gets(fp, line, sizeof(line)) != NULL,
         "kept size: the first read is served from the smaller buffer");
   CHECK(alloc_last_granted == RBUF_LEN / 4,
         "kept size: the first fill took the next size down");

   /* The bad moment passes. */
   alloc_refuse_at_least = 0;
   off                   = strlen(line);

   while (filestream_gets(fp, line, sizeof(line)))
   {
      size_t n = strlen(line);
      if (n == 0 || off + n > fixture_len
            || memcmp(fixture + off, line, n) != 0)
      {
         off = (size_t)-1;
         break;
      }
      off += n;
   }

   CHECK(off == fixture_len, "kept size: whole file read back byte-exact");
   CHECK(alloc_calls == 2,
         "kept size: nothing is reallocated once a buffer exists");
   CHECK(vfs_read_calls == EXPECT_FILLS(RBUF_LEN / 4),
         "kept size: the handle stays on the 4 KiB read count for its life");

   printf("info: kept size %lu bytes held across %lu read calls\n",
         (unsigned long)alloc_last_granted, vfs_read_calls);

   filestream_close(fp);
}

int main(void)
{
   if (!make_fixture())
   {
      printf("FAIL: could not build the fixture\n");
      return 1;
   }

   install_shim_vfs();

   printf("fixture: %lu bytes, lookahead %d bytes\n",
         (unsigned long)fixture_len, RBUF_LEN);

   case_baseline();
   case_tell_failure_is_latched();
   case_tell_latch_clears_on_seek();
   case_failed_seek_keeps_the_latch();
   case_latch_rearms_once_after_seek();
   case_alloc_failure_is_not_latched();
   case_alloc_falls_back_to_smaller();
   case_smaller_buffer_is_kept();
   case_transient_alloc_failure_recovers();

   remove(FIXTURE_PATH);
   free(fixture);

   if (test_fails)
      printf("\n%d check(s) failed\n", test_fails);
   else
      printf("\nall checks passed\n");

   return test_fails ? 1 : 0;
}
