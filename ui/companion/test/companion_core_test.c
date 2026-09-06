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

/* Regression test for ui/companion/companion_core: the toolkit-free
 * model every desktop companion (Qt, Win32, Cocoa) drives. Links the
 * real companion_core.c and playlist.c against libretro-common, with
 * RetroArch's state replaced by fixtures (companion_core_stubs.c), and
 * runs on Linux from tools/companion_core_test.sh.
 *
 * Covered, against real .lpl files it writes itself:
 *   - playlist listing: "All Playlists" first (token path), then the
 *     special playlists that are configured, then the files, in order
 *   - selecting a playlist parses it through the budgeted iterate and
 *     exposes its entries
 *   - "All Playlists" merges every file, sorted by label
 *     case-insensitively, and maps each row back to its file and index
 *   - the thumbnail path rule: repository layout, label sanitising,
 *     extension probe, image content as its own thumbnail
 *   - the file browser: folders first with "..", the folder / file
 *     split, descending, going up, the root
 *   - Run on an entry without a core reports needs-core with the path;
 *     with a core path it pushes the load task with that core
 *   - launch options with no cores installed: the entry's own core
 *     (by path) is offered, nothing is duplicated */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include <boolean.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <file/file_path.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../companion_core.h"

extern settings_t test_settings;
extern runloop_state_t test_runloop;
extern int  stub_calls_load_with_new_core;
extern int  stub_calls_load_with_current_core;
extern char stub_last_content[PATH_MAX_LENGTH];
extern char stub_last_core[PATH_MAX_LENGTH];

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char root[512];
static int  playlist_changed, playlists_changed;

static void on_playlists_changed(void *ud) { (void)ud; playlists_changed++; }
static void on_playlist_changed(void *ud)  { (void)ud; playlist_changed++; }

static void mkdirp(const char *p) { mkdir(p, 0755); }

static void writef(const char *path, const char *text)
{
   FILE *f = fopen(path, "wb");
   if (!f)
   {
      printf("cannot write %s\n", path);
      exit(2);
   }
   fputs(text, f);
   fclose(f);
}

static void fixture(char *out, size_t len, const char *rel)
{
   snprintf(out, len, "%s/%s", root, rel);
}

/* A RetroArch JSON playlist with the given entries. */
static void write_lpl(const char *rel, const char *default_core,
      const char *const *labels, const char *const *paths,
      const char *const *cores, int n)
{
   char path[512];
   char buf[8192];
   int i, off = 0;
   fixture(path, sizeof(path), rel);
   off += snprintf(buf + off, sizeof(buf) - off,
         "{\n  \"version\": \"1.5\",\n  \"default_core_path\": \"%s\",\n"
         "  \"default_core_name\": \"\",\n  \"items\": [\n", default_core ? default_core : "");
   for (i = 0; i < n; i++)
      off += snprintf(buf + off, sizeof(buf) - off,
            "    {\n      \"path\": \"%s\",\n      \"label\": \"%s\",\n"
            "      \"core_path\": \"%s\",\n      \"core_name\": \"DETECT\",\n"
            "      \"crc32\": \"00000000|crc\",\n      \"db_name\": \"%s\"\n    }%s\n",
            paths[i], labels[i], cores ? cores[i] : "DETECT",
            rel, i + 1 < n ? "," : "");
   snprintf(buf + off, sizeof(buf) - off, "  ]\n}\n");
   writef(path, buf);
}

/* Run the budgeted iterate until the playlist callback fires. */
static bool iterate_until_loaded(companion_core_t *core)
{
   int i;
   int before = playlist_changed;
   for (i = 0; i < 100000 && playlist_changed == before; i++)
      companion_core_iterate(core, 2000);
   return playlist_changed != before;
}

/* --- fixtures ------------------------------------------------------------- */

static const char *nes_labels[] = { "Zelda II - The Adventure of Link (USA)", "Metroid (USA)", "Alpha Mission (USA)" };
static const char *nes_paths[]  = { "/roms/nes/zelda2.nes", "/roms/nes/metroid.nes", "/roms/nes/alpha.nes" };
static const char *nes_cores[]  = { "DETECT", "/cores/fceumm_libretro.so", "DETECT" };
static const char *gen_labels[] = { "sonic the hedgehog (USA)", "'89 Dennou Kyuusei Uranai (Japan)" };
static const char *gen_paths[]  = { "/roms/gen/sonic.md", "/roms/gen/dennou.md" };

static void setup(void)
{
   char p[512];
   snprintf(root, sizeof(root), "/tmp/companion_core_test_%ld", (long)time(NULL));
   mkdirp(root);
   fixture(p, sizeof(p), "playlists"); mkdirp(p);
   fixture(p, sizeof(p), "thumbnails"); mkdirp(p);
   fixture(p, sizeof(p), "thumbnails/Nintendo - Nintendo Entertainment System"); mkdirp(p);
   fixture(p, sizeof(p), "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts"); mkdirp(p);
   fixture(p, sizeof(p), "content"); mkdirp(p);
   fixture(p, sizeof(p), "content/sub"); mkdirp(p);
   fixture(p, sizeof(p), "content/sub/deeper"); mkdirp(p);
   fixture(p, sizeof(p), "content/a.nes"); writef(p, "x");
   fixture(p, sizeof(p), "content/b.sfc"); writef(p, "x");
   fixture(p, sizeof(p), "content/sub/c.gb"); writef(p, "x");
   fixture(p, sizeof(p), "content/cover.png"); writef(p, "x");

   write_lpl("playlists/Nintendo - Nintendo Entertainment System.lpl",
         "/cores/nestopia_libretro.so", nes_labels, nes_paths, nes_cores, 3);
   write_lpl("playlists/Sega - Mega Drive - Genesis.lpl", NULL,
         gen_labels, gen_paths, NULL, 2);
   /* history is a special: configured path outside the playlist dir */
   write_lpl("history.lpl", NULL, gen_labels, gen_paths, NULL, 1);

   /* a thumbnail whose file name is the sanitised label */
   fixture(p, sizeof(p),
         "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Zelda II - The Adventure of Link (USA).png");
   writef(p, "png");

   memset(&test_settings, 0, sizeof(test_settings));
   memset(&test_runloop, 0, sizeof(test_runloop));
   fixture(test_settings.paths.directory_playlist, sizeof(test_settings.paths.directory_playlist), "playlists");
   fixture(test_settings.paths.directory_thumbnails, sizeof(test_settings.paths.directory_thumbnails), "thumbnails");
   fixture(test_settings.paths.directory_menu_content, sizeof(test_settings.paths.directory_menu_content), "content");
   fixture(test_settings.paths.path_content_history, sizeof(test_settings.paths.path_content_history), "history.lpl");
   /* favorites / images / music / video left empty: skipped in the list */
}

static void teardown(void)
{
   char cmd[600];
   snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
   if (system(cmd) != 0)
      printf("(could not remove %s)\n", root);
}

/* --- tests ---------------------------------------------------------------- */

static companion_core_t *make_core(void)
{
   companion_callbacks_t cb;
   memset(&cb, 0, sizeof(cb));
   cb.on_playlists_changed = on_playlists_changed;
   cb.on_playlist_changed  = on_playlist_changed;
   return companion_core_new(&cb, NULL);
}

static void test_playlist_listing(void)
{
   companion_core_t *c = make_core();
   size_t n;
   char nes[512];
   companion_core_refresh_playlists(c);
   n = companion_core_playlist_count(c);
   /* All + History (the one special configured) + 2 files */
   CHECK(n == 4, "playlist count 4, got %u", (unsigned)n);
   CHECK(string_is_equal(companion_core_playlist_name(c, 0), "All Playlists"), "slot 0 is All Playlists");
   CHECK(string_is_equal(companion_core_playlist_path(c, 0), COMPANION_ALL_PLAYLISTS_TOKEN), "slot 0 path is the token");
   CHECK(string_is_equal(companion_core_playlist_name(c, 1), "History"), "slot 1 is History (got %s)", companion_core_playlist_name(c, 1));
   CHECK(string_is_equal(companion_core_playlist_name(c, 2), "Nintendo - Nintendo Entertainment System"), "files sorted: NES first (got %s)", companion_core_playlist_name(c, 2));
   CHECK(string_is_equal(companion_core_playlist_name(c, 3), "Sega - Mega Drive - Genesis"), "then Genesis");
   fixture(nes, sizeof(nes), "playlists/Nintendo - Nintendo Entertainment System.lpl");
   CHECK(string_is_equal(companion_core_playlist_path(c, 2), nes), "file path");
   companion_core_free(c);
}

static void test_select_and_entries(void)
{
   companion_core_t *c = make_core();
   const struct playlist_entry *e;
   companion_core_refresh_playlists(c);
   CHECK(companion_core_select_playlist(c, 2), "select NES");
   CHECK(iterate_until_loaded(c), "parse finished through iterate");
   CHECK(companion_core_entry_count(c) == 3, "3 entries, got %u", (unsigned)companion_core_entry_count(c));
   e = companion_core_entry(c, 1);
   CHECK(e && string_is_equal(e->label, "Metroid (USA)"), "entry 1 label");
   CHECK(e && string_is_equal(e->core_path, "/cores/fceumm_libretro.so"), "entry 1 core path");
   CHECK(string_is_equal(companion_core_entry_playlist_path(c, 1), companion_core_playlist_path(c, 2)), "entry's playlist is the selected file");
   CHECK(companion_core_entry_index_in_playlist(c, 1) == 1, "index within file");
   companion_core_free(c);
}

static void test_all_playlists(void)
{
   companion_core_t *c = make_core();
   const struct playlist_entry *e;
   char gen[512];
   companion_core_refresh_playlists(c);
   CHECK(companion_core_select_playlist(c, 0), "select All");
   CHECK(iterate_until_loaded(c), "aggregate finished");
   /* NES 3 + Genesis 2 = 5 (History is a special, not a file: not merged) */
   CHECK(companion_core_entry_count(c) == 5, "5 merged entries, got %u", (unsigned)companion_core_entry_count(c));
   e = companion_core_entry(c, 0);
   CHECK(e && string_is_equal(e->label, "'89 Dennou Kyuusei Uranai (Japan)"), "sorted: '89 first (got %s)", e ? e->label : "-");
   e = companion_core_entry(c, 1);
   CHECK(e && string_is_equal(e->label, "Alpha Mission (USA)"), "then Alpha (got %s)", e ? e->label : "-");
   e = companion_core_entry(c, 3);
   CHECK(e && string_is_equal(e->label, "sonic the hedgehog (USA)"), "case-insensitive: sonic before Zelda (got %s)", e ? e->label : "-");
   fixture(gen, sizeof(gen), "playlists/Sega - Mega Drive - Genesis.lpl");
   CHECK(string_is_equal(companion_core_entry_playlist_path(c, 0), gen), "row 0 came from the Genesis file");
   CHECK(companion_core_entry_index_in_playlist(c, 0) == 1, "and is its second entry");
   companion_core_free(c);
}

static void test_thumbnail_path(void)
{
   companion_core_t *c = make_core();
   char out[PATH_MAX_LENGTH], want[PATH_MAX_LENGTH], img[512];
   size_t n;
   /* existing file: found through the extension probe */
   n = companion_core_thumbnail_path(c, "Nintendo - Nintendo Entertainment System",
         COMPANION_THUMB_BOXART, "Zelda II - The Adventure of Link (USA)",
         "/roms/nes/zelda2.nes", out, sizeof(out));
   fixture(want, sizeof(want),
         "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Zelda II - The Adventure of Link (USA).png");
   CHECK(n > 0 && string_is_equal(out, want), "existing thumbnail found: %s", out);
   /* missing: the default .png path (the save target), which does not
    * exist - backends decide by path_is_valid, and a decode of it
    * fails cleanly */
   n = companion_core_thumbnail_path(c, "Nintendo - Nintendo Entertainment System",
         COMPANION_THUMB_BOXART, "Metroid (USA)", "/roms/nes/metroid.nes", out, sizeof(out));
   CHECK(n > 0 && string_ends_with(out, "Named_Boxarts/Metroid (USA).png"), "missing thumbnail gives the default path: %s", out);
   CHECK(!path_is_valid(out), "which does not exist");
   /* sanitising: a label with the characters the repository replaces */
   fixture(want, sizeof(want),
         "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/A_B_C_D_E_F_G_H_I.png");
   writef(want, "png");
   n = companion_core_thumbnail_path(c, "Nintendo - Nintendo Entertainment System",
         COMPANION_THUMB_BOXART, "A&B*C/D:E`F<G>H?I", "/x", out, sizeof(out));
   CHECK(n > 0 && string_is_equal(out, want), "&*/:`<>? sanitised to _: %s", out);
   /* image content is its own thumbnail */
   fixture(img, sizeof(img), "content/cover.png");
   n = companion_core_thumbnail_path(c, "Whatever", COMPANION_THUMB_BOXART,
         "cover", img, out, sizeof(out));
   CHECK(n > 0 && string_is_equal(out, img), "image content is its own thumbnail");
   companion_core_free(c);
}

static void test_browser(void)
{
   companion_core_t *c = make_core();
   char content[512];
   size_t n, dc;
   bool needs_core = false;
   char pick[PATH_MAX_LENGTH];
   fixture(content, sizeof(content), "content");
   CHECK(companion_core_browse_open(c, NULL), "open defaults to the content directory");
   CHECK(string_is_equal(companion_core_browse_dir(c), content), "browse dir");
   n  = companion_core_browse_count(c);
   dc = companion_core_browse_dir_count(c);
   /* "..", sub | a.nes, b.sfc, cover.png */
   CHECK(n == 5, "5 entries (.. + 1 dir + 3 files), got %u", (unsigned)n);
   CHECK(dc == 2, "2 folder rows (.. and sub), got %u", (unsigned)dc);
   CHECK(string_is_equal(companion_core_browse_name(c, 0), ".."), "first is ..");
   CHECK(string_is_equal(companion_core_browse_name(c, 1), "sub"), "then the folder");
   CHECK(companion_core_browse_is_dir(c, 1) && !companion_core_browse_is_dir(c, 2), "dir / file flags");
   /* descend */
   CHECK(companion_core_browse_activate(c, 1, NULL, &needs_core, pick, sizeof(pick)) == 0, "activate a folder descends");
   CHECK(companion_core_browse_dir_count(c) == 2, "sub: .. and deeper");
   CHECK(companion_core_browse_count(c) == 3, "sub: .. deeper c.gb");
   /* a file with no core: needs-core with its path */
   needs_core = false;
   CHECK(companion_core_browse_activate(c, 2, NULL, &needs_core, pick, sizeof(pick)) < 0 && needs_core, "file without a core asks for one");
   CHECK(strstr(pick, "c.gb") != NULL, "and hands back its path: %s", pick);
   /* up */
   CHECK(companion_core_browse_up(c), "up");
   CHECK(string_is_equal(companion_core_browse_dir(c), content), "back at content (dir is [%s])", companion_core_browse_dir(c));
   /* keep going up to the root: eventually no parent */
   {
      int guard = 0;
      while (companion_core_browse_up(c) && guard++ < 64)
         ;
      CHECK(guard < 64, "reaches a top (dir is [%s])", companion_core_browse_dir(c));
      CHECK(!companion_core_browse_up(c), "up at the top is refused");
   }
   companion_core_free(c);
}

static void test_run_paths(void)
{
   companion_core_t *c = make_core();
   char content[PATH_MAX_LENGTH];
   companion_core_refresh_playlists(c);
   companion_core_select_playlist(c, 2);
   iterate_until_loaded(c);
   /* entry 0: DETECT and nothing loaded -> pick a core */
   CHECK(companion_core_entry_needs_core(c, 0, content, sizeof(content)), "DETECT entry needs a core");
   CHECK(string_is_equal(content, "/roms/nes/zelda2.nes"), "with its content path");
   /* entry 1: has a core path -> loads with it */
   CHECK(!companion_core_entry_needs_core(c, 1, content, sizeof(content)), "entry with a core does not ask");
   stub_calls_load_with_new_core = 0;
   CHECK(companion_core_request_load_entry(c, 1), "load entry 1");
   CHECK(stub_calls_load_with_new_core == 1, "pushed the load task once");
   CHECK(string_is_equal(stub_last_core, "/cores/fceumm_libretro.so"), "with the entry's core (%s)", stub_last_core);
   CHECK(string_is_equal(stub_last_content, "/roms/nes/metroid.nes"), "and its content");
   companion_core_free(c);
}

static void test_launch_options(void)
{
   companion_core_t *c = make_core();
   companion_launch_option_t opts[6];
   size_t n;
   /* No core loaded, no cores installed: the entry's own path is the
    * one candidate; the playlist default (same path) is not repeated. */
   n = companion_core_launch_options(c, "/cores/fceumm_libretro.so", "FCEUmm",
         "Nintendo - Nintendo Entertainment System", false, opts, 6);
   CHECK(n >= 1, "at least the entry's core, got %u", (unsigned)n);
   CHECK(n >= 1 && string_is_equal(opts[0].path, "/cores/fceumm_libretro.so"), "entry core first");
   {
      size_t i, j, dup = 0;
      for (i = 0; i < n; i++)
         for (j = i + 1; j < n; j++)
            if (string_is_equal(opts[i].path, opts[j].path))
               dup++;
      CHECK(dup == 0, "no duplicate paths");
   }
   companion_core_free(c);
}

int main(void)
{
   setup();
   test_playlist_listing();
   test_select_and_entries();
   test_all_playlists();
   test_thumbnail_path();
   test_browser();
   test_run_paths();
   test_launch_options();
   teardown();
   if (fails)
   {
      printf("companion_core_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("companion_core_test: OK\n");
   return 0;
}
