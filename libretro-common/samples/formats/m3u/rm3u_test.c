/* Oracle for formats/m3u/rm3u.c and formats/rm3u_stream.h.
 *
 * The codec's read path was rewritten from a string_list split into
 * a single bounded cursor pass, and the write path from per-line
 * filestream_printf calls into rm3u_dump() + one write.  These
 * lanes pin the rewrite to the old behaviour:
 *
 *   parse      - every line construct the old loader understood
 *                (#LABEL:, #EXTINF:, path|label, plain paths,
 *                comments, CRLF endings, surrounding whitespace,
 *                empty lines, dangling labels) produces the same
 *                entries, with relative paths resolved against the
 *                handle's directory as before.
 *   dump       - the rendered bytes equal, literally, what the old
 *                per-line writer emitted, for all four label types.
 *   round-trip - parse(dump(x)) reproduces x for all label types.
 *   adapter    - rm3u_load_filestream / rm3u_save_filestream /
 *                rm3u_is_m3u_filestream reproduce the old built-in
 *                load / save / is-m3u semantics against real files:
 *                wrong extension rejects, a missing file loads as
 *                an empty handle, an empty file is not "an m3u",
 *                and a save/load cycle is lossless.
 *
 * No threads anywhere in the codec or adapters, so the sanitizer
 * sweep is ASan+UBSan+LSan; a TSan run would exercise nothing. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <formats/rm3u.h>
#include <formats/rm3u_stream.h>
#include <streams/file_stream.h>

static char fixture_dir[256];

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

static bool entry_is(rm3u_t *m3u, size_t idx,
      const char *path, const char *label, const char *full_path)
{
   rm3u_entry_t *e = NULL;
   if (!rm3u_get_entry(m3u, idx, &e) || !e)
      return false;
   if (strcmp(e->path, path) != 0)
      return false;
   if (label && (!e->label || strcmp(e->label, label) != 0))
      return false;
   if (!label && e->label)
      return false;
   if (full_path && strcmp(e->full_path, full_path) != 0)
      return false;
   return true;
}

/* ------------------------------------------------------------------ */

static void lane_parse(void)
{
   char m3u_path[512];
   char full[512];
   rm3u_t *m3u = NULL;

   /* Every construct in one document; CRLF on some lines, spaces
    * around others, a comment, empty lines, a dangling label. */
   static const char data[] =
      "#EXTM3U\r\n"
      "\r\n"
      "#LABEL: First Game  \r\n"
      "disc1.cue\r\n"
      "#EXTINF:0,Second Game\n"
      "  disc2.cue  \n"
      "disc3.cue|Third Game\n"
      "/abs/path/disc4.cue\n"
      "\n"
      "#LABEL:Dangling At EOF";

   snprintf(m3u_path, sizeof(m3u_path), "%s/test.m3u", fixture_dir);
   m3u = rm3u_init(m3u_path);
   CHECK(m3u != NULL, "init failed");
   if (!m3u)
      return;

   CHECK(rm3u_parse(m3u, data), "parse failed");
   CHECK(rm3u_get_size(m3u) == 4,
         "entry count %u, wanted 4", (unsigned)rm3u_get_size(m3u));

   snprintf(full, sizeof(full), "%s/disc1.cue", fixture_dir);
   CHECK(entry_is(m3u, 0, "disc1.cue", "First Game", full),
         "entry 0 mismatch");
   snprintf(full, sizeof(full), "%s/disc2.cue", fixture_dir);
   CHECK(entry_is(m3u, 1, "disc2.cue", "Second Game", full),
         "entry 1 mismatch");
   snprintf(full, sizeof(full), "%s/disc3.cue", fixture_dir);
   CHECK(entry_is(m3u, 2, "disc3.cue", "Third Game", full),
         "entry 2 mismatch");
   CHECK(entry_is(m3u, 3, "/abs/path/disc4.cue", NULL,
         "/abs/path/disc4.cue"), "entry 3 mismatch");

   /* NULL data: an empty playlist, not an error */
   CHECK(rm3u_parse(m3u, NULL), "NULL data must succeed");
   CHECK(rm3u_get_size(m3u) == 4, "NULL data must add nothing");

   rm3u_free(m3u);
   fprintf(stderr, "[pass] parse lane\n");
}

static rm3u_t *make_handle(void)
{
   char m3u_path[512];
   rm3u_t *m3u = NULL;
   char rel[560];

   snprintf(m3u_path, sizeof(m3u_path), "%s/out.m3u", fixture_dir);
   if (!(m3u = rm3u_init(m3u_path)))
      return NULL;

   /* One labelled entry inside the base dir (renders relative),
    * one label-less entry. */
   snprintf(rel, sizeof(rel), "%s/discA.cue", fixture_dir);
   if (!rm3u_add_entry(m3u, rel, "Game A"))
      goto error;
   snprintf(rel, sizeof(rel), "%s/discB.cue", fixture_dir);
   if (!rm3u_add_entry(m3u, rel, NULL))
      goto error;
   return m3u;
error:
   rm3u_free(m3u);
   return NULL;
}

static void lane_dump(void)
{
   static const struct
   {
      enum rm3u_label_type type;
      const char *expected;
   } cases[] = {
      { RM3U_LABEL_NONE,   "discA.cue\ndiscB.cue\n" },
      { RM3U_LABEL_NONSTD, "#LABEL:Game A\ndiscA.cue\ndiscB.cue\n" },
      { RM3U_LABEL_EXTSTD, "#EXTINF:,Game A\ndiscA.cue\ndiscB.cue\n" },
      { RM3U_LABEL_RETRO,  "discA.cue|Game A\ndiscB.cue\n" },
   };
   size_t ci;

   for (ci = 0; ci < sizeof(cases) / sizeof(cases[0]); ci++)
   {
      rm3u_t *m3u = make_handle();
      size_t _len = 0;
      char *out   = NULL;

      CHECK(m3u != NULL, "handle build failed");
      if (!m3u)
         return;
      out = rm3u_dump(m3u, cases[ci].type, &_len);
      CHECK(out != NULL, "dump %u returned NULL", (unsigned)ci);
      if (out)
      {
         CHECK(strcmp(out, cases[ci].expected) == 0,
               "dump %u mismatch:\n  got      \"%s\"\n  expected \"%s\"",
               (unsigned)ci, out, cases[ci].expected);
         CHECK(_len == strlen(cases[ci].expected),
               "dump %u length %u, wanted %u", (unsigned)ci,
               (unsigned)_len, (unsigned)strlen(cases[ci].expected));
         free(out);
      }
      rm3u_free(m3u);
   }

   /* Empty handle: nothing to write */
   {
      char m3u_path[512];
      rm3u_t *m3u = NULL;
      snprintf(m3u_path, sizeof(m3u_path), "%s/empty.m3u", fixture_dir);
      m3u = rm3u_init(m3u_path);
      CHECK(m3u && !rm3u_dump(m3u, RM3U_LABEL_NONE, NULL),
            "empty handle must dump NULL");
      rm3u_free(m3u);
   }
   fprintf(stderr, "[pass] dump lane\n");
}

static void lane_round_trip(void)
{
   enum rm3u_label_type types[] = {
      RM3U_LABEL_NONE, RM3U_LABEL_NONSTD,
      RM3U_LABEL_EXTSTD, RM3U_LABEL_RETRO
   };
   size_t ti;

   for (ti = 0; ti < sizeof(types) / sizeof(types[0]); ti++)
   {
      rm3u_t *src = make_handle();
      rm3u_t *dst = NULL;
      char m3u_path[512];
      char full[560];
      char *out   = NULL;

      CHECK(src != NULL, "handle build failed");
      if (!src)
         return;
      out = rm3u_dump(src, types[ti], NULL);
      CHECK(out != NULL, "round-trip dump %u failed", (unsigned)ti);

      snprintf(m3u_path, sizeof(m3u_path), "%s/out.m3u", fixture_dir);
      dst = rm3u_init(m3u_path);
      CHECK(dst && rm3u_parse(dst, out), "round-trip parse failed");

      if (dst)
      {
         const char *labelA =
               (types[ti] == RM3U_LABEL_NONE) ? NULL : "Game A";
         CHECK(rm3u_get_size(dst) == 2,
               "round-trip %u count %u", (unsigned)ti,
               (unsigned)rm3u_get_size(dst));
         snprintf(full, sizeof(full), "%s/discA.cue", fixture_dir);
         CHECK(entry_is(dst, 0, "discA.cue", labelA, full),
               "round-trip %u entry 0", (unsigned)ti);
         snprintf(full, sizeof(full), "%s/discB.cue", fixture_dir);
         CHECK(entry_is(dst, 1, "discB.cue", NULL, full),
               "round-trip %u entry 1", (unsigned)ti);
      }

      free(out);
      rm3u_free(src);
      rm3u_free(dst);
   }
   fprintf(stderr, "[pass] round-trip lane\n");
}

static bool write_whole(const char *path, const char *data)
{
   FILE *f = fopen(path, "wb");
   if (!f)
      return false;
   if (data && *data)
      fwrite(data, 1, strlen(data), f);
   fclose(f);
   return true;
}

static void lane_adapter(void)
{
   char p_real[512], p_empty[512], p_wrong[512], p_missing[512];
   char p_out[512];

   snprintf(p_real,    sizeof(p_real),    "%s/real.m3u",   fixture_dir);
   snprintf(p_empty,   sizeof(p_empty),   "%s/empty.m3u",  fixture_dir);
   snprintf(p_wrong,   sizeof(p_wrong),   "%s/wrong.txt",  fixture_dir);
   snprintf(p_missing, sizeof(p_missing), "%s/gone.m3u",   fixture_dir);
   snprintf(p_out,     sizeof(p_out),     "%s/save.m3u",   fixture_dir);

   CHECK(write_whole(p_real, "disc1.cue\ndisc2.cue|Two\n"),
         "fixture write failed");
   CHECK(write_whole(p_empty, ""), "fixture write failed");
   CHECK(write_whole(p_wrong, "not an m3u"), "fixture write failed");

   /* is-m3u: one stat, old verdicts */
   CHECK( rm3u_is_m3u_filestream(p_real),   "real.m3u must be m3u");
   CHECK(!rm3u_is_m3u_filestream(p_empty),  "empty file is not an m3u");
   CHECK(!rm3u_is_m3u_filestream(p_wrong),  "wrong ext is not an m3u");
   CHECK(!rm3u_is_m3u_filestream(p_missing),"missing file is not an m3u");
   /* codec-side check stays pure: extension only */
   CHECK( rm3u_is_m3u(p_missing), "pure check is extension-only");

   /* load: wrong ext rejects; missing loads empty; real parses */
   CHECK(!rm3u_load_filestream(p_wrong), "wrong ext must not load");
   {
      rm3u_t *m3u = rm3u_load_filestream(p_missing);
      CHECK(m3u && rm3u_get_size(m3u) == 0,
            "missing file must load as an empty handle");
      rm3u_free(m3u);
   }
   {
      rm3u_t *m3u = rm3u_load_filestream(p_real);
      char full[560];
      CHECK(m3u && rm3u_get_size(m3u) == 2, "real load failed");
      if (m3u)
      {
         snprintf(full, sizeof(full), "%s/disc1.cue", fixture_dir);
         CHECK(entry_is(m3u, 0, "disc1.cue", NULL, full),
               "loaded entry 0");
         snprintf(full, sizeof(full), "%s/disc2.cue", fixture_dir);
         CHECK(entry_is(m3u, 1, "disc2.cue", "Two", full),
               "loaded entry 1");
      }
      rm3u_free(m3u);
   }

   /* save: one write; bytes on disk == rm3u_dump */
   {
      rm3u_t *m3u = NULL;
      char full[560];
      char *expect = NULL;
      uint8_t *got = NULL;
      int64_t got_len = 0;
      size_t expect_len = 0;

      m3u = rm3u_init(p_out);
      CHECK(m3u != NULL, "save handle init");
      snprintf(full, sizeof(full), "%s/discA.cue", fixture_dir);
      CHECK(rm3u_add_entry(m3u, full, "Game A"), "add_entry");
      expect = rm3u_dump(m3u, RM3U_LABEL_RETRO, &expect_len);
      CHECK(expect != NULL, "dump for save");
      CHECK(rm3u_save_filestream(m3u, RM3U_LABEL_RETRO),
            "save_filestream failed");
      CHECK(filestream_read_file(p_out, (void**)&got, &got_len) >= 0,
            "read back failed");
      CHECK(got && (size_t)got_len == expect_len
            && memcmp(got, expect, expect_len) == 0,
            "bytes on disk differ from rm3u_dump");
      free(got);
      free(expect);
      rm3u_free(m3u);
   }
   fprintf(stderr, "[pass] adapter lane\n");
}

int main(void)
{
   char cmd[600];

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/rm3u_fixture_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
   {
      fprintf(stderr, "fixture mkdir failed\n");
      return 1;
   }

   lane_parse();
   lane_dump();
   lane_round_trip();
   lane_adapter();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL rm3u_test: %u failures\n", failures);
      return 1;
   }
   fprintf(stderr, "PASS rm3u_test\n");
   return 0;
}
