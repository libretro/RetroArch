/* Unit lanes for the extraction manifest
 * (tasks/task_decompress_manifest.c), called directly on small files.
 * Every mtime a lane depends on is set with path_set_mtime(), so no
 * lane waits for the clock to move.
 *
 * What these lanes pin:
 *
 *   path        - making and freeing an object touches no file; an
 *                 empty folder or archive name makes none.
 *   roundtrip   - the text written, and that it is matched again in
 *                 any order; a wrong size, CRC or name, a changed
 *                 mtime and a deleted file do not match.
 *   mtime_slack - an mtime one second either side still matches, two
 *                 seconds do not.
 *   untouched   - a run that changes nothing leaves the file alone.
 *   pending     - the member being written gets its line only once
 *                 the next member is reached or the walk ends clean.
 *   prune       - lines not asked about go after a clean walk only.
 *   bad_input   - a foreign header or a malformed line is ignored and
 *                 costs only its own member.
 *   names       - names with spaces round-trip, names that cannot be
 *                 one line get none; empty and huge members.
 *   no_mtime    - a written member without an mtime ends recording
 *                 and skipping for the run.
 *   mtime_zero  - an mtime of 0 counts as none.
 *   many        - 3000 lines found again out of order.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../tasks/tasks_internal.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

#define OUT_DIR "decompress_manifest_out"
#define ARCHIVE "some/dir/cheats.zip"
#define LEAF    ".cheats.zip.crc"
#define HEADER  "decompress_manifest 1\n"
#define MTIME   1700000000
#define OLD     1600000000

typedef struct
{
   const char *name;       /* as the archive names it; also its file */
   const char *size_str;
   uint64_t    size;
   uint32_t    crc;
} spec_t;

static const spec_t three[3] = {
   { "f0.cht",     "10", 10, 0x00000a0b },
   { "a/f1.cht",   "11", 11, 0x1a2b3c4d },
   { "b/c/f2.cht", "12", 12, 0xfedcba98 }
};
static const spec_t spec_c       = { "c.cht",    "7", 7, 0x0c0c0c0c };
static const spec_t spec_d       = { "d.cht",    "8", 8, 0x0d0d0d0d };
static const spec_t spec_gone    = { "gone.cht", "9", 9, 0x90909090 };
static const spec_t spec_unknown = { "nope.cht", "10", 10, 0x00000a0b };

static char     manifest[256];       /* the manifest's path */
static char     manifest_tmp[256];
static char     made[16][128];       /* files to delete at the end */
static unsigned n_made;

static void out_path(char *s, size_t len, const char *rel)
{
   fill_pathname_join_special(s, OUT_DIR, rel, len);
}

static void set_mtime(const char *path, int64_t mtime)
{
   CHECK(path_set_mtime(path, mtime), "path_set_mtime failed");
}

static int64_t mtime_of(const char *path)
{
   int64_t mtime = -1;
   if (!path_get_mtime(path, &mtime))
      return -1;
   return mtime;
}

/* Writes @size bytes to @rel in the output folder and sets its mtime */
static void put_file(const char *rel, unsigned size, int64_t mtime)
{
   unsigned i;
   char path[128];
   char data[64];

   out_path(path, sizeof(path), rel);
   memset(data, 'x', sizeof(data));
   CHECK(size <= sizeof(data), "member too large for the test");
   CHECK(filestream_write_file(path, data, size), "could not write a member");
   set_mtime(path, mtime);

   for (i = 0; i < n_made; i++)
      if (!strcmp(made[i], path))
         return;
   if (n_made < ARRAY_SIZE(made))
      strlcpy(made[n_made++], path, sizeof(made[0]));
   else
      CHECK(false, "too many member files for the test");
}

static void put_three(void)
{
   unsigned i;
   for (i = 0; i < 3; i++)
      put_file(three[i].name, (unsigned)three[i].size, MTIME + i);
}

static void clear_manifest(void)
{
   filestream_delete(manifest);
   filestream_delete(manifest_tmp);
}

static void put_manifest(const char *text, size_t len, int64_t mtime)
{
   CHECK(filestream_write_file(manifest, text, (int64_t)len),
         "could not write the manifest");
   set_mtime(manifest, mtime);
}

static bool manifest_is(const char *text)
{
   void   *buf = NULL;
   int64_t len = 0;
   bool    ok;

   if (!filestream_read_file(manifest, &buf, &len))
      return false;
   ok = (len == (int64_t)strlen(text) && !memcmp(buf, text, (size_t)len));
   if (!ok)
      fprintf(stderr, "manifest is:\n%.*s", (int)len, (const char*)buf);
   free(buf);
   return ok;
}

/* Appends the line a member has in the manifest */
static void add_line(char *s, size_t len, const spec_t *f, long mtime)
{
   size_t at = strlen(s);
   snprintf(s + at, len - at, "%08x %s %ld %s\n",
         (unsigned)f->crc, f->size_str, mtime, f->name);
}

static decompress_manifest_t *open_manifest(void)
{
   decompress_manifest_t *m = decompress_manifest_new(OUT_DIR, ARCHIVE);
   CHECK(m != NULL, "decompress_manifest_new failed");
   return m;
}

static bool ask(decompress_manifest_t *m, const spec_t *f)
{
   char path[128];
   out_path(path, sizeof(path), f->name);
   return decompress_manifest_unchanged(m, f->name, path, f->size, f->crc);
}

static void begin(decompress_manifest_t *m, const spec_t *f)
{
   char path[128];
   out_path(path, sizeof(path), f->name);
   decompress_manifest_begin(m, f->name, path, f->size, f->crc);
}

/* The three files and a manifest of their three lines, mtime OLD */
static void put_three_manifest(char *text, size_t len)
{
   unsigned i;
   decompress_manifest_t *m;

   clear_manifest();
   put_three();
   m = open_manifest();
   for (i = 0; i < 3; i++)
      begin(m, &three[i]);
   CHECK(decompress_manifest_save(m, true), "saving three lines failed");
   decompress_manifest_free(m);
   set_mtime(manifest, OLD);

   strlcpy(text, HEADER, len);
   for (i = 0; i < 3; i++)
      add_line(text, len, &three[i], MTIME + i);
}

static void manifest_path(void)
{
   decompress_manifest_t *m;

   clear_manifest();
   m = decompress_manifest_new(OUT_DIR, ARCHIVE);
   CHECK(m != NULL, "decompress_manifest_new failed");
   decompress_manifest_free(m);
   CHECK(!path_is_valid(manifest), "new and free made a manifest file");

   CHECK(!decompress_manifest_new(NULL, ARCHIVE), "NULL folder accepted");
   CHECK(!decompress_manifest_new("", ARCHIVE), "empty folder accepted");
   CHECK(!decompress_manifest_new(OUT_DIR, NULL), "NULL archive accepted");
   CHECK(!decompress_manifest_new(OUT_DIR, ""), "empty archive accepted");
   decompress_manifest_free(NULL);
}

static void manifest_roundtrip(void)
{
   int i;
   char text[512];
   char path[128];
   spec_t wrong;
   decompress_manifest_t *m;

   put_three_manifest(text, sizeof(text));
   CHECK(manifest_is(text), "the manifest's text is not the three lines");

   m = open_manifest();
   for (i = 0; i < 3; i++)
      CHECK(ask(m, &three[i]), "a recorded member did not match in order");
   decompress_manifest_free(m);

   m = open_manifest();
   for (i = 2; i >= 0; i--)
      CHECK(ask(m, &three[i]), "a recorded member did not match in reverse");
   wrong      = three[0];
   wrong.size = 9;
   CHECK(!ask(m, &wrong), "a wrong size matched");
   wrong      = three[0];
   wrong.crc  = 0x00000a0c;
   CHECK(!ask(m, &wrong), "a wrong CRC matched");
   CHECK(!ask(m, &spec_unknown), "an unknown name matched");
   decompress_manifest_free(m);

   out_path(path, sizeof(path), three[1].name);
   set_mtime(path, MTIME + 99);
   out_path(path, sizeof(path), three[2].name);
   CHECK(filestream_delete(path) == 0, "could not delete a member");

   m = open_manifest();
   CHECK(ask(m, &three[0]), "the untouched member did not match");
   CHECK(!ask(m, &three[1]), "a member with another mtime matched");
   CHECK(!ask(m, &three[2]), "a deleted member matched");
   decompress_manifest_free(m);
}

static void manifest_mtime_slack(void)
{
   char text[256];
   char path[128];
   decompress_manifest_t *m;

   clear_manifest();
   put_file(three[0].name, (unsigned)three[0].size, MTIME + 101);
   out_path(path, sizeof(path), three[0].name);
   m = open_manifest();
   begin(m, &three[0]);
   CHECK(decompress_manifest_save(m, true), "save failed");
   decompress_manifest_free(m);
   strlcpy(text, HEADER, sizeof(text));
   add_line(text, sizeof(text), &three[0], MTIME + 101);
   CHECK(manifest_is(text), "the recorded mtime is not the file's");

   m = open_manifest();
   set_mtime(path, MTIME + 100);
   CHECK(ask(m, &three[0]), "an mtime one second earlier did not match");
   set_mtime(path, MTIME + 102);
   CHECK(ask(m, &three[0]), "an mtime one second later did not match");
   CHECK(decompress_manifest_save(m, true), "save failed");
   CHECK(manifest_is(text), "a match through the slack changed the line");
   set_mtime(path, MTIME + 99);
   CHECK(!ask(m, &three[0]), "an mtime two seconds earlier matched");
   set_mtime(path, MTIME + 103);
   CHECK(!ask(m, &three[0]), "an mtime two seconds later matched");
   decompress_manifest_free(m);
}

static void manifest_untouched(void)
{
   unsigned i;
   char text[512];
   decompress_manifest_t *m;

   put_three_manifest(text, sizeof(text));

   m = open_manifest();
   for (i = 0; i < 3; i++)
      CHECK(ask(m, &three[i]), "a recorded member did not match");
   CHECK(decompress_manifest_save(m, true), "save with nothing to do failed");
   decompress_manifest_free(m);
   CHECK(mtime_of(manifest) == OLD, "an unchanged manifest was rewritten");
   CHECK(manifest_is(text), "an unchanged manifest changed");

   m = open_manifest();
   decompress_manifest_free(m);
   CHECK(mtime_of(manifest) == OLD, "new and free rewrote the manifest");
   CHECK(manifest_is(text), "new and free changed the manifest");
   CHECK(!path_is_valid(manifest_tmp), "a temporary file was left behind");
}

static void manifest_pending(void)
{
   char text[256];
   spec_t renewed;
   decompress_manifest_t *m;

   clear_manifest();
   put_three();

   m = open_manifest();
   begin(m, &three[0]);
   CHECK(decompress_manifest_save(m, false), "save with nothing to do failed");
   decompress_manifest_free(m);
   CHECK(!path_is_valid(manifest),
         "the member in flight at a stop was recorded");

   strlcpy(text, HEADER, sizeof(text));
   add_line(text, sizeof(text), &three[0], MTIME);

   m = open_manifest();
   begin(m, &three[0]);
   begin(m, &three[1]);
   CHECK(decompress_manifest_save(m, false), "save failed");
   decompress_manifest_free(m);
   CHECK(manifest_is(text),
         "begin, begin, stop: not exactly the first member's line");

   clear_manifest();
   m = open_manifest();
   begin(m, &three[0]);
   CHECK(!ask(m, &three[1]), "a member with no line matched");
   CHECK(decompress_manifest_save(m, false), "save failed");
   decompress_manifest_free(m);
   CHECK(manifest_is(text),
         "begin, ask, stop: not exactly the first member's line");

   /* The same name written again replaces its line */
   renewed          = three[0];
   renewed.size     = 13;
   renewed.size_str = "13";
   renewed.crc      = 0x13131313;
   put_file(three[0].name, 13, MTIME + 50);
   m = open_manifest();
   begin(m, &renewed);
   CHECK(decompress_manifest_save(m, true), "save failed");
   decompress_manifest_free(m);
   strlcpy(text, HEADER, sizeof(text));
   add_line(text, sizeof(text), &renewed, MTIME + 50);
   CHECK(manifest_is(text), "a rewritten member did not replace its line");
   CHECK(!path_is_valid(manifest_tmp), "a temporary file was left behind");

   /* A folder that is not there is not made, and the save says so */
   m = decompress_manifest_new(OUT_DIR "/missing", ARCHIVE);
   CHECK(m != NULL, "decompress_manifest_new failed");
   if (m)
   {
      begin(m, &three[1]);
      CHECK(!decompress_manifest_save(m, true),
            "a save that could not write reported success");
      decompress_manifest_free(m);
   }
   CHECK(!path_is_directory(OUT_DIR "/missing"), "save made the folder");
}

static void manifest_prune(void)
{
   char text[512];
   decompress_manifest_t *m;

   put_three_manifest(text, sizeof(text));

   m = open_manifest();
   CHECK(ask(m, &three[0]), "a recorded member did not match");
   CHECK(ask(m, &three[2]), "a recorded member did not match");
   CHECK(decompress_manifest_save(m, false), "save failed");
   decompress_manifest_free(m);
   CHECK(mtime_of(manifest) == OLD, "a stopped walk rewrote the manifest");
   CHECK(manifest_is(text), "a stopped walk dropped a line");

   m = open_manifest();
   CHECK(ask(m, &three[0]), "a recorded member did not match");
   CHECK(ask(m, &three[2]), "a recorded member did not match");
   CHECK(decompress_manifest_save(m, true), "save failed");
   decompress_manifest_free(m);
   strlcpy(text, HEADER, sizeof(text));
   add_line(text, sizeof(text), &three[0], MTIME);
   add_line(text, sizeof(text), &three[2], MTIME + 2);
   CHECK(manifest_is(text), "a clean walk kept the line it did not reach");
}

/* @text with its header line replaced by @header */
static void put_with_header(const char *header, const char *text)
{
   char other[640];
   strlcpy(other, header, sizeof(other));
   strlcat(other, text + STRLEN_CONST(HEADER), sizeof(other));
   put_manifest(other, strlen(other), OLD);
}

static void nothing_matches(const char *what)
{
   unsigned i;
   decompress_manifest_t *m = open_manifest();
   for (i = 0; i < 3; i++)
      CHECK(!ask(m, &three[i]), what);
   decompress_manifest_free(m);
}

static void manifest_bad_input(void)
{
   static char blob[4096];
   static char mixed[8192];
   static char long_name[5000];
   static const spec_t x      = { "x", "1", 1, 0 };
   static const spec_t dup_a  = { "d.cht", "8", 8, 0x000000d0 };
   static const spec_t dup_b  = { "d.cht", "8", 8, 0x000000d1 };
   char text[512];
   char line[128];
   char path[128];
   size_t at;
   size_t long_len;
   decompress_manifest_t *m;

   put_three_manifest(text, sizeof(text));
   out_path(path, sizeof(path), three[0].name);

   put_with_header("decompress_manifest 2\n", text);
   nothing_matches("a line under a newer header matched");
   put_with_header("core_updater_crc 1\n", text);
   nothing_matches("a line under a foreign header matched");
   put_with_header("decompress_manifest 10\n", text);
   nothing_matches("a line under a longer header matched");
   put_manifest("", 0, OLD);
   nothing_matches("an empty manifest matched");

   /* The header and two good lines, NULs where the newlines go */
   memset(blob, 0, sizeof(blob));
   at    = STRLEN_CONST(HEADER) - 1;
   memcpy(blob, HEADER, at);
   at++;
   line[0] = '\0';
   add_line(line, sizeof(line), &three[0], MTIME);
   memcpy(blob + at, line, strlen(line) - 1);
   at   += strlen(line);
   line[0] = '\0';
   add_line(line, sizeof(line), &three[1], MTIME + 1);
   memcpy(blob + at, line, strlen(line) - 1);
   put_manifest(blob, sizeof(blob), OLD);
   nothing_matches("a line of a file of NULs matched");

   /* A NUL inside a name ("f0", NUL, "cht") spoils that line only */
   line[0] = '\0';
   add_line(line, sizeof(line), &three[0], MTIME);
   strlcpy(blob, text, sizeof(blob));
   at      = strlen(blob);
   blob[STRLEN_CONST(HEADER) + strlen(line) - STRLEN_CONST(".cht\n")] = '\0';
   put_manifest(blob, at, OLD);
   m = open_manifest();
   CHECK(!ask(m, &three[0]), "a line with a NUL in its name matched");
   CHECK(ask(m, &three[1]), "the line after one with a NUL did not match");
   CHECK(!decompress_manifest_unchanged(m, "f0", path, three[0].size,
            three[0].crc), "a name was taken up to its NUL");
   decompress_manifest_free(m);

   /* Good lines between bad ones */
   put_file(x.name, 1, 1);
   put_file(dup_a.name, 8, MTIME);
   long_len = sizeof(long_name) - 1 - STRLEN_CONST("0000000c 3 1700000000 ");
   memset(long_name, 'y', long_len);
   long_name[long_len] = '\0';

   strlcpy(mixed, HEADER, sizeof(mixed));
   add_line(mixed, sizeof(mixed), &three[0], MTIME);
   strlcat(mixed, "zzzzzzzz 1 1 x\n", sizeof(mixed));
   strlcat(mixed, "0000000a 12\n", sizeof(mixed));
   strlcat(mixed, "0000000b 10 1700000000 \n", sizeof(mixed));
   strlcat(mixed, "0000000c 3 1700000000 ", sizeof(mixed));
   strlcat(mixed, long_name, sizeof(mixed));
   strlcat(mixed, "\n", sizeof(mixed));
   strlcat(mixed, "0000000e 10 -1700000000 neg.cht\n", sizeof(mixed));
   add_line(mixed, sizeof(mixed), &three[1], MTIME + 1);
   mixed[strlen(mixed) - 1] = '\0';
   strlcat(mixed, "\r\n", sizeof(mixed));
   add_line(mixed, sizeof(mixed), &dup_a, MTIME);
   add_line(mixed, sizeof(mixed), &dup_b, MTIME);
   add_line(mixed, sizeof(mixed), &three[2], MTIME + 2);
   at = strlen(mixed) - 1;   /* no newline after the last line */
   CHECK(at > 5000, "the mixed manifest was cut short");
   put_manifest(mixed, at, OLD);

   m = open_manifest();
   CHECK(ask(m, &three[0]), "a good line before bad ones did not match");
   CHECK(ask(m, &three[1]), "a good line ending in CR LF did not match");
   CHECK(ask(m, &dup_a), "the first of two lines with one name lost");
   CHECK(ask(m, &three[2]), "a good last line with no newline did not match");
   decompress_manifest_free(m);

   m = open_manifest();
   CHECK(!ask(m, &dup_b), "the second of two lines with one name won");
   CHECK(ask(m, &dup_a), "the first of two lines with one name lost");
   CHECK(!ask(m, &x), "a line with a CRC that is not hex matched");
   CHECK(!decompress_manifest_unchanged(m, "", path, 12, 0x0000000a),
         "a line cut short matched");
   CHECK(!decompress_manifest_unchanged(m, "", path, 10, 0x0000000b),
         "a line with an empty name matched");
   CHECK(!decompress_manifest_unchanged(m, long_name, path, 3, 0x0000000c),
         "a 5000-byte line matched");
   CHECK(!decompress_manifest_unchanged(m, "neg.cht", path, 10, 0x0000000e),
         "a line with a negative mtime matched");
   CHECK(ask(m, &three[2]), "a good line stopped matching");
   decompress_manifest_free(m);
}

static void manifest_names(void)
{
   static const spec_t ok0     = { "ok0.cht",    "5", 5, 0x000000a0 };
   static const spec_t ok1     = { "ok1.cht",    "6", 6, 0x000000a1 };
   static const spec_t spaced  = { "a/b c.cht",  "4", 4, 0x000000a2 };
   static const spec_t empty   = { "empty.bin",  "0", 0, 0 };
   static const spec_t huge    = { "huge.bin",   "5000000000",
      5000000000ULL, 0xffffffff };
   static const spec_t *good[5] = { &ok0, &ok1, &spaced, &empty, &huge };
   unsigned i;
   char text[512];
   char path[128];
   decompress_manifest_t *m;

   clear_manifest();
   for (i = 0; i < 5; i++)
      put_file(good[i]->name, i == 4 ? 1 : (unsigned)good[i]->size,
            MTIME + 10 + i);
   out_path(path, sizeof(path), ok0.name);

   m = open_manifest();
   begin(m, &ok0);
   decompress_manifest_begin(m, "bad\nname", path, 5, 0x000000b0);
   begin(m, &ok1);
   decompress_manifest_begin(m, "bad\rname", path, 5, 0x000000b1);
   begin(m, &spaced);
   begin(m, &empty);
   begin(m, &huge);
   CHECK(decompress_manifest_save(m, true), "save failed");
   decompress_manifest_free(m);

   strlcpy(text, HEADER, sizeof(text));
   for (i = 0; i < 5; i++)
      add_line(text, sizeof(text), good[i], MTIME + 10 + i);
   CHECK(manifest_is(text),
         "not exactly the lines of the names that fit one line");

   m = open_manifest();
   for (i = 0; i < 5; i++)
      CHECK(ask(m, good[i]), "a recorded name did not match");
   CHECK(!decompress_manifest_unchanged(m, "bad\nname", path, 5, 0x000000b0),
         "a name holding a newline matched");
   CHECK(!decompress_manifest_unchanged(m, "bad\rname", path, 5, 0x000000b1),
         "a name holding a carriage return matched");
   CHECK(!decompress_manifest_unchanged(m, "bad", path, 5, 0x000000b0),
         "a name holding a newline was recorded cut");
   decompress_manifest_free(m);
}

static void manifest_no_mtime(void)
{
   unsigned clean;
   char text[512];
   char path[128];
   decompress_manifest_t *m;

   clear_manifest();
   put_three();
   put_file(spec_c.name, (unsigned)spec_c.size, MTIME + 20);
   put_file(spec_d.name, (unsigned)spec_d.size, MTIME + 21);
   out_path(path, sizeof(path), spec_gone.name);
   CHECK(!path_is_valid(path), "the missing member exists");

   m = open_manifest();
   begin(m, &spec_gone);
   CHECK(decompress_manifest_save(m, true), "save with nothing to do failed");
   decompress_manifest_free(m);
   CHECK(!path_is_valid(manifest), "a member without an mtime was recorded");

   /* Recording and skipping stop at the first written member without
    * an mtime, however the walk ends */
   for (clean = 0; clean < 2; clean++)
   {
      put_three_manifest(text, sizeof(text));
      m = open_manifest();
      CHECK(ask(m, &three[0]), "a recorded member did not match");
      begin(m, &spec_gone);
      CHECK(!ask(m, &three[1]),
            "a member matched after one was written without an mtime");
      begin(m, &spec_c);
      begin(m, &spec_d);
      CHECK(!ask(m, &three[2]),
            "a member matched after one was written without an mtime");
      CHECK(decompress_manifest_save(m, clean != 0), "save failed");
      decompress_manifest_free(m);
      CHECK(manifest_is(text),
            "the lines held before the member without an mtime changed");
      CHECK(mtime_of(manifest) == OLD,
            "the manifest was rewritten with nothing recorded");
   }

   /* A file that is gone costs only its own member */
   put_three_manifest(text, sizeof(text));
   out_path(path, sizeof(path), three[1].name);
   CHECK(filestream_delete(path) == 0, "could not delete a member");
   m = open_manifest();
   CHECK(ask(m, &three[0]), "a recorded member did not match");
   CHECK(!ask(m, &three[1]), "a deleted member matched");
   CHECK(ask(m, &three[2]), "the member after a deleted one did not match");
   begin(m, &spec_c);
   CHECK(decompress_manifest_save(m, true), "save failed");
   decompress_manifest_free(m);
   add_line(text, sizeof(text), &spec_c, MTIME + 20);
   CHECK(manifest_is(text), "recording stopped after a deleted member");
}

/* An mtime of 0 is none: some platforms' stat leaves it 0 (the 3DS's) */
static void manifest_mtime_zero(void)
{
   char text[512];
   char path[128];
   decompress_manifest_t *m;

   clear_manifest();
   put_three();
   put_file(spec_c.name, (unsigned)spec_c.size, 0);
   m = open_manifest();
   begin(m, &spec_c);
   CHECK(decompress_manifest_save(m, true), "save with nothing to do failed");
   decompress_manifest_free(m);
   CHECK(!path_is_valid(manifest), "a member with mtime 0 was recorded");

   put_three_manifest(text, sizeof(text));
   m = open_manifest();
   CHECK(ask(m, &three[0]), "a recorded member did not match");
   begin(m, &spec_c);
   CHECK(!ask(m, &three[1]),
         "a member matched after one was written with mtime 0");
   CHECK(!ask(m, &three[2]),
         "a member matched after one was written with mtime 0");
   CHECK(decompress_manifest_save(m, true), "save failed");
   decompress_manifest_free(m);
   CHECK(manifest_is(text), "a member with mtime 0 changed the manifest");

   strlcpy(text, HEADER, sizeof(text));
   add_line(text, sizeof(text), &three[0], 0);
   put_manifest(text, strlen(text), OLD);
   out_path(path, sizeof(path), three[0].name);
   set_mtime(path, 0);
   m = open_manifest();
   CHECK(!ask(m, &three[0]), "a file with mtime 0 matched its line");
   decompress_manifest_free(m);
}

#define N_MANY 3000

static void many_spec(spec_t *f, char *name, size_t len, unsigned i)
{
   snprintf(name, len, "many/%04u.cht", i);
   f->name     = name;
   f->size_str = "";
   f->size     = i;
   f->crc      = i * 2654435761u;
}

static void manifest_many(void)
{
   int i;
   unsigned hits;
   spec_t f;
   char name[32];
   char path[128];
   decompress_manifest_t *m;

   clear_manifest();
   put_file(three[0].name, (unsigned)three[0].size, MTIME);
   out_path(path, sizeof(path), three[0].name);

   /* One file stands for every member: only its mtime is read */
   m = open_manifest();
   for (i = 0; i < N_MANY; i++)
   {
      many_spec(&f, name, sizeof(name), (unsigned)i);
      decompress_manifest_begin(m, f.name, path, f.size, f.crc);
   }
   CHECK(decompress_manifest_save(m, true), "saving 3000 lines failed");
   decompress_manifest_free(m);

   m = open_manifest();
   for (hits = 0, i = N_MANY - 1; i >= 0; i--)
   {
      many_spec(&f, name, sizeof(name), (unsigned)i);
      if (decompress_manifest_unchanged(m, f.name, path, f.size, f.crc))
         hits++;
   }
   CHECK(hits == N_MANY, "not every line was found in reverse order");
   CHECK(decompress_manifest_save(m, true), "save with nothing to do failed");
   decompress_manifest_free(m);

   m = open_manifest();
   for (hits = 0, i = 0; i < N_MANY; i++)
   {
      many_spec(&f, name, sizeof(name), (unsigned)i);
      if (decompress_manifest_unchanged(m, f.name, path, f.size, f.crc))
         hits++;
   }
   CHECK(hits == N_MANY, "not every line was found in order");
   decompress_manifest_free(m);
}

static void run_lane(const char *name, void (*lane)(void))
{
   unsigned before = failures;
   lane();
   printf("[%s] %s\n", name, failures == before ? "ok" : "FAILED");
}

int main(void)
{
   unsigned i;

   CHECK(path_mkdir(OUT_DIR "/a"), "could not make the output folders");
   CHECK(path_mkdir(OUT_DIR "/b/c"), "could not make the output folders");
   out_path(manifest, sizeof(manifest), LEAF);
   strlcpy(manifest_tmp, manifest, sizeof(manifest_tmp));
   strlcat(manifest_tmp, ".tmp", sizeof(manifest_tmp));

   run_lane("path", manifest_path);
   run_lane("roundtrip", manifest_roundtrip);
   run_lane("mtime_slack", manifest_mtime_slack);
   run_lane("untouched", manifest_untouched);
   run_lane("pending", manifest_pending);
   run_lane("prune", manifest_prune);
   run_lane("bad_input", manifest_bad_input);
   run_lane("names", manifest_names);
   run_lane("no_mtime", manifest_no_mtime);
   run_lane("mtime_zero", manifest_mtime_zero);
   run_lane("many", manifest_many);

   clear_manifest();
   for (i = 0; i < n_made; i++)
      filestream_delete(made[i]);

   if (failures)
   {
      fprintf(stderr, "%u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] decompress_manifest_test\n");
   return 0;
}
