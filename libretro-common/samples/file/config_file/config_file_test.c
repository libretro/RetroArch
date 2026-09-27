/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file_test.c).
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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <file/config_file.h>
#include <file/file_path.h>

static void test_config_file_parse_contains(
      const char *cfgtext,
      const char *key, const char *val)
{
   char *cfgtext_copy = strdup(cfgtext);
   config_file_t *cfg = config_file_new_from_string(cfgtext_copy, NULL);
   char          *out = NULL;
   bool            ok = false;

   free(cfgtext_copy);

   if (!cfg)
      abort();

   ok = config_get_string(cfg, key, &out);
   if (ok != (bool)val)
      abort();
   if (!val)
   {
      /* The harness frees everything it allocates so that under
       * strict LeakSanitizer any surviving allocation belongs to
       * config_file itself - this early return previously leaked
       * the conf (and masked real parser leaks behind harness
       * noise). */
      config_file_free(cfg);
      return;
   }

   if (!out)
      out = strdup("");
   if (strcmp(out, val) != 0)
   {
      printf("[FAILED] Key [%s] Doesn't contain val [%s]\n", key, val);
      abort();
   }
   printf("[SUCCESS] Key [%s] contains val [%s]\n", key, val);
   free(out);
   config_file_free(cfg);
}

/* Regression for commit 87f2d0b (memcmp OOB on short '#' comment lines).
 *
 * The bug was in config_file_parse_line() reading 8 or 10 bytes past
 * the end of a shrunken line buffer produced by filestream_getline().
 * Triggering it requires going through the file path, not the
 * from-string path: config_file_new_from_string() keeps the entire
 * string live, while filestream_getline() realloc-shrinks each line
 * to exactly strlen+1 bytes for any line shorter than ~192 chars.
 *
 * Under AddressSanitizer the unpatched code aborts with a
 * heap-buffer-overflow READ on any short '#' line.  On non-ASan
 * builds the comparison's result depends on stale heap bytes
 * adjacent to the allocation -- a real attacker-observable
 * non-determinism, not a cosmetic issue.
 */
static void test_config_file_short_comments(void)
{
   const char *content =
      "#\n"
      "#h\n"
      "#hi\n"
      "#inc\n"
      "#includ\n"
      "#includez\n"
      "#referenc\n"
      "#referencez\n"
      "foo = \"bar\"\n";
   const char *tmp_path = "rarch_cfg_short_comment_test.cfg";
   FILE          *fp    = fopen(tmp_path, "wb");
   config_file_t *cfg;
   char          *out   = NULL;

   if (!fp)
      abort();
   fputs(content, fp);
   fclose(fp);

   cfg = config_file_new(tmp_path);
   remove(tmp_path);
   if (!cfg)
      abort();

   if (!config_get_string(cfg, "foo", &out) || !out || strcmp(out, "bar") != 0)
   {
      printf("[FAILED] short-comment regression: foo!=bar (got %s)\n",
            out ? out : "(null)");
      abort();
   }
   printf("[SUCCESS] short '#' comment lines parsed without OOB\n");
   free(out);
   config_file_free(cfg);
}

/* Regression for commit <round2-TBD> (config_get_int family silent zero).
 *
 * config_get_int, config_get_uint, config_get_uint64 and
 * config_get_hex used to return true with *in = 0 when handed a
 * string that has no leading digits at all.  A typo in a config
 * file (width = abc) would silently become width = 0 with no
 * indication of failure.  The patch adds the same end-pointer and
 * no-digits-consumed checks that config_get_size_t already used.
 *
 * These test cases all must return false on patched code.  On
 * unpatched code they return true with *in = 0.
 */
static void test_config_get_int_rejects(const char *raw_val)
{
   char cfgtext[256];
   char *copy;
   config_file_t *cfg;
   int              out_int   = 0x5a5a5a;
   unsigned         out_uint  = 0x5a5a5a;
   uint64_t         out_u64   = 0x5a5a5a;

   /* Quote the value so the parser preserves trailing text and
    * whitespace; without quotes the parser stops at the first space
    * and "42 extra" would be stored as just "42". */
   snprintf(cfgtext, sizeof(cfgtext),
         "ival = \"%s\"\nuval = \"%s\"\nu64 = \"%s\"\n",
         raw_val, raw_val, raw_val);

   copy = strdup(cfgtext);
   cfg  = config_file_new_from_string(copy, NULL);
   free(copy);
   if (!cfg)
      abort();

   if (config_get_int(cfg, "ival", &out_int))
   {
      printf("[FAILED] config_get_int accepted \"%s\" -> %d\n", raw_val, out_int);
      abort();
   }
   if (out_int != 0x5a5a5a)
   {
      printf("[FAILED] config_get_int wrote *in on reject for \"%s\": got %d\n",
            raw_val, out_int);
      abort();
   }
   if (config_get_uint(cfg, "uval", &out_uint))
   {
      printf("[FAILED] config_get_uint accepted \"%s\" -> %u\n", raw_val, out_uint);
      abort();
   }
   if (config_get_uint64(cfg, "u64", &out_u64))
   {
      printf("[FAILED] config_get_uint64 accepted \"%s\" -> %llu\n",
            raw_val, (unsigned long long)out_u64);
      abort();
   }
   printf("[SUCCESS] rejected non-numeric value \"%s\"\n", raw_val);
   config_file_free(cfg);
}

/* config_get_hex accepts base-16 digits ([0-9a-fA-F]) which includes
 * strings like "abc" and "face" -- those are valid hex.  Test with
 * characters that are not valid in any base. */
static void test_config_get_hex_rejects(const char *raw_val)
{
   char cfgtext[256];
   char *copy;
   config_file_t *cfg;
   unsigned out_hex = 0x5a5a5a;

   snprintf(cfgtext, sizeof(cfgtext), "hval = \"%s\"\n", raw_val);
   copy = strdup(cfgtext);
   cfg  = config_file_new_from_string(copy, NULL);
   free(copy);
   if (!cfg)
      abort();

   if (config_get_hex(cfg, "hval", &out_hex))
   {
      printf("[FAILED] config_get_hex accepted \"%s\" -> 0x%x\n",
            raw_val, out_hex);
      abort();
   }
   if (out_hex != 0x5a5a5a)
   {
      printf("[FAILED] config_get_hex wrote *in on reject for \"%s\"\n", raw_val);
      abort();
   }
   printf("[SUCCESS] config_get_hex rejected non-hex \"%s\"\n", raw_val);
   config_file_free(cfg);
}

static void test_config_get_int_accepts(const char *raw_val, int want_int)
{
   char cfgtext[256];
   char *copy;
   config_file_t *cfg;
   int out_int = 0;

   snprintf(cfgtext, sizeof(cfgtext), "ival = \"%s\"\n", raw_val);
   copy = strdup(cfgtext);
   cfg  = config_file_new_from_string(copy, NULL);
   free(copy);
   if (!cfg)
      abort();

   if (!config_get_int(cfg, "ival", &out_int) || out_int != want_int)
   {
      printf("[FAILED] config_get_int(\"%s\") expected %d got %d\n",
            raw_val, want_int, out_int);
      abort();
   }
   printf("[SUCCESS] config_get_int(\"%s\") == %d\n", raw_val, want_int);
   config_file_free(cfg);
}

/* Regression for commit <round4-TBD> (config_file_deinitialize
 * leaves dangling pointers).
 *
 * config_file_deinitialize() is a public API.  Pre-patch it freed
 * entries, includes, references, path and the hash map but left the
 * struct\'s pointer fields pointing at the just-freed memory.  Any
 * subsequent call on that struct -- whether accidental double-
 * deinit, reuse, or another access via the public API -- chased
 * dangling pointers.  Post-patch all fields are NULLed.
 *
 * This test loads a config, deinitializes it without freeing the
 * struct, then verifies that the struct\'s internal pointers are
 * all NULL.  On unpatched code several of these would be stale
 * non-NULL pointers to freed memory.
 *
 * Note: this inspects the config_file_t fields directly (white-box
 * test).  The public header exposes the struct definition so this
 * is legal, though a little unusual; there is no public getter for
 * "is this struct still live".  The alternative -- provoking a
 * real UAF via a second API call -- would fire ASan on unpatched
 * but also crash on patched for unrelated reasons (add_reference
 * dereferences conf->path unconditionally).  This direct field
 * inspection is the cleanest way to assert the patch\'s invariant.
 */
static void test_config_file_deinitialize_clears_fields(void)
{
   config_file_t *cfg;
   const char    *tmp_path = "rarch_cfg_deinit_test.cfg";
   FILE          *fp       = fopen(tmp_path, "wb");

   if (!fp)
      abort();
   fputs("foo = \"bar\"\nbaz = \"qux\"\n", fp);
   fclose(fp);

   cfg = config_file_new(tmp_path);
   remove(tmp_path);
   if (!cfg)
      abort();

   /* Add a reference so conf->references is non-NULL before deinit. */
   config_file_add_reference(cfg, "some_ref");

   /* Deinitialize without freeing the struct. */
   config_file_deinitialize(cfg);

   /* Every pointer field must now be NULL.  Pre-patch these would
    * be stale pointers to freed memory. */
   if (cfg->entries != NULL)
   {
      printf("[FAILED] deinit left entries as dangling %p\n", (void*)cfg->entries);
      free(cfg);
      abort();
   }
   if (cfg->includes != NULL)
   {
      printf("[FAILED] deinit left includes as dangling %p\n", (void*)cfg->includes);
      free(cfg);
      abort();
   }
   if (cfg->references != NULL)
   {
      printf("[FAILED] deinit left references as dangling %p\n", (void*)cfg->references);
      free(cfg);
      abort();
   }
   if (cfg->path != NULL)
   {
      printf("[FAILED] deinit left path as dangling %p\n", (void*)cfg->path);
      free(cfg);
      abort();
   }
   /* entries_map is cleared by RHMAP_FREE on all versions so we do
    * not check it here. */

   printf("[SUCCESS] config_file_deinitialize cleared all dangling pointer fields\n");
   free(cfg);
}

/* Smoke test for commit <round4-TBD> (isgraph((int)char) UB on
 * signed-char platforms).
 *
 * In the config parser, isgraph() is called on each byte of the
 * key / unquoted value to find the token end.  Pre-patch the cast
 * was (int), so bytes >= 0x80 became negative ints on signed-char
 * platforms.  The C standard says ctype functions must be called
 * with EOF or an unsigned-char value; anything else is undefined
 * behaviour.  glibc and musl happen to handle negative arguments
 * gracefully, but stricter libcs (Solaris, some embedded toolchains)
 * trip an assert or array-bounds fault.  Post-patch the cast is
 * (unsigned char).
 *
 * This is explicitly a smoke test: glibc and musl do not fire on
 * the pre-patch code either, so this test passes on both patched
 * and unpatched sources when run on a typical Linux host.  Its
 * value is two-fold:
 *   - Under UBSan with ctype function-arg instrumentation, the
 *     pre-patch code would trip (currently not wired into this
 *     test suite).
 *   - On a stricter libc, the pre-patch code would crash; this
 *     test therefore documents the expected contract and catches
 *     any future regression on such a platform.
 *
 * The test feeds a config value containing bytes in the 0x80-0xFF
 * range and verifies the parser does not crash.  Per the isgraph
 * contract these bytes are non-graph in the C locale, so the parser
 * will reject the key -- which is the CORRECT behaviour.  The test
 * passes if the parser completes cleanly rather than crashing.
 */
static void test_config_file_high_bit_bytes_smoke(void)
{
   /* Config with a high-bit byte (0xC3 0xA9 is UTF-8 "e-acute") in
    * both the key and the value.  The parser\'s isgraph() check
    * terminates the key at the first non-graph byte, so this line
    * is rejected as a syntactic error -- that is fine; what we care
    * about is that the ctype call did not trip UB on the 0xC3 byte. */
   const char    *cfgtext  = "caf\xc3\xa9 = \"valu\xc3\xa9\"\n"
                             "plain = \"ok\"\n";
   char          *copy     = strdup(cfgtext);
   config_file_t *cfg      = config_file_new_from_string(copy, NULL);
   char          *out      = NULL;
   free(copy);

   if (!cfg)
   {
      printf("[FAILED] parser refused to load config containing high-bit bytes\n");
      abort();
   }

   /* Sanity: the plain key on the following line should still parse.
    * This confirms the parser recovered from the rejected key and
    * kept going rather than bailing on the whole file. */
   if (!config_get_string(cfg, "plain", &out) || !out || strcmp(out, "ok") != 0)
   {
      printf("[FAILED] high-bit byte line disrupted subsequent parsing: plain=%s\n",
            out ? out : "(null)");
      free(out);
      config_file_free(cfg);
      abort();
   }

   free(out);
   config_file_free(cfg);
   printf("[SUCCESS] high-bit byte in config parsed without crash\n");
}

static void test_config_file_hash_map_agreement(void)
{
   /* The parser computes each entry's map hash inline during its key
    * scan instead of re-walking the key with rhmap_hash_string; every
    * *lookup* still hashes with the real rhmap_hash_string.  So if the
    * inline fold ever diverges from rhmap_hash_string for any key
    * byte, the entry becomes unfindable through the map while still
    * sitting in the entry list.  Parse a corpus that covers the whole
    * accepted key alphabet (0x21..0x7e), then require that every
    * listed key resolves through config_get_entry to the entry
    * holding it. */
   char cfgtext[8192];
   size_t _len   = 0;
   int i;
   int checked   = 0;
   int misses    = 0;
   config_file_t *cfg           = NULL;
   struct config_file_entry ent;

   /* One key per accepted byte: k<byte>x = "v" */
   for (i = 0x21; i <= 0x7e; i++)
   {
      if (i == '=' || i == '#' || i == '"')
         continue; /* structural characters cannot appear bare */
      _len += snprintf(cfgtext + _len, sizeof(cfgtext) - _len,
            "k%cx = \"v%d\"\n", (char)i, i);
   }
   /* Plus long keys and a duplicate */
   _len += snprintf(cfgtext + _len, sizeof(cfgtext) - _len,
         "%s = \"long\"\n", "a_rather_long_configuration_key_name_to_cross_hash_word_sizes");
   _len += snprintf(cfgtext + _len, sizeof(cfgtext) - _len,
         "dup = \"first\"\ndup = \"second\"\n");

   {
      char *copy = strdup(cfgtext);
      cfg        = config_file_new_from_string(copy, NULL);
      free(copy);
   }

   if (!cfg)
   {
      printf("[FAILED] hash-map agreement: parse failed\n");
      abort();
   }

   if (config_get_entry_list_head(cfg, &ent))
   {
      do
      {
         if (ent.key)
         {
            const struct config_entry_list *hit =
                  config_get_entry(cfg, ent.key);
            checked++;
            if (!hit)
               misses++;
            /* The map keeps the *first* entry for a duplicated
             * key; for unique keys the value must match. */
            else if (strcmp(ent.key, "dup") && strcmp(hit->value, ent.value))
               misses++;
         }
      } while (config_get_entry_list_next(&ent));
   }

   if (misses == 0 && checked > 80)
      printf("[SUCCESS] all %d parsed keys resolve through the hash map\n",
            checked);
   else
   {
      printf("[FAILED] hash-map agreement: %d misses of %d keys\n",
            misses, checked);
      abort();
   }

   config_file_free(cfg);
}

static void test_config_file_stream_matches_from_string(void)
{
   /* Push the same text through the streaming parser in packet
    * sizes that cross every line boundary (1 and 7 bytes) plus one
    * larger than the whole text, and require the entry list to
    * match from_string exactly.  The window logic (tail retention,
    * NUL displacement, slide-back) has its off-by-ones exercised
    * hardest by the 1-byte case. */
   static const char *cfgtext =
         "alpha = \"1\"\n"
         "# comment line\n"
         "beta = \"two words\"\n"
         "   gamma   =   bare\r\n"
         "delta = \"has # inside\"\n"
         "epsilon = \"\"\n"
         "zeta = last_line_without_newline";
   static const size_t packets[] = { 1, 7, 4096 };
   size_t pi;

   for (pi = 0; pi < sizeof(packets) / sizeof(packets[0]); pi++)
   {
      size_t off;
      size_t text_len            = strlen(cfgtext);
      char *copy                 = strdup(cfgtext);
      config_file_t *slurped     = config_file_new_from_string(copy, NULL);
      config_file_stream_t *st   = config_file_stream_new(NULL);
      config_file_t *streamed    = NULL;
      struct config_file_entry a;
      struct config_file_entry b;
      bool more_a, more_b;

      free(copy);
      if (!slurped || !st)
         abort();

      for (off = 0; off < text_len; off += packets[pi])
      {
         size_t n = (text_len - off < packets[pi])
               ? (text_len - off) : packets[pi];
         if (!config_file_stream_push(st, cfgtext + off, n))
            abort();
      }
      if (!(streamed = config_file_stream_finish(st)))
         abort();

      more_a = config_get_entry_list_head(slurped, &a);
      more_b = config_get_entry_list_head(streamed, &b);
      while (more_a && more_b)
      {
         if (       !!a.key   != !!b.key
               ||   !!a.value != !!b.value
               || (a.key   && strcmp(a.key,   b.key))
               || (a.value && strcmp(a.value, b.value)))
         {
            printf("[FAILED] stream/packet=%u entry mismatch: [%s]=[%s] vs [%s]=[%s]\n",
                  (unsigned)packets[pi],
                  a.key ? a.key : "(null)", a.value ? a.value : "(null)",
                  b.key ? b.key : "(null)", b.value ? b.value : "(null)");
            abort();
         }
         more_a = config_get_entry_list_next(&a);
         more_b = config_get_entry_list_next(&b);
      }
      if (more_a != more_b)
      {
         printf("[FAILED] stream/packet=%u entry count mismatch\n",
               (unsigned)packets[pi]);
         abort();
      }

      config_file_free(slurped);
      config_file_free(streamed);
   }
   printf("[SUCCESS] streamed parse matches from_string at packet sizes 1/7/4096\n");
}

static void test_config_file_stream_nul_ends_stream(void)
{
   /* An embedded NUL must end the stream the way it ends the slurp
    * path's line walk: the truncated line parses as final, and
    * everything after - even in later packets - is dropped.  Push
    * byte-at-a-time so a packet boundary is guaranteed to fall
    * between the NUL and the later lines (the divergent case the
    * clamp exists for). */
   static const char raw[] = "a = \"1\"\nb\0c = \"2\"\nd = \"3\"\n";
   size_t raw_len          = sizeof(raw) - 1; /* keep embedded NUL */
   config_file_stream_t *st = config_file_stream_new(NULL);
   config_file_t *conf;
   char *out               = NULL;
   size_t i;

   if (!st)
      abort();
   for (i = 0; i < raw_len; i++)
      if (!config_file_stream_push(st, raw + i, 1))
         abort();
   if (!(conf = config_file_stream_finish(st)))
      abort();

   if (!config_get_string(conf, "a", &out))
   {
      printf("[FAILED] NUL-ended stream lost the entry before the NUL\n");
      abort();
   }
   free(out);
   out = NULL;
   if (config_get_string(conf, "d", &out) || config_get_string(conf, "c", &out))
   {
      printf("[FAILED] NUL-ended stream parsed entries past the NUL\n");
      abort();
   }
   printf("[SUCCESS] embedded NUL ends the stream with slurp semantics\n");
   config_file_free(conf);
}

static void test_config_file_pathless_reference_no_crash(void)
{
   /* '#reference' in a pathless string config previously handed a
    * NULL base path to fill_pathname_abbreviated_or_relative,
    * whose strlcpy runs strlen on it - UB found by the
    * differential fuzzer.  The reference must now be recorded
    * verbatim. */
   char *copy         = strdup("#reference \"some/other.cfg\"\nfoo = \"bar\"\n");
   config_file_t *cfg = config_file_new_from_string(copy, NULL);

   free(copy);
   if (!cfg)
      abort();
   if (     !cfg->references
         || !cfg->references->path
         || strcmp(cfg->references->path, "some/other.cfg"))
   {
      printf("[FAILED] pathless #reference not recorded verbatim (got %s)\n",
            (cfg->references && cfg->references->path)
                  ? cfg->references->path : "(none)");
      abort();
   }
   printf("[SUCCESS] pathless '#reference' recorded verbatim without UB\n");
   config_file_free(cfg);
}

static void test_config_file_borrowed_entry_lifecycle(void)
{
   /* Path-loaded entries borrow their strings from the adopted file
    * buffer.  Exercise every mutation path against borrowed
    * entries - overwrite (must not free borrowed storage), unset
    * (same), append pilfering (buffers must travel), then teardown
    * (borrowed skipped, owned freed, buffers released) - under the
    * suite's sanitizers this catches any wrong free or dangling
    * borrow. */
   const char *tmp_a = "/tmp/cfg_borrow_a.cfg";
   const char *tmp_b = "/tmp/cfg_borrow_b.cfg";
   FILE *f;
   config_file_t *cfg;
   char *out = NULL;

   f = fopen(tmp_a, "w");
   fprintf(f, "alpha = \"one\"\nbeta = \"two\"\ngamma = \"three\"\n");
   fclose(f);
   f = fopen(tmp_b, "w");
   fprintf(f, "beta = \"TWO\"\ndelta = \"four\"\n");
   fclose(f);

   if (!(cfg = config_file_new(tmp_a)))
      abort();

   /* Overwrite a borrowed value with an owned one, twice */
   config_set_string(cfg, "alpha", "replaced");
   config_set_string(cfg, "alpha", "replaced-again");
   if (!config_get_string(cfg, "alpha", &out) || strcmp(out, "replaced-again"))
      abort();
   free(out);
   out = NULL;

   /* Unset a borrowed entry */
   config_unset(cfg, "gamma");
   if (config_get_entry(cfg, "gamma"))
      abort();

   /* Append a second borrowed config: its buffers must move over */
   if (!config_append_file(cfg, tmp_b))
      abort();
   if (!config_get_string(cfg, "delta", &out) || strcmp(out, "four"))
      abort();
   free(out);
   out = NULL;
   if (!config_get_string(cfg, "beta", &out) || strcmp(out, "TWO"))
      abort();
   free(out);
   out = NULL;

   /* New owned entry alongside borrowed ones */
   config_set_string(cfg, "epsilon", "five");

   config_file_free(cfg);
   remove(tmp_a);
   remove(tmp_b);
   printf("[SUCCESS] borrowed-entry lifecycle (set/unset/append/free) clean\n");
}

static void test_config_file_take_string(void)
{
   /* take_string must parse identically to from_string on the same
    * bytes while owning the buffer (entries borrow from it), and
    * mutation after the take must behave like any borrowed conf. */
   static const char *cfgtext =
         "one = \"1\"\ntwo = \"double # inside\"\n   three = bare\nfour = \"\"\n";
   char *copy_a               = strdup(cfgtext);
   char *copy_b               = strdup(cfgtext);
   config_file_t *ref         = config_file_new_from_string(copy_a, NULL);
   config_file_t *took        = config_file_new_take_string(copy_b,
         strlen(cfgtext), NULL);
   struct config_file_entry ea;
   struct config_file_entry eb;
   bool ma, mb;
   char *out = NULL;

   free(copy_a); /* from_string copied; take_string owns copy_b now */
   if (!ref || !took)
      abort();

   ma = config_get_entry_list_head(ref, &ea);
   mb = config_get_entry_list_head(took, &eb);
   while (ma && mb)
   {
      if (     strcmp(ea.key, eb.key)
            || strcmp(ea.value, eb.value))
      {
         printf("[FAILED] take_string mismatch: [%s]=[%s] vs [%s]=[%s]\n",
               ea.key, ea.value, eb.key, eb.value);
         abort();
      }
      ma = config_get_entry_list_next(&ea);
      mb = config_get_entry_list_next(&eb);
   }
   if (ma != mb)
      abort();

   /* Mutate through the API against the borrowed storage */
   config_set_string(took, "two", "owned-now");
   config_unset(took, "three");
   if (!config_get_string(took, "two", &out) || strcmp(out, "owned-now"))
      abort();
   free(out);

   config_file_free(ref);
   config_file_free(took);
   printf("[SUCCESS] take_string parses identically and owns its buffer\n");
}

static void test_config_take_string(void)
{
   /* config_take_string exists because core_info's historical idiom
    * - lift entry->value out of the entry, NULL it, free it later -
    * corrupts the heap against borrowed entries: the pointer lands
    * in the middle of the conf's adopted file buffer (found as a
    * STATUS_HEAP_CORRUPTION crash in core_info_free on Windows).
    * The take must hand out a real allocation in both ownership
    * modes, outliving the conf. */
   const char *tmp_path = "/tmp/cfg_take.cfg";
   FILE *f;
   config_file_t *cfg;
   char *taken_borrowed = NULL;
   char *taken_owned    = NULL;
   struct config_entry_list *entry;

   f = fopen(tmp_path, "w");
   fprintf(f, "borrowed_key = \"from file\"\nempty_key = \"\"\n");
   fclose(f);

   if (!(cfg = config_file_new(tmp_path)))   /* borrowed entries */
      abort();
   config_set_string(cfg, "owned_key", "from set");

   /* Borrowed: must be copied out */
   if (     !(taken_borrowed = config_take_string(cfg, "borrowed_key"))
         || strcmp(taken_borrowed, "from file"))
      abort();
   /* Owned: stolen; either way the entry is emptied */
   if (     !(taken_owned = config_take_string(cfg, "owned_key"))
         || strcmp(taken_owned, "from set"))
      abort();
   if (     !(entry = config_get_entry(cfg, "borrowed_key"))
         || entry->value)
      abort();
   /* Missing and empty: NULL, entry untouched */
   if (config_take_string(cfg, "no_such_key"))
      abort();
   if (config_take_string(cfg, "empty_key"))
      abort();
   if (     !(entry = config_get_entry(cfg, "empty_key"))
         || !entry->value)
      abort();

   /* The taken strings must outlive the conf and be free()-able:
    * under this suite's sanitizers, a borrowed pointer leaking
    * through here is a bad-free. */
   config_file_free(cfg);
   if (strcmp(taken_borrowed, "from file") || strcmp(taken_owned, "from set"))
      abort();
   free(taken_borrowed);
   free(taken_owned);
   remove(tmp_path);
   printf("[SUCCESS] config_take_string owns its result in both ownership modes\n");
}

static void test_config_entry_cached_lengths(void)
{
   /* key_len/value_len are consumed by the write path, so a stale
    * one silently writes a truncated or over-long line.  Walk every
    * entry after a parse and after each kind of mutation and assert
    * the cache either matches strlen exactly or is 0 ("unknown",
    * which readers must handle by measuring). */
   const char *tmp_path = "/tmp/cfg_lens.cfg";
   FILE *f;
   config_file_t *cfg;
   struct config_file_entry it;
   bool more;
   int checked = 0;

   f = fopen(tmp_path, "w");
   fprintf(f, "short = \"1\"\n");
   fprintf(f, "longer_key_name = \"a rather longer value with spaces\"\n");
   fprintf(f, "bare = unquoted\n");
   fprintf(f, "empty = \"\"\n");
   fclose(f);

   if (!(cfg = config_file_new(tmp_path)))
      abort();

   /* Mutations: overwrite a parsed (borrowed) value, add a fresh
    * owned entry, overwrite that again, and unset one. */
   config_set_string(cfg, "short", "replaced with something longer");
   config_set_string(cfg, "added", "brand new");
   config_set_string(cfg, "added", "shorter");
   config_unset(cfg, "bare");

   {
      const struct config_entry_list *e;
      for (e = cfg->entries; e; e = e->next)
      {
         if (e->key)
         {
            if (e->key_len && e->key_len != (uint16_t)strlen(e->key))
            {
               printf("[FAILED] key_len %u != strlen %u for [%s]\n",
                     (unsigned)e->key_len, (unsigned)strlen(e->key), e->key);
               abort();
            }
            checked++;
         }
         else if (e->key_len)
            abort();   /* no key, but a cached key length */

         if (e->value)
         {
            if (e->value_len && e->value_len != (uint16_t)strlen(e->value))
            {
               printf("[FAILED] value_len %u != strlen %u for [%s]\n",
                     (unsigned)e->value_len, (unsigned)strlen(e->value),
                     e->key ? e->key : "?");
               abort();
            }
         }
         else if (e->value_len)
            abort();   /* no value, but a cached value length */
      }
   }
   if (checked < 4)
      abort();

   /* And the write path must still round-trip: read the dump back
    * and confirm the values survive intact. */
   if (!config_file_write(cfg, "/tmp/cfg_lens_out.cfg", false))
      abort();
   config_file_free(cfg);

   if (!(cfg = config_file_new("/tmp/cfg_lens_out.cfg")))
      abort();
   more = config_get_entry_list_head(cfg, &it);
   while (more)
   {
      if (!it.key || !it.value)
         abort();
      more = config_get_entry_list_next(&it);
   }
   {
      char *v = NULL;
      if (     !config_get_string(cfg, "short", &v)
            || strcmp(v, "replaced with something longer"))
         abort();
      free(v);
      v = NULL;
      if (     !config_get_string(cfg, "longer_key_name", &v)
            || strcmp(v, "a rather longer value with spaces"))
         abort();
      free(v);
   }
   config_file_free(cfg);
   remove(tmp_path);
   remove("/tmp/cfg_lens_out.cfg");
   printf("[SUCCESS] cached entry lengths agree with strlen and round-trip\n");
}

static struct { const char *name; const char *data; } test_io_members[] =
{
   { "root.cfg",
     "a = \"1\"\n#include \"sub.cfg\"\nc = \"3\"\n" },
   { "sub.cfg", "b = \"2\"\n" },
   { NULL, NULL }
};
static int test_io_reads = 0;

static char *test_io_read_file(const char *path, int64_t *len, void *ud)
{
   const char *base = strrchr(path, '/');
   size_t i;
   base = base ? base + 1 : path;
   for (i = 0; test_io_members[i].name; i++)
      if (!strcmp(test_io_members[i].name, base))
      {
         size_t n  = strlen(test_io_members[i].data);
         char  *out = (char*)malloc(n + 1);
         if (!out)
            return NULL;
         memcpy(out, test_io_members[i].data, n + 1);
         if (len)
            *len = (int64_t)n;
         test_io_reads++;
         return out;
      }
   return NULL;
}

static void test_io_free_file(char *buf, void *ud) { free(buf); }

static void test_config_file_per_config_io(void)
{
   /* A config whose text is in hand while the files it includes are not
    * reachable by path - an archive member is the motivating case.  With
    * the default io the include resolves to a path beside the archive
    * and is silently dropped; with a caller-supplied io it resolves
    * through that io, and the process-wide default is not disturbed,
    * which is what lets a task thread do this safely. */
   static const config_file_io_t io =
   { test_io_read_file, test_io_free_file, NULL };
   const config_file_io_t *before = config_file_get_io_default();
   config_file_t *conf;
   char *buf;

   /* Default io: the include cannot be found, so 'b' is absent while
    * the config's own entries are still parsed. */
   buf  = strdup(test_io_members[0].data);
   if (!(conf = config_file_new_take_string(buf, 0, "/nowhere/pack.zip#root.cfg")))
      abort();
   if (!config_get_entry(conf, "a") || !config_get_entry(conf, "c"))
      abort();
   if (config_get_entry(conf, "b"))
      abort();   /* nothing should have satisfied the include */
   config_file_free(conf);

   /* Caller-supplied io: the include resolves. */
   test_io_reads = 0;
   buf  = strdup(test_io_members[0].data);
   if (!(conf = config_file_new_take_string_with_io(buf, 0,
         "/nowhere/pack.zip#root.cfg", &io)))
      abort();
   if (     !config_get_entry(conf, "a")
         || !config_get_entry(conf, "b")
         || !config_get_entry(conf, "c"))
      abort();
   if (test_io_reads != 1)
      abort();
   config_file_free(conf);

   /* And a path load served entirely by the io, includes and all. */
   if (!(conf = config_file_new_with_io("root.cfg", &io)))
      abort();
   if (!config_get_entry(conf, "b"))
      abort();
   config_file_free(conf);

   /* A NULL io is refused rather than silently falling back. */
   if (config_file_new_with_io("root.cfg", NULL))
      abort();

   if (config_file_get_io_default() != before)
      abort();   /* the global must be untouched throughout */

   printf("[SUCCESS] per-config io resolves includes without touching the default\n");
}

/* Regression: config_file_append_conf() into an EMPTY destination
 * left conf->entries non-NULL with conf->tail still NULL.  The parser's insert path does "if (conf->entries)
 * conf->tail->next = list", so the next load through the same conf
 * dereferenced NULL; config_set_string() under
 * CONF_FILE_FLG_GUARANTEED_NO_DUPLICATES fell back to
 * last = conf->entries and spliced onto the head, orphaning
 * everything behind it.  The matrix below covers both destination
 * shapes against both donor shapes, and then uses the result. */
static config_file_t *cfg_from(const char *text)
{
   char *copy         = strdup(text);
   config_file_t *cfg = config_file_new_from_string(copy, NULL);
   free(copy);
   if (!cfg)
      abort();
   return cfg;
}

static size_t cfg_count(config_file_t *cfg)
{
   const struct config_entry_list *e;
   size_t n = 0;
   for (e = cfg->entries; e; e = e->next)
      if (e->key)
         n++;
   return n;
}

static void cfg_check_tail(config_file_t *cfg, const char *what)
{
   const struct config_entry_list *e;
   const struct config_entry_list *last = NULL;
   for (e = cfg->entries; e; e = e->next)
      last = e;
   if (cfg->tail != last)
   {
      printf("[FAILED] %s: conf->tail is not the last list node\n", what);
      abort();
   }
   if (cfg->entries && !cfg->tail)
   {
      printf("[FAILED] %s: non-empty list with NULL tail\n", what);
      abort();
   }
}

static void test_config_file_append_conf_tail_matrix(void)
{
   static const char *shapes[2] = { "", "a = \"1\"\nb = \"2\"\n" };
   size_t d, n;

   for (d = 0; d < 2; d++)
   {
      for (n = 0; n < 2; n++)
      {
         config_file_t *dst = cfg_from(shapes[d]);
         config_file_t *src = cfg_from(n ? "c = \"3\"\nd = \"4\"\n" : "");
         size_t expect      = cfg_count(dst) + cfg_count(src);
         char *out          = NULL;

         if (!config_file_append_conf(dst, src))
            abort();
         cfg_check_tail(dst, "after append_conf");
         if (cfg_count(dst) != expect)
         {
            printf("[FAILED] append_conf lost entries (%u != %u)\n",
                  (unsigned)cfg_count(dst), (unsigned)expect);
            abort();
         }

         /* The two operations that consumed the stale tail. */
         config_set_string(dst, "appended", "yes");
         cfg_check_tail(dst, "after set following append_conf");
         if (cfg_count(dst) != expect + 1)
         {
            printf("[FAILED] set after append_conf orphaned the list\n");
            abort();
         }
         if (!config_get_string(dst, "appended", &out) || !out)
            abort();
         free(out);
         if (d || n)
         {
            const char *probe = d ? "a" : "c";
            out = NULL;
            if (!config_get_string(dst, probe, &out) || !out)
            {
               printf("[FAILED] key [%s] unreachable after append+set\n",
                     probe);
               abort();
            }
            free(out);
         }
         config_file_free(dst);
      }
   }
   printf("[SUCCESS] append_conf keeps tail/last valid in all four shapes\n");
}

/* Regression: config_get_config_path() passed conf->path straight to
 * strlcpy(), and a config built from a string has none. */
static void test_config_get_config_path_pathless(void)
{
   config_file_t *cfg = cfg_from("foo = \"bar\"\n");
   char buf[64];
   size_t len;

   memset(buf, 'x', sizeof(buf));
   len = config_get_config_path(cfg, buf, sizeof(buf));
   if (len != 0 || buf[0] != '\0')
   {
      printf("[FAILED] pathless config_get_config_path did not "
            "produce an empty string\n");
      abort();
   }
   config_file_free(cfg);
   printf("[SUCCESS] config_get_config_path handles a pathless config\n");
}

/* Regression: config_take_string() leaves the entry keyed and
 * valueless with value_len cleared, so the dump loop's
 * "value_len ? value_len : strlen(value)" fallback measured NULL. */
static void test_config_file_dump_after_take_string(void)
{
   config_file_t *cfg = cfg_from("kept = \"1\"\ntaken = \"2\"\n");
   char *taken        = config_take_string(cfg, "taken");
   FILE *sink         = tmpfile();

   if (!taken || strcmp(taken, "2") != 0)
      abort();
   free(taken);
   if (!sink)
      abort();
   if (!config_file_dump(cfg, sink, false))
   {
      printf("[FAILED] dump reported failure on a valid config\n");
      abort();
   }
   if (!config_file_dump(cfg, sink, true))
      abort();
   fclose(sink);
   config_file_free(cfg);
   printf("[SUCCESS] dump survives an entry emptied by "
         "config_take_string\n");
}

/* config_set_string() overwriting an existing value must leave the
 * entry consistent: new bytes, matching cached length, and the
 * borrowed flag cleared so the value is freed exactly once (checked
 * by the sanitizers CI runs this under). */
static void test_config_set_string_overwrite(void)
{
   config_file_t *cfg              = cfg_from("k = \"borrowed\"\n");
   struct config_entry_list *entry;
   char *out                       = NULL;

   config_set_string(cfg, "k", "replacement");
   if (!(entry = config_get_entry(cfg, "k")))
      abort();
   if (!entry->value || strcmp(entry->value, "replacement") != 0)
      abort();
   if (entry->value_len != strlen("replacement"))
   {
      printf("[FAILED] overwrite left a stale cached length\n");
      abort();
   }
   /* Same value again: must be a no-op, not a free-and-redup. */
   config_set_string(cfg, "k", "replacement");
   if (!config_get_string(cfg, "k", &out) || !out
         || strcmp(out, "replacement") != 0)
      abort();
   free(out);
   config_file_free(cfg);
   printf("[SUCCESS] config_set_string overwrite is consistent\n");
}

/* config_file_dump(sort=true) used to run a merge sort over the live
 * entry list and assign the result back to conf->entries, so saving a
 * config reordered it in memory.  The written order must be sorted;
 * the in-memory order must be exactly what the caller had.  Duplicate
 * keys are legal and the topmost wins on reload, so their relative
 * order is meaning and must survive the sort. */
static char *cfg_dump_to_string(config_file_t *cfg, bool sort)
{
   FILE *f    = tmpfile();
   char *out;
   long  size;

   if (!f)
      abort();
   if (!config_file_dump(cfg, f, sort))
      abort();
   fflush(f);
   if ((size = ftell(f)) < 0)
      abort();
   rewind(f);
   if (!(out = (char*)malloc((size_t)size + 1)))
      abort();
   if (size && fread(out, 1, (size_t)size, f) != (size_t)size)
      abort();
   out[size] = '\0';
   fclose(f);
   return out;
}

static void test_config_file_dump_sort_is_side_effect_free(void)
{
   static const char *text = "zeta = \"1\"\nAlpha = \"2\"\n"
                             "dup = \"first\"\nmid = \"3\"\ndup = \"second\"\n";
   config_file_t *cfg      = cfg_from(text);
   const struct config_entry_list *e;
   char before[256];
   char after[256];
   char *dumped;
   size_t used             = 0;

   before[0] = '\0';
   for (e = cfg->entries; e; e = e->next)
      if (e->key)
         used += (size_t)snprintf(before + used, sizeof(before) - used,
               "%s,", e->key);

   dumped = cfg_dump_to_string(cfg, true);

   used      = 0;
   after[0]  = '\0';
   for (e = cfg->entries; e; e = e->next)
      if (e->key)
         used += (size_t)snprintf(after + used, sizeof(after) - used,
               "%s,", e->key);

   if (strcmp(before, after) != 0)
   {
      printf("[FAILED] dump(sort) reordered the live list: "
            "[%s] -> [%s]\n", before, after);
      abort();
    }
   cfg_check_tail(cfg, "after dump(sort)");

   /* Sorted output, case-insensitive, duplicates in original order. */
   if (strcmp(dumped,
            "Alpha = \"2\"\ndup = \"first\"\ndup = \"second\"\n"
            "mid = \"3\"\nzeta = \"1\"\n") != 0)
   {
      printf("[FAILED] unexpected sorted dump:\n%s", dumped);
      abort();
   }
   free(dumped);

   /* Unsorted dump must still follow list order. */
   dumped = cfg_dump_to_string(cfg, false);
   if (strcmp(dumped,
            "zeta = \"1\"\nAlpha = \"2\"\ndup = \"first\"\n"
            "mid = \"3\"\ndup = \"second\"\n") != 0)
   {
      printf("[FAILED] unexpected unsorted dump:\n%s", dumped);
      abort();
   }
   free(dumped);
   config_file_free(cfg);
   printf("[SUCCESS] dump(sort) sorts stably without touching the "
         "live list\n");
}

/* Characterisation, not endorsement: config_file.h says a setter
 * "will not write to entry if the entry was obtained from an
 * #include", and config_set_string() neither honours entry->readonly
 * nor leaves it set.  This pins the behaviour that ships today so
 * that changing it is a deliberate act with a visible diff here,
 * rather than something that drifts.  If the header is what should
 * win, this test is the one to invert. */
static void test_config_set_string_on_readonly_entry(void)
{
   config_file_t *cfg = cfg_from("inc = \"original\"\n");
   struct config_entry_list *entry;
   char *dumped;

   if (!(entry = config_get_entry(cfg, "inc")))
      abort();
   entry->readonly = true;   /* as config_file_add_child_list marks it */

   config_set_string(cfg, "inc", "overwritten");

   if (!entry->value || strcmp(entry->value, "overwritten") != 0)
   {
      printf("[FAILED] readonly-entry behaviour changed: value is "
            "now [%s] - update this test deliberately\n",
            entry->value ? entry->value : "(null)");
      abort();
   }
   if (entry->readonly)
   {
      printf("[FAILED] readonly-entry behaviour changed: the flag "
            "survived the set - update this test deliberately\n");
      abort();
   }
   /* Consequence worth seeing: the entry is no longer readonly, so
    * it is now serialised into files it was only included from. */
   dumped = cfg_dump_to_string(cfg, false);
   if (strcmp(dumped, "inc = \"overwritten\"\n") != 0)
   {
      printf("[FAILED] unexpected dump of a formerly readonly "
            "entry:\n%s", dumped);
      abort();
   }
   free(dumped);
   config_file_free(cfg);
   printf("[SUCCESS] setter-vs-#include semantics are pinned "
         "(current behaviour: the setter wins)\n");
}

/* The sort fallback runs only when the pointer array cannot be
 * allocated, so it would otherwise never execute under test.  It has
 * to produce byte-identical output to the array merge - same order,
 * same stability for duplicate keys - and it has to leave the list it
 * reordered internally consistent, which the old in-place sort did
 * not: it moved conf->entries and left conf->tail mid-list. */
extern bool config_file_force_linked_sort;

static void test_config_file_dump_sort_fallback(void)
{
   static const char *text = "zeta = \"1\"\nAlpha = \"2\"\n"
                             "dup = \"first\"\nmid = \"3\"\ndup = \"second\"\n";
   config_file_t *cfg      = cfg_from(text);
   config_file_t *fb       = cfg_from(text);
   char *via_array;
   char *via_list;

   via_array = cfg_dump_to_string(cfg, true);

   config_file_force_linked_sort = true;
   via_list = cfg_dump_to_string(fb, true);
   config_file_force_linked_sort = false;

   if (strcmp(via_array, via_list) != 0)
   {
      printf("[FAILED] sort fallback disagrees with the array merge:\n"
            "--- array ---\n%s--- list ---\n%s", via_array, via_list);
      abort();
   }
   /* The fallback is allowed to reorder the list - that is its cost -
    * but not to leave it inconsistent. */
   cfg_check_tail(fb, "after the fallback sort");
   config_set_string(fb, "added", "x");
   cfg_check_tail(fb, "after a set following the fallback sort");
   {
      const struct config_entry_list *e;
      size_t n = 0;
      for (e = fb->entries; e; e = e->next)
         if (e->key)
            n++;
      if (n != 6)
      {
         printf("[FAILED] set after the fallback sort lost entries "
               "(%u != 6)\n", (unsigned)n);
         abort();
      }
   }
   free(via_array);
   free(via_list);
   config_file_free(cfg);
   config_file_free(fb);
   printf("[SUCCESS] zero-allocation sort fallback matches the array "
         "merge and leaves the list consistent\n");
}

/* conf->tail is the only tracker now, so the paths that extend the
 * list all have to write it: parse, append, set, include merge and
 * the sort fallback.  This exercises them in sequence through one
 * config and checks the invariant after each, which is what the two
 * drifting fields used to break.
 *
 * The GUARANTEED_NO_DUPLICATES leg is the cheat_manager.c shape:
 * parse a file, flag it, then set into it.  With two trackers that
 * dropped every parsed entry but the first. */
static void test_config_tail_is_single_authority(void)
{
   const char *inc_path  = "/tmp/cfg_tail_inc.cfg";
   const char *main_path = "/tmp/cfg_tail_main.cfg";
   config_file_t *cfg;
   config_file_t *donor;
   const struct config_entry_list *e;
   size_t n = 0;
   FILE *f;

   f = fopen(inc_path, "w");
   fprintf(f, "inc_a = \"1\"\ninc_b = \"2\"\n");
   fclose(f);
   f = fopen(main_path, "w");
   fprintf(f, "main_a = \"1\"\n#include \"cfg_tail_inc.cfg\"\nmain_b = \"2\"\n");
   fclose(f);

   if (!(cfg = config_file_new(main_path)))
      abort();
   cfg_check_tail(cfg, "after parse with an #include");

   donor = cfg_from("donor_a = \"1\"\n");
   if (!config_file_append_conf(cfg, donor))
      abort();
   cfg_check_tail(cfg, "after append_conf");

   /* No-duplicates fast path: appends onto the tail with no lookup. */
   cfg->flags |= CONF_FILE_FLG_GUARANTEED_NO_DUPLICATES;
   config_set_string(cfg, "fast_a", "1");
   config_set_string(cfg, "fast_b", "2");
   cfg_check_tail(cfg, "after two no-duplicate sets");
   cfg->flags &= (uint8_t)~CONF_FILE_FLG_GUARANTEED_NO_DUPLICATES;

   config_set_string(cfg, "slow_a", "1");
   cfg_check_tail(cfg, "after a lookup-path set");

   for (e = cfg->entries; e; e = e->next)
      if (e->key)
         n++;
   /* main_a, main_b, inc_a, inc_b, donor_a, fast_a, fast_b, slow_a */
   if (n != 8)
   {
      printf("[FAILED] entries lost across the tail-writing paths "
            "(%u != 8)\n", (unsigned)n);
      abort();
   }
   if (     !config_get_entry(cfg, "inc_a")
         || !config_get_entry(cfg, "donor_a")
         || !config_get_entry(cfg, "fast_a")
         || !config_get_entry(cfg, "slow_a"))
   {
      printf("[FAILED] an entry became unreachable\n");
      abort();
   }
   config_file_free(cfg);
   remove(inc_path);
   remove(main_path);
   printf("[SUCCESS] conf->tail stays authoritative across parse, "
         "include, append and both set paths\n");
}

/* Includes are appended through conf->includes_tail rather than by
 * walking the list.  Their order is user-visible - config_file_dump()
 * writes the '#include' lines back in list order - so it has to
 * survive. struct config_include_list is private to config_file.c,
 * so the check goes through the serialised form, which is the part
 * that actually matters anyway. */
static void test_config_include_order_preserved(void)
{
   const char *paths[3] = { "/tmp/cfg_inc_1.cfg",
                            "/tmp/cfg_inc_2.cfg",
                            "/tmp/cfg_inc_3.cfg" };
   const char *main_path = "/tmp/cfg_inc_main.cfg";
   config_file_t *cfg;
   char *dumped;
   const char *p1;
   const char *p2;
   const char *p3;
   FILE *f;
   int i;

   for (i = 0; i < 3; i++)
   {
      f = fopen(paths[i], "w");
      fprintf(f, "k%d = \"%d\"\n", i, i);
      fclose(f);
   }
   f = fopen(main_path, "w");
   fprintf(f, "#include \"cfg_inc_1.cfg\"\n#include \"cfg_inc_2.cfg\"\n"
              "#include \"cfg_inc_3.cfg\"\n");
   fclose(f);

   if (!(cfg = config_file_new(main_path)))
      abort();
   dumped = cfg_dump_to_string(cfg, false);

   p1 = strstr(dumped, "#include \"cfg_inc_1.cfg\"");
   p2 = strstr(dumped, "#include \"cfg_inc_2.cfg\"");
   p3 = strstr(dumped, "#include \"cfg_inc_3.cfg\"");
   if (!p1 || !p2 || !p3)
   {
      printf("[FAILED] an #include went missing from the dump:\n%s",
            dumped);
      abort();
   }
   if (!(p1 < p2 && p2 < p3))
   {
      printf("[FAILED] #include lines came back out of order:\n%s",
            dumped);
      abort();
   }
   free(dumped);
   config_file_free(cfg);
   for (i = 0; i < 3; i++)
      remove(paths[i]);
   remove(main_path);
   printf("[SUCCESS] include order survives O(1) appending\n");
}

/* Setting a key a parsed config already holds must replace it, not
 * append a second copy: the file is written with both, and the first
 * of a duplicate pair is what wins on reload, so a duplicate-writing
 * save silently keeps the old value and grows the file by a copy
 * every time.  This is the cheat_manager.c save shape. */
static void test_config_set_over_parsed_keys_round_trips(void)
{
   config_file_t *cfg = cfg_from("cheats = \"2\"\ncheat0_desc = \"A\"\n"
                                 "cheat1_desc = \"B\"\n");
   config_file_t *back;
   const struct config_entry_list *e;
   char *dumped;
   size_t n = 0;

   config_set_string(cfg, "cheats",      "3");
   config_set_string(cfg, "cheat0_desc", "EDITED");
   config_set_string(cfg, "cheat2_desc", "C");

   dumped = cfg_dump_to_string(cfg, false);
   back   = cfg_from(dumped);

   for (e = back->entries; e; e = e->next)
      if (e->key)
         n++;
   if (n != 4)
   {
      printf("[FAILED] round-trip carries %u entries, expected 4 - "
            "duplicates were written:\n%s", (unsigned)n, dumped);
      abort();
   }
   if (     strcmp(config_get_entry(back, "cheats")->value,      "3")
         || strcmp(config_get_entry(back, "cheat0_desc")->value, "EDITED")
         || strcmp(config_get_entry(back, "cheat1_desc")->value, "B")
         || strcmp(config_get_entry(back, "cheat2_desc")->value, "C"))
   {
      printf("[FAILED] a value did not survive the round trip:\n%s",
            dumped);
      abort();
   }
   free(dumped);
   config_file_free(cfg);
   config_file_free(back);
   printf("[SUCCESS] setting over parsed keys replaces rather than "
         "duplicating\n");
}

/* The list is only ever appended to at conf->tail, so an inserting
 * set no longer walks to find the end.  Quadratic insert behaviour
 * would take this from milliseconds to minutes, so a generous wall
 * clock bound is enough to catch a regression without being flaky. */
static void test_config_set_insert_is_not_quadratic(void)
{
   const int n        = 40000;
   config_file_t *cfg = config_file_new_alloc();
   clock_t t0;
   double elapsed;
   int i;

   if (!cfg)
      abort();
   t0 = clock();
   for (i = 0; i < n; i++)
   {
      char key[32];
      snprintf(key, sizeof(key), "cheat%d_value", i);
      config_set_string(cfg, key, "1");
   }
   elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
   if (elapsed > 5.0)
   {
      printf("[FAILED] %d inserts took %.2fs - the insert path is "
            "walking the list again\n", n, elapsed);
      abort();
   }
   if (!config_get_entry(cfg, "cheat0_value")
         || !config_get_entry(cfg, "cheat39999_value"))
      abort();
   config_file_free(cfg);
   printf("[SUCCESS] %d inserts in %.2fs without the no-duplicates "
         "flag\n", n, elapsed);
}

/* Setter-created entries are slab-allocated like parsed ones, so
 * they must not be free()d individually at teardown and a failed
 * insert must give its slot back.  Both are invisible in a plain
 * run and immediate under the sanitizers this suite runs with, so
 * this exercises the mix that would catch a mistake: pooled entries
 * either side of a parsed one, entries whose values are later
 * replaced and unset, and a config pilfered by append_conf. */
static void test_config_setter_entries_are_pooled(void)
{
   config_file_t *cfg = cfg_from("parsed_a = \"1\"\nparsed_b = \"2\"\n");
   config_file_t *donor;
   struct config_entry_list *entry;
   const struct config_entry_list *e;
   size_t n = 0;
   int i;

   /* Enough to cross the pool's block growth more than once. */
   for (i = 0; i < 200; i++)
   {
      char key[32];
      snprintf(key, sizeof(key), "set_%d", i);
      config_set_string(cfg, key, "v");
   }
   if (!(entry = config_get_entry(cfg, "set_0")))
      abort();
   if (!(entry->flags & CONF_ENTRY_FLG_POOLED))
   {
      printf("[FAILED] a setter-created entry is not pooled\n");
      abort();
   }
   /* Replacing a value must not disturb the entry's pooled-ness -
    * clearing it made teardown free() a pool-interior pointer. */
   config_set_string(cfg, "set_0", "replaced");
   if (!(entry->flags & CONF_ENTRY_FLG_POOLED))
   {
      printf("[FAILED] replacing a value cleared POOLED\n");
      abort();
   }
   config_unset(cfg, "set_1");
   config_set_string(cfg, "parsed_a", "overwritten");

   donor = cfg_from("donor = \"1\"\n");
   config_set_string(donor, "donor_set", "1");
   if (!config_file_append_conf(cfg, donor))
      abort();
   cfg_check_tail(cfg, "after appending a config with pooled entries");

   for (e = cfg->entries; e; e = e->next)
      if (e->key)
         n++;
   /* 2 parsed + 200 set - 1 unset + 2 donor */
   if (n != 203)
   {
      printf("[FAILED] entry count is %u, expected 203\n", (unsigned)n);
      abort();
   }
   if (     strcmp(config_get_entry(cfg, "set_0")->value, "replaced")
         || strcmp(config_get_entry(cfg, "parsed_a")->value, "overwritten")
         || !config_get_entry(cfg, "donor_set"))
      abort();
   config_file_free(cfg);
   printf("[SUCCESS] setter entries share the parser's pool and "
         "survive replace, unset and append\n");
}

int main(void)
{
   test_config_file_parse_contains("foo = \"bar\"\n",   "foo", "bar");
   test_config_file_parse_contains("foo = \"bar\"",     "foo", "bar");
   test_config_file_parse_contains("foo = \"bar\"\r\n", "foo", "bar");
   test_config_file_parse_contains("foo = \"bar\"",     "foo", "bar");

   test_config_file_parse_contains("foo = \"\"\n",   "foo", "");
   test_config_file_parse_contains("foo = \"\"",     "foo", "");
   test_config_file_parse_contains("foo = \"\"\r\n", "foo", "");
   test_config_file_parse_contains("foo = \"\"",     "foo", "");

   test_config_file_parse_contains("foo = \"\"\n",   "bar", NULL);
   test_config_file_parse_contains("foo = \"\"",     "bar", NULL);
   test_config_file_parse_contains("foo = \"\"\r\n", "bar", NULL);
   test_config_file_parse_contains("foo = \"\"",     "bar", NULL);

   test_config_file_short_comments();

   /* Non-numeric input -- must all be rejected. */
   test_config_get_int_rejects("abc");
   test_config_get_int_rejects("");
   test_config_get_int_rejects(".");
   test_config_get_int_rejects("-");
   test_config_get_int_rejects("42abc");     /* trailing garbage */
   test_config_get_int_rejects("42 extra");  /* trailing text after space */

   /* config_get_hex accepts [0-9a-fA-F] -- use characters outside it. */
   test_config_get_hex_rejects("xyz");
   test_config_get_hex_rejects("");
   test_config_get_hex_rejects("g");
   test_config_get_hex_rejects("deadbeefz");  /* trailing non-hex */
   test_config_get_hex_rejects("42 extra");   /* trailing text */

   /* Positive cases -- must still accept normal integers. */
   test_config_get_int_accepts("0",      0);
   test_config_get_int_accepts("42",     42);
   test_config_get_int_accepts("-17",   -17);
   test_config_get_int_accepts("0x10",   16);  /* hex via base-0 detection */
   test_config_get_int_accepts("010",     8);  /* octal  via base-0 detection */

   test_config_file_deinitialize_clears_fields();
   test_config_file_high_bit_bytes_smoke();
   test_config_file_hash_map_agreement();
   test_config_file_stream_matches_from_string();
   test_config_file_stream_nul_ends_stream();
   test_config_file_pathless_reference_no_crash();
   test_config_file_borrowed_entry_lifecycle();
   test_config_file_take_string();
   test_config_take_string();
   test_config_entry_cached_lengths();
   test_config_file_per_config_io();
   test_config_file_append_conf_tail_matrix();
   test_config_get_config_path_pathless();
   test_config_file_dump_after_take_string();
   test_config_set_string_overwrite();
   test_config_file_dump_sort_is_side_effect_free();
   test_config_set_string_on_readonly_entry();
   test_config_file_dump_sort_fallback();
   test_config_tail_is_single_authority();
   test_config_include_order_preserved();
   test_config_set_over_parsed_keys_round_trips();
   test_config_set_insert_is_not_quadratic();
   test_config_setter_entries_are_pooled();
}
