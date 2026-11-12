/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* The thumbnail result table, its main-thread store and the task that
 * fills it from <playlist>.lpl.thumbs and the art folders.
 * Fixtures live in ./fixture, inside this directory. The table, engine
 * and task sources are included so lanes can see their state. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include <encodings/crc32.h>
#include <features/features_cpu.h>
#include <formats/rjson.h>
#include <queues/task_queue.h>
#include <vfs/vfs_implementation.h>

#include "../../../core_info.h"
#include "../../../file_path_special.h"
#include "../../../playlist.h"
#include "../../../gfx/gfx_thumbnail_results.c"
#include "../../../libretro-db/libretrodb.h"
#include "../../../tasks/tasks_internal.h"

/* Engine allocations of oom_size bytes fail, to fake running out. */
static size_t oom_size;
static unsigned oom_hits;
static void *oom_malloc(size_t size)
{
   if (oom_size && size == oom_size)
   {
      oom_hits++;
      return NULL;
   }
   return malloc(size);
}
#define malloc oom_malloc
#include "../../../gfx/gfx_thumbnail_match.c"
#undef malloc

/* Bytes the task sums in the current work item */
static size_t crc_item;
static uint32_t counted_crc32(uint32_t crc, const uint8_t *data, size_t len)
{
   crc_item += len;
   return encoding_crc32(crc, data, len);
}
/* rdb records the task reads in the current work item */
static size_t read_item;
typedef struct
{
   libretrodb_scan2_cb cb;
   void *ctx;
} counted_scan_t;
static int counted_record(void *ctx, const uint8_t *key, size_t key_len,
      const uint8_t *key2, size_t key2_len, const char *name,
      size_t name_len, uint64_t offset, const uint64_t *aux)
{
   counted_scan_t *c = (counted_scan_t*)ctx;
   read_item++;
   return c->cb(c->ctx, key, key_len, key2, key2_len, name, name_len,
         offset, aux);
}
static int counted_scan_step(libretrodb_scan_t *scan, unsigned max_records,
      libretrodb_scan2_cb cb, void *ctx)
{
   counted_scan_t c;
   c.cb  = cb;
   c.ctx = ctx;
   return libretrodb_scan_step(scan, max_records, counted_record, &c);
}

/* The task's table adds and sets fail while these are set */
static bool fail_adds;
static bool fail_sets;
static size_t failing_results_add(gfx_thumbnail_results_t *t,
      const char *content_path, const char *label, const char *crc32,
      const char *system)
{
   if (fail_adds)
      return (size_t)-1;
   return gfx_thumbnail_results_add(t, content_path, label, crc32, system);
}
static bool failing_results_set(gfx_thumbnail_results_t *t, size_t rec,
      unsigned type, const char *name)
{
   if (fail_sets)
      return false;
   return gfx_thumbnail_results_set(t, rec, type, name);
}
#define encoding_crc32 counted_crc32
#define libretrodb_scan_step counted_scan_step
#define gfx_thumbnail_results_add failing_results_add
#define gfx_thumbnail_results_set failing_results_set
#include "../../../tasks/task_pl_thumbnail_match.c"
#undef gfx_thumbnail_results_set
#undef gfx_thumbnail_results_add
#undef libretrodb_scan_step
#undef encoding_crc32

#define FIX "fixture"

static unsigned failures;

#define CHECK(cond, ...) \
   do \
   { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

static bool write_text(const char *path, const char *text)
{
   return filestream_write_file(path, text, (int64_t)strlen(text));
}

static const char *find_box(const char *path, const char *label,
      const char *system)
{
   return gfx_thumbnail_results_lookup(path, label, system,
         GFX_THUMBNAIL_RESULT_BOXARTS);
}

static void lane_names(void)
{
   char s[PATH_MAX_LENGTH];
   unsigned had = failures;

   gfx_thumbnail_scrub_name(s, sizeof(s), "A&B*C/D:E`F\"G<H>I?J\\K|L");
   CHECK(string_is_equal(s, "A_B_C_D_E_F_G_H_I_J_K_L"), "scrub: %s", s);
   gfx_thumbnail_system_name(s, sizeof(s), "MAME 2003-Plus.lpl");
   CHECK(string_is_equal(s, "MAME"), "MAME: %s", s);
   gfx_thumbnail_system_name(s, sizeof(s),
         "Sega - Mega Drive - Genesis.lpl|Sega - 32X.lpl");
   CHECK(string_is_equal(s, "Sega - Mega Drive - Genesis"), "first of |: %s", s);
   gfx_thumbnail_system_name(s, sizeof(s), "Nintendo - Game Boy");
   CHECK(string_is_equal(s, "Nintendo - Game Boy"), "no extension: %s", s);
   CHECK(gfx_thumbnail_result_type_of("Named_Boxarts")
         == GFX_THUMBNAIL_RESULT_BOXARTS, "Boxarts");
   CHECK(gfx_thumbnail_result_type_of("Named_Logos")
         == GFX_THUMBNAIL_RESULT_LOGOS, "Logos");
   CHECK(gfx_thumbnail_result_type_of("Off")
         == GFX_THUMBNAIL_RESULT_TYPES, "unknown folder");
   CHECK(string_is_equal(gfx_thumbnail_result_type_dir(
         GFX_THUMBNAIL_RESULT_SNAPS), "Named_Snaps"), "Snaps folder");
   CHECK(gfx_thumbnail_name_allowed("a.png", false), "png");
   CHECK(gfx_thumbnail_name_allowed("a.PNG", false), "PNG");
   CHECK(!gfx_thumbnail_name_allowed("a.jpg", false), "jpg off");
   CHECK(gfx_thumbnail_name_allowed("a.jpg", true), "jpg on");
   CHECK(!gfx_thumbnail_name_allowed("a.txt", true), "txt");
   CHECK(!gfx_thumbnail_name_allowed("png", true), "no extension");
   CHECK(!gfx_thumbnail_name_allowed("../x.png", false), "parent folder");
   CHECK(!gfx_thumbnail_name_allowed("a/x.png", false), "subfolder");
   CHECK(!gfx_thumbnail_name_allowed("a\\x.png", false), "backslash");
   CHECK(gfx_thumbnail_name_allowed("Presumed ...!_.png", false), "dots");
   if (failures == had)
      fprintf(stderr, "[pass] names\n");
}

static void lane_table(void)
{
   const char *key[4];
   const char *names[GFX_THUMBNAIL_RESULT_TYPES];
   unsigned had               = failures;
   gfx_thumbnail_results_t *t = gfx_thumbnail_results_new();
   size_t b                   = gfx_thumbnail_results_add(t,
         "/r/b.nes", "B", "00000000|crc", "Sys");
   size_t a                   = gfx_thumbnail_results_add(t,
         "/r/a.nes", "A", "1234ABCD|crc", "Sys");

   CHECK(t && a == 1 && b == 0, "records in add order");
   CHECK(gfx_thumbnail_results_set(t, a, GFX_THUMBNAIL_RESULT_BOXARTS,
         "A (USA).png"), "set A");
   CHECK(gfx_thumbnail_results_set(t, b, GFX_THUMBNAIL_RESULT_SNAPS,
         "B (Europe).png"), "set B");
   CHECK(string_is_equal(gfx_thumbnail_results_find(t, "/r/a.nes", "A",
         "Sys", GFX_THUMBNAIL_RESULT_BOXARTS), "A (USA).png"),
         "find before sort");
   gfx_thumbnail_results_sort(t);
   CHECK(gfx_thumbnail_results_count(t) == 2, "count");
   gfx_thumbnail_results_get(t, 0, key, names);
   CHECK(string_is_equal(key[0], "/r/a.nes")
         && string_is_equal(key[2], "1234ABCD|crc"), "sorted: %s", key[0]);
   CHECK(!names[GFX_THUMBNAIL_RESULT_SNAPS], "no snap for A");
   CHECK(string_is_equal(gfx_thumbnail_results_find(t, "/r/b.nes", "B",
         "Sys", GFX_THUMBNAIL_RESULT_SNAPS), "B (Europe).png"),
         "find after sort");
   CHECK(!gfx_thumbnail_results_find(t, "/r/b.nes", "B2", "Sys",
         GFX_THUMBNAIL_RESULT_SNAPS), "the label is part of the key");
   CHECK(!gfx_thumbnail_results_find(t, "/r/b.nes", "B", "Other",
         GFX_THUMBNAIL_RESULT_SNAPS), "the system is part of the key");
   CHECK(!gfx_thumbnail_results_find(t, "/r/b.nes", "B", "Sys",
         GFX_THUMBNAIL_RESULT_BOXARTS), "no boxart for B");
   gfx_thumbnail_results_free(t);
   gfx_thumbnail_results_free(NULL);
   if (failures == had)
      fprintf(stderr, "[pass] table\n");
}

static void lane_table_edges(void)
{
   const char *key[4];
   const char *key2[4];
   const char *names[GFX_THUMBNAIL_RESULT_TYPES];
   static char big[20000];
   unsigned had               = failures;
   gfx_thumbnail_results_t *t = gfx_thumbnail_results_new();
   size_t a                   = gfx_thumbnail_results_add(t,
         "/r/a.nes", "A", "", "Sys");
   size_t k1                  = gfx_thumbnail_results_add(t,
         "/r/k.nes", "K", "", "Sys");
   size_t k2                  = gfx_thumbnail_results_add(t,
         "/r/k.nes", "K", "", "Sys");

   gfx_thumbnail_results_set(t, k1, GFX_THUMBNAIL_RESULT_BOXARTS,
         "K box.png");
   gfx_thumbnail_results_set(t, k2, GFX_THUMBNAIL_RESULT_SNAPS,
         "K snap.png");
   gfx_thumbnail_results_get(t, a, key, names);
   gfx_thumbnail_results_get(t, k1, key2, names);
   CHECK(key[2] == key2[2] && key[3] == key2[3],
         "one copy of a shared crc32 and system");
   gfx_thumbnail_results_sort(t);
   CHECK(string_is_equal(gfx_thumbnail_results_find(t, "/r/k.nes", "K",
         "Sys", GFX_THUMBNAIL_RESULT_BOXARTS), "K box.png")
      && string_is_equal(gfx_thumbnail_results_find(t, "/r/k.nes", "K",
         "Sys", GFX_THUMBNAIL_RESULT_SNAPS), "K snap.png"),
         "sorted: every record of a duplicate key is searched");
   CHECK(gfx_thumbnail_results_add(t, "/r/z.nes", "Z", "", "Sys")
         == (size_t)-1 && gfx_thumbnail_results_count(t) == 3,
         "no add after sort");
   gfx_thumbnail_results_free(t);

   memset(big, 'b', sizeof(big) - 1);
   big[sizeof(big) - 1] = '\0';
   t = gfx_thumbnail_results_new();
   a = gfx_thumbnail_results_add(t, "/r/big.nes", "Big", "", "Sys");
   CHECK(gfx_thumbnail_results_set(t, a, GFX_THUMBNAIL_RESULT_BOXARTS, big)
         && gfx_thumbnail_results_set(t, a, GFX_THUMBNAIL_RESULT_SNAPS,
            "after.png"), "set past one block");
   CHECK(string_is_equal(gfx_thumbnail_results_find(t, "/r/big.nes", "Big",
         "Sys", GFX_THUMBNAIL_RESULT_BOXARTS), big)
      && string_is_equal(gfx_thumbnail_results_find(t, "/r/big.nes", "Big",
         "Sys", GFX_THUMBNAIL_RESULT_SNAPS), "after.png"),
         "a name larger than a block");
   gfx_thumbnail_results_free(t);
   if (failures == had)
      fprintf(stderr, "[pass] table edges\n");
}

#define STEPPED_RECS (3 * GFX_THUMBNAIL_MATCH_SORT_STEP + 17)

struct stepped_ref
{
   gfx_thumb_result_t rec;
   size_t seq;
};

static int stepped_ref_cmp(const void *a, const void *b)
{
   const struct stepped_ref *x = (const struct stepped_ref*)a;
   const struct stepped_ref *y = (const struct stepped_ref*)b;
   int r = gfx_thumb_results_cmp_rec(&x->rec, &y->rec);
   return r ? r : (x->seq > y->seq) - (x->seq < y->seq);
}

/* Few keys for many records, so keys repeat; every third has no name. */
static gfx_thumbnail_results_t *stepped_table(size_t n)
{
   gfx_thumbnail_results_t *t = gfx_thumbnail_results_new();
   size_t i;
   for (i = 0; t && i < n; i++)
   {
      char path[32];
      char label[16];
      char name[32];
      unsigned v = (unsigned)((i * 7919) % 16384);
      size_t rec;
      snprintf(path, sizeof(path), "/r/%u.nes", v % 1500);
      snprintf(label, sizeof(label), "L%u", v % 7);
      rec = gfx_thumbnail_results_add(t, path, label,
            (v & 1) ? "" : "00000000|crc", (v & 2) ? "Sys" : "Other");
      if (rec == (size_t)-1)
      {
         gfx_thumbnail_results_free(t);
         return NULL;
      }
      snprintf(name, sizeof(name), "%u-%u.png", v, (unsigned)i);
      if (v % 3)
         gfx_thumbnail_results_set(t, rec,
               v % GFX_THUMBNAIL_RESULT_TYPES, name);
   }
   return t;
}

static bool same_str(const char *a, const char *b)
{
   return (!a || !b) ? a == b : string_is_equal(a, b);
}

/* Stepped equals qsort made stable, and finds the first match in add
 * order, as an unsorted table's linear find does. */
static void lane_table_steps(void)
{
   static const size_t lens[] = { 0, 1, 2, STEPPED_RECS };
   unsigned had = failures;
   size_t n;
   for (n = 0; n < ARRAY_SIZE(lens); n++)
   {
      size_t len                 = lens[n];
      gfx_thumbnail_results_t *a = stepped_table(len);
      gfx_thumbnail_results_t *b = stepped_table(len);
      struct stepped_ref *ref    = (struct stepped_ref*)
         malloc((len + 1) * sizeof(*ref));
      size_t i;
      unsigned type;
      if (!a || !b || !ref)
      {
         CHECK(false, "OOM at %u records", (unsigned)len);
         gfx_thumbnail_results_free(a);
         gfx_thumbnail_results_free(b);
         free(ref);
         continue;
      }
      for (i = 0; i < len; i++)
      {
         ref[i].rec = a->recs[i];
         ref[i].seq = i;
      }
      qsort(ref, len, sizeof(*ref), stepped_ref_cmp);
      while (gfx_thumbnail_results_sort_step(a));
      CHECK(a->sorted && !b->sorted && !a->scratch
            && gfx_thumbnail_results_count(a) == len,
            "sorted at %u records", (unsigned)len);
      for (i = 0; i < len; i++)
      {
         const gfx_thumb_result_t *x = &a->recs[i];
         bool same                   = true;
         if (memcmp(x, &ref[i].rec, sizeof(*x)))
         {
            CHECK(false, "record %u of %u is not qsort's stable order",
                  (unsigned)i, (unsigned)len);
            break;
         }
         for (type = 0; type < GFX_THUMBNAIL_RESULT_TYPES; type++)
            same = same && same_str(gfx_thumbnail_results_find(a,
                     x->key[0], x->key[1], x->key[3], type),
                  gfx_thumbnail_results_find(b, x->key[0], x->key[1],
                     x->key[3], type));
         if (!same)
         {
            CHECK(false, "record %u of %u: not the first match in add order",
                  (unsigned)i, (unsigned)len);
            break;
         }
      }
      gfx_thumbnail_results_free(a);
      gfx_thumbnail_results_free(b);
      free(ref);
   }
   if (failures == had)
      fprintf(stderr, "[pass] table steps\n");
}

/* No step moves more than the step size; the table is shut meanwhile. */
static void lane_table_steps_bounded(void)
{
   const char *key[4];
   const char *names[GFX_THUMBNAIL_RESULT_TYPES];
   const gfx_thumb_result_t *probe = NULL;
   unsigned had                    = failures;
   size_t steps                    = 0;
   size_t most                     = 0;
   size_t i;
   gfx_thumbnail_results_t *t      = stepped_table(STEPPED_RECS);

   key[0] = key[1] = key[2] = NULL;
   for (i = 0; t && i < t->count && !probe; i++)
      if (t->recs[i].names[GFX_THUMBNAIL_RESULT_BOXARTS])
         probe = &t->recs[i];
   CHECK(probe, "a record with a boxart");
   if (probe)
   {
      key[0] = probe->key[0];
      key[1] = probe->key[1];
      key[2] = probe->key[3];
   }
   for (;;)
   {
      const char *base = t ? t->sort.base : NULL;
      size_t moved     = t ? t->sort.moved : 0;
      bool more        = gfx_thumbnail_results_sort_step(t);
      size_t m;
      if (!t)
         break;
      m = (t->sort.base == base) ? t->sort.moved - moved : t->sort.moved;
      if (m > most)
         most = m;
      if (!more)
         break;
      if (!steps++)
      {
         const char *got[4];
         bool cleared = true;
         for (i = 0; i < 4; i++)
            got[i] = "stale";
         for (i = 0; i < GFX_THUMBNAIL_RESULT_TYPES; i++)
            names[i] = "stale";
         gfx_thumbnail_results_get(t, 0, got, names);
         for (i = 0; i < 4; i++)
            cleared = cleared && !got[i];
         for (i = 0; i < GFX_THUMBNAIL_RESULT_TYPES; i++)
            cleared = cleared && !names[i];
         CHECK(!gfx_thumbnail_results_find(t, key[0], key[1], key[2],
               GFX_THUMBNAIL_RESULT_BOXARTS)
            && gfx_thumbnail_results_add(t, "/r/z.nes", "Z", "", "Sys")
               == (size_t)-1
            && !gfx_thumbnail_results_set(t, 0,
               GFX_THUMBNAIL_RESULT_BOXARTS, "z.png")
            && gfx_thumbnail_results_count(t) == STEPPED_RECS,
            "the table is shut while it sorts");
         CHECK(cleared, "a refused get clears its outputs");
      }
   }
   CHECK(most && most <= GFX_THUMBNAIL_MATCH_SORT_STEP,
         "at most %u moves a step, not %u",
         (unsigned)GFX_THUMBNAIL_MATCH_SORT_STEP, (unsigned)most);
   CHECK(steps > STEPPED_RECS / GFX_THUMBNAIL_MATCH_SORT_STEP,
         "%u steps", (unsigned)steps);
   CHECK(t && t->sorted && probe && gfx_thumbnail_results_find(t, key[0],
         key[1], key[2], GFX_THUMBNAIL_RESULT_BOXARTS),
         "found once sorted");
   gfx_thumbnail_results_free(t);
   if (failures == had)
      fprintf(stderr, "[pass] table steps bounded (%u steps, at most %u "
            "moves)\n", (unsigned)steps + 1, (unsigned)most);
}

static void lane_store(void)
{
   char cache[PATH_MAX_LENGTH];
   unsigned had = failures;
   gfx_thumbnail_results_t *t;
   size_t rec;

   CHECK(gfx_thumbnail_results_open("/p/one.lpl")
         == (GFX_THUMBNAIL_RESULTS_LOAD | GFX_THUMBNAIL_RESULTS_RESOLVE),
         "first open needs a load and a resolve");
   t   = gfx_thumbnail_results_new();
   rec = gfx_thumbnail_results_add(t, "/r/a.nes", "A", "", "Sys");
   gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
         "A (USA).png");
   gfx_thumbnail_results_sort(t);
   CHECK(gfx_thumbnail_results_install("/p/one.lpl", t, false),
         "install while kept");
   CHECK(gfx_thumbnail_results_open("/p/one.lpl")
         == GFX_THUMBNAIL_RESULTS_RESOLVE, "loaded, not resolved");
   CHECK(string_is_equal(find_box("/r/a.nes", "A", "Sys"), "A (USA).png"),
         "lookup");
   CHECK(gfx_thumbnail_results_note("/p/one.lpl", "/r/a.nes", "A", "Sys",
         GFX_THUMBNAIL_RESULT_BOXARTS, "A.png"), "note");
   CHECK(string_is_equal(find_box("/r/a.nes", "A", "Sys"), "A.png"),
         "a note comes first");
   CHECK(!gfx_thumbnail_results_note("/p/never.lpl", "/r/a.nes", "A", "Sys",
         GFX_THUMBNAIL_RESULT_BOXARTS, "x.png"), "no note for a playlist not kept");

   t = gfx_thumbnail_results_new();
   gfx_thumbnail_results_sort(t);
   CHECK(gfx_thumbnail_results_install("/p/one.lpl", t, true),
         "install resolved");
   CHECK(gfx_thumbnail_results_open("/p/one.lpl") == 0, "nothing more needed");
   CHECK(string_is_equal(find_box("/r/a.nes", "A", "Sys"), "A.png"),
         "notes outlive a new table");

   gfx_thumbnail_results_open("/p/2.lpl");
   gfx_thumbnail_results_open("/p/3.lpl");
   gfx_thumbnail_results_open("/p/4.lpl");
   gfx_thumbnail_results_open("/p/5.lpl");
   CHECK(!find_box("/r/a.nes", "A", "Sys"), "four newer playlists push it out");
   CHECK(!gfx_thumbnail_results_install("/p/one.lpl",
         gfx_thumbnail_results_new(), true), "no install once pushed out");
   CHECK(gfx_thumbnail_results_open("/p/one.lpl")
         == GFX_THUMBNAIL_RESULTS_LOAD, "reopened: resolved this session");

   path_mkdir(FIX);
   fill_pathname_join_special(cache, FIX, "gone.lpl.thumbs", sizeof(cache));
   CHECK(write_text(cache, "{}"), "write %s", cache);
   fill_pathname_join_special(cache, FIX, "gone.lpl", sizeof(cache));
   gfx_thumbnail_results_open(cache);
   gfx_thumbnail_results_forget(cache);
   strlcat(cache, ".thumbs", sizeof(cache));
   CHECK(!path_is_valid(cache), "forget deletes %s", cache);

   gfx_thumbnail_results_deinit();
   CHECK(gfx_thumbnail_results_open("/p/one.lpl")
         == (GFX_THUMBNAIL_RESULTS_LOAD | GFX_THUMBNAIL_RESULTS_RESOLVE),
         "deinit ends the session");
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] store\n");
}

static void lane_store_order(void)
{
   unsigned had = failures;
   gfx_thumbnail_results_t *t;
   size_t rec;

   gfx_thumbnail_results_open("/p/history.lpl");
   t   = gfx_thumbnail_results_new();
   rec = gfx_thumbnail_results_add(t, "/r/a.nes", "A", "", "Sys");
   gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
         "A (Japan).png");
   gfx_thumbnail_results_sort(t);
   gfx_thumbnail_results_install("/p/history.lpl", t, false);
   gfx_thumbnail_results_open("/p/b.lpl");
   CHECK(gfx_thumbnail_results_note("/p/b.lpl", "/r/a.nes", "A", "Sys",
         GFX_THUMBNAIL_RESULT_BOXARTS, "A.png"), "note");
   CHECK(string_is_equal(find_box("/r/a.nes", "A", "Sys"), "A.png"),
         "a note beats an earlier playlist's table");
   gfx_thumbnail_results_note("/p/b.lpl", "/r/c.nes", "C", "Sys",
         GFX_THUMBNAIL_RESULT_BOXARTS, "C.png");
   CHECK(gfx_thumbnail_results_note("/p/b.lpl", "/r/c.nes", "C", "Sys",
         GFX_THUMBNAIL_RESULT_BOXARTS, "C (USA).png"), "second note");
   CHECK(string_is_equal(find_box("/r/c.nes", "C", "Sys"), "C (USA).png"),
         "the newest note wins");
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] store order\n");
}

static void lane_forget(void)
{
   static char playlist[PATH_MAX_LENGTH];
   static char cache[PATH_MAX_LENGTH];
   unsigned had = failures;
   gfx_thumbnail_results_t *t;
   size_t rec;
   size_t n;

   path_mkdir(FIX);
   gfx_thumbnail_results_open(FIX "/r1.lpl");
   gfx_thumbnail_results_open(FIX "/r2.lpl");
   gfx_thumbnail_results_open(FIX "/r3.lpl");
   gfx_thumbnail_results_install(FIX "/r1.lpl",
         gfx_thumbnail_results_new(), true);
   gfx_thumbnail_results_install(FIX "/r2.lpl",
         gfx_thumbnail_results_new(), true);
   gfx_thumbnail_results_install(FIX "/r3.lpl",
         gfx_thumbnail_results_new(), true);
   gfx_thumbnail_results_forget(FIX "/r1.lpl");
   CHECK(gfx_thumbnail_results_open(FIX "/r1.lpl")
         & GFX_THUMBNAIL_RESULTS_RESOLVE, "forgotten: resolve again");
   CHECK(!(gfx_thumbnail_results_open(FIX "/r2.lpl")
         & GFX_THUMBNAIL_RESULTS_RESOLVE)
      && !(gfx_thumbnail_results_open(FIX "/r3.lpl")
         & GFX_THUMBNAIL_RESULTS_RESOLVE), "the others stay resolved");

   gfx_thumbnail_results_open(FIX "/held.lpl");
   t   = gfx_thumbnail_results_new();
   rec = gfx_thumbnail_results_add(t, "/r/h.nes", "H", "", "Sys");
   gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS, "H.png");
   gfx_thumbnail_results_sort(t);
   gfx_thumbnail_results_install(FIX "/held.lpl", t, false);
   gfx_thumbnail_results_note(FIX "/held.lpl", "/r/n.nes", "N", "Sys",
         GFX_THUMBNAIL_RESULT_BOXARTS, "N.png");
   CHECK(string_is_equal(find_box("/r/h.nes", "H", "Sys"), "H.png")
      && string_is_equal(find_box("/r/n.nes", "N", "Sys"), "N.png"),
         "held");
   gfx_thumbnail_results_forget(FIX "/held.lpl");
   CHECK(!find_box("/r/h.nes", "H", "Sys")
      && !find_box("/r/n.nes", "N", "Sys"), "forget drops table and notes");

   /* A task that ends after its playlist is gone */
   gfx_thumbnail_results_open(FIX "/late.lpl");
   gfx_thumbnail_results_forget(FIX "/late.lpl");
   t   = gfx_thumbnail_results_new();
   rec = gfx_thumbnail_results_add(t, "/r/l.nes", "L", "", "Sys");
   gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS, "L.png");
   gfx_thumbnail_results_sort(t);
   CHECK(!gfx_thumbnail_results_install(FIX "/late.lpl", t, true)
      && !find_box("/r/l.nes", "L", "Sys"), "no table after forget");
   CHECK(gfx_thumbnail_results_open(FIX "/late.lpl")
         & GFX_THUMBNAIL_RESULTS_RESOLVE, "not marked resolved after forget");

   /* <playlist>.thumbs cut short to <playlist>.th */
   n = strlcpy(playlist, FIX "/", sizeof(playlist));
   while (n + STRLEN_CONST("./t.lpl") <= sizeof(playlist) - 4)
      n = strlcat(playlist, "./", sizeof(playlist));
   while (n + STRLEN_CONST("t.lpl") < sizeof(playlist) - 4)
      n = strlcat(playlist, "x", sizeof(playlist));
   strlcat(playlist, "t.lpl", sizeof(playlist));
   strlcpy(cache, playlist, sizeof(cache));
   strlcat(cache, ".th", sizeof(cache));
   if (write_text(cache, "x"))
   {
      gfx_thumbnail_results_forget(playlist);
      CHECK(path_is_valid(cache), "a cut-short cache path is not deleted");
      filestream_delete(cache);
   }
   else
      fprintf(stderr, "[skip] a %u-byte path is not writable here\n",
            (unsigned)strlen(cache));
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] forget\n");
}

/* Virtual clock: NBIO_XFER_TICK_USEC is 4000, so 1500 per reading
 * ends the window after a couple of work items. */
static retro_time_t clock_now;
static retro_time_t clock_step;
static unsigned notified;

/* The threaded lane runs with a step of 0, so the worker only reads. */
retro_time_t cpu_features_get_time_usec(void)
{
   if (clock_step)
      clock_now += clock_step;
   return clock_now;
}
uint64_t cpu_features_get(void) { return 0; }
/* History entries' core: its info lists the NES database first. */
#define HIST_CORE "/cores/nes_libretro.so"
static core_info_t hist_core;
bool core_info_find(const char *core_path, core_info_t **core_info)
{
   bool found = hist_core.databases && string_is_equal(core_path, HIST_CORE);
   if (core_info)
      *core_info = found ? &hist_core : NULL;
   return found;
}
bool core_info_core_file_id_is_equal(const char *a, const char *b) { return false; }
void frontend_driver_attach_console(void) { }
void frontend_driver_detach_console(void) { }
void ui_companion_driver_notify_refresh(void) { notified++; }

/* The menu shows the cached playlist; a refresh reloads its art. */
static unsigned refreshed;
static void count_refresh(void *data, size_t i) { refreshed++; }
static menu_ctx_driver_t refresh_driver;
static struct menu_state menu_stub;
struct menu_state *menu_state_get_ptr(void) { return &menu_stub; }

/* One work item per check: past the window at the second reading */
#define ONE_ITEM 5000

#define SYS     "Nintendo - Nintendo Entertainment System"
#define DB_NAME SYS ".lpl"

typedef struct
{
   const char *path;
   const char *label;
   const char *crc32;
} fx_entry_t;

static const fx_entry_t fx_entries[] =
{
   { "/roms/zelda.nes",           "Legend of Zelda, The (U) [!]", "00000000|crc" },
   { "/roms/smb.nes",             "super mario bros",             "00000000|crc" },
   { "/roms/poke.nes",            "Pokemon Yellow (U)",           "00000000|crc" },
   { "/roms/tetris-tengen.nes",   "Tetris (USA) (Tengen) (Unl)",  "00000000|crc" },
   { "/roms/metroid.nes",         "Metroid (USA)",                "00000000|crc" },
   { "/roms/Kid Icarus (USA).nes", "",                            "00000000|crc" }
};
#define FX_COUNT (sizeof(fx_entries) / sizeof(fx_entries[0]))

static char pl_path[PATH_MAX_LENGTH];
static char cache_path[PATH_MAX_LENGTH];
static char thumbs_dir[PATH_MAX_LENGTH];
static char db_dir[PATH_MAX_LENGTH];

static unsigned cache_reads;
static bool forget_at_rename;

static struct retro_vfs_file_handle *shim_open(const char *path,
      unsigned mode, unsigned hints)
{
   if (     mode == RETRO_VFS_FILE_ACCESS_READ
         && string_is_equal(path, cache_path))
      cache_reads++;
   return retro_vfs_file_open_impl(path, mode, hints);
}

/* The main thread's forget, landing after the commit checked the
 * playlist and before its rename */
static int shim_rename(const char *old_path, const char *new_path)
{
   if (forget_at_rename && string_is_equal(new_path, cache_path))
   {
      forget_at_rename = false;
      filestream_delete(pl_path);
      gfx_thumbnail_results_forget(pl_path);
   }
   return retro_vfs_file_rename_impl(old_path, new_path);
}

static void shim_vfs(void)
{
   static struct retro_vfs_interface iface;
   struct retro_vfs_interface_info info;

   iface.get_path = retro_vfs_file_get_path_impl;
   iface.open     = shim_open;
   iface.close    = retro_vfs_file_close_impl;
   iface.size     = retro_vfs_file_size_impl;
   iface.tell     = retro_vfs_file_tell_impl;
   iface.seek     = retro_vfs_file_seek_impl;
   iface.read     = retro_vfs_file_read_impl;
   iface.write    = retro_vfs_file_write_impl;
   iface.flush    = retro_vfs_file_flush_impl;
   iface.remove   = retro_vfs_file_remove_impl;
   iface.rename   = shim_rename;
   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                      = &iface;
   filestream_vfs_init(&info);
}

static void raw(rjsonwriter_t *w, const char *s)
{
   rjsonwriter_raw(w, s, (int)strlen(s));
}

static bool write_playlist(const fx_entry_t *e, size_t n)
{
   size_t i;
   int _len;
   bool ok;
   rjsonwriter_t *w = rjsonwriter_open_memory();
   raw(w, "{\"version\": \"1.5\", \"items\": [");
   for (i = 0; i < n; i++)
   {
      raw(w, i ? ", {\"path\": " : "{\"path\": ");
      rjsonwriter_add_string(w, e[i].path);
      raw(w, ", \"label\": ");
      rjsonwriter_add_string(w, e[i].label);
      raw(w, ", \"core_path\": \"DETECT\", \"core_name\": \"DETECT\", \"crc32\": ");
      rjsonwriter_add_string(w, e[i].crc32);
      raw(w, ", \"db_name\": ");
      rjsonwriter_add_string(w, DB_NAME);
      raw(w, "}");
   }
   raw(w, "]}\n");
   ok = write_text(pl_path, rjsonwriter_get_memory_buffer(w, &_len));
   rjsonwriter_free(w);
   return ok;
}

static void art(const char *type_dir, const char *name, bool present)
{
   char path[PATH_MAX_LENGTH];
   fill_pathname_join_special(path, thumbs_dir, SYS, sizeof(path));
   fill_pathname_join_special(path, path, type_dir, sizeof(path));
   path_mkdir(path);
   fill_pathname_join_special(path, path, name, sizeof(path));
   /* Wine names files in the Unix locale's charset, so an accented
    * name needs a UTF-8 locale there. */
   if (present)
      CHECK(write_text(path, "png"), "write %s", path);
   else
      filestream_delete(path);
}

/* The names fixture_init puts in Named_Boxarts */
#define FX_BOXARTS 7

static void fixture_init(void)
{
   /* A fresh frame window: a frozen clock never ends a spent one */
   clock_now  += 20000;
   clock_step  = 0;
   notified    = 0;
   refreshed   = 0;
   cache_reads = 0;
   refresh_driver.refresh_thumbnail_image = count_refresh;
   menu_stub.driver_ctx                   = &refresh_driver;
   path_mkdir(FIX);
   fill_pathname_join_special(pl_path, FIX, "Test.lpl", sizeof(pl_path));
   strlcpy(cache_path, pl_path, sizeof(cache_path));
   strlcat(cache_path, ".thumbs", sizeof(cache_path));
   fill_pathname_join_special(thumbs_dir, FIX, "thumbnails", sizeof(thumbs_dir));
   fill_pathname_join_special(db_dir, FIX, "db", sizeof(db_dir));
   filestream_delete(cache_path);
   art("Named_Boxarts", "Legend of Zelda, The (USA).png", true);
   art("Named_Snaps",   "Legend of Zelda, The (Europe).png", true);
   art("Named_Boxarts", "Super Mario Bros. (World).png", true);
   art("Named_Boxarts", "Pok\xc3\xa9mon Yellow (USA).png", true);
   art("Named_Boxarts", "Tetris.png", true);
   art("Named_Boxarts", "Tetris (USA).png", true);
   art("Named_Boxarts", "Metroid (USA).png", true);
   art("Named_Boxarts", "Kid Icarus (USA).png", true);
   write_playlist(fx_entries, FX_COUNT);
}

static bool any_finder(retro_task_t *task, void *userdata)
{
   return true;
}

static bool queue_busy(void)
{
   task_finder_data_t find_data;
   find_data.func     = any_finder;
   find_data.userdata = NULL;
   return task_queue_find(&find_data);
}

/* @userdata, when given, takes the resolve's state */
static bool resolve_finder(retro_task_t *task, void *userdata)
{
   pl_thumb_match_t *m = (pl_thumb_match_t*)task->state;
   if (     task->handler != task_pl_thumbnail_match_handler || !m
         || !(m->flags & PL_THUMB_MATCH_FLAG_RESOLVE))
      return false;
   if (userdata)
      *(pl_thumb_match_t**)userdata = m;
   return true;
}

/* Unthreaded only: a worker writes the flags. */
static bool resolve_queued(void)
{
   task_finder_data_t find_data;
   find_data.func     = resolve_finder;
   find_data.userdata = NULL;
   return task_queue_find(&find_data);
}

static pl_thumb_match_t *resolve_state(void)
{
   task_finder_data_t find_data;
   pl_thumb_match_t *m = NULL;
   find_data.func      = resolve_finder;
   find_data.userdata  = &m;
   task_queue_find(&find_data);
   return m;
}

static bool resolve_seen;

/* Tasks that finished leaving work for the main thread's cleanup */
static unsigned leftovers;
static void count_leftovers(retro_task_t *task, retro_time_t took)
{
   pl_thumb_match_t *m = (pl_thumb_match_t*)task->state;
   if (     task->handler == task_pl_thumbnail_match_handler && m
         && m->status == PL_THUMB_MATCH_DONE
         && (     m->dir || m->index || m->playlist || m->want || m->hits
               || m->rdbs || m->system_art || m->scratch || m->scan))
      leftovers++;
}

static bool push(const char *dir_database)
{
   playlist_config_t config;
   memset(&config, 0, sizeof(config));
   config.capacity = 8192;
   playlist_config_set_path(&config, pl_path);
   return task_push_pl_thumbnail_match(&config, NULL, thumbs_dir,
         dir_database, false);
}

/* Pumps the queue dry; returns the checks it took. A finished task's
 * callback runs on a later check and may push the next task, so stop
 * only after three checks that found nothing queued. */
static unsigned run(retro_time_t step)
{
   unsigned ticks = 0;
   unsigned idle  = 0;
   if (clock_step != step)
      clock_step = step;
   while (idle < 3 && ticks < 1000000)
   {
      task_queue_check();
      ticks++;
      idle = queue_busy() ? 0 : idle + 1;
      if (!task_queue_is_threaded() && resolve_queued())
         resolve_seen = true;
   }
   return ticks;
}

static const char *box(size_t i)
{
   return find_box(fx_entries[i].path, fx_entries[i].label, SYS);
}

static char *read_cache(void)
{
   void *buf   = NULL;
   int64_t len = 0;
   if (!filestream_read_file(cache_path, &buf, &len))
      return NULL;
   return (char*)buf;
}

static void check_first_session(const char *what)
{
   char *cache;
   CHECK(string_is_equal(box(0), "Legend of Zelda, The (USA).png"),
         "%s: articles and GoodTools tags: %s", what, box(0));
   CHECK(string_is_equal(gfx_thumbnail_results_lookup(fx_entries[0].path,
         fx_entries[0].label, SYS, GFX_THUMBNAIL_RESULT_SNAPS),
         "Legend of Zelda, The (Europe).png"), "%s: types resolve apart", what);
   CHECK(string_is_equal(box(1), "Super Mario Bros. (World).png"),
         "%s: case and punctuation: %s", what, box(1));
   CHECK(string_is_equal(box(2), "Pok\xc3\xa9mon Yellow (USA).png"),
         "%s: accents: %s", what, box(2));
   CHECK(!box(3), "%s: the Tengen entry gets nothing: %s", what, box(3));
   CHECK(!box(4), "%s: a label match is left to the menu", what);
   CHECK(!find_box(fx_entries[5].path, "Kid Icarus (USA)", SYS),
         "%s: a file-name match is left to the menu", what);
   cache = read_cache();
   CHECK(cache && strstr(cache, "\"version\": \"1\"")
         && strstr(cache, "Super Mario Bros. (World).png")
         && !strstr(cache, "Metroid") && !strstr(cache, "Tengen"),
         "%s: cache contents", what);
   free(cache);
}

static void lane_resolve(bool threaded)
{
   char *before;
   char *after;
   unsigned had = failures;

   fixture_init();
   task_queue_init(threaded, NULL);
   notified = 0;
   CHECK(push(""), "push");
   /* Threaded, the load may already be done and gone from the queue */
   if (!threaded)
      CHECK(!push(""), "a second push while one runs is refused");
   run(0);
   CHECK(threaded || cache_reads == 1,
         "the resolve takes the load's crc: %u reads", cache_reads);
   check_first_session(threaded ? "threaded" : "first session");
   CHECK(notified == 1, "a new table tells the companion: %u", notified);

   /* A new session over an unchanged folder does not rewrite. */
   before = read_cache();
   gfx_thumbnail_results_deinit();
   notified    = 0;
   cache_reads = 0;
   CHECK(push(""), "push again");
   run(0);
   CHECK(threaded || cache_reads == 1,
         "one read of the cache a session: %u", cache_reads);
   after = read_cache();
   check_first_session("second session");
   CHECK(notified == 0, "nothing changed: no notice");
   CHECK(before && after && string_is_equal(before, after), "cache unchanged");
   free(before);
   free(after);
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] resolve (%s)\n", threaded ? "threaded" : "unthreaded");
}

static void lane_changes(void)
{
   fx_entry_t renamed[FX_COUNT];
   char *cache;
   unsigned had = failures;

   fixture_init();
   task_queue_init(false, NULL);
   push("");
   run(0);

   /* A label rename and a deleted file re-resolve next session. */
   memcpy(renamed, fx_entries, sizeof(renamed));
   renamed[0].label = "Legend of Zelda, The (E)";
   write_playlist(renamed, FX_COUNT);
   art("Named_Boxarts", "Pok\xc3\xa9mon Yellow (USA).png", false);
   gfx_thumbnail_results_deinit();
   push("");
   run(0);
   CHECK(string_is_equal(find_box(renamed[0].path, renamed[0].label, SYS),
         "Legend of Zelda, The (USA).png"), "renamed label resolves");
   CHECK(!box(0), "the old label no longer matches");
   CHECK(!box(2), "a deleted file is dropped");
   cache = read_cache();
   CHECK(cache && !strstr(cache, "Pok"), "cache drops the deleted file");
   free(cache);

   /* A corrupt cache is ignored and rewritten. */
   write_playlist(fx_entries, FX_COUNT);
   art("Named_Boxarts", "Pok\xc3\xa9mon Yellow (USA).png", true);
   write_text(cache_path, "{ not json");
   gfx_thumbnail_results_deinit();
   push("");
   run(0);
   check_first_session("after a corrupt cache");

   /* Another version is not read, even before the resolve ends. */
   write_text(cache_path, "{\"version\": \"2\", \"items\": [{\"path\": "
         "\"/roms/zelda.nes\", \"label\": \"Legend of Zelda, The (U) [!]\", "
         "\"crc32\": \"\", \"system\": \"" SYS "\", "
         "\"Named_Boxarts\": \"Bogus.png\"}]}");
   gfx_thumbnail_results_deinit();
   push("");
   clock_step = 0;
   while (queue_busy())
   {
      task_queue_check();
      CHECK(!box(0) || !string_is_equal(box(0), "Bogus.png"),
            "version 2 was read");
   }
   check_first_session("after a version 2 cache");
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] changes\n");
}

/* A playlist deleted while its cache is written gets none. */
static void lane_deleted(void)
{
   char tmp[PATH_MAX_LENGTH];
   bool seen    = false;
   unsigned had = failures;

   fixture_init();
   filestream_atomic_temp_name(tmp, sizeof(tmp), cache_path);
   task_queue_init(false, NULL);
   push("");
   clock_step = 1500;
   while (!seen && queue_busy())
   {
      task_queue_check();
      seen = path_is_valid(tmp);
   }
   CHECK(seen, "the cache is written over several checks");
   filestream_delete(pl_path);
   gfx_thumbnail_results_forget(pl_path);
   run(1500);
   CHECK(!path_is_valid(cache_path) && !path_is_valid(tmp),
         "no cache for a deleted playlist");
   CHECK(gfx_thumbnail_results_open(pl_path) & GFX_THUMBNAIL_RESULTS_RESOLVE,
         "a deleted playlist is not marked resolved");
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] deleted\n");
}

static void lane_load_first(void)
{
   unsigned had   = failures;
   bool shown     = false;
   bool pending   = false;

   fixture_init();
   task_queue_init(false, NULL);
   push("");
   run(0);

   /* Unthreaded: the load's callback pushes the resolve, which can
    * only run on a later check, so the cached table shows first. */
   gfx_thumbnail_results_deinit();
   push("");
   clock_step = 0;
   while (queue_busy() && !shown)
   {
      task_queue_check();
      if ((shown = (box(1) != NULL)))
         pending = queue_busy();
   }
   CHECK(shown && pending, "cached matches show before the resolve ends");
   run(0);

   /* Pushed out and reopened in the same session: load only. */
   write_text(cache_path, "{\"version\": \"1\", \"items\": [{\"path\": "
         "\"/roms/smb.nes\", \"label\": \"super mario bros\", "
         "\"crc32\": \"00000000|crc\", \"system\": \"" SYS "\", "
         "\"Named_Boxarts\": \"Hand Edited.png\"}]}");
   gfx_thumbnail_results_open("/p/a.lpl");
   gfx_thumbnail_results_open("/p/b.lpl");
   gfx_thumbnail_results_open("/p/c.lpl");
   gfx_thumbnail_results_open("/p/d.lpl");
   CHECK(!box(1), "pushed out");
   push("");
   run(0);
   CHECK(string_is_equal(box(1), "Hand Edited.png"),
         "resolved once per session: reopening only loads");

   /* A table whose playlist was pushed out meanwhile is dropped, and
    * no resolve follows it. */
   gfx_thumbnail_results_deinit();
   push("");
   gfx_thumbnail_results_open("/p/a.lpl");
   gfx_thumbnail_results_open("/p/b.lpl");
   gfx_thumbnail_results_open("/p/c.lpl");
   gfx_thumbnail_results_open("/p/d.lpl");
   resolve_seen = false;
   run(0);
   CHECK(!box(1), "no table for a playlist no longer kept");
   CHECK(!resolve_seen, "no resolve for a playlist no longer kept");

   /* Pushed out while its resolve runs: still marked resolved, so the
    * next open only loads. */
   gfx_thumbnail_results_deinit();
   push("");
   clock_step = ONE_ITEM;
   while (queue_busy() && !resolve_queued())
      task_queue_check();
   CHECK(resolve_queued(), "the resolve runs");
   gfx_thumbnail_results_open("/p/a.lpl");
   gfx_thumbnail_results_open("/p/b.lpl");
   gfx_thumbnail_results_open("/p/c.lpl");
   gfx_thumbnail_results_open("/p/d.lpl");
   run(ONE_ITEM);
   CHECK(!box(1), "no table once pushed out");
   CHECK(gfx_thumbnail_results_open(pl_path)
         == GFX_THUMBNAIL_RESULTS_LOAD,
         "pushed out while resolving: its next open only loads");
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] load first\n");
}

/* History entries without a db_name take the folder their core's info
 * names; core info is read when the task is pushed. */
static void lane_history(void)
{
   char hist[128];
   char hist_cache[128];
   playlist_config_t config;
   playlist_t *playlist;
   void *buf    = NULL;
   int64_t len  = 0;
   char *cache  = NULL;
   unsigned had = failures;

   fixture_init();
   fill_pathname_join_special(hist, FIX, FILE_PATH_CONTENT_HISTORY,
         sizeof(hist));
   strlcpy(hist_cache, hist, sizeof(hist_cache));
   strlcat(hist_cache, ".thumbs", sizeof(hist_cache));
   filestream_delete(hist_cache);
   write_text(hist, "{\"version\": \"1.5\", \"items\": ["
         "{\"path\": \"/roms/smb.nes\", \"label\": \"super mario bros\", "
         "\"core_path\": \"" HIST_CORE "\", \"core_name\": \"NES\", "
         "\"crc32\": \"\", \"db_name\": \"\"}, "
         "{\"path\": \"/roms/metroid.nes\", \"label\": \"Metroid (U)\", "
         "\"core_path\": \"/cores/other_libretro.so\", "
         "\"core_name\": \"Other\", \"crc32\": \"\", \"db_name\": \"\"}, "
         "{\"path\": \"/roms/" SYS "/kid.nes\", \"label\": \"Kid Icarus (U)\", "
         "\"core_path\": \"/cores/other_libretro.so\", "
         "\"core_name\": \"Other\", \"crc32\": \"\", \"db_name\": \"\"}]}\n");
   memset(&config, 0, sizeof(config));
   config.capacity = 8192;
   playlist_config_set_path(&config, hist);
   hist_core.databases = (char*)SYS "|Nintendo - Family Computer Disk System";

   task_queue_init(false, NULL);
   playlist = playlist_init(&config);
   CHECK(playlist && playlist_size(playlist) == 3, "history loads");
   CHECK(task_push_pl_thumbnail_match(&config, playlist, thumbs_dir, "",
         false), "push history");
   /* The task keeps nothing of the caller's playlist */
   playlist_free(playlist);
   run(0);
   CHECK(string_is_equal(find_box("/roms/smb.nes", "super mario bros", SYS),
         "Super Mario Bros. (World).png"),
         "no db_name: the folder of the entry's core");
   CHECK(string_is_equal(find_box("/roms/" SYS "/kid.nes", "Kid Icarus (U)",
         SYS), "Kid Icarus (USA).png"),
         "a core naming no database: the content's folder");
   CHECK(!find_box("/roms/metroid.nes", "Metroid (U)", SYS),
         "another core's database is not borrowed");
   if (filestream_read_file(hist_cache, &buf, &len))
      cache = (char*)buf;
   CHECK(cache && strstr(cache, "/roms/smb.nes") && strstr(cache, SYS),
         "the history cache holds the match");
   free(cache);

   /* Without the caller's playlist those entries are left to the menu */
   gfx_thumbnail_results_deinit();
   filestream_delete(hist_cache);
   CHECK(task_push_pl_thumbnail_match(&config, NULL, thumbs_dir, "",
         false), "push history without its playlist");
   run(0);
   CHECK(!find_box("/roms/smb.nes", "super mario bros", SYS),
         "no playlist given: no folder");
   hist_core.databases = NULL;
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] history\n");
}

static void lane_pacing(void)
{
   unsigned had = failures;
   unsigned unpaced, paced;

   fixture_init();
   task_queue_init(false, NULL);
   push("");
   unpaced = run(0);
   gfx_thumbnail_results_deinit();
   filestream_delete(cache_path);
   push("");
   paced = run(1500);
   CHECK(paced > unpaced, "work is spread: %u paced vs %u unpaced",
         paced, unpaced);
   check_first_session("paced");
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] pacing (%u paced vs %u unpaced)\n",
            paced, unpaced);
}

/* A MsgPack writer of its own, so the fixture is not built by the
 * library under test (as samples/tasks/database does). */
typedef struct { uint8_t *data; size_t len, cap; } mp_buf_t;

static void mp_put(mp_buf_t *b, const void *p, size_t n)
{
   if (b->len + n > b->cap)
   {
      size_t want = b->cap ? b->cap * 2 : 256;
      while (want < b->len + n)
         want *= 2;
      b->data = (uint8_t*)realloc(b->data, want);
      b->cap  = want;
   }
   memcpy(b->data + b->len, p, n);
   b->len += n;
}
static void mp_byte(mp_buf_t *b, uint8_t v) { mp_put(b, &v, 1); }
static void mp_str(mp_buf_t *b, const char *s)
{
   size_t n = strlen(s);
   if (n > 255)
   {
      mp_byte(b, 0xda); /* str 16 */
      mp_byte(b, (uint8_t)(n >> 8));
   }
   else
      mp_byte(b, 0xd9); /* str 8 */
   mp_byte(b, (uint8_t)n);
   mp_put(b, s, n);
}
static void mp_bin(mp_buf_t *b, const void *p, uint8_t n)
{
   mp_byte(b, 0xc4);
   mp_byte(b, n);
   mp_put(b, p, n);
}
static void mp_crc_item(mp_buf_t *b, const char *name, uint32_t c)
{
   uint8_t crc[4];
   crc[0] = (uint8_t)(c >> 24); crc[1] = (uint8_t)(c >> 16);
   crc[2] = (uint8_t)(c >> 8);  crc[3] = (uint8_t)c;
   mp_byte(b, 0x82);
   mp_str(b, "name");
   mp_str(b, name);
   mp_str(b, "crc");
   mp_bin(b, crc, 4);
}
static void mp_serial_item(mp_buf_t *b, const char *name, const char *serial)
{
   mp_byte(b, 0x82);
   mp_str(b, "name");
   mp_str(b, name);
   mp_str(b, "serial");
   mp_bin(b, serial, (uint8_t)strlen(serial));
}

/* Header, records, nil, then the {count} map the header points at. */
static bool write_rdb(const char *path, mp_buf_t *body, uint32_t count)
{
   mp_buf_t file;
   uint64_t off = 16 + (uint64_t)body->len + 1;
   int i;
   bool ok;

   memset(&file, 0, sizeof(file));
   mp_put(&file, "RARCHDB", 7);
   mp_byte(&file, 0);
   for (i = 0; i < 8; i++)
      mp_byte(&file, (uint8_t)(off >> (56 - 8 * i)));
   mp_put(&file, body->data, body->len);
   mp_byte(&file, 0xc0);
   mp_byte(&file, 0x81);
   mp_str(&file, "count");
   mp_byte(&file, 0xce); /* uint 32 */
   for (i = 0; i < 4; i++)
      mp_byte(&file, (uint8_t)(count >> (24 - 8 * i)));
   ok = filestream_write_file(path, file.data, (int64_t)file.len);
   free(file.data);
   free(body->data);
   return ok;
}

static const fx_entry_t t1_entries[] =
{
   { "/roms/ng.nes",   "nes_ninja",            "1A2B3C4D|crc" },
   { "/roms/ab.nes",   "zzz",                  "0BADF00D|crc" },
   { "/roms/zero.nes", "qqq",                  "00000000|crc" },
   { "/roms/ser.nes",  "disc_one",             "SLUS-00001|serial" },
   { "/roms/ngj.nes",  "Ninja Gaiden (Japan)", "1A2B3C4D|crc" },
   { "/roms/sg.nes",   "Serial Game (U)",      "" },
   { "/roms/gl.nes",   "gl",                   "C0FFEE00|crc" },
   { "/roms/gu.nes",   "gu",                   "C0FFEE01|crc" },
   /* Each reads as 001A2B3C to strtoul */
   { "/roms/l1.nes",   "l1",                   "1A2B3C|crc" },
   { "/roms/l2.nes",   "l2",                   "0x1A2B3C|crc" },
   { "/roms/l3.nes",   "l3",                   " 01A2B3C|crc" },
   { "/roms/l4.nes",   "l4",                   "+01A2B3C|crc" },
   { "/roms/l5.nes",   "l5",                   "1001A2B3C|crc" }
};
#define T1_COUNT (sizeof(t1_entries) / sizeof(t1_entries[0]))
#define T1_LOOSE 8
#define BARE_SYS "Synth - Bare"

static const char *t1_box(size_t i)
{
   return find_box(t1_entries[i].path, t1_entries[i].label, SYS);
}

static const char *const t1_art[] =
{
   "Ninja Gaiden (USA).png",
   "Ninja Gaiden (Japan).png",
   "Alpha (USA).png",
   "Beta (USA).png",
   "Nil Game (USA).png",
   "Serial Game (USA).png",
   "Gamma (USA).png",
   "Loose Crc (USA).png"
};

enum t1_rdb { T1_FULL, T1_BAD, T1_NIL, T1_LOW, T1_LEGACY };

/* Beta, Alpha's other name, comes a scan step after it. T1_BAD: Beta's
 * map header unreadable. T1_NIL: a nil in place of Beta, short of the
 * metadata's count. T1_LOW: whole, but counted one record low.
 * T1_LEGACY: whole, but a legacy header (offset 0), so no count. */
static bool write_t1_rdb(const char *path, enum t1_rdb form)
{
   char long_name[300];
   mp_buf_t body;
   mp_buf_t file;
   size_t beta;
   int i;
   bool ok;

   memset(long_name, 'G', sizeof(long_name) - 1);
   long_name[sizeof(long_name) - 1] = '\0';
   memset(&body, 0, sizeof(body));
   mp_crc_item(&body, "Ninja Gaiden (USA)", 0x1A2B3C4D);
   mp_crc_item(&body, "Gamma (USA)", 0xC0FFEE00);
   mp_crc_item(&body, long_name, 0xC0FFEE00);
   mp_crc_item(&body, long_name, 0xC0FFEE01);
   mp_crc_item(&body, "Alpha (USA)", 0x0BADF00D);
   for (i = 0; i < PL_THUMB_MATCH_RECORDS; i++)
      mp_crc_item(&body, "Filler (USA)", 0xF0000000 + (uint32_t)i);
   beta = body.len;
   mp_crc_item(&body, "Beta (USA)", 0x0BADF00D);
   mp_crc_item(&body, "Nil Game (USA)", 0);
   mp_serial_item(&body, "Serial Game (USA)", "SLUS-00001");
   mp_crc_item(&body, "Loose Crc (USA)", 0x001A2B3C);
   if (form == T1_NIL)
      body.data[beta] = 0xc0;
   else if (form == T1_BAD)
      body.data[beta] = 0xc1; /* never used */
   if (form != T1_LEGACY)
      return write_rdb(path, &body,
            9 + PL_THUMB_MATCH_RECORDS - (form == T1_LOW));
   memset(&file, 0, sizeof(file));
   mp_put(&file, "RARCHDB", 7);
   for (i = 0; i < 9; i++)
      mp_byte(&file, 0);
   mp_put(&file, body.data, body.len);
   mp_byte(&file, 0xc0);
   ok = filestream_write_file(path, file.data, (int64_t)file.len);
   free(file.data);
   free(body.data);
   return ok;
}

static void t1_database_list(void)
{
   char hist[128];
   char hist_cache[128];
   playlist_config_t config;
   playlist_t *playlist;

   fill_pathname_join_special(hist, FIX, FILE_PATH_CONTENT_HISTORY,
         sizeof(hist));
   strlcpy(hist_cache, hist, sizeof(hist_cache));
   strlcat(hist_cache, ".thumbs", sizeof(hist_cache));
   filestream_delete(hist_cache);
   write_text(hist, "{\"version\": \"1.5\", \"items\": ["
         "{\"path\": \"/roms/ng.nes\", \"label\": \"nes_ninja\", "
         "\"core_path\": \"" HIST_CORE "\", \"core_name\": \"NES\", "
         "\"crc32\": \"1A2B3C4D|crc\", \"db_name\": \"\"}]}\n");
   memset(&config, 0, sizeof(config));
   config.capacity = 8192;
   playlist_config_set_path(&config, hist);
   hist_core.databases = (char*)SYS "|Nintendo - Family Computer Disk System";
   playlist = playlist_init(&config);
   CHECK(task_push_pl_thumbnail_match(&config, playlist, thumbs_dir, db_dir,
         false), "push history");
   playlist_free(playlist);
   run(0);
   CHECK(!find_box("/roms/ng.nes", "nes_ninja", SYS),
         "a list of databases: no T1: %s",
         find_box("/roms/ng.nes", "nes_ninja", SYS));
   hist_core.databases = NULL;
   filestream_delete(hist_cache);
}

static void lane_t1(void)
{
   char rdb[256];
   char bare[256];
   mp_buf_t body;
   size_t i;
   int form;
   pl_thumb_match_t *m = NULL;
   unsigned had = failures;

   fixture_init();
   for (i = 0; i < sizeof(t1_art) / sizeof(t1_art[0]); i++)
      art("Named_Boxarts", t1_art[i], true);
   write_playlist(t1_entries, T1_COUNT);
   path_mkdir(db_dir);
   fill_pathname_join_special(rdb, db_dir, SYS ".rdb", sizeof(rdb));
   CHECK(write_t1_rdb(rdb, T1_FULL), "write %s", rdb);

   task_queue_init(false, NULL);
   push("");
   run(0);
   CHECK(!t1_box(0) && !t1_box(3), "no database: no T1");

   gfx_thumbnail_results_deinit();
   filestream_delete(cache_path);
   leftovers = 0;
   task_queue_set_slow_handler_cb(count_leftovers, 1);
   push(db_dir);
   run(ONE_ITEM);
   CHECK(string_is_equal(t1_box(0), "Ninja Gaiden (USA).png"),
         "a CRC names the game: %s", t1_box(0));
   CHECK(!t1_box(1), "two names for one CRC, neither the label: no T1: %s",
         t1_box(1));
   CHECK(!t1_box(2), "a CRC of 00000000 is no anchor");
   CHECK(string_is_equal(t1_box(3), "Serial Game (USA).png"),
         "a serial names the game: %s", t1_box(3));
   CHECK(string_is_equal(t1_box(4), "Ninja Gaiden (USA).png"),
         "the database name beats the label's own image: %s", t1_box(4));
   CHECK(!t1_box(6), "a CRC's other name too long to read: no T1: %s",
         t1_box(6));
   CHECK(!t1_box(7), "a CRC's one name too long to read: no T1: %s",
         t1_box(7));
   for (i = T1_LOOSE; i < T1_COUNT; i++)
      CHECK(!t1_box(i), "\"%s\" is no anchor: %s", t1_entries[i].crc32,
            t1_box(i));
   CHECK(!leftovers, "the resolve left T1 state to cleanup");

   /* A cancel mid-rdb: the handler frees what the pass holds */
   gfx_thumbnail_results_deinit();
   filestream_delete(cache_path);
   push(db_dir);
   clock_step = ONE_ITEM;
   while (     queue_busy()
         && !((m = resolve_state()) && m->scan && m->hit_count))
      task_queue_check();
   CHECK(m && m->scan && m->hit_count, "the rdb pass holds names");
   task_queue_reset();
   run(ONE_ITEM);
   CHECK(!leftovers, "a cancel mid-rdb left its state to cleanup");

   /* A read error, an end other than the metadata's count or no count
    * drops what the rdb named in earlier steps */
   for (form = T1_BAD; form <= T1_LEGACY; form++)
   {
      const char *what = form == T1_BAD ? "a damaged rdb"
            : form == T1_NIL ? "a short rdb"
            : form == T1_LOW ? "an rdb counted low" : "a legacy rdb";
      gfx_thumbnail_results_deinit();
      filestream_delete(cache_path);
      CHECK(write_t1_rdb(rdb, (enum t1_rdb)form), "write %s", rdb);
      leftovers = 0;
      push(db_dir);
      run(ONE_ITEM);
      CHECK(!t1_box(0) && !t1_box(1) && !t1_box(3),
            "%s names nothing: %s, %s, %s", what, t1_box(0), t1_box(1),
            t1_box(3));
      CHECK(string_is_equal(t1_box(5), "Serial Game (USA).png"),
            "%s: the rest resolves: %s", what, t1_box(5));
      CHECK(!leftovers, "%s left its state to cleanup", what);
   }

   gfx_thumbnail_results_deinit();
   filestream_delete(cache_path);
   filestream_delete(rdb);
   leftovers = 0;
   push(db_dir);
   run(ONE_ITEM);
   CHECK(!t1_box(0) && !t1_box(3)
         && string_is_equal(t1_box(5), "Serial Game (USA).png"),
         "a missing rdb names nothing, the rest resolves");
   CHECK(!leftovers, "a missing rdb left its state to cleanup");
   task_queue_set_slow_handler_cb(NULL, 0);

   /* A core that lists several databases names no one rdb */
   gfx_thumbnail_results_deinit();
   CHECK(write_t1_rdb(rdb, T1_FULL), "write %s", rdb);
   t1_database_list();

   /* No art folder for the system: its rdb is not read */
   gfx_thumbnail_results_deinit();
   filestream_delete(cache_path);
   fill_pathname_join_special(bare, db_dir, BARE_SYS ".rdb", sizeof(bare));
   memset(&body, 0, sizeof(body));
   mp_crc_item(&body, "Bare Game (USA)", 0x1A2B3C4D);
   CHECK(write_rdb(bare, &body, 1), "write %s", bare);
   write_text(pl_path, "{\"version\": \"1.5\", \"items\": ["
         "{\"path\": \"/roms/bare.nes\", \"label\": \"bare\", "
         "\"core_path\": \"DETECT\", \"core_name\": \"DETECT\", "
         "\"crc32\": \"1A2B3C4D|crc\", \"db_name\": \"" BARE_SYS ".lpl\"}]}\n");
   read_item = 0;
   push(db_dir);
   run(0);
   CHECK(!read_item, "no art folder: %u rdb records read",
         (unsigned)read_item);
   filestream_delete(bare);

   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   for (i = 0; i < sizeof(t1_art) / sizeof(t1_art[0]); i++)
      art("Named_Boxarts", t1_art[i], false);
   write_playlist(fx_entries, FX_COUNT);
   if (failures == had)
      fprintf(stderr, "[pass] T1\n");
}

/* Quitting cancels every task; a write cut short leaves no file. */
static void lane_cancel_write(void)
{
   char tmp[PATH_MAX_LENGTH];
   bool seen    = false;
   unsigned had = failures;

   fixture_init();
   filestream_atomic_temp_name(tmp, sizeof(tmp), cache_path);
   task_queue_init(false, NULL);
   push("");
   clock_step = ONE_ITEM;
   while (!seen && queue_busy())
   {
      task_queue_check();
      seen = path_is_valid(tmp);
   }
   CHECK(seen, "the write is under way");
   task_queue_reset();
   run(ONE_ITEM);
   CHECK(!path_is_valid(tmp), "no temporary file after a cancel");
   CHECK(!path_is_valid(cache_path), "no cache from a cancelled write");
   CHECK(gfx_thumbnail_results_open(pl_path) & GFX_THUMBNAIL_RESULTS_RESOLVE,
         "a cancelled resolve is not marked resolved");
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] cancel mid-write\n");
}

/* A forget between the commit's check and its rename */
static void lane_forget_race(void)
{
   unsigned had = failures;

   fixture_init();
   task_queue_init(false, NULL);
   forget_at_rename = true;
   push("");
   run(0);
   CHECK(!forget_at_rename, "the forget landed at the rename");
   CHECK(!path_is_valid(cache_path),
         "no cache for a playlist forgotten at its rename");
   forget_at_rename = false;
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] forget at the rename\n");
}

/* Portable paths: the task never writes the playlist, whose temporary
 * file the main thread's writes use too. */
static void lane_portable(void)
{
   char tmp[128];
   playlist_config_t config;
   void *before       = NULL;
   void *after        = NULL;
   int64_t before_len = 0;
   int64_t after_len  = 0;
   unsigned had       = failures;

   fixture_init();
   strlcpy(tmp, pl_path, sizeof(tmp));
   strlcat(tmp, ".tmp", sizeof(tmp));
   filestream_read_file(pl_path, &before, &before_len);
   memset(&config, 0, sizeof(config));
   config.capacity = 8192;
   playlist_config_set_path(&config, pl_path);
   playlist_config_set_base_content_directory(&config, FIX "/moved");
   task_queue_init(false, NULL);
   CHECK(task_push_pl_thumbnail_match(&config, NULL, thumbs_dir, "", false),
         "push");
   run(0);
   filestream_read_file(pl_path, &after, &after_len);
   CHECK(before && after && before_len == after_len
      && !memcmp(before, after, (size_t)before_len),
         "the playlist is not rewritten");
   CHECK(!path_is_valid(tmp), "no temporary playlist file");
   CHECK(!gfx_thumbnail_results_open(pl_path), "resolved");
   free(before);
   free(after);
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] portable paths\n");
}

/* @userdata takes the first match task's state */
static bool match_finder(retro_task_t *task, void *userdata)
{
   if (task->handler != task_pl_thumbnail_match_handler || !task->state)
      return false;
   *(pl_thumb_match_t**)userdata = (pl_thumb_match_t*)task->state;
   return true;
}

/* Waits for the resolve, then opens enough playlists to evict Test.lpl */
static void evict_while_resolving(void)
{
   while (!resolve_queued() && queue_busy())
      task_queue_check();
   CHECK(resolve_queued(), "the resolve is queued");
   gfx_thumbnail_results_open(FIX "/e1.lpl");
   gfx_thumbnail_results_open(FIX "/e2.lpl");
   gfx_thumbnail_results_open(FIX "/e3.lpl");
   gfx_thumbnail_results_open(FIX "/e4.lpl");
}

/* Explore lists span more playlists than the store keeps. */
static void lane_evict_resolve(void)
{
   task_finder_data_t find_data;
   pl_thumb_match_t *m = NULL;
   unsigned had        = failures;

   fixture_init();
   task_queue_init(false, NULL);
   CHECK(push(""), "push");
   evict_while_resolving();
   run(0);
   CHECK(path_is_valid(cache_path), "the evicted resolve wrote its cache");
   CHECK(push(""), "reopen");
   find_data.func     = match_finder;
   find_data.userdata = &m;
   CHECK(task_queue_find(&find_data) && m
      && !(m->flags & (PL_THUMB_MATCH_FLAG_RESOLVE
                     | PL_THUMB_MATCH_FLAG_THEN_RESOLVE)),
         "an evicted resolve is kept: the reopen only loads");
   run(0);
   check_first_session("reopened after eviction");

   /* Forgotten while evicted: resolve again */
   gfx_thumbnail_results_deinit();
   CHECK(push(""), "push again");
   evict_while_resolving();
   gfx_thumbnail_results_forget(pl_path);
   run(0);
   CHECK(gfx_thumbnail_results_open(pl_path) & GFX_THUMBNAIL_RESULTS_RESOLVE,
         "a resolve forgotten while evicted is not marked");
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] evicted while resolving\n");
}

/* The menu reloads its art only when the table can differ. */
static void lane_refresh(void)
{
   playlist_config_t config;
   unsigned had = failures;

   fixture_init();
   memset(&config, 0, sizeof(config));
   config.capacity = 8192;
   playlist_config_set_path(&config, pl_path);
   CHECK(playlist_init_cached(&config), "the menu shows the playlist");
   task_queue_init(false, NULL);
   push("");
   run(0);
   CHECK(refreshed == 1, "first session: once, for the new matches, not %u",
         refreshed);
   gfx_thumbnail_results_deinit();
   refreshed = 0;
   push("");
   run(0);
   CHECK(refreshed == 1, "unchanged: once, for the loaded matches, not %u",
         refreshed);
   playlist_free_cached();
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] refresh\n");
}

/* An engine out of memory keeps the cache and the loaded matches. */
static void lane_engine_oom(void)
{
   char *before;
   char *after;
   unsigned had = failures;

   fixture_init();
   task_queue_init(false, NULL);
   push("");
   run(0);
   before = read_cache();
   gfx_thumbnail_results_deinit();
   notified  = 0;
   oom_hits  = 0;
   leftovers = 0;
   oom_size  = FX_BOXARTS * sizeof(struct gtm_rec);
   task_queue_set_slow_handler_cb(count_leftovers, 1);
   push("");
   run(ONE_ITEM);
   oom_size = 0;
   after    = read_cache();
   CHECK(oom_hits == 1, "one engine allocation failed: %u", oom_hits);
   CHECK(!leftovers, "the failed resolve left its index to cleanup");
   CHECK(before && after && string_is_equal(before, after),
         "the cache is kept");
   CHECK(string_is_equal(box(1), "Super Mario Bros. (World).png"),
         "the loaded matches stay: %s", box(1));
   CHECK(!notified, "no notice");
   CHECK(gfx_thumbnail_results_open(pl_path) == GFX_THUMBNAIL_RESULTS_RESOLVE,
         "resolves again on its next open");
   free(after);

   /* That resolve has no load to take the crc from */
   cache_reads = 0;
   CHECK(push(""), "push the resolve");
   run(ONE_ITEM);
   CHECK(cache_reads == 1, "a resolve alone reads the cache: %u",
         cache_reads);
   after = read_cache();
   CHECK(!notified && before && after && string_is_equal(before, after),
         "and finds it unchanged");
   CHECK(gfx_thumbnail_results_open(pl_path) == 0, "resolved");
   free(after);

   /* Out of memory while listing, with the folder open */
   gfx_thumbnail_results_deinit();
   oom_hits  = 0;
   leftovers = 0;
   oom_size  = sizeof(struct gtm_pool) + GTM_POOL_BLOCK;
   push("");
   run(ONE_ITEM);
   oom_size = 0;
   after    = read_cache();
   CHECK(oom_hits == 1, "the listing's first name failed: %u", oom_hits);
   CHECK(before && after && string_is_equal(before, after),
         "a failed listing keeps the cache");
   CHECK(!leftovers, "the failed listing left its folder to cleanup");
   free(before);
   free(after);
   task_queue_set_slow_handler_cb(NULL, 0);
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] engine out of memory\n");
}

/* A table add or set out of memory keeps the cache, as the engine's
 * does. */
static void lane_add_oom(void)
{
   int i;
   unsigned had = failures;

   fixture_init();
   task_queue_init(false, NULL);
   push("");
   run(0);
   task_queue_set_slow_handler_cb(count_leftovers, 1);
   for (i = 0; i < 2; i++)
   {
      bool *fail       = i ? &fail_sets : &fail_adds;
      const char *what = i ? "set" : "add";
      char *before     = read_cache();
      char *after;

      gfx_thumbnail_results_deinit();
      notified  = 0;
      leftovers = 0;
      push("");
      clock_step = ONE_ITEM;
      while (!resolve_queued() && queue_busy())
         task_queue_check();
      *fail = true;
      run(ONE_ITEM);
      *fail = false;
      after = read_cache();
      CHECK(before && after && string_is_equal(before, after),
            "%s: the cache is kept", what);
      CHECK(string_is_equal(box(1), "Super Mario Bros. (World).png"),
            "%s: the loaded matches stay: %s", what, box(1));
      CHECK(!notified, "%s: no notice", what);
      CHECK(gfx_thumbnail_results_open(pl_path)
            == GFX_THUMBNAIL_RESULTS_RESOLVE,
            "%s: resolves again on its next open", what);
      CHECK(!leftovers, "%s: the failed resolve left its index to cleanup",
            what);
      free(before);
      free(after);
   }
   task_queue_set_slow_handler_cb(NULL, 0);
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] table add and set out of memory\n");
}

#define BIG_SYS     "Synth - Big"
/* Names and entries: more than two sort steps each */
#define BIG_NAMES   9000
#define BIG_ENTRIES (2 * GFX_THUMBNAIL_MATCH_SORT_STEP + 808)
/* Entries with a crc in the rdb: several sort steps */
#define BIG_ANCHORS (BIG_ENTRIES / 8)

static size_t most_moves;
static size_t most_sum;
static size_t most_reads;
static size_t all_moves;
static size_t all_sum;
static size_t all_reads;
static unsigned index_items;
static unsigned t1_items;
static unsigned kept_playlist;
static unsigned kept_rdbs;
static unsigned kept_hits;
static const void *last_index;
static const void *last_results;
static const void *last_t1;
static size_t last_index_moved;
static size_t last_results_moved;
static size_t last_t1_moved;

/* Moves since the last item, for a sort that may be new or restarted */
static size_t sort_delta(const void *owner, const gfx_thumbnail_match_sort_t *s,
      const void **last, size_t *last_moved)
{
   size_t d = 0;
   if (owner)
      d = (owner == *last && s->moved >= *last_moved)
         ? s->moved - *last_moved : s->moved;
   *last       = owner;
   *last_moved = owner ? s->moved : 0;
   return d;
}

static void measure_item(retro_task_t *task, retro_time_t took)
{
   size_t moves, index_moves, t1_moves;
   pl_thumb_match_t *m = (pl_thumb_match_t*)task->state;
   if (task->handler != task_pl_thumbnail_match_handler || !m)
      return;
   count_leftovers(task, took);
   index_moves = sort_delta(m->index, m->index ? &m->index->sort : NULL,
         &last_index, &last_index_moved);
   /* The want and hit sorts share m->sort */
   t1_moves    = sort_delta(m->sort.base, &m->sort, &last_t1,
         &last_t1_moved);
   moves       = index_moves + t1_moves + sort_delta(m->results,
         m->results ? &m->results->sort : NULL,
         &last_results, &last_results_moved);
   if (index_moves)
      index_items++;
   if (t1_moves)
      t1_items++;
   if (moves > most_moves)
      most_moves = moves;
   if (crc_item > most_sum)
      most_sum = crc_item;
   if (read_item > most_reads)
      most_reads = read_item;
   all_moves += moves;
   all_sum   += crc_item;
   all_reads += read_item;
   crc_item   = 0;
   read_item  = 0;
   if (!(m->flags & PL_THUMB_MATCH_FLAG_RESOLVE))
      return;
   if (m->status > PL_THUMB_MATCH_RESOLVE && m->playlist)
      kept_playlist++;
   if (     m->status > PL_THUMB_MATCH_RDB
         && (m->want || m->rdbs || m->system_art || m->scan))
      kept_rdbs++;
   if (m->status > PL_THUMB_MATCH_RESOLVE && m->hits)
      kept_hits++;
}

static void write_big(const char *big_pl)
{
   char path[256];
   char name[64];
   mp_buf_t body;
   rjsonwriter_t *w;
   size_t i;
   int _len;

   fill_pathname_join_special(path, thumbs_dir, BIG_SYS, sizeof(path));
   fill_pathname_join_special(path, path, "Named_Boxarts", sizeof(path));
   path_mkdir(path);
   for (i = 0; i < BIG_NAMES; i++)
   {
      char file[256];
      snprintf(name, sizeof(name), "Synth %05u (USA).png", (unsigned)i);
      fill_pathname_join_special(file, path, name, sizeof(file));
      write_text(file, "png");
   }
   w = rjsonwriter_open_memory();
   raw(w, "{\"version\": \"1.5\", \"items\": [");
   for (i = 0; i < BIG_ENTRIES; i++)
   {
      snprintf(name, sizeof(name), "/roms/s%05u.nes", (unsigned)i);
      raw(w, i ? ", {\"path\": " : "{\"path\": ");
      rjsonwriter_add_string(w, name);
      snprintf(name, sizeof(name), "Synth %05u (U)", (unsigned)i);
      raw(w, ", \"label\": ");
      rjsonwriter_add_string(w, name);
      if (i < BIG_ANCHORS)
         snprintf(name, sizeof(name), "%08X|crc", (unsigned)i + 1);
      else
         name[0] = '\0';
      raw(w, ", \"core_path\": \"DETECT\", \"core_name\": \"DETECT\", "
            "\"crc32\": ");
      rjsonwriter_add_string(w, name);
      raw(w, ", \"db_name\": \"" BIG_SYS ".lpl\"}");
   }
   raw(w, "]}\n");
   write_text(big_pl, rjsonwriter_get_memory_buffer(w, &_len));
   rjsonwriter_free(w);
   path_mkdir(db_dir);
   fill_pathname_join_special(path, db_dir, BIG_SYS ".rdb", sizeof(path));
   memset(&body, 0, sizeof(body));
   for (i = 0; i < BIG_ANCHORS; i++)
   {
      snprintf(name, sizeof(name), "Synth %05u (USA)", (unsigned)i);
      mp_crc_item(&body, name, (uint32_t)i + 1);
   }
   CHECK(write_rdb(path, &body, BIG_ANCHORS), "write %s", path);
}

/* A big folder, playlist and rdb: no work item does a whole sort or
 * sum, or reads a whole rdb. */
static void lane_bounded(void)
{
   char big_pl[128];
   char big_cache[128];
   playlist_config_t config;
   unsigned had = failures;

   fixture_init();
   fill_pathname_join_special(big_pl, FIX, "Big.lpl", sizeof(big_pl));
   strlcpy(big_cache, big_pl, sizeof(big_cache));
   strlcat(big_cache, ".thumbs", sizeof(big_cache));
   filestream_delete(big_cache);
   write_big(big_pl);

   memset(&config, 0, sizeof(config));
   config.capacity = BIG_ENTRIES;
   playlist_config_set_path(&config, big_pl);
   most_moves  = most_sum = most_reads = 0;
   all_moves   = all_sum  = all_reads  = 0;
   index_items = t1_items = 0;
   kept_playlist = kept_rdbs = kept_hits = leftovers = 0;
   last_index    = last_results = last_t1 = NULL;
   crc_item      = read_item = 0;
   task_queue_init(false, NULL);
   task_queue_set_slow_handler_cb(measure_item, 1);
   CHECK(task_push_pl_thumbnail_match(&config, NULL, thumbs_dir, db_dir,
         false), "push the big playlist");
   run(ONE_ITEM);
   /* The next session loads the big cache, then resolves it again */
   gfx_thumbnail_results_deinit();
   CHECK(task_push_pl_thumbnail_match(&config, NULL, thumbs_dir, db_dir,
         false), "push it again");
   run(ONE_ITEM);
   CHECK(string_is_equal(find_box("/roms/s04242.nes", "Synth 04242 (U)",
         BIG_SYS), "Synth 04242 (USA).png"), "the big playlist resolves");
   CHECK(most_moves <= GFX_THUMBNAIL_MATCH_SORT_STEP,
         "one item moved %u records", (unsigned)most_moves);
   CHECK(most_sum <= PL_THUMB_MATCH_CHUNK, "one item summed %u bytes",
         (unsigned)most_sum);
   CHECK(most_reads <= PL_THUMB_MATCH_RECORDS, "one item read %u rdb records",
         (unsigned)most_reads);
   CHECK(   all_moves > 8 * GFX_THUMBNAIL_MATCH_SORT_STEP
         && all_sum > 8 * PL_THUMB_MATCH_CHUNK
         && all_reads >= 2 * BIG_ANCHORS
         && BIG_ANCHORS > 4 * PL_THUMB_MATCH_RECORDS,
         "big enough: %u moves, %u bytes summed, %u records read",
         (unsigned)all_moves, (unsigned)all_sum, (unsigned)all_reads);
   /* More items than sorts: 2 finalizes, 2 want and 2 hit sorts */
   CHECK(index_items > 2 && t1_items > 4,
         "sorted in steps: %u index items, %u T1 items",
         index_items, t1_items);
   CHECK(!kept_playlist, "the playlist outlived the resolve for %u items",
         kept_playlist);
   CHECK(!kept_rdbs, "the want list outlived the rdb pass for %u items",
         kept_rdbs);
   CHECK(!kept_hits, "the rdb names outlived the resolve for %u items",
         kept_hits);
   CHECK(!leftovers, "the resolve left its state to cleanup");
   task_queue_set_slow_handler_cb(NULL, 0);
   task_queue_deinit();
   gfx_thumbnail_results_deinit();
   if (failures == had)
      fprintf(stderr, "[pass] bounded (at most %u moves, %u bytes and %u "
            "records an item, of %u, %u and %u; %u index and %u T1 sort "
            "items)\n", (unsigned)most_moves, (unsigned)most_sum,
            (unsigned)most_reads, (unsigned)all_moves, (unsigned)all_sum,
            (unsigned)all_reads, index_items, t1_items);
}

int main(void)
{
   shim_vfs();
   lane_names();
   lane_table();
   lane_table_edges();
   lane_table_steps();
   lane_table_steps_bounded();
   lane_store();
   lane_store_order();
   lane_forget();
   lane_resolve(false);
   lane_resolve(true);
   lane_changes();
   lane_deleted();
   lane_cancel_write();
   lane_forget_race();
   lane_portable();
   lane_evict_resolve();
   lane_load_first();
   lane_history();
   lane_pacing();
   lane_t1();
   lane_refresh();
   lane_engine_oom();
   lane_add_oom();
   lane_bounded();
   fprintf(stderr, "%s (%u failures)\n", failures ? "FAILED" : "PASS",
         failures);
   return failures ? 1 : 0;
}
