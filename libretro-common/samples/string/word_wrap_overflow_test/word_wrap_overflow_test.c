/* Regression test for return-value-exceeds-bytes-written in
 * word_wrap_wideglyph (libretro-common/string/stdstring.c).
 *
 * Pre-patch, word_wrap_wideglyph had three classes of return path
 * that propagated strlcpy()'s return value directly, e.g.:
 *
 *   if (src_end - src < line_width)
 *      return strlcpy(s, src, len);
 *
 *   ...
 *
 *   return (size_t)(s - s_start) + strlcpy(s, src, remaining);
 *
 * strlcpy() returns strlen(src), not bytes-actually-written, so on
 * truncation (destination smaller than source) the returned value
 * exceeds the number of bytes written into the destination buffer.
 *
 * The sister function word_wrap() has a `len < src_len + 1` guard
 * up front that bails to 0 when the buffer is too small, sidestepping
 * the issue.  word_wrap_wideglyph has no such guard and tries to fit
 * what it can -- which is correct behaviour, but the inflated return
 * value violates the implicit contract every caller in the tree
 * depends on.
 *
 * Concrete impact: three menu drivers (xmb, ozone, materialui) feed
 * this return value as the `n` argument to memchr() over the wrapped
 * destination buffer, looking for newline boundaries when laying out
 * messageboxes.  Pre-patch, an inflated `n` walks memchr past the
 * buffer end into adjacent stack memory.  A '\n' (0x0A) byte found
 * there yields a wild pointer used in pointer arithmetic and as a
 * length argument to font_driver_get_message_width(), leading to
 * stack info disclosure or a crash.  Reachable in CJK locales
 * (which select word_wrap_wideglyph via msg_hash_get_wideglyph_str)
 * via any messagebox payload that exceeds MENU_LABEL_MAX_LENGTH
 * (default 256 bytes) -- error notifications, file paths, network
 * handshake text, search results.
 *
 * Post-patch, every return path computes bytes-actually-written
 * from strlcpy's contract:
 *
 *   copied = strlcpy(s, src, n);
 *   if (copied >= n) copied = (n > 0) ? n - 1 : 0;
 *   return ... + copied;
 *
 * so the returned value is always a valid offset into the
 * destination buffer.  This test asserts that invariant directly
 * and additionally exercises the call shape used by the menu
 * drivers (memchr over the destination using the returned length),
 * so ASan flags pre-patch builds with heap-buffer-overflow.
 *
 * The test uses heap allocation (not a stack buffer) so ASan's
 * red-zone instrumentation gives a sharp signal when the bug is
 * present.  Without ASan, the test still detects the bug on its
 * own via the return-value-exceeds-buffer-size assertion.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>
#include <encodings/utf.h>

static int failures = 0;
static size_t glyph_scans;

const char *word_wrap_test_utf8skip(const char *str, size_t chars)
{
   glyph_scans++;
   return utf8skip(str, chars);
}

/* A long ASCII source with no embedded newlines or wide glyphs.
 * "ASCII through wideglyph" is the worst-case for this function:
 * line 358's early-return branch fires only when the entire input
 * is shorter than line_width, so we must use input >= line_width
 * to drive flow through the main loop and the late-return paths
 * at lines 384 / 420 / 438.  The rewinds happen at every space.
 */
static const char *long_src =
   "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam "
   "nec enim quis orci euismod efficitur at nec arcu. Vivamus "
   "imperdiet est feugiat massa rhoncus porttitor at vitae ante. "
   "Nunc a orci vel ipsum tempor posuere sed a lacus. Ut erat "
   "odio, ultrices vitae iaculis fringilla, iaculis ut eros. Sed "
   "facilisis viverra lectus et ullamcorper. Aenean risus ex, "
   "ornare eget scelerisque ac, imperdiet eu ipsum. Morbi "
   "pellentesque erat metus, sit amet aliquet libero rutrum et. "
   "Integer non ullamcorper tellus.";

static void check_return_value(const char *case_name,
      char *dst, size_t dst_size, size_t returned,
      const char *src)
{
   /* Returned length must NEVER exceed bytes-actually-written.
    * Bytes written is at most dst_size - 1 (room for the NUL). */
   if (returned >= dst_size)
   {
      printf("[ERROR] %s: word_wrap_wideglyph returned %zu, "
            "destination buffer is only %zu bytes (cannot exceed "
            "%zu bytes-written)\n",
            case_name, returned, dst_size,
            dst_size > 0 ? dst_size - 1 : 0);
      failures++;
      return;
   }

   /* Returned length must equal strlen of the destination -- this
    * is the contract menu-driver callers depend on. */
   {
      size_t actual = strlen(dst);
      if (returned != actual)
      {
         printf("[ERROR] %s: word_wrap_wideglyph returned %zu but "
               "strlen(dst) is %zu\n",
               case_name, returned, actual);
         failures++;
         return;
      }
   }

   /* Mimic the ozone/xmb/materialui messagebox call shape: use the
    * returned value as the `n` argument to memchr() over the
    * destination buffer.  Pre-patch, an inflated returned value
    * walks memchr past the buffer.  ASan flags this as a heap-
    * buffer-overflow.  Post-patch, the read stays within dst. */
   {
      const char *nl = (const char *)memchr(dst, '\n', returned);
      (void)nl; /* presence/absence not asserted; the read itself
                 * is the test (ASan is the discriminator) */
   }

   /* (void) src to suppress unused-parameter when not in debug. */
   (void)src;

   printf("[SUCCESS] %s: returned=%zu, strlen(dst)=%zu, dst_size=%zu\n",
         case_name, returned, strlen(dst), dst_size);
}

/* Case 1: destination smaller than line_width.
 * Pre-patch this hits the `src_end - src < line_width` branch?
 * No -- src_len > line_width, so we reach the main loop.  The
 * rewinds at lastspace cause early returns at lines 384/420/438
 * via strlcpy with a too-small `remaining`. */
static void test_truncating_normal_case(void)
{
   const size_t dst_size = 64;
   char *dst = (char *)malloc(dst_size);
   size_t returned;

   if (!dst) { printf("[FATAL] OOM\n"); failures++; return; }

   memset(dst, 0xCC, dst_size);   /* poison so strlen catches non-NUL-termination */
   dst[dst_size - 1] = '\0';      /* ensure strlen() terminates if NUL is missing */

   returned = word_wrap_wideglyph(dst, dst_size, long_src,
         strlen(long_src),
         /* line_width */ 40,
         /* wideglyph_width */ 100,
         /* max_lines */ 0);

   check_return_value("truncating_normal", dst, dst_size, returned, long_src);
   free(dst);
}

/* Case 2: very small destination (smaller than even one line
 * worth of source).  Pre-patch this still hits the buggy path
 * because the loop's char_len-vs-len check at line 367 stopped
 * the loop, leaving s_start..s short, but the late strlcpy
 * (whichever fired first) still returned a too-large value. */
static void test_truncating_tiny_dest(void)
{
   const size_t dst_size = 16;
   char *dst = (char *)malloc(dst_size);
   size_t returned;

   if (!dst) { printf("[FATAL] OOM\n"); failures++; return; }

   memset(dst, 0xCC, dst_size);
   dst[dst_size - 1] = '\0';

   returned = word_wrap_wideglyph(dst, dst_size, long_src,
         strlen(long_src), 40, 100, 0);

   check_return_value("truncating_tiny_dest", dst, dst_size,
         returned, long_src);
   free(dst);
}

/* Case 3: destination is exactly large enough for the whole
 * wrapped output.  Tests that the fix doesn't regress the
 * non-truncating case -- return value should equal strlen(dst). */
static void test_fits(void)
{
   const size_t dst_size = 1024;
   char *dst = (char *)malloc(dst_size);
   size_t returned;

   if (!dst) { printf("[FATAL] OOM\n"); failures++; return; }

   memset(dst, 0xCC, dst_size);
   dst[dst_size - 1] = '\0';

   returned = word_wrap_wideglyph(dst, dst_size, long_src,
         strlen(long_src), 40, 100, 0);

   check_return_value("fits", dst, dst_size, returned, long_src);
   free(dst);
}

/* Case 4: input shorter than line_width -- exercises the early
 * return at line 358 (the simplest of the buggy paths).  Pre-
 * patch, this returns strlen(short_src), which is fine when the
 * destination is large enough.  This case verifies post-patch
 * doesn't regress the easy path. */
static void test_short_input_large_dest(void)
{
   const char *short_src = "Hello, world.";
   const size_t dst_size = 64;
   char *dst = (char *)malloc(dst_size);
   size_t returned;

   if (!dst) { printf("[FATAL] OOM\n"); failures++; return; }

   memset(dst, 0xCC, dst_size);
   dst[dst_size - 1] = '\0';

   returned = word_wrap_wideglyph(dst, dst_size, short_src,
         strlen(short_src), 40, 100, 0);

   check_return_value("short_input_large_dest", dst, dst_size,
         returned, short_src);
   free(dst);
}

/* Case 5: input shorter than line_width AND destination too small.
 * This is the line-358 truncation case: pre-patch returns
 * strlen(src) which exceeds dst_size; post-patch clamps to dst_size-1. */
static void test_short_input_tiny_dest(void)
{
   const char *src = "Hello, world! This is a moderately long string.";
   const size_t dst_size = 16; /* smaller than src */
   char *dst = (char *)malloc(dst_size);
   size_t returned;

   if (!dst) { printf("[FATAL] OOM\n"); failures++; return; }

   memset(dst, 0xCC, dst_size);
   dst[dst_size - 1] = '\0';

   /* line_width chosen larger than src_len so the function takes
    * the line-358 early-return path. */
   returned = word_wrap_wideglyph(dst, dst_size, src,
         strlen(src),
         /* line_width */ 100, /* > strlen(src) */
         100, 0);

   check_return_value("short_input_tiny_dest", dst, dst_size,
         returned, src);
   free(dst);
}

static void test_regular_wrap_copy(void)
{
   static const struct {
      const char *src;
      const char *expected;
      int width;
      unsigned max_lines;
   } cases[] = {
      { "alpha beta gamma delta", "alpha\nbeta\ngamma\ndelta", 8, 0 },
      { "caf\xc3\xa9 d\xc3\xa9j\xc3\xa0 vu encore",
        "caf\xc3\xa9\nd\xc3\xa9j\xc3\xa0 vu\nencore", 8, 0 },
      { "a b\ncccc dddd eeee", "a b\ncccc\ndddd\neeee", 6, 0 },
      { "  leading  spaces and trailing  ",
        " \nleading  spaces and trailing  ", 8, 2 },
      { "one\ntwo\nthree\n", "one\ntwo\nthree\n", 4, 0 },
      { "abcdefghij", "abcdefghij", 4, 0 },
      { "", "", 4, 0 }
   };
   size_t i;
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      size_t len = strlen(cases[i].src);
      size_t returned;
      char *dst = (char*)malloc(len + 1);
      if (!dst)
      {
         failures++;
         continue;
      }
      memset(dst, 0xcc, len + 1);
      returned = word_wrap(dst, len + 1, cases[i].src, len,
            cases[i].width, 100, cases[i].max_lines);
      if (returned != strlen(cases[i].expected)
            || strcmp(dst, cases[i].expected) != 0)
      {
         printf("[ERROR] regular wrap case %zu changed output or length\n", i);
         failures++;
      }
      free(dst);
   }
   {
      char small[4] = {'X', 'X', 'X', 'X'};
      if (word_wrap(small, sizeof(small), "too long", 8, 4, 100, 0) != 0
            || memcmp(small, "XXXX", sizeof(small)) != 0)
      {
         printf("[ERROR] regular wrap changed an undersized destination\n");
         failures++;
      }
   }
}

static void test_forward_glyph_scan(void)
{
   static const char *inputs[] = {
      "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda",
      "a abcdefghijklmnopqrstuvwxyz a abcdefghijklmnopqrstuvwxyz",
      "caf\xc3\xa9 d\xc3\xa9j\xc3\xa0 vu encore caf\xc3\xa9 d\xc3\xa9j\xc3\xa0 vu encore"
   };
   size_t i;
   unsigned width;
   for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++)
      for (width = 4; width <= 32; width *= 2)
      {
         size_t len = strlen(inputs[i]);
         size_t returned;
         char *dst = (char*)malloc(len + 1);
         if (!dst)
         {
            failures++;
            continue;
         }
         glyph_scans = 0;
         returned = word_wrap(dst, len + 1, inputs[i], len, width, 100, 0);
         if (returned != len || glyph_scans > utf8len(inputs[i]))
         {
            printf("[ERROR] regular wrap rescanned glyphs at width %u\n", width);
            failures++;
         }
         free(dst);
      }
}

static void test_wide_word_tail_scan(void)
{
   const char *src = "a \xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9 a "
      "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9 a "
      "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9";
   char dst[128];
   size_t returned;
   glyph_scans = 0;
   returned = word_wrap_wideglyph(dst, sizeof(dst), src, strlen(src), 4, 200, 0);
   if (returned != strlen(src) || strcmp(dst,
            "a\n\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9 a\n"
            "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9 a\n"
            "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9") != 0
         || glyph_scans != 15)
   {
      printf("[ERROR] wide wrap tail: scans=%zu length=%zu output=%s\n", glyph_scans, returned, dst);
      failures++;
   }
}

static void test_ascii_glyph_scan(void)
{
   const char *src = "alpha beta gamma delta epsilon zeta eta theta";
   char dst[128];
   unsigned wide;
   for (wide = 0; wide < 2; wide++)
   {
      size_t returned;
      glyph_scans = 0;
      if (wide)
         returned = word_wrap_wideglyph(dst, sizeof(dst), src, strlen(src), 8, 200, 0);
      else
         returned = word_wrap(dst, sizeof(dst), src, strlen(src), 8, 100, 0);
      if (returned != strlen(src) || strcmp(dst,
               "alpha\nbeta\ngamma\ndelta\nepsilon\nzeta\neta\ntheta") != 0
            || glyph_scans != 0)
      {
         printf("[ERROR] ASCII wrap used UTF-8 decoding (wide=%u)\n", wide);
         failures++;
      }
   }
}

static void test_wide_glyph_scan(void)
{
   const char *src = "\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad"
      "\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad"
      "\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad";
   char dst[128];
   unsigned width;
   for (width = 4; width <= 16; width *= 2)
   {
      size_t returned;
      glyph_scans = 0;
      returned = word_wrap_wideglyph(dst, sizeof(dst), src, strlen(src), width, 200, 0);
      if (returned != strlen(dst) || !strchr(dst, '\n')
            || !glyph_scans || glyph_scans > utf8len(src))
      {
         printf("[ERROR] wide wrap rescanned glyphs at width %u\n", width);
         failures++;
      }
   }
}
static void test_bounded_utf8_copy(void)
{
   const char *src = "abc\xe4\xb8\xad\xf0\x9f\x98\x80\xc3\xa9xyz";
   static const size_t lengths[] = {0, 0, 1, 2, 3, 3, 3, 6, 6,
      6, 6, 10, 10, 12, 13, 14, 15, 15};
   size_t cap;
   for (cap = 0; cap < sizeof(lengths) / sizeof(lengths[0]); cap++)
   {
      unsigned char dst[32];
      size_t i, returned;
      memset(dst, 0xcc, sizeof(dst));
      returned = utf8cpy((char*)dst, cap, src, 1000);
      if (returned != lengths[cap] || memcmp(dst, src, returned)
            || (cap && dst[returned] != 0))
      {
         printf("[ERROR] UTF-8 copy split a glyph at capacity %zu\n", cap);
         failures++;
      }
      for (i = cap; i < sizeof(dst); i++)
         if (dst[i] != 0xcc)
         {
            printf("[ERROR] UTF-8 copy exceeded capacity %zu\n", cap);
            failures++;
            break;
         }
   }
}

int main(void)
{
   test_bounded_utf8_copy();
   test_wide_word_tail_scan();
   test_ascii_glyph_scan();
   test_wide_glyph_scan();
   test_forward_glyph_scan();
   test_regular_wrap_copy();
   test_truncating_normal_case();
   test_truncating_tiny_dest();
   test_fits();
   test_short_input_large_dest();
   test_short_input_tiny_dest();

   if (failures)
   {
      printf("\n%d word_wrap_wideglyph regression test(s) failed\n",
            failures);
      return 1;
   }
   printf("\nAll word wrapping regression tests passed.\n");
   return 0;
}
