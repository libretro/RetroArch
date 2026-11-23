/* local_to_utf8_string(): into another buffer and in place, through
 * the stack or, too long for it or for @s, the heap. Built twice: with
 * shim/windows.h as _WIN32, so the conversion runs, and as POSIX, where
 * it is a copy. Neither path may allocate when the result fits.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <encodings/utf.h>

static unsigned failures  = 0;
static unsigned callocs   = 0;
static int      fail_heap = 0;

void *__real_calloc(size_t n, size_t size);
void *__wrap_calloc(size_t n, size_t size)
{
   callocs++;
   if (fail_heap)
      return NULL;
   return __real_calloc(n, size);
}

#define CHECK(cond, what) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what); \
         failures++; \
      } \
   } while (0)

int main(void)
{
   char buf[1024];
   char out[1024];
#ifdef _WIN32
   char want[1024];
   size_t i;
#endif

   CHECK(!local_to_utf8_string("abc", NULL, 4), "NULL @s");
   CHECK(!local_to_utf8_string("abc", out, 0), "zero @len");

   strcpy(out, "x");
   CHECK(local_to_utf8_string("", out, sizeof(out)) && !*out, "empty");
   strcpy(out, "x");
   CHECK(local_to_utf8_string(NULL, out, sizeof(out)) && !*out, "NULL @in");

   callocs = 0;
   CHECK(local_to_utf8_string("abc", out, sizeof(out))
         && !strcmp(out, "abc"), "ASCII");
   strcpy(buf, "abc");
   CHECK(local_to_utf8_string(buf, buf, sizeof(buf))
         && !strcmp(buf, "abc"), "ASCII in place");
   CHECK(!callocs, "a result that fits allocated");
   strcpy(buf, "abcdef");
   CHECK(local_to_utf8_string(buf, buf, 4) && !strcmp(buf, "abc"),
         "ASCII in place, truncated");

#ifdef _WIN32
   /* x, U+3141, U+00E9 */
   callocs = 0;
   CHECK(local_to_utf8_string("x\x82\x41\xE9", out, sizeof(out))
         && !strcmp(out, "x\xE3\x85\x81\xC3\xA9"), "converted");
   strcpy(buf, "x\x82\x41\xE9");
   CHECK(local_to_utf8_string(buf, buf, sizeof(buf))
         && !strcmp(buf, "x\xE3\x85\x81\xC3\xA9"), "converted in place");
   strcpy(buf, "\x82\x41\x82\x41");
   CHECK(local_to_utf8_string(buf, buf, 7)
         && !strcmp(buf, "\xE3\x85\x81\xE3\x85\x81"),
         "in place, exactly filling @s");
   CHECK(!callocs, "a result that fits allocated");

   /* Longer than @s: truncated as strlcpy() would, byte for byte */
   strcpy(buf, "\x82\x41\x82\x41");
   CHECK(local_to_utf8_string(buf, buf, 6)
         && !strcmp(buf, "\xE3\x85\x81\xE3\x85"), "in place, truncated");
   strcpy(out, "\x82\x41\x82\x41");
   CHECK(local_to_utf8_string(out, buf, 4)
         && !strcmp(buf, "\xE3\x85\x81"), "truncated");

   /* Longer than the stack buffer: 300 characters */
   for (i = 0; i < 300; i++)
   {
      buf[i * 2]     = '\x82';
      buf[i * 2 + 1] = '\x41';
      memcpy(want + i * 3, "\xE3\x85\x81", 3);
   }
   buf[600]  = '\0';
   want[900] = '\0';
   strcpy(out, buf);
   CHECK(local_to_utf8_string(out, out, sizeof(out))
         && !strcmp(out, want), "long, in place");
   CHECK(local_to_utf8_string(buf, out, sizeof(out))
         && !strcmp(out, want), "long");

   /* A failure leaves @s as it was in place, empty otherwise */
   fail_heap = 1;
   strcpy(out, buf);
   CHECK(!local_to_utf8_string(out, out, sizeof(out))
         && !strcmp(out, buf), "failed in place");
   strcpy(out, "x");
   CHECK(!local_to_utf8_string(buf, out, sizeof(out)) && !*out, "failed");
   fail_heap = 0;
#endif

   if (failures)
   {
      fprintf(stderr, "FAIL local_utf8_test: %u failures\n", failures);
      return 1;
   }
   #ifdef _WIN32
   printf("PASS local_utf8_test (Win32 path)\n");
#else
   printf("PASS local_utf8_test (copy path)\n");
#endif
   return 0;
}
