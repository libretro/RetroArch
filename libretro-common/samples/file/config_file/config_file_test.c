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
}
