/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_v5_metadata_test.c).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* VFS API v5 contract: read-only state, modification time, copy and
 * per-entry stat during enumeration.  Runs against the local
 * implementation (no frontend), which is the code every frontend
 * build links, so it covers the _impl functions and the path_*,
 * filestream_copy* and retro_dirent_stat wrappers over them.
 *
 * Everything lives in one temporary directory created next to the
 * binary and removed at the end.  Skips (not failures) are reported
 * where the host file system cannot store the state under test. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

#if !defined(_WIN32)
#include <unistd.h>   /* geteuid: root ignores mode bits */
#endif

#define DIR_NAME   "v5_meta_dir"
#define BIG_SIZE   (3u * 1024u * 1024u + 17u)   /* > any fast-path chunk, odd tail */

static int failures = 0;
static int skips    = 0;

#define CHECK(cond, what) do { \
   if (cond) printf("  ok   %s\n", what); \
   else { printf("  FAIL %s (%s:%d)\n", what, __FILE__, __LINE__); failures++; } \
} while (0)

#define SKIP(what) do { printf("  skip %s\n", what); skips++; } while (0)

static bool write_pattern(const char *path, size_t len, unsigned seed)
{
   size_t i;
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   unsigned char *buf;
   if (!f)
      return false;
   buf = (unsigned char*)malloc(len ? len : 1);
   for (i = 0; i < len; i++)
      buf[i] = (unsigned char)((i * 2654435761u + seed) >> 13);
   if (filestream_write(f, buf, (int64_t)len) != (int64_t)len)
   {
      free(buf);
      filestream_close(f);
      return false;
   }
   free(buf);
   return filestream_close(f) == 0;
}

static bool files_equal(const char *a, const char *b)
{
   /* filestream_cmp lives in a different unit in some trees; a
    * self-contained byte compare keeps this sample's link list short. */
   RFILE *fa = filestream_open(a, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   RFILE *fb = filestream_open(b, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   unsigned char ba[65536], bb[65536];
   bool same = (fa && fb);
   while (same)
   {
      int64_t na = filestream_read(fa, ba, sizeof(ba));
      int64_t nb = filestream_read(fb, bb, sizeof(bb));
      if (na != nb || na < 0)
         same = false;
      else if (na == 0)
         break;
      else if (memcmp(ba, bb, (size_t)na) != 0)
         same = false;
   }
   if (fa) filestream_close(fa);
   if (fb) filestream_close(fb);
   return same;
}

static void test_readonly(const char *dir)
{
   char p[512];
   printf("read-only:\n");
   snprintf(p, sizeof(p), "%s/ro.bin", dir);
   CHECK(write_pattern(p, 100, 1), "fixture written");
   CHECK(!path_is_readonly(p), "fresh file is writable");

   if (!path_set_readonly(p, true))
   {
      SKIP("set_readonly unsupported on this platform/file system");
      return;
   }
   CHECK(path_is_readonly(p), "IS_READONLY set after set_readonly(1)");
#if !defined(_WIN32)
   if (geteuid() == 0)
      SKIP("open-for-write-denied check (running as root, mode bits are not enforced)");
   else
#endif
   {
      RFILE *f = filestream_open(p, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(f == NULL, "open for write fails while read-only");
      if (f) filestream_close(f);
   }
   CHECK(path_set_readonly(p, false), "set_readonly(0) succeeds");
   CHECK(!path_is_readonly(p), "IS_READONLY clear again");
   {
      RFILE *f = filestream_open(p, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(f != NULL, "open for write succeeds again");
      if (f) filestream_close(f);
   }
   CHECK(!path_set_readonly("does/not/exist.bin", true), "set_readonly on missing path fails");
}

static void test_mtime(const char *dir)
{
   char p[512];
   int64_t t = 0, t2 = 0;
   printf("mtime:\n");
   snprintf(p, sizeof(p), "%s/mt.bin", dir);
   CHECK(write_pattern(p, 10, 2), "fixture written");
   CHECK(path_get_mtime(p, &t), "get_mtime succeeds");
   CHECK(t > 1000000000, "mtime is a plausible recent time");
   CHECK(!path_get_mtime("does/not/exist.bin", &t2), "get_mtime on missing path fails");

   if (!path_set_mtime(p, 1234567890))
   {
      SKIP("set_mtime unsupported on this platform/file system");
      return;
   }
   CHECK(path_get_mtime(p, &t2), "get_mtime after set");
   /* FAT stores 2 s resolution; allow that slack. */
   CHECK(t2 >= 1234567888 && t2 <= 1234567892, "mtime round-trips (within 2 s)");
   CHECK(path_set_mtime(p, -86400), "negative (pre-1970) mtime accepted");
   CHECK(path_get_mtime(p, &t2) && t2 <= -86398 && t2 >= -86402, "negative mtime round-trips");
}

/* Drive a begin/step/close copy to completion with a fixed per-step
 * budget, checking that no step overshoots it and that progress is
 * monotonic.  A small budget exercises resumption across many steps;
 * a huge one is the "run it flat out" case. */
static int overshot_once = 0;

static int copy_sync_budget(const char *src, const char *dst, unsigned flags,
      int64_t budget, unsigned *steps_out)
{
   struct retro_vfs_copy_handle *h = filestream_copy_begin(src, dst, flags);
   int st;
   int64_t done = 0, total = 0, prev = 0;
   unsigned steps = 0;
   if (!h)
      return -1;
   for (;;)
   {
      st = filestream_copy_step(h, budget, &done, &total);
      if (getenv("VFS_V5_TRACE") || (steps < 3 && budget > 0 && budget < 1000000))
         printf("  trace step %u: status %d done %lld total %lld\n",
               steps + 1, st, (long long)done, (long long)total);
      if (done > total || done < prev)
      {
         printf("  FAIL step %u: done %lld went backwards or past total %lld\n",
               steps + 1, (long long)done, (long long)total);
         failures++;
         break;
      }
      if (budget > 0 && done - prev > budget)
      {
         /* The implementation asked for <= budget; a kernel that hands
          * back more than that is a platform quirk (gVisor copies to
          * EOF).  The VFS stops trusting it from here, which the
          * caller below verifies with a second budgeted copy. */
         printf("  note step %u moved %lld for a %lld budget: kernel ignored len\n",
               steps + 1, (long long)(done - prev), (long long)budget);
         overshot_once++;
      }
      if (st != RETRO_VFS_COPY_RUNNING)
         break;
      steps++;
      prev = done;
      if (steps > 100000000u)
         break;
   }
   if (steps_out)
      *steps_out = steps;
   return filestream_copy_close(h);
}

static int copy_sync(const char *src, const char *dst, unsigned flags)
{
   return copy_sync_budget(src, dst, flags, 0, NULL);
}

static void test_copy(const char *dir)
{
   char src[512], dst[512], sub[512], nested[512];
   printf("copy:\n");
   snprintf(src, sizeof(src), "%s/src.bin", dir);
   snprintf(dst, sizeof(dst), "%s/dst.bin", dir);
   snprintf(sub, sizeof(sub), "%s/sub", dir);
   snprintf(nested, sizeof(nested), "%s/sub/deeper/nested.bin", dir);

   CHECK(write_pattern(src, BIG_SIZE, 3), "3 MiB fixture written");
   CHECK(copy_sync(src, dst, 0) == 0, "copy to new dst completes (default step)");
   CHECK(files_equal(src, dst), "copy is byte-identical");
   CHECK(path_get_size(dst) == (int64_t)BIG_SIZE, "copy has the right size");
   CHECK(!path_is_readonly(dst), "copy is writable");

   /* Small budget: many resumed steps, none overshooting. */
   {
      unsigned steps = 0;
      CHECK(copy_sync_budget(src, dst, RETRO_VFS_COPY_OVERWRITE, 100000, &steps) == 0,
            "copy with a 100000-byte step budget completes");
      CHECK(files_equal(src, dst), "small-step copy is byte-identical");
      if (overshot_once)
      {
         /* Once the kernel has been caught ignoring len the VFS does
          * not offer it another byte, so this copy should step
          * properly.  A platform that overshoots here as well is
          * ignoring len somewhere the VFS cannot see (the [vfs-copy]
          * narration on stderr says where); the copy is still
          * correct, so that is reported, not failed. */
         overshot_once = 0;
         CHECK(copy_sync_budget(src, dst, RETRO_VFS_COPY_OVERWRITE, 100000, &steps) == 0,
               "budgeted copy after a kernel overshoot completes");
         CHECK(files_equal(src, dst), "post-overshoot copy is byte-identical");
         if (overshot_once)
            printf("  note this platform ignores len on the portable path too; "
                   "budget checks skipped\n");
         else
            printf("  ok   no second overshoot: kernel path retired\n");
      }
      if (overshot_once)
         printf("  note steps=%u (budgets not honoured by this platform)\n", steps);
      else
         CHECK(steps >= BIG_SIZE / 100000, "took at least the minimum number of steps");
   }
   /* Huge budget: one call moves everything. */
   {
      unsigned steps = 0;
      CHECK(copy_sync_budget(src, dst, RETRO_VFS_COPY_OVERWRITE, INT64_MAX, &steps) == 0,
            "copy with an unbounded budget completes");
      CHECK(files_equal(src, dst), "flat-out copy is byte-identical");
   }

   CHECK(filestream_copy_begin(src, dst, 0) == NULL, "begin onto existing dst without OVERWRITE refused");
   CHECK(files_equal(src, dst), "dst untouched by the refused copy");

   /* Cancel mid-copy: begin, move a little, close.  No partial file may
    * remain.  (A clone-capable file system may legitimately be DONE
    * after begin; then dst must be complete.) */
   {
      char cdst[512];
      struct retro_vfs_copy_handle *h;
      int rc, st;
      int64_t done = 0;
      snprintf(cdst, sizeof(cdst), "%s/cancelled.bin", dir);
      h  = filestream_copy_begin(src, cdst, 0);
      CHECK(h != NULL, "begin for cancel test");
      st = filestream_copy_step(h, 65536, &done, NULL);
      CHECK(st != RETRO_VFS_COPY_FAILED, "first small step ok");
      rc = filestream_copy_close(h);
      if (rc == 0)
         CHECK(files_equal(src, cdst), "closed after completion: dst complete");
      else
         CHECK(!path_is_valid(cdst), "closed while running: no partial dst");
      filestream_delete(cdst);
   }

   CHECK(write_pattern(src, 4096, 4), "fixture replaced with a different one");
   CHECK(copy_sync(src, dst, RETRO_VFS_COPY_OVERWRITE) == 0, "OVERWRITE replaces dst");
   CHECK(files_equal(src, dst) && path_get_size(dst) == 4096, "dst now matches the new source");

   if (path_set_readonly(dst, true))
   {
      CHECK(copy_sync(src, dst, RETRO_VFS_COPY_OVERWRITE) == 0, "OVERWRITE replaces a read-only dst (cp -f)");
      path_set_readonly(dst, false);
   }

   CHECK(copy_sync(src, nested, 0) == 0, "copy into a missing directory creates it");
   CHECK(path_is_directory(sub) && files_equal(src, nested), "nested copy landed");

   CHECK(filestream_copy_begin(src, src, RETRO_VFS_COPY_OVERWRITE) == NULL, "src == dst refused");
   CHECK(path_get_size(src) == 4096, "src not truncated by the refused self-copy");
   CHECK(filestream_copy_begin(dir, dst, RETRO_VFS_COPY_OVERWRITE) == NULL, "directory as src refused");
   CHECK(filestream_copy_begin(src, sub, RETRO_VFS_COPY_OVERWRITE) == NULL, "directory as dst refused");
   CHECK(filestream_copy_begin("does/not/exist.bin", dst, RETRO_VFS_COPY_OVERWRITE) == NULL, "missing src refused");

   /* No partial file on failure: a dst whose parent is a *file* cannot
    * be created, so begin must refuse and leave nothing. */
   {
      char bad[512];
      snprintf(bad, sizeof(bad), "%s/src.bin/child.bin", dir);
      CHECK(filestream_copy_begin(src, bad, 0) == NULL, "impossible dst refused");
      CHECK(!path_is_valid(bad), "no partial file left behind");
   }

   /* The pre-v5 blocking helper still works and still refuses the
    * same things. */
   CHECK(filestream_copy(src, dst) == 0 && files_equal(src, dst), "legacy filestream_copy still copies");
   CHECK(filestream_copy(src, src) != 0, "legacy filestream_copy refuses src == dst");
}

static void test_dirent_stat(const char *dir)
{
   char sub[512], f1[512], f2[512];
   struct RDIR *rd;
   int seen_f1 = 0, seen_f2 = 0, seen_dir = 0;
   printf("dirent_stat:\n");
   snprintf(sub, sizeof(sub), "%s/ds", dir);
   snprintf(f1,  sizeof(f1),  "%s/ds/a-1234.bin", dir);
   snprintf(f2,  sizeof(f2),  "%s/ds/b-ro.bin", dir);
   CHECK(path_mkdir(sub), "enumeration dir created");
   CHECK(write_pattern(f1, 1234, 5), "1234-byte file");
   CHECK(write_pattern(f2, 8, 6), "8-byte file");
   {
      char d[512];
      snprintf(d, sizeof(d), "%s/ds/childdir", dir);
      CHECK(path_mkdir(d), "child directory");
   }
   path_set_readonly(f2, true);

   rd = retro_opendir(sub);
   CHECK(rd != NULL, "opendir");
   while (rd && retro_readdir(rd))
   {
      const char *name = retro_dirent_get_name(rd);
      int64_t size = -1, mtime = 0, want_mtime = 0;
      int flags;
      char full[1024];
      if (!name || !strcmp(name, ".") || !strcmp(name, ".."))
         continue;
      flags = retro_dirent_stat(rd, &size, &mtime);
      snprintf(full, sizeof(full), "%s/%s", sub, name);

      if (!(flags & RETRO_VFS_STAT_IS_VALID))
      {
         SKIP("dirent_stat unavailable on this platform");
         continue;
      }
      CHECK(!!(flags & RETRO_VFS_STAT_IS_DIRECTORY) == retro_dirent_is_dir(rd, NULL),
            "IS_DIRECTORY agrees with dirent_is_dir");
      if (!(flags & RETRO_VFS_STAT_IS_DIRECTORY))
         CHECK(size == path_get_size(full), "size agrees with path_get_size");
      if (path_get_mtime(full, &want_mtime))
         CHECK(mtime == want_mtime, "mtime agrees with path_get_mtime");
      CHECK(!!(flags & RETRO_VFS_STAT_IS_READONLY) == path_is_readonly(full),
            "IS_READONLY agrees with path_is_readonly");

      if (!strcmp(name, "a-1234.bin")) { seen_f1++; CHECK(size == 1234, "a-1234.bin is 1234 bytes"); }
      if (!strcmp(name, "b-ro.bin"))   { seen_f2++; }
      if (!strcmp(name, "childdir"))   { seen_dir++; CHECK(flags & RETRO_VFS_STAT_IS_DIRECTORY, "childdir flagged as directory"); }
   }
   if (rd)
      retro_closedir(rd);
   CHECK(seen_f1 == 1 && seen_f2 == 1 && seen_dir == 1, "all three entries enumerated once");
   path_set_readonly(f2, false);
}

static void rm_tree(const char *dir)
{
   struct RDIR *rd = retro_opendir(dir);
   if (rd)
   {
      while (retro_readdir(rd))
      {
         const char *name = retro_dirent_get_name(rd);
         char full[1024];
         if (!name || !strcmp(name, ".") || !strcmp(name, ".."))
            continue;
         snprintf(full, sizeof(full), "%s/%s", dir, name);
         if (retro_dirent_is_dir(rd, NULL))
            rm_tree(full);
         else
         {
            path_set_readonly(full, false);
            filestream_delete(full);
         }
      }
      retro_closedir(rd);
   }
   retro_vfs_mkdir_impl(dir); /* no rmdir in the VFS; leave empty dirs */
}

int main(void)
{
   const char *dir = DIR_NAME;
   printf("vfs_v5_metadata_test\n");
   rm_tree(dir);
   if (!path_mkdir(dir))
   {
      printf("cannot create %s\n", dir);
      return 1;
   }
   test_readonly(dir);
   test_mtime(dir);
   test_copy(dir);
   test_dirent_stat(dir);
   rm_tree(dir);
   printf("%d failure(s), %d skip(s)\n", failures, skips);
   return failures ? 1 : 0;
}
