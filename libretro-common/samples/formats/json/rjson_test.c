/* Regression tests for libretro-common/formats/json/rjson.c
 *
 * Exercises the parser and writer on inputs that previously
 * triggered the following bugs (all fixed in the adjoining
 * patch):
 *
 *  1. _rjson_grow_string: size_t doubling overflow on ~2 GiB
 *     inputs on 32-bit.  Not reproducible in a CI sample, so
 *     the cap is validated indirectly via a happy-path parse
 *     that repeatedly grows string_cap and verifies the output
 *     is intact.
 *
 *  2. _rjsonwriter_memory_io: int overflow on buf_num + len + 512
 *     when the writer's in-memory buffer approaches 2 GiB.  Also
 *     not reproducible in CI; the rewritten arithmetic is
 *     exercised by a happy-path write/read-back.
 *
 *  3. rjsonwriter_raw: did not guard a negative len parameter.
 *     A caller passing a negative len could pre-patch advance
 *     buf_num past buf_cap through the signed-overflow path.
 *     This is directly testable and IS a true discriminator.
 *
 *  4. rjson_open_user: no lower bound on io_block_size.  A very
 *     small (or negative) block size underallocated input_buf
 *     and the first read overran it.  Directly testable.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rjson.h>
#include <formats/rjson_helpers.h>

static int failures = 0;

/* ------------------------------------------------------------------ */
/* Parser smoke tests                                                 */
/* ------------------------------------------------------------------ */

struct count_ctx { int strings; int numbers; int objects; int arrays; };

static bool cb_string(void *ctx, const char *s, size_t n)
{ (void)s; (void)n; ((struct count_ctx*)ctx)->strings++; return true; }
static bool cb_number(void *ctx, const char *s, size_t n)
{ (void)s; (void)n; ((struct count_ctx*)ctx)->numbers++; return true; }
static bool cb_start_obj(void *ctx)
{ ((struct count_ctx*)ctx)->objects++; return true; }
static bool cb_start_arr(void *ctx)
{ ((struct count_ctx*)ctx)->arrays++; return true; }
static bool cb_ignore_default(void *ctx) { (void)ctx; return true; }
static bool cb_ignore_bool(void *ctx, bool v) { (void)ctx; (void)v; return true; }

static void test_parser_happy_path(void)
{
   const char *json = "{\"name\":\"libretro\",\"version\":42,\"tags\":[\"a\",\"b\",\"c\"],\"nested\":{\"x\":1}}";
   struct count_ctx ctx;
   bool rc;

   memset(&ctx, 0, sizeof(ctx));
   rc = rjson_parse_quick(json, strlen(json), &ctx, 0,
         cb_string, cb_string, cb_number,
         cb_start_obj, cb_ignore_default,
         cb_start_arr, cb_ignore_default,
         cb_ignore_bool, cb_ignore_default, NULL);

   if (!rc)
   {
      printf("[ERROR] happy path: parse failed\n");
      failures++;
      return;
   }
   /* "name", "libretro", "version", "tags", "a", "b", "c", "nested", "x"
    * => member-or-value strings = 9 */
   if (ctx.strings != 9)
   {
      printf("[ERROR] happy path: strings=%d, want 9\n", ctx.strings);
      failures++;
      return;
   }
   if (ctx.numbers != 2)
   {
      printf("[ERROR] happy path: numbers=%d, want 2\n", ctx.numbers);
      failures++;
      return;
   }
   if (ctx.objects != 2 || ctx.arrays != 1)
   {
      printf("[ERROR] happy path: objects=%d arrays=%d\n",
             ctx.objects, ctx.arrays);
      failures++;
      return;
   }
   printf("[SUCCESS] parser happy path\n");
}

static void test_parser_long_string(void)
{
   /* Forces multiple _rjson_grow_string growths: start from the
    * inline 512-byte string and grow to > 8 KiB.  Post-patch the
    * cap logic must not interfere with legitimate growth. */
   size_t N = 9000;
   size_t i;
   char *input;
   struct count_ctx ctx;
   bool rc;

   input = (char*)malloc(N + 16);
   if (!input) { printf("[ERROR] long string: OOM\n"); failures++; return; }

   input[0] = '"';
   for (i = 1; i <= N; ++i)
      input[i] = 'x';
   input[N + 1] = '"';
   input[N + 2] = '\0';

   memset(&ctx, 0, sizeof(ctx));
   rc = rjson_parse_quick(input, N + 2, &ctx, 0,
         cb_string, cb_string, cb_number,
         cb_start_obj, cb_ignore_default,
         cb_start_arr, cb_ignore_default,
         cb_ignore_bool, cb_ignore_default, NULL);
   free(input);

   if (!rc || ctx.strings != 1)
   {
      printf("[ERROR] long string: rc=%d strings=%d\n",
            (int)rc, ctx.strings);
      failures++;
      return;
   }
   printf("[SUCCESS] long-string parse (grew past inline buffer)\n");
}

static void test_parser_malformed(void)
{
   /* A handful of malformed inputs that must NOT crash. */
   static const char *bad[] = {
      "",
      "{",
      "[",
      "[,",
      "{\"x\":",
      "[1,2,",
      "\"unterminated",
      "\\u",
      "{\"\\uD800\":1}",  /* dangling high surrogate */
      "1e",
      "0.",
      "-",
      NULL
   };
   int i;
   for (i = 0; bad[i]; ++i)
   {
      struct count_ctx ctx;
      memset(&ctx, 0, sizeof(ctx));
      /* We don't care about rc; we care that this returns rather
       * than crashing.  Post-patch all error paths are clean. */
      (void)rjson_parse_quick(bad[i], strlen(bad[i]), &ctx, 0,
            cb_string, cb_string, cb_number,
            cb_start_obj, cb_ignore_default,
            cb_start_arr, cb_ignore_default,
            cb_ignore_bool, cb_ignore_default, NULL);
   }
   printf("[SUCCESS] malformed inputs did not crash (%d variants)\n", i);
}

/* ------------------------------------------------------------------ */
/* Writer tests                                                       */
/* ------------------------------------------------------------------ */

static void test_writer_happy_path(void)
{
   rjsonwriter_t *w = rjsonwriter_open_memory();
   const char *out;
   int outlen = 0;

   if (!w) { printf("[ERROR] writer open failed\n"); failures++; return; }

   rjsonwriter_add_start_object(w);
   rjsonwriter_add_string(w, "key");
   rjsonwriter_raw(w, ":", 1);
   rjsonwriter_add_string(w, "value");
   rjsonwriter_add_end_object(w);

   out = rjsonwriter_get_memory_buffer(w, &outlen);
   if (!out || outlen <= 0 || strcmp(out, "{\"key\":\"value\"}") != 0)
   {
      printf("[ERROR] writer happy path: out='%s' len=%d\n",
            out ? out : "(null)", outlen);
      failures++;
      rjsonwriter_free(w);
      return;
   }
   printf("[SUCCESS] writer happy path: '%s'\n", out);
   rjsonwriter_free(w);
}

static void test_writer_negative_len(void)
{
   /* Pre-patch: rjsonwriter_raw with a negative len performed
    *    (buf_num + NEG > buf_cap)
    * in signed int, which may be FALSE for large negative NEG
    * (skipping the flush), then memcpy(buf+buf_num, buf_src, NEG)
    * was called with a huge size_t.
    * Post-patch: negative len is rejected at the top of the
    * function and the call is a no-op.
    *
    * We validate by writing a bunch of sane bytes around a
    * negative-len call and confirming the output is exactly the
    * sane bytes. */
   rjsonwriter_t *w = rjsonwriter_open_memory();
   const char *out;
   int outlen = 0;

   if (!w) { printf("[ERROR] open memory writer failed\n"); failures++; return; }

   rjsonwriter_raw(w, "AAA", 3);
   rjsonwriter_raw(w, "SHOULDBEIGNORED", -99);
   rjsonwriter_raw(w, "BBB", 3);

   out = rjsonwriter_get_memory_buffer(w, &outlen);
   if (!out || outlen != 6 || memcmp(out, "AAABBB", 6) != 0)
   {
      printf("[ERROR] negative len: out='%.*s' len=%d\n",
            outlen, out ? out : "", outlen);
      failures++;
      rjsonwriter_free(w);
      return;
   }
   printf("[SUCCESS] negative-len raw() ignored (out='%.*s')\n",
         outlen, out);
   rjsonwriter_free(w);
}

/* ------------------------------------------------------------------ */
/* open_user floor                                                    */
/* ------------------------------------------------------------------ */

static int dummy_io(void *buf, int len, void *user)
{
   /* Pretend EOF immediately.  Just needs to not crash. */
   (void)buf; (void)len; (void)user;
   return 0;
}

static void test_open_user_tiny_block(void)
{
   /* Pre-patch: io_block_size = 0 allocated only the fixed part
    * of rjson_t, zero bytes for input_buf.  The first read into
    * input_buf was OOB.
    * Post-patch: io_block_size is floored at 16.  This opens
    * and frees without crashing. */
   rjson_t *json;
   int i;
   for (i = -10; i <= 8; ++i)
   {
      json = rjson_open_user(dummy_io, NULL, i);
      if (!json)
      {
         printf("[ERROR] open_user(%d) returned NULL\n", i);
         failures++;
         continue;
      }
      (void)rjson_next(json);  /* triggers a read into input_buf */
      rjson_free(json);
   }
   printf("[SUCCESS] open_user with tiny io_block_size floors cleanly\n");
}

int main(void)
{
   test_parser_happy_path();
   test_parser_long_string();
   test_parser_malformed();
   test_writer_happy_path();
   test_writer_negative_len();
   test_open_user_tiny_block();

   if (failures)
   {
      printf("\n%d rjson test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll rjson regression tests passed.\n");
   return 0;
}
