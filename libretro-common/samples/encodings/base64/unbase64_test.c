/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (unbase64_test.c).
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

/* Regression tests for unbase64.
 *
 * Fixed in commit 87f2d0b: malformed base64 lengths used to cause a
 * 1-byte heap-buffer-overflow.  Under AddressSanitizer, feeding "AB="
 * to the unpatched version produces:
 *   ERROR: AddressSanitizer: heap-buffer-overflow
 *   WRITE of size 1 at 0 bytes after 1-byte region
 *
 * Build/run with ASan for full coverage:
 *   make CFLAGS='-fsanitize=address -g -O0' LDFLAGS='-fsanitize=address'
 *   ./unbase64_test
 *
 * Exits 0 on success, aborts on failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/base64.h>

static int failures = 0;

static void expect_reject(const char *ascii, int len)
{
   int flen                  = -1;
   unsigned char *out        = unbase64(ascii, len, &flen);
   if (out || flen != 0)
   {
      printf("[FAILED] expected reject for len=%d input \"%s\", got out=%p flen=%d\n",
            len, ascii ? ascii : "(null)", (void*)out, flen);
      free(out);
      failures++;
      return;
   }
   printf("[SUCCESS] rejected malformed input (len=%d) \"%s\"\n",
         len, ascii ? ascii : "(null)");
}

static void expect_decode(const char *ascii, int len,
      const char *want, int want_len)
{
   int flen                  = 0;
   unsigned char *out        = unbase64(ascii, len, &flen);
   if (!out)
   {
      printf("[FAILED] expected decode for \"%s\", got NULL\n", ascii);
      failures++;
      return;
   }
   if (flen != want_len || memcmp(out, want, want_len) != 0)
   {
      printf("[FAILED] decode \"%s\": got flen=%d want=%d\n",
            ascii, flen, want_len);
      free(out);
      failures++;
      return;
   }
   printf("[SUCCESS] decoded \"%s\" -> %d bytes\n", ascii, flen);
   free(out);
}

int main(void)
{
   /* --- Malformed inputs that used to overflow or misbehave --- */
   expect_reject("AB=", 3);            /* canonical repro: 1-byte OOB write */
   expect_reject("A==", 3);            /* another 3-char pad case          */
   expect_reject("A",   1);            /* below old 2-char floor           */
   expect_reject("",    0);            /* empty                            */
   expect_reject("ABC", 3);            /* len not multiple of 4            */
   expect_reject("ABCDE", 5);          /* len not multiple of 4            */
   expect_reject("ABCDEFG", 7);        /* len not multiple of 4            */

   /* --- Valid inputs must still decode --- */
   expect_decode("QUJD",     4, "ABC",    3);   /* no padding */
   expect_decode("SGVsbG8h", 8, "Hello!", 6);   /* no padding */
   expect_decode("SGVsbG8=", 8, "Hello",  5);   /* 1 pad      */
   expect_decode("SGk=",     4, "Hi",     2);   /* 1 pad      */
   expect_decode("QQ==",     4, "A",      1);   /* 2 pad      */
   expect_decode("QUI=",     4, "AB",     2);   /* 2 pad      */

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll unbase64 regression tests passed.\n");
   return 0;
}
