/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rzip_matches_buf_test.c).
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

/* Regression test for rzipstream_matches_buf().
 *
 * The SRAM dirty-check asks "does the file on disk already hold what
 * is in memory?", and answered it by decompressing the whole save
 * into a fresh allocation, comparing, and freeing - checking the size
 * only afterwards, so the one case where the answer is free still
 * paid a full decompress.  rzipstream_matches_buf() settles the size
 * from the header first, decompresses a chunk at a time, and stops at
 * the first difference.
 *
 * What makes this worth its own test rather than sharing
 * filestream_matches_buf's: rzip reads are decompressions, so the
 * chunk loop is re-entering an inflate stream rather than walking a
 * buffer, and its boundaries do not line up with the compressor's
 * chunking.  A difference sitting on a read boundary, an rzip chunk
 * boundary, or between the two is where an off-by-one lives, and
 * those are three different offsets.
 *
 * rzip is also transparent about uncompressed input, so both file
 * shapes go through the same entry point and both are checked here -
 * a save written before compression was enabled must still compare
 * correctly.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./rzip_matches_buf_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <streams/rzip_stream.h>
#include <streams/file_stream.h>

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

/* Compressible enough that rzip actually compresses, varied enough
 * that a misaligned compare cannot pass by accident. */
static void fill_body(uint8_t *body, size_t len)
{
   size_t i;
   for (i = 0; i < len; i++)
      body[i] = (uint8_t)((i / 61) ^ (i & 0x3f));
}

static bool write_rzip(const char *path, const uint8_t *body, size_t len)
{
   rzipstream_t *s = rzipstream_open(path, RETRO_VFS_FILE_ACCESS_WRITE);
   bool ok;
   if (!s)
      return false;
   ok = (rzipstream_write(s, body, (int64_t)len) == (int64_t)len);
   rzipstream_close(s);
   return ok;
}

static void write_plain(const char *path, const uint8_t *body, size_t len)
{
   FILE *f = fopen(path, "wb");
   if (len)
      fwrite(body, 1, len, f);
   fclose(f);
}

/* Flip one byte, assert the mismatch is seen, flip it back. */
static void expect_diff_at(const char *path, uint8_t *probe, size_t len,
      size_t at, const char *msg)
{
   if (at >= len)
      return;
   probe[at] ^= 0xff;
   CHECK(!rzipstream_matches_buf(path, probe, len), msg);
   probe[at] ^= 0xff;
}

int main(void)
{
   /* Spans several 64 KiB read chunks and does not end on one. */
   size_t   len   = 300 * 1024 + 77;
   uint8_t *body  = (uint8_t*)malloc(len);
   uint8_t *probe = (uint8_t*)malloc(len);
   int      pass;

   fill_body(body, len);

   if (!write_rzip("rzip_match.rzip", body, len))
   {
      printf("FAIL: could not write the compressed fixture\n");
      return 1;
   }
   write_plain("rzip_match.plain", body, len);
   write_plain("rzip_match.empty", body, 0);

   /* Both file shapes through the same entry point. */
   for (pass = 0; pass < 2; pass++)
   {
      const char *path = pass ? "rzip_match.plain" : "rzip_match.rzip";

      printf("-- %s input --\n", pass ? "uncompressed" : "compressed");
      memcpy(probe, body, len);

      CHECK(rzipstream_matches_buf(path, probe, len),
            "identical content matches");

      expect_diff_at(path, probe, len, 0,
            "difference at the first byte is caught");
      expect_diff_at(path, probe, len, len / 2,
            "difference mid-file is caught");
      expect_diff_at(path, probe, len, len - 1,
            "difference at the last byte is caught");

      /* Read-chunk boundaries, taken from the header rather than
       * repeated here: the chunk has already been resized once, and a
       * boundary case aimed at the old value is just another mid-file
       * check wearing a boundary's name. */
      expect_diff_at(path, probe, len, RZIPSTREAM_MATCHES_BUF_CHUNK - 1,
            "difference at a read-chunk boundary is caught");
      expect_diff_at(path, probe, len, RZIPSTREAM_MATCHES_BUF_CHUNK,
            "difference just past a read-chunk boundary is caught");
      expect_diff_at(path, probe, len, RZIPSTREAM_MATCHES_BUF_CHUNK + 1,
            "difference just after a read-chunk boundary is caught");
      expect_diff_at(path, probe, len, 8 * RZIPSTREAM_MATCHES_BUF_CHUNK,
            "difference at a later read-chunk boundary is caught");

      /* rzip's own chunking is independent of the read size, so a
       * difference landing on one is a separate offset to check. */
      expect_diff_at(path, probe, len, 32 * 1024,
            "difference at an rzip chunk boundary is caught");
      expect_diff_at(path, probe, len, 32 * 1024 - 1,
            "difference just before an rzip chunk boundary is caught");

      /* Size disagreements resolve from the header, and a prefix
       * must not be reported as a match. */
      CHECK(!rzipstream_matches_buf(path, probe, len - 1),
            "shorter length does not match");
      CHECK(!rzipstream_matches_buf(path, probe, len + 1),
            "longer length does not match");

      CHECK(!rzipstream_matches_buf(path, probe, 0),
            "zero length does not match a non-empty file");
   }

   printf("-- edge cases --\n");
   CHECK(!rzipstream_matches_buf("rzip_match.missing", probe, len),
         "missing file does not match");
   CHECK(!rzipstream_matches_buf(NULL, probe, len),
         "NULL path answers false rather than faulting");
   /* Deliberately the opposite of filestream_matches_buf, which
    * matches an empty file against zero bytes.  An empty file has no
    * rzip header, so rzipstream_open() cannot open it at all and
    * there is nothing to compare - and false is the answer that
    * serves the caller, since "write it" replaces a zero-byte file
    * with a valid one.  Asserted so the asymmetry stays deliberate
    * rather than becoming a surprise for anyone switching a call site
    * between the two. */
   CHECK(!rzipstream_matches_buf("rzip_match.empty", NULL, 0),
         "empty file does not match zero bytes (no rzip header to read)");
   CHECK(!rzipstream_matches_buf("rzip_match.empty", probe, len),
         "empty file does not match a non-empty buffer");

   free(body);
   free(probe);
   remove("rzip_match.rzip");
   remove("rzip_match.plain");
   remove("rzip_match.empty");

   if (test_fails)
   {
      printf("== %d FAILURES ==\n", test_fails);
      return 1;
   }
   printf("== rzip_matches_buf_test: all tests pass ==\n");
   return 0;
}
