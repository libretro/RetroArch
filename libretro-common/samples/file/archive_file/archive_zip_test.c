/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_zip_test.c).
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

/* Regression tests for ZIP central-directory hardening.
 *
 * These tests write hand-crafted malformed ZIP bytes to a temp file
 * and feed them to file_archive_get_file_list().  On patched code
 * every case parses cleanly (empty list or NULL).  On unpatched code
 * (commit e044ef6 and earlier), under AddressSanitizer:
 *
 * Strong regression discriminators (ASan fires on unpatched):
 *   - Case A "truncated central-dir entry"   -> ASan heap-buffer-overflow
 *                                               READ in read_le, line 561
 *   - Case B "oversized namelength"          -> ASan heap-buffer-overflow
 *                                               READ of size N in memcpy,
 *                                               line 558
 *
 * Smoke tests (exercise the code path but do not OOB in the unpatched
 * build; retained as defence-in-depth against future regressions):
 *   - Case C "empty filename"                -> reaches parser but not
 *                                               the zip_file_decompressed
 *                                               strlen-1 call via
 *                                               get_file_list
 *   - Case D "combined offset+size overflow" -> already partially caught
 *                                               by pre-existing per-field
 *                                               checks; tests the new
 *                                               combined check
 *   - Case E "directory_size = UINT32_MAX"   -> caught by pre-existing
 *                                               "> archive_size" check;
 *                                               tests the 32-bit alloc
 *                                               overflow guard
 *
 * Rationale for keeping smoke tests: if someone ever refactors the
 * parser to remove the individual sanity checks (thinking them
 * redundant) the smoke tests should still exercise the patched guards.
 *
 * All bytes are crafted in C rather than committed as binary fixtures.
 *
 * Build with AddressSanitizer for full coverage:
 *   make SANITIZER=address
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <file/archive_file.h>
#include <lists/string_list.h>

static int failures = 0;

/* --- tiny ZIP builders (little-endian byte layout) ----------------- */

static void put_u16(uint8_t *p, uint16_t v)
{
   p[0] = v & 0xff;
   p[1] = (v >> 8) & 0xff;
}

static void put_u32(uint8_t *p, uint32_t v)
{
   p[0] =  v        & 0xff;
   p[1] = (v >>  8) & 0xff;
   p[2] = (v >> 16) & 0xff;
   p[3] = (v >> 24) & 0xff;
}

#define EOCD_SIG   0x06054b50u
#define CFH_SIG    0x02014b50u
#define LFH_SIG    0x04034b50u

/* Minimal End-Of-Central-Directory record (22 bytes, no comment).
 * See PKWARE APPNOTE section 4.3.16. */
static size_t write_eocd(uint8_t *dst, uint16_t num_entries,
      uint32_t directory_size, uint32_t directory_offset)
{
   put_u32(dst + 0,  EOCD_SIG);
   put_u16(dst + 4,  0);             /* disk number                    */
   put_u16(dst + 6,  0);             /* disk w/ start of cdir          */
   put_u16(dst + 8,  num_entries);   /* entries on this disk           */
   put_u16(dst + 10, num_entries);   /* entries total                  */
   put_u32(dst + 12, directory_size);
   put_u32(dst + 16, directory_offset);
   put_u16(dst + 20, 0);             /* comment length                 */
   return 22;
}

/* Minimal Local File Header for a STORED zero-byte file.  We only
 * need this if a central-directory entry points at it -- the parser
 * seeks to cdata+26 and reads the 4 name+extra-length bytes. */
static size_t write_minimal_lfh(uint8_t *dst, const char *name)
{
   size_t namelen = name ? strlen(name) : 0;
   put_u32(dst + 0,  LFH_SIG);
   put_u16(dst + 4,  20);        /* version needed to extract          */
   put_u16(dst + 6,  0);         /* flags                              */
   put_u16(dst + 8,  0);         /* method = STORED                    */
   put_u16(dst + 10, 0);         /* mtime                              */
   put_u16(dst + 12, 0);         /* mdate                              */
   put_u32(dst + 14, 0);         /* crc32                              */
   put_u32(dst + 18, 0);         /* compressed size                    */
   put_u32(dst + 22, 0);         /* uncompressed size                  */
   put_u16(dst + 26, (uint16_t)namelen);
   put_u16(dst + 28, 0);         /* extra length                       */
   if (namelen > 0)
      memcpy(dst + 30, name, namelen);
   return 30 + namelen;
}

static size_t write_stored_lfh(uint8_t *dst, const char *name,
      const uint8_t *payload, size_t payload_len, uint32_t crc32)
{
   size_t namelen = name ? strlen(name) : 0;
   put_u32(dst + 0,  LFH_SIG);
   put_u16(dst + 4,  20);        /* version needed to extract          */
   put_u16(dst + 6,  0);         /* flags                              */
   put_u16(dst + 8,  0);         /* method = STORED                    */
   put_u16(dst + 10, 0);         /* mtime                              */
   put_u16(dst + 12, 0);         /* mdate                              */
   put_u32(dst + 14, crc32);
   put_u32(dst + 18, (uint32_t)payload_len);
   put_u32(dst + 22, (uint32_t)payload_len);
   put_u16(dst + 26, (uint16_t)namelen);
   put_u16(dst + 28, 0);         /* extra length                       */
   if (namelen > 0)
      memcpy(dst + 30, name, namelen);
   if (payload_len > 0)
      memcpy(dst + 30 + namelen, payload, payload_len);
   return 30 + namelen + payload_len;
}

static size_t write_stored_cfh(uint8_t *dst, const char *name,
      size_t payload_len, uint32_t crc32, uint32_t lfh_offset)
{
   size_t namelen = name ? strlen(name) : 0;
   put_u32(dst + 0,  CFH_SIG);
   put_u16(dst + 4,  20);        /* version made by                    */
   put_u16(dst + 6,  20);        /* version needed to extract          */
   put_u16(dst + 8,  0);         /* flags                              */
   put_u16(dst + 10, 0);         /* method = STORED                    */
   put_u16(dst + 12, 0);         /* mtime                              */
   put_u16(dst + 14, 0);         /* mdate                              */
   put_u32(dst + 16, crc32);
   put_u32(dst + 20, (uint32_t)payload_len);
   put_u32(dst + 24, (uint32_t)payload_len);
   put_u16(dst + 28, (uint16_t)namelen);
   put_u16(dst + 30, 0);         /* extra length                       */
   put_u16(dst + 32, 0);         /* comment length                     */
   put_u16(dst + 34, 0);         /* disk number start                  */
   put_u16(dst + 36, 0);         /* internal attributes                */
   put_u32(dst + 38, 0);         /* external attributes                */
   put_u32(dst + 42, lfh_offset);
   if (namelen > 0)
      memcpy(dst + 46, name, namelen);
   return 46 + namelen;
}

static size_t write_appledouble_collision_zip(uint8_t *dst)
{
   static const char *names[]        = {
      "__MACOSX/regular.nes",
      "folder/._sidecar.nes",
      "folder\\._backslash.nes",
      "__MACOSX/._game.nes",
      "game.nes"
   };
   static const char metadata[]      = "METADATA";
   static const char rom[]           = "ROMDATA";
   uint32_t lfh_offsets[5]           = {0};
   uint32_t cdir_offset              = 0;
   uint32_t cdir_size                = 0;
   size_t len                        = 0;
   size_t i;

   for (i = 0; i < 4; i++)
   {
      lfh_offsets[i] = (uint32_t)len;
      len += write_stored_lfh(dst + len, names[i],
            (const uint8_t*)metadata, sizeof(metadata) - 1, 0x8927a858u);
   }

   lfh_offsets[4] = (uint32_t)len;
   len += write_stored_lfh(dst + len, names[4],
         (const uint8_t*)rom, sizeof(rom) - 1, 0xcd69c26au);

   cdir_offset = (uint32_t)len;
   for (i = 0; i < 4; i++)
      len += write_stored_cfh(dst + len, names[i],
            sizeof(metadata) - 1, 0x8927a858u, lfh_offsets[i]);
   len += write_stored_cfh(dst + len, names[4],
         sizeof(rom) - 1, 0xcd69c26au, lfh_offsets[4]);
   cdir_size = (uint32_t)len - cdir_offset;

   len += write_eocd(dst + len, 5, cdir_size, cdir_offset);
   return len;
}

/* --- test harness -------------------------------------------------- */

static void write_file(const char *path, const void *data, size_t len)
{
   FILE *fp = fopen(path, "wb");
   if (!fp)
      abort();
   if (fwrite(data, 1, len, fp) != len)
      abort();
   fclose(fp);
}

static bool parse_malformed_zip(const char *label,
      const void *bytes, size_t len)
{
   const char        *tmp_path = "rarch_zip_regression_test.zip";
   struct string_list *list    = NULL;

   write_file(tmp_path, bytes, len);
   /* The success criterion is "did not trigger a memory error".
    * On unpatched code AddressSanitizer fires on the OOB reads
    * inside the parser and aborts here.  On patched code the
    * parser skips malformed entries and returns either NULL (init
    * failed) or an empty list (iterate skipped every entry) --
    * either is correct.  Reaching this line is the pass signal. */
   list = file_archive_get_file_list(tmp_path, NULL);
   remove(tmp_path);
   if (list)
      string_list_free(list);
   printf("[SUCCESS] %-40s parsed without memory error\n", label);
   return true;
}

/* ================================================================== */
/* Case A: central-directory entry truncated before its 46-byte
 * fixed header is complete.                                          */
/* ================================================================== */
static void test_truncated_entry(void)
{
   uint8_t  buf[128];
   size_t   len   = 0;
   uint8_t *cdir  = NULL;
   uint8_t *eocd  = NULL;
   size_t   cdir_len;

   memset(buf, 0, sizeof(buf));

   /* Central directory starts at offset 0.  We write only 40 bytes
    * (less than the 46 needed), starting with a valid CFH signature
    * so the parser thinks it has found an entry. */
   cdir = buf;
   put_u32(cdir + 0, CFH_SIG);      /* valid signature               */
   cdir_len = 40;                    /* DELIBERATELY short (< 46)    */
   len = cdir_len;

   /* EOCD immediately after.  Directory size matches the short
    * region so the offset + size == archive_size check passes. */
   eocd = buf + len;
   len += write_eocd(eocd, 1, (uint32_t)cdir_len, 0);

   parse_malformed_zip("truncated central-dir entry",
         buf, len);
}

/* ================================================================== */
/* Case B: central-directory entry declares namelength larger than
 * the remaining directory bytes.                                     */
/* ================================================================== */
static void test_oversize_namelength(void)
{
   uint8_t  buf[128];
   size_t   len = 0;
   uint8_t *cdir = buf;
   uint8_t *eocd;
   size_t   cdir_len;

   memset(buf, 0, sizeof(buf));

   /* Full 46-byte fixed header with valid signature... */
   put_u32(cdir + 0, CFH_SIG);
   /* ...and a namelength bigger than the bytes actually remaining
    * in the directory.  memcpy(filename, entry+46, namelength) used
    * to read past the directory allocation. */
   put_u16(cdir + 28, 1000);        /* namelength = 1000             */
   put_u16(cdir + 30, 0);           /* extralength                   */
   put_u16(cdir + 32, 0);           /* commentlength                 */
   /* No room for 1000 bytes of name; directory ends at 46. */
   cdir_len = 46;
   len = cdir_len;

   eocd = buf + len;
   len += write_eocd(eocd, 1, (uint32_t)cdir_len, 0);

   parse_malformed_zip("entry declares oversized namelength",
         buf, len);
}

/* ================================================================== */
/* Case C: central-directory entry with a zero-length filename and
 * a valid local file header, used by the decompress callback path.
 * This specifically tests zip_file_decompressed()'s strlen-1 guard.
 * We only exercise it through file_archive_get_file_list, so the
 * critical check is that parsing doesn't crash.                      */
/* ================================================================== */
static void test_empty_filename(void)
{
   /* Layout:
    *   [0]   local file header (30 bytes, empty name)
    *   [30]  central directory (46 bytes, empty name, points at [0])
    *   [76]  EOCD (22 bytes)
    */
   uint8_t  buf[128];
   size_t   lfh_len, cdir_len;
   uint8_t *lfh  = buf;
   uint8_t *cdir;
   uint8_t *eocd;
   size_t   len = 0;

   memset(buf, 0, sizeof(buf));

   lfh_len = write_minimal_lfh(lfh, "");
   len = lfh_len;

   /* Central directory entry pointing at the LFH. */
   cdir = buf + len;
   put_u32(cdir + 0,  CFH_SIG);
   put_u16(cdir + 4,  20);    /* version made by                    */
   put_u16(cdir + 6,  20);    /* version needed                     */
   put_u16(cdir + 10, 0);     /* method = STORED                    */
   put_u32(cdir + 16, 0);     /* crc                                */
   put_u32(cdir + 20, 0);     /* compressed size                    */
   put_u32(cdir + 24, 0);     /* uncompressed size                  */
   put_u16(cdir + 28, 0);     /* namelength = 0 -- the bug trigger  */
   put_u16(cdir + 30, 0);     /* extralength                        */
   put_u16(cdir + 32, 0);     /* commentlength                      */
   put_u32(cdir + 42, 0);     /* LFH offset                         */
   cdir_len = 46;
   len += cdir_len;

   eocd = buf + len;
   len += write_eocd(eocd, 1, (uint32_t)cdir_len, (uint32_t)lfh_len);

   /* Patched behaviour: zip_file_decompressed sees name_len==0 and
    * returns 1 without touching name[strlen-1].  Parsing completes
    * and the list may be empty or one-entry -- we accept either,
    * as long as we don't crash. */
   {
      const char         *tmp_path = "rarch_zip_regression_test.zip";
      struct string_list *list;
      write_file(tmp_path, buf, len);
      list = file_archive_get_file_list(tmp_path, NULL);
      remove(tmp_path);
      /* NULL *or* list are both acceptable; the key point is we
       * reached this line without a SIGSEGV / ASan abort. */
      if (list)
         string_list_free(list);
      printf("[SUCCESS] empty-filename entry parsed without OOB read\n");
   }
}

/* ================================================================== */
/* Case D: directory_offset + directory_size > archive_size.
 * Each individual value passes the existing pair of comparisons
 * against archive_size, but their sum points past EOF -- this is the
 * combined-sanity-check that patch 7 added.                          */
/* ================================================================== */
static void test_combined_offset_size_overflow(void)
{
   uint8_t  buf[128];
   size_t   len = 0;
   uint8_t *eocd;

   memset(buf, 0, sizeof(buf));

   /* 22 bytes total archive.  Both offset and size individually are
    * <= 22, but their sum is 44.  Unpatched code would malloc 22
    * bytes, short-read, and fail later; patched code rejects up
    * front in zip_parse_file_init. */
   eocd = buf;
   len = write_eocd(eocd, 0,
         /* directory_size   */ 20,
         /* directory_offset */ 20);

   parse_malformed_zip("offset+size exceeds archive_size",
         buf, len);
}

/* ================================================================== */
/* Case E: directory_size near UINT32_MAX.                            */
/* ================================================================== */
static void test_directory_size_overflow(void)
{
   uint8_t  buf[128];
   size_t   len = 0;
   uint8_t *eocd;

   memset(buf, 0, sizeof(buf));

   /* A 4-GiB directory can't possibly fit in a 22-byte archive, so
    * this specific value is caught by the existing
    * "directory_size > archive_size" check -- but the alloc-
    * overflow guard in patch 7 catches the more subtle case where a
    * 32-bit host's size_t wraps.  On a 64-bit host with plenty of
    * memory, the existing check already rejects UINT32_MAX.  This
    * case is therefore best-effort: confirm that large values reject
    * cleanly on all platforms. */
   eocd = buf;
   len = write_eocd(eocd, 0,
         /* directory_size   */ 0xFFFFFFFFu,
         /* directory_offset */ 0);

   parse_malformed_zip("directory_size = UINT32_MAX",
         buf, len);
}

/* ================================================================== */
/* Case F: macOS AppleDouble metadata before the real content file.
 * The archive list must skip the metadata entry, and explicit member
 * reads must match "game.nes" exactly instead of substring-matching
 * "__MACOSX/._game.nes".                                             */
/* ================================================================== */
static void test_appledouble_exact_member_selection(void)
{
   uint8_t  zip_bytes[1024];
   uint8_t  source_bytes[16];
   int      failures_before           = failures;
   size_t   len                       = 0;
   void    *read_buf                  = NULL;
   int64_t  read_len                  = 0;
   int64_t  source_size               = 0;
   int64_t  source_read               = 0;
   char     archive_path[128];
   const char *tmp_path               = "rarch_zip_regression_test.zip";
   const char *member_path            = "rarch_zip_regression_test.zip#game.nes";
   const char *rom_name               = "game.nes";
   const char *rom_payload            = "ROMDATA";
   struct string_list *list           = NULL;
   file_archive_entry_source_t *src   = NULL;
   const struct file_archive_file_backend *zip_backend =
         file_archive_get_zlib_file_backend();

   memset(zip_bytes, 0, sizeof(zip_bytes));
   memset(source_bytes, 0, sizeof(source_bytes));

   len = write_appledouble_collision_zip(zip_bytes);
   write_file(tmp_path, zip_bytes, len);

   if (!zip_backend)
   {
      printf("[FAIL] AppleDouble regression requires the ZIP backend\n");
      failures++;
   }
   else if (file_archive_get_file_backend(tmp_path) != zip_backend)
   {
      printf("[FAIL] AppleDouble regression did not select the ZIP backend\n");
      failures++;
   }

   list = file_archive_get_file_list(tmp_path, "nes");
   if (!list)
   {
      printf("[FAIL] AppleDouble archive produced no file list\n");
      failures++;
   }
   else if (list->size != 1
         || strcmp(list->elems[0].data, rom_name) != 0)
   {
      const char *selected = list->size ? list->elems[0].data : "<empty>";
      printf("[FAIL] AppleDouble archive selected \"%s\" instead of \"%s\"\n",
            selected, rom_name);
      failures++;
   }
   if (list)
      string_list_free(list);
   list = NULL;

   list = file_archive_get_file_list(tmp_path, NULL);
   if (!list)
   {
      printf("[FAIL] Unfiltered AppleDouble archive produced no file list\n");
      failures++;
   }
   else if (list->size != 1
         || strcmp(list->elems[0].data, rom_name) != 0)
   {
      const char *selected = list->size ? list->elems[0].data : "<empty>";
      printf("[FAIL] Unfiltered AppleDouble archive selected \"%s\" instead of \"%s\"\n",
            selected, rom_name);
      failures++;
   }
   if (list)
      string_list_free(list);
   list = NULL;

   if (!file_archive_compressed_read(member_path, &read_buf, NULL, &read_len)
         || read_len != (int64_t)strlen(rom_payload)
         || memcmp(read_buf, rom_payload, (size_t)read_len) != 0)
   {
      printf("[FAIL] compressed read did not select exact ZIP member\n");
      failures++;
   }

   snprintf(archive_path, sizeof(archive_path), "%s", member_path);
   src = file_archive_entry_source_open(archive_path, &source_size);
   if (!src || source_size != (int64_t)strlen(rom_payload))
   {
      printf("[FAIL] entry source did not open exact ZIP member\n");
      failures++;
   }
   else
   {
      source_read = file_archive_entry_source_read(src,
            source_bytes, sizeof(source_bytes));
      if (source_read != (int64_t)strlen(rom_payload)
            || memcmp(source_bytes, rom_payload, (size_t)source_read) != 0)
      {
         printf("[FAIL] entry source did not read exact ZIP member\n");
         failures++;
      }
   }

   if (failures == failures_before)
      printf("[SUCCESS] AppleDouble metadata ignored during ZIP member selection\n");

   if (src)
      file_archive_entry_source_close(src);
   if (read_buf)
      free(read_buf);
   remove(tmp_path);
}

static void test_init_failure_cleanup(void)
{
   uint8_t buf[22];
   int failures_before = failures;
   bool returnerr = true;
   const char *tmp_path = "rarch_zip_regression_test.zip";
   file_archive_transfer_t state;
   struct archive_extract_userdata userdata;

   memset(buf, 0, sizeof(buf));
   write_eocd(buf, 0, 20, 20);
   write_file(tmp_path, buf, sizeof(buf));

   memset(&state, 0, sizeof(state));
   memset(&userdata, 0, sizeof(userdata));
   state.type        = ARCHIVE_TRANSFER_INIT;
   userdata.transfer = &state;

   if (file_archive_parse_file_iterate(&state, &returnerr,
            tmp_path, NULL, NULL, &userdata) != -1
         || returnerr
         || state.type != ARCHIVE_TRANSFER_DEINIT_ERROR
         || state.archive_file
         || state.context
         || userdata.transfer)
   {
      printf("[FAIL] archive init failure did not clean up immediately\n");
      failures++;
   }
#ifdef HAVE_MMAP
   if (state.archive_mmap_data || state.archive_mmap_fd != 0)
   {
      printf("[FAIL] archive init failure retained mmap ownership\n");
      failures++;
   }
#endif

   if (state.archive_file)
      file_archive_parse_file_iterate_stop(&state);

   memset(&state, 0, sizeof(state));
   state.type = ARCHIVE_TRANSFER_INIT;

   if (file_archive_parse_file_iterate(&state, NULL,
            tmp_path, NULL, NULL, NULL) != -1
         || state.archive_file
         || state.context)
   {
      printf("[FAIL] archive init cleanup requires an error-result pointer\n");
      failures++;
   }

   if (state.archive_file)
      file_archive_parse_file_iterate_stop(&state);

   remove(tmp_path);

   if (failures == failures_before)
      printf("[SUCCESS] archive init failure cleaned up immediately\n");
}

/* file_archive_get_file_crc32_and_size() used to loop with no way out.
 * Its iterate call is guarded on the transfer still being in ITERATE,
 * but nothing broke once it left that state, and the name tests then
 * re-read a current_file_path that could no longer change - so asking
 * for a member the archive does not hold spun at full CPU forever.
 *
 * An archive that fails to open produces an empty walk and lands in
 * exactly the same place, which is how it was found: a directory of
 * 7z files stopped a scan dead rather than being skipped.
 *
 * A regression here does not fail this test so much as hang it. That
 * is deliberate - there is no portable way to bound the call from
 * inside - and a stuck test is a visible failure either way. */
static void test_missing_member_terminates(void)
{
   const char *tmp_path = "rarch_zip_regression_test.zip";
   uint8_t     buf[512];
   size_t      len = 0;
   char        member[256];
   uint64_t    size = 12345;
   uint32_t    crc;

   /* One stored entry named "present.bin", built the same way as the
    * cases above. */
   len += write_minimal_lfh(buf + len, "present.bin");
   len += write_eocd(buf + len, 1, 0, (uint32_t)len);

   write_file(tmp_path, buf, len);

   snprintf(member, sizeof(member), "%s#absent.bin", tmp_path);
   crc = file_archive_get_file_crc32_and_size(member, &size);
   remove(tmp_path);

   if (crc != 0 || size != 0)
   {
      printf("FAIL  missing member reported crc=%08X size=%llu, "
             "expected nothing\n", crc, (unsigned long long)size);
      failures++;
   }
   else
      printf("ok    missing member returns nothing rather than hanging\n");
}

int main(void)
{
   test_truncated_entry();
   test_oversize_namelength();
   test_empty_filename();
   test_combined_offset_size_overflow();
   test_directory_size_overflow();
   test_appledouble_exact_member_selection();
   test_init_failure_cleanup();
   test_missing_member_terminates();

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll archive-zip regression tests passed.\n");
   return 0;
}
