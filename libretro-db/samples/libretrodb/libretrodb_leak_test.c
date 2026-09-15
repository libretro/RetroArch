/* Regression test for memory leaks in libretrodb_open and
 * libretrodb_close (libretro-db/libretrodb.c).
 *
 * Two pre-this-patch leaks:
 *
 *   1. libretrodb_open's error: label freed neither db->path
 *      (strdup'd at line 209) nor the intfstream_t struct (alloc'd
 *      by intfstream_open_file at line 201).  Triggered by any
 *      malformed .rdb -- bad magic, bad metadata_offset, truncated
 *      header.  Reachable across a directory scan: every malformed
 *      .rdb the user has on disk leaks 48 + (path-length) bytes.
 *
 *   2. libretrodb_close (success-path counterpart) called
 *      intfstream_close on db->fd but never freed the struct
 *      itself.  Same 48-byte leak per opened-and-closed database
 *      file.
 *
 *   3. libretrodb_cursor_close had the same close-without-free
 *      pattern on cursor->fd.
 *
 * libretro-common convention is for callers to free the
 * intfstream_t after intfstream_close (see core_info.c,
 * core_backup.c, cdfs.c, rpng_encode.c trailing free() calls).
 * libretrodb's own teardown paths just didn't follow it.
 *
 * The test writes a few minimal .rdb files into /tmp, runs them
 * through libretrodb_open + libretrodb_close cycles, and exits
 * cleanly.  Under -fsanitize=address (the SANITIZER=address
 * Makefile build) any reintroduced leak is flagged at exit.
 *
 * Run with: make SANITIZER=address && ./libretrodb_leak_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "../../libretrodb.h"

#ifndef RETRO_VFS_FILE_ACCESS_READ
#define RETRO_VFS_FILE_ACCESS_READ              (1 << 0)
#endif

static int failures = 0;

static void put64be(uint8_t *p, uint64_t v)
{
   int i;
   for (i = 0; i < 8; i++)
      p[i] = (uint8_t)(v >> (56 - 8 * i));
}

/* Build a .rdb header at *p.  Returns bytes written.
 * Magic is "RARCHDB\0" (8) + 8-byte big-endian metadata_offset. */
static size_t make_header(uint8_t *p, uint64_t metadata_offset)
{
   memcpy(p, "RARCHDB", 7);
   p[7] = '\0';
   put64be(p + 8, metadata_offset);
   return 16;
}

/* Write `len` bytes from `buf` to a temp file at `path`.  Returns
 * 0 on success, -1 on failure. */
static int write_temp(const char *path, const uint8_t *buf, size_t len)
{
   FILE *f = fopen(path, "wb");
   if (!f)
      return -1;
   if (fwrite(buf, 1, len, f) != len)
   {
      fclose(f);
      return -1;
   }
   fclose(f);
   return 0;
}

static void test_open_close_valid(void)
{
   /* Minimal valid .rdb: header pointing at a fixmap-1 with
    * {"count": 0} immediately after. */
   uint8_t buf[64];
   const char *path = "/tmp/libretrodb_leak_test_valid.rdb";
   libretrodb_t *db;
   int rv;
   size_t len = make_header(buf, 16);
   buf[len++] = 0x81;                             /* fixmap-1 */
   buf[len++] = 0xa5; memcpy(buf + len, "count", 5); len += 5;
   buf[len++] = 0x00;                             /* fixint 0 */
   if (write_temp(path, buf, len) < 0)
   {
      printf("[ERROR] write_temp failed\n"); failures++; return;
   }

   db = libretrodb_new();
   if (!db) { printf("[ERROR] libretrodb_new\n"); failures++; goto cleanup; }
   rv = libretrodb_open(path, db, false);
   if (rv != 0)
   {
      printf("[ERROR] valid .rdb open failed (rv=%d)\n", rv);
      failures++;
   }
   else
   {
      libretrodb_close(db);
      printf("[SUCCESS] valid .rdb open + close cycle clean\n");
   }
   libretrodb_free(db);
cleanup:
   unlink(path);
}

static void test_open_bad_magic(void)
{
   /* Bad magic: header starts with 'XXXX...' instead of 'RARCHDB'.
    * Pre-fix this leaked 48 bytes (intfstream) + 11 bytes
    * (strdup'd /tmp/...) per call. */
   uint8_t buf[16];
   const char *path = "/tmp/libretrodb_leak_test_badmagic.rdb";
   libretrodb_t *db;
   int rv;
   memset(buf, 'X', 8);
   put64be(buf + 8, 16);
   if (write_temp(path, buf, sizeof(buf)) < 0)
   {
      printf("[ERROR] write_temp failed\n"); failures++; return;
   }

   db = libretrodb_new();
   if (!db) { printf("[ERROR] libretrodb_new\n"); failures++; goto cleanup; }
   rv = libretrodb_open(path, db, false);
   if (rv == 0)
   {
      printf("[ERROR] bad-magic .rdb accepted\n");
      failures++;
      libretrodb_close(db);
   }
   else
      printf("[SUCCESS] bad-magic .rdb rejected without leak\n");
   libretrodb_free(db);
cleanup:
   unlink(path);
}

static void test_open_bad_metadata_offset(void)
{
   /* metadata_offset past EOF: 16-byte file, offset = 100. */
   uint8_t buf[16];
   const char *path = "/tmp/libretrodb_leak_test_badoff.rdb";
   libretrodb_t *db;
   int rv;
   make_header(buf, 100);
   if (write_temp(path, buf, sizeof(buf)) < 0)
   {
      printf("[ERROR] write_temp failed\n"); failures++; return;
   }

   db = libretrodb_new();
   if (!db) { printf("[ERROR] libretrodb_new\n"); failures++; goto cleanup; }
   rv = libretrodb_open(path, db, false);
   if (rv == 0)
   {
      printf("[ERROR] bad-offset .rdb accepted\n");
      failures++;
      libretrodb_close(db);
   }
   else
      printf("[SUCCESS] bad-offset .rdb rejected without leak\n");
   libretrodb_free(db);
cleanup:
   unlink(path);
}

static void test_repeat_open_close(void)
{
   /* Open and close the valid .rdb several times on the same
    * libretrodb_t.  libretrodb_open at line ~206-209 frees
    * db->path if it's already set then strdup's the new one --
    * which is fine, but the intfstream_t leak compounded across
    * each call pre-fix. */
   uint8_t buf[64];
   const char *path = "/tmp/libretrodb_leak_test_repeat.rdb";
   libretrodb_t *db;
   int i;
   size_t len = make_header(buf, 16);
   buf[len++] = 0x81;
   buf[len++] = 0xa5; memcpy(buf + len, "count", 5); len += 5;
   buf[len++] = 0x00;
   if (write_temp(path, buf, len) < 0)
   {
      printf("[ERROR] write_temp failed\n"); failures++; return;
   }

   db = libretrodb_new();
   if (!db) { printf("[ERROR] libretrodb_new\n"); failures++; goto cleanup; }
   for (i = 0; i < 5; i++)
   {
      int rv = libretrodb_open(path, db, false);
      if (rv != 0)
      {
         printf("[ERROR] repeat open #%d failed\n", i);
         failures++;
         break;
      }
      libretrodb_close(db);
   }
   libretrodb_free(db);
   if (i == 5)
      printf("[SUCCESS] 5x open+close cycle clean\n");
cleanup:
   unlink(path);
}

int main(void)
{
   test_open_close_valid();
   test_open_bad_magic();
   test_open_bad_metadata_offset();
   test_repeat_open_close();

   if (failures)
   {
      printf("\n%d libretrodb leak test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll libretrodb leak regression tests passed.\n");
   return 0;
}
