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
 * The allocation arm has no such seam, so file_stream.c routes that
 * one allocation through FILESTREAM_RBUF_MALLOC(); with
 * FILESTREAM_RBUF_TEST_HOOKS undefined that macro is malloc() and
 * nothing else exists.
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

extern void *(*filestream_rbuf_test_malloc)(size_t len);

static unsigned long alloc_calls;
static unsigned long alloc_refusals;
static size_t        alloc_last_granted;
static size_t        alloc_refuse_at_least;  /* 0 = refuse nothing */
static unsigned long alloc_refuse_first_n;   /* transient failure    */

static void *fault_malloc(size_t len)
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

   filestream_rbuf_test_malloc = fault_malloc;
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

   while (filestream_gets(fp, line, sizeof(line)))
   {
      size_t n = strlen(line);

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

/* The latch.  tell() fails on this handle from the first call, which
 * is what a pipe, a FIFO, or a frontend VFS without a tell callback
 * looks like.  The scan must still be byte-exact - the fallback path
 * is the pre-lookahead code and has to keep behaving like it - and the
 * implementation must ask exactly once. */
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
   CHECK(alloc_calls > 1,
         "alloc refused: the handle is not latched - allocation is retried");
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
   case_alloc_failure_is_not_latched();
   case_alloc_falls_back_to_smaller();
   case_transient_alloc_failure_recovers();

   filestream_rbuf_test_malloc = NULL;
   remove(FIXTURE_PATH);
   free(fixture);

   if (test_fails)
      printf("\n%d check(s) failed\n", test_fails);
   else
      printf("\nall checks passed\n");

   return test_fails ? 1 : 0;
}
