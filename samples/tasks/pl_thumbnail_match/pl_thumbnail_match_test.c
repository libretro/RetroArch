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

/* The thumbnail result table, its main-thread store and (from I3 on)
 * the task that fills it from <playlist>.lpl.thumbs and the art folders.
 * Fixtures live in ./fixture, inside this directory. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../gfx/gfx_thumbnail_results.h"

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

int main(void)
{
   lane_names();
   lane_table();
   lane_table_edges();
   lane_store();
   lane_store_order();
   lane_forget();
   fprintf(stderr, "%s (%u failures)\n", failures ? "FAILED" : "PASS",
         failures);
   return failures ? 1 : 0;
}
