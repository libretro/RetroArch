/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (http_emscripten_headers_test.c).
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

/* Regression tests for the header marshalling in
 * tasks/task_http_emscripten.c.
 *
 * That backend sits between two different header representations.
 * RetroArch passes request headers around as one raw CRLF-delimited
 * blob, because the native path in tasks/task_http.c writes it
 * straight onto the wire:
 *
 *     "Authorization: Basic eA==\r\nDepth: 1\r\n"
 *
 * emscripten_fetch instead wants a NULL-terminated array of
 * alternating key/value C strings:
 *
 *     { "Authorization", "Basic eA==", "Depth", "1", NULL }
 *
 * and hands response headers back as a single lower-cased blob that
 * has to be split into the "Name: Value" string_list that net_http.c
 * produces, so that consumers such as network/cloud_sync/webdav.c see
 * the same shape on both backends.
 *
 * Neither conversion existed before: emscripten_async_wget2_data has
 * no header support at all, so the `headers` argument was accepted
 * and silently dropped on every entry point, and every webdav call
 * logged "Response headers not supported, webdav won't work" and did
 * exactly that.
 *
 * Both functions are static to task_http_emscripten.c and the file
 * only builds under emcc, so this test keeps behavioural copies as
 * the oracle -- the same convention http_method_match_test.c and
 * archive_name_safety_test.c use.  If the backend changes how it
 * marshals headers, update the copies here to match.
 *
 * The parsing is where the sharp edges are: hand-built blobs in
 * task_push_webdav_move() and task_push_http_transfer_with_content()
 * mean bare LF, missing trailing CRLF, absent values and malformed
 * lines all reach it, and a header dropped or mangled here is an auth
 * failure rather than a crash -- silent, and awkward to trace back.
 *
 * Build standalone:
 *   cc -Wall -std=gnu99 -g -O0 -o http_emscripten_headers_test \
 *      http_emscripten_headers_test.c
 *   ./http_emscripten_headers_test
 *
 * Worth running under ASan/LSan, which covers the allocation and
 * teardown of both the key/value array and the split list:
 *   make -f Makefile.emsc_headers check SANITIZER=address
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int checks;

#define CHECK(cond, ...) \
   do { \
      checks++; \
      if (!(cond)) \
      { \
         printf("    FAIL: "); printf(__VA_ARGS__); printf("\n"); \
         failures++; \
      } \
   } while (0)

/* ================================================================= */
/* Behavioural copies of the marshalling helpers                      */
/* ================================================================= */

static void http_req_headers_free(char **h)
{
   size_t i;
   if (!h)
      return;
   for (i = 0; h[i]; i++)
      free(h[i]);
   free(h);
}

static char **http_req_headers_parse(const char *blob)
{
   const char *p   = blob;
   size_t      cap = 8;
   size_t      n   = 0;
   char      **out;

   if (!blob || !*blob)
      return NULL;

   if (!(out = (char**)calloc(cap + 1, sizeof(char*))))
      return NULL;

   while (*p)
   {
      const char *eol;
      const char *colon;
      const char *v;
      size_t      klen, vlen;
      char       *k;

      if (!(eol = strchr(p, '\n')))
         eol = p + strlen(p);

      if (!(colon = (const char*)memchr(p, ':', (size_t)(eol - p))))
      {
         p = (*eol) ? eol + 1 : eol;
         continue;
      }

      klen = (size_t)(colon - p);
      v    = colon + 1;
      while (v < eol && (*v == ' ' || *v == '\t'))
         v++;
      vlen = (size_t)(eol - v);
      while (vlen && (v[vlen - 1] == '\r' || v[vlen - 1] == ' '))
         vlen--;

      if (!klen)
      {
         p = (*eol) ? eol + 1 : eol;
         continue;
      }

      if (n + 2 > cap)
      {
         char **tmp;
         size_t ncap = cap * 2;
         if (!(tmp = (char**)realloc(out, (ncap + 1) * sizeof(char*))))
            goto error;
         memset(tmp + cap + 1, 0, (ncap - cap) * sizeof(char*));
         out = tmp;
         cap = ncap;
      }

      if (!(k = (char*)malloc(klen + 1)))
         goto error;
      memcpy(k, p, klen);
      k[klen] = '\0';
      out[n++] = k;

      if (!(k = (char*)malloc(vlen + 1)))
         goto error;
      memcpy(k, v, vlen);
      k[vlen] = '\0';
      out[n++] = k;

      p = (*eol) ? eol + 1 : eol;
   }

   out[n] = NULL;

   if (!n)
   {
      http_req_headers_free(out);
      return NULL;
   }

   return out;

error:
   out[n] = NULL;
   http_req_headers_free(out);
   return NULL;
}

/* Response side: split the blob into "Name: Value" lines.  The real
 * one appends to a string_list; this collects into a plain array so
 * the test stays dependency-free. */
static char **http_response_headers_split(const char *blob, size_t *count)
{
   char  *raw;
   char  *p;
   char **out;
   size_t cap = 16;
   size_t n   = 0;
   size_t len = blob ? strlen(blob) : 0;

   *count = 0;
   if (!len)
      return NULL;

   if (!(raw = (char*)malloc(len + 1)))
      return NULL;
   memcpy(raw, blob, len + 1);

   if (!(out = (char**)calloc(cap + 1, sizeof(char*))))
   {
      free(raw);
      return NULL;
   }

   p = raw;
   while (*p)
   {
      char *eol = strchr(p, '\n');
      char *end;

      if (!eol)
         eol = p + strlen(p);
      end = eol;
      while (end > p && (end[-1] == '\r' || end[-1] == ' '))
         end--;

      if (end > p)
      {
         char save = *end;
         *end = '\0';
         if (n + 1 > cap)
         {
            char **tmp = (char**)realloc(out, (cap * 2 + 1) * sizeof(char*));
            if (!tmp)
               break;
            memset(tmp + cap + 1, 0, cap * sizeof(char*));
            out = tmp;
            cap *= 2;
         }
         out[n] = (char*)malloc((size_t)(end - p) + 1);
         if (!out[n])
            break;
         memcpy(out[n], p, (size_t)(end - p) + 1);
         n++;
         *end = save;
      }

      p = (*eol) ? eol + 1 : eol;
   }

   out[n]  = NULL;
   *count  = n;
   free(raw);
   return out;
}

/* ================================================================= */
/* Helpers                                                           */
/* ================================================================= */

static size_t kv_count(char **kv)
{
   size_t n = 0;
   if (!kv)
      return 0;
   while (kv[n])
      n++;
   return n;
}

static int kv_has(char **kv, const char *key, const char *val)
{
   size_t i;
   if (!kv)
      return 0;
   for (i = 0; kv[i] && kv[i + 1]; i += 2)
      if (strcmp(kv[i], key) == 0 && strcmp(kv[i + 1], val) == 0)
         return 1;
   return 0;
}

/* ================================================================= */
/* Request header tests                                              */
/* ================================================================= */

static void test_req_basic(void)
{
   char **kv = http_req_headers_parse(
         "Authorization: Basic eA==\r\nDepth: 1\r\n");

   printf("  request: well-formed CRLF blob\n");

   CHECK(kv_count(kv) == 4, "expected 2 pairs, got %u entries",
         (unsigned)kv_count(kv));
   CHECK(kv_has(kv, "Authorization", "Basic eA=="),
         "Authorization header lost or mangled");
   CHECK(kv_has(kv, "Depth", "1"), "Depth header lost or mangled");

   http_req_headers_free(kv);
}

static void test_req_no_trailing_crlf(void)
{
   /* task_push_http_transfer_with_content() folds Content-Type in and
    * then appends the caller's blob, which may not end in CRLF. */
   char **kv = http_req_headers_parse(
         "Content-Type: application/json\r\nX-Last: tail");

   printf("  request: no trailing CRLF\n");

   CHECK(kv_count(kv) == 4, "expected 2 pairs, got %u entries",
         (unsigned)kv_count(kv));
   CHECK(kv_has(kv, "X-Last", "tail"),
         "final header without trailing CRLF was dropped");

   http_req_headers_free(kv);
}

static void test_req_bare_lf(void)
{
   char **kv = http_req_headers_parse("A: 1\nB: 2\n");

   printf("  request: bare LF separators\n");

   CHECK(kv_count(kv) == 4, "expected 2 pairs, got %u entries",
         (unsigned)kv_count(kv));
   CHECK(kv_has(kv, "A", "1") && kv_has(kv, "B", "2"),
         "bare-LF separated headers not parsed");

   http_req_headers_free(kv);
}

static void test_req_value_whitespace(void)
{
   /* Leading whitespace after the colon is optional padding and must
    * not become part of the value; a value may legitimately contain
    * spaces and colons. */
   char **kv = http_req_headers_parse(
         "Destination:   http://h/a b\r\n"
         "Tight:x\r\n");

   printf("  request: value whitespace and embedded colons\n");

   CHECK(kv_has(kv, "Destination", "http://h/a b"),
         "leading padding not stripped, or value truncated at a colon");
   CHECK(kv_has(kv, "Tight", "x"),
         "value with no padding after the colon was mishandled");

   http_req_headers_free(kv);
}

static void test_req_empty_value(void)
{
   char **kv = http_req_headers_parse("X-Empty:\r\nX-After: 1\r\n");

   printf("  request: empty value\n");

   CHECK(kv_has(kv, "X-Empty", ""),
         "header with an empty value was dropped");
   CHECK(kv_has(kv, "X-After", "1"),
         "header following an empty value was dropped");

   http_req_headers_free(kv);
}

static void test_req_malformed_lines_skipped(void)
{
   /* A line with no colon is not a header.  It must be skipped, not
    * emitted as a key with a garbage value and not allowed to
    * swallow the rest of the blob. */
   char **kv = http_req_headers_parse(
         "garbage line\r\nGood: yes\r\n:novalue\r\nAlso: fine\r\n");

   printf("  request: malformed lines skipped\n");

   CHECK(kv_has(kv, "Good", "yes"),
         "valid header after a malformed line was dropped");
   CHECK(kv_has(kv, "Also", "fine"),
         "valid header after an empty key was dropped");
   CHECK(kv_count(kv) == 4,
         "expected exactly 2 pairs to survive, got %u entries",
         (unsigned)kv_count(kv));

   http_req_headers_free(kv);
}

static void test_req_growth(void)
{
   /* The array starts at 8 slots and doubles; walk well past that so
    * the realloc path and its NULL terminator are exercised. */
   char blob[4096];
   char **kv;
   size_t i;
   size_t off = 0;

   printf("  request: array growth past initial capacity\n");

   for (i = 0; i < 40; i++)
      off += (size_t)snprintf(blob + off, sizeof(blob) - off,
            "H%u: v%u\r\n", (unsigned)i, (unsigned)i);

   kv = http_req_headers_parse(blob);
   CHECK(kv_count(kv) == 80, "expected 40 pairs, got %u entries",
         (unsigned)kv_count(kv));
   CHECK(kv_has(kv, "H0", "v0"),  "first header lost after growth");
   CHECK(kv_has(kv, "H39", "v39"), "last header lost after growth");

   http_req_headers_free(kv);
}

static void test_req_degenerate(void)
{
   printf("  request: degenerate inputs\n");

   CHECK(http_req_headers_parse(NULL) == NULL, "NULL blob must yield NULL");
   CHECK(http_req_headers_parse("") == NULL, "empty blob must yield NULL");
   CHECK(http_req_headers_parse("\r\n\r\n") == NULL,
         "blank-lines-only blob must yield NULL, not an empty array");
   CHECK(http_req_headers_parse("no colons here") == NULL,
         "blob with no valid header must yield NULL");
}

/* ================================================================= */
/* Response header tests                                             */
/* ================================================================= */

static void test_resp_split(void)
{
   size_t n;
   char **lines = http_response_headers_split(
         "content-length: 42\r\n"
         "www-authenticate: Digest realm=\"r\", nonce=\"n\"\r\n"
         "content-type: text/plain\r\n", &n);

   printf("  response: split into \"Name: Value\" lines\n");

   CHECK(n == 3, "expected 3 header lines, got %u", (unsigned)n);
   if (n == 3)
   {
      CHECK(strcmp(lines[0], "content-length: 42") == 0,
            "line 0 mangled: \"%s\"", lines[0]);
      CHECK(strcmp(lines[1],
               "www-authenticate: Digest realm=\"r\", nonce=\"n\"") == 0,
            "line 1 mangled: \"%s\"", lines[1]);
      CHECK(strcmp(lines[2], "content-type: text/plain") == 0,
            "line 2 mangled: \"%s\"", lines[2]);
   }

   http_req_headers_free(lines);
}

static void test_resp_trailing_and_blank(void)
{
   size_t n;
   char **lines = http_response_headers_split(
         "a: 1\r\n\r\nb: 2\r\nc: 3", &n);

   printf("  response: blank lines dropped, final line kept\n");

   CHECK(n == 3, "expected 3 lines, got %u", (unsigned)n);
   if (n == 3)
      CHECK(strcmp(lines[2], "c: 3") == 0,
            "final line without trailing CRLF was dropped or mangled");

   http_req_headers_free(lines);
}

/* The browser lower-cases response header names, so consumers that
 * matched them case-sensitively broke on this backend.  webdav.c's
 * digest challenge check is the one that mattered; it now uses
 * string_starts_with_case_insensitive.  This pins the property that
 * made the fix necessary. */
static void test_resp_lowercased_names(void)
{
   size_t n;
   char **lines = http_response_headers_split(
         "www-authenticate: Digest realm=\"r\"\r\n", &n);
   static const char want[] = "WWW-Authenticate: Digest ";

   printf("  response: lower-cased names need case-insensitive matching\n");

   CHECK(n == 1, "expected 1 line, got %u", (unsigned)n);
   if (n == 1)
   {
      CHECK(strncmp(lines[0], want, sizeof(want) - 1) != 0,
            "fixture is not actually lower-cased; test is not testing "
            "what it claims");
      CHECK(strncasecmp(lines[0], want, sizeof(want) - 1) == 0,
            "case-insensitive match failed against the browser's "
            "lower-cased header name");
      /* The digest parser skips a fixed byte count past the prefix,
       * so a case-insensitive match must not shift the offset. */
      CHECK(strlen(lines[0]) > sizeof(want) - 1
            && strncmp(lines[0] + sizeof(want) - 1, "realm=", 6) == 0,
            "fixed-length skip past the prefix no longer lands on the "
            "digest parameters");
   }

   http_req_headers_free(lines);
}

static void test_resp_degenerate(void)
{
   size_t n;
   printf("  response: degenerate inputs\n");

   CHECK(http_response_headers_split(NULL, &n) == NULL && n == 0,
         "NULL blob must yield NULL/0");
   CHECK(http_response_headers_split("", &n) == NULL && n == 0,
         "empty blob must yield NULL/0");
}

/* ================================================================= */

int main(void)
{
   printf("task_http_emscripten header marshalling tests\n\n");

   printf("[request headers]\n");
   test_req_basic();
   test_req_no_trailing_crlf();
   test_req_bare_lf();
   test_req_value_whitespace();
   test_req_empty_value();
   test_req_malformed_lines_skipped();
   test_req_growth();
   test_req_degenerate();

   printf("\n[response headers]\n");
   test_resp_split();
   test_resp_trailing_and_blank();
   test_resp_lowercased_names();
   test_resp_degenerate();

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
