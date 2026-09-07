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
#ifdef _WIN32
#include <windows.h>
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <boolean.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <lists/dir_list.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../core_option_manager.h"
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
#ifdef _WIN32
#include <direct.h>
static void mkdirp(const char *p) { _mkdir(p); }
static void sleep_briefly_ms(long ns) { Sleep((DWORD)(ns / 1000000 + 1)); }
#else
static void mkdirp(const char *p) { mkdir(p, 0755); }
static void sleep_briefly_ms(long ns) { struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = ns; nanosleep(&ts, NULL); }
#endif

static int  playlist_changed, playlists_changed, browse_changed;

static void on_playlists_changed(void *ud) { (void)ud; playlists_changed++; }
static void on_playlist_changed(void *ud)  { (void)ud; playlist_changed++; }
static void on_browse_changed(void *ud)    { (void)ud; browse_changed++; }

/* The listing is enumerated off the UI thread and lands in iterate():
 * spin it until the browse callback fires (or a timeout). */
static bool wait_browse(companion_core_t *core)
{
   int i;
   int before = browse_changed;
   for (i = 0; i < 200000 && browse_changed == before; i++)
   {
      companion_core_iterate(core, 1000);
      if (browse_changed == before)
      {
         sleep_briefly_ms(200000);
      }
   }
   return browse_changed != before;
}


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

#include <formats/image.h>
static void write_tga_rgba(const char *path, unsigned w, unsigned h, uint32_t argb)
{
   FILE *f = fopen(path, "wb");
   uint8_t hdr[18];
   unsigned i;
   if (!f) return;
   memset(hdr, 0, sizeof(hdr));
   hdr[2] = 2; hdr[12] = (uint8_t)w; hdr[13] = (uint8_t)(w >> 8);
   hdr[14] = (uint8_t)h; hdr[15] = (uint8_t)(h >> 8); hdr[16] = 32; hdr[17] = 0x28;
   fwrite(hdr, 1, 18, f);
   for (i = 0; i < w * h; i++)
   {
      uint8_t px[4];
      px[0] = (uint8_t)argb; px[1] = (uint8_t)(argb >> 8); px[2] = (uint8_t)(argb >> 16); px[3] = (uint8_t)(argb >> 24);
      fwrite(px, 1, 4, f);
   }
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

/* Exported for other harnesses (companion_cocoa_test): the same
 * fixtures and stub settings. */
void companion_test_setup_fixtures(char *out_root, size_t len)
{
   setup();
   if (out_root && len)
      strlcpy(out_root, root, len);
}

static void teardown(void);
void companion_test_teardown_fixtures(const char *r)
{
   (void)r;
   teardown();
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
   cb.on_browse_changed    = on_browse_changed;
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
   char content[512], sub[512];
   char buf[64];
   size_t n, dc;
   bool needs_core = false;
   char pick[PATH_MAX_LENGTH];
   fixture(content, sizeof(content), "content");
   fixture(sub, sizeof(sub), "content/sub");
   CHECK(companion_core_browse_open(c, NULL), "open defaults to the content directory");
   CHECK(wait_browse(c), "listing lands through iterate()");
   CHECK(!companion_core_browse_busy(c), "not busy once landed");
   CHECK(string_is_equal(companion_core_browse_dir(c), content), "browse dir");
   n  = companion_core_browse_count(c);
   dc = companion_core_browse_dir_count(c);
   /* "..", sub | a.nes, b.sfc, cover.png */
   CHECK(n == 5, "5 entries (.. + 1 dir + 3 files), got %u", (unsigned)n);
   CHECK(dc == 2, "2 folder rows (.. and sub), got %u", (unsigned)dc);
   CHECK(string_is_equal(companion_core_browse_name(c, 0), ".."), "first is ..");
   CHECK(string_is_equal(companion_core_browse_name(c, 1), "sub"), "then the folder");
   CHECK(companion_core_browse_is_dir(c, 1) && !companion_core_browse_is_dir(c, 2), "dir / file flags");
   /* metadata gathered with the listing, formatted by the core */
   CHECK(companion_core_browse_size(c, 2) == 1, "a.nes is 1 byte (got %u)", (unsigned)companion_core_browse_size(c, 2));
   CHECK(string_is_equal(companion_core_browse_size_str(c, 2, buf, sizeof(buf)), "1 KB"), "size string: %s", buf);
   CHECK(string_is_equal(companion_core_browse_size_str(c, 1, buf, sizeof(buf)), ""), "folders have no size");
   CHECK(string_is_equal(companion_core_browse_type_str(c, 2, buf, sizeof(buf)), "NES File"), "type string: %s", buf);
   CHECK(string_is_equal(companion_core_browse_type_str(c, 1, buf, sizeof(buf)), "File Folder"), "folder type: %s", buf);
   CHECK(string_is_equal(companion_core_browse_type_str(c, 0, buf, sizeof(buf)), ""), ".. has no type");
   CHECK(companion_core_browse_mtime(c, 2) > 1000000000, "mtime is a real time");
   CHECK(strlen(companion_core_browse_date_str(c, 2, buf, sizeof(buf))) == 16, "date string YYYY-MM-DD HH:MM: %s", buf);
   /* descend */
   CHECK(companion_core_browse_activate(c, 1, NULL, &needs_core, pick, sizeof(pick)) == 0, "activate a folder descends");
   CHECK(wait_browse(c), "sub lands");
   CHECK(string_is_equal(companion_core_browse_dir(c), sub), "in sub");
   CHECK(companion_core_browse_dir_count(c) == 2, "sub: .. and deeper");
   CHECK(companion_core_browse_count(c) == 3, "sub: .. deeper c.gb");
   /* a file with no core: needs-core with its path */
   needs_core = false;
   CHECK(companion_core_browse_activate(c, 2, NULL, &needs_core, pick, sizeof(pick)) < 0 && needs_core, "file without a core asks for one");
   CHECK(strstr(pick, "c.gb") != NULL, "and hands back its path: %s", pick);
   /* up */
   CHECK(companion_core_browse_up(c), "up");
   CHECK(wait_browse(c), "parent lands");
   CHECK(string_is_equal(companion_core_browse_dir(c), content), "back at content (dir is [%s])", companion_core_browse_dir(c));
   /* supersession: open two directories back to back; only the second
    * lands, and nothing blocks meanwhile */
   browse_changed = 0;
   CHECK(companion_core_browse_open(c, sub), "open sub");
   CHECK(companion_core_browse_open(c, content), "open content right after");
   CHECK(wait_browse(c), "the later open lands");
   CHECK(string_is_equal(companion_core_browse_dir(c), content), "and it is content, not sub");
   {
      sleep_briefly_ms(20000000);
      companion_core_iterate(c, 1000);
   }
   CHECK(browse_changed == 1, "the superseded open never delivered (callbacks: %d)", browse_changed);
   /* keep going up to the root: eventually no parent */
   {
      int guard = 0;
      while (companion_core_browse_up(c) && guard++ < 64)
         wait_browse(c);
      CHECK(guard < 64, "reaches a top (dir is [%s])", companion_core_browse_dir(c));
      CHECK(!companion_core_browse_up(c), "up at the top is refused");
   }
   companion_core_free(c);
}

static void test_browser_sort(void)
{
   companion_core_t *c = make_core();
   char big[512];
   size_t n;
   /* a bigger file so size order differs from name order */
   fixture(big, sizeof(big), "content/zz_big.sfc");
   writef(big, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
   browse_changed = 0;
   companion_core_browse_open(c, NULL);
   wait_browse(c);
   n = companion_core_browse_count(c);
   /* .., sub | a.nes b.sfc cover.png zz_big.sfc */
   CHECK(n == 6, "6 entries, got %u", (unsigned)n);
   CHECK(string_is_equal(companion_core_browse_name(c, 2), "a.nes"), "default: a.nes first file");

   browse_changed = 0;
   companion_core_browse_sort(c, COMPANION_BROWSE_SORT_NAME, false);
   CHECK(browse_changed == 1, "sort fires on_browse_changed");
   CHECK(string_is_equal(companion_core_browse_name(c, 0), ".."), "name desc: .. still first");
   CHECK(string_is_equal(companion_core_browse_name(c, 1), "sub"), "name desc: folders still before files");
   CHECK(string_is_equal(companion_core_browse_name(c, 2), "zz_big.sfc"), "name desc: zz_big first file (got %s)", companion_core_browse_name(c, 2));
   CHECK(string_is_equal(companion_core_browse_name(c, 5), "a.nes"), "name desc: a.nes last");

   companion_core_browse_sort(c, COMPANION_BROWSE_SORT_SIZE, false);
   CHECK(string_is_equal(companion_core_browse_name(c, 2), "zz_big.sfc"), "size desc: biggest first (got %s)", companion_core_browse_name(c, 2));
   companion_core_browse_sort(c, COMPANION_BROWSE_SORT_SIZE, true);
   CHECK(string_is_equal(companion_core_browse_name(c, 5), "zz_big.sfc"), "size asc: biggest last (got %s)", companion_core_browse_name(c, 5));
   CHECK(string_is_equal(companion_core_browse_name(c, 2), "a.nes"), "size asc ties broken by name: a.nes (got %s)", companion_core_browse_name(c, 2));

   companion_core_browse_sort(c, COMPANION_BROWSE_SORT_TYPE, true);
   CHECK(string_is_equal(companion_core_browse_name(c, 2), "a.nes"), "type asc: nes < png < sfc (got %s)", companion_core_browse_name(c, 2));
   CHECK(string_is_equal(companion_core_browse_name(c, 3), "cover.png"), "then png (got %s)", companion_core_browse_name(c, 3));
   CHECK(string_is_equal(companion_core_browse_name(c, 4), "b.sfc"), "then sfc by name: b before zz (got %s)", companion_core_browse_name(c, 4));

   /* Date: distinct mtimes set explicitly (the files were written within
    * the same second), newest first when descending. */
   {
      char pa[512], pb[512], pc[512];
#ifdef _WIN32
      struct _utimbuf ut;
#define utime _utime
#else
      struct utimbuf ut;
#endif
      fixture(pa, sizeof(pa), "content/a.nes");
      fixture(pb, sizeof(pb), "content/b.sfc");
      fixture(pc, sizeof(pc), "content/cover.png");
      ut.actime = ut.modtime = 1000000000; utime(pa, &ut);   /* oldest */
      ut.actime = ut.modtime = 1300000000; utime(pb, &ut);
      ut.actime = ut.modtime = 1200000000; utime(pc, &ut);
      companion_core_browse_open(c, NULL);
      wait_browse(c);
      companion_core_browse_sort(c, COMPANION_BROWSE_SORT_DATE, false);
      CHECK(string_is_equal(companion_core_browse_name(c, 2), "zz_big.sfc"), "date desc: the just-written file newest (got %s)", companion_core_browse_name(c, 2));
      CHECK(string_is_equal(companion_core_browse_name(c, 3), "b.sfc"), "then b (1.3e9) (got %s)", companion_core_browse_name(c, 3));
      CHECK(string_is_equal(companion_core_browse_name(c, 4), "cover.png"), "then cover (1.2e9) (got %s)", companion_core_browse_name(c, 4));
      CHECK(string_is_equal(companion_core_browse_name(c, 5), "a.nes"), "a.nes oldest last (got %s)", companion_core_browse_name(c, 5));
      companion_core_browse_sort(c, COMPANION_BROWSE_SORT_DATE, true);
      CHECK(string_is_equal(companion_core_browse_name(c, 2), "a.nes"), "date asc: oldest first (got %s)", companion_core_browse_name(c, 2));
   }
   /* Type: many extensions, grouped and ordered by extension, names
    * within a group. */
   {
      static const char *files[] = { "content/m.zip", "content/k.iso", "content/z.bin", "content/l.iso" };
      char p[512];
      int i;
      for (i = 0; i < 4; i++) { fixture(p, sizeof(p), files[i]); writef(p, "x"); }
      companion_core_browse_open(c, NULL);
      wait_browse(c);
      companion_core_browse_sort(c, COMPANION_BROWSE_SORT_TYPE, true);
      /* bin < iso < nes < png < sfc < zip */
      CHECK(string_is_equal(companion_core_browse_name(c, 2), "z.bin"), "type asc: bin first (got %s)", companion_core_browse_name(c, 2));
      CHECK(string_is_equal(companion_core_browse_name(c, 3), "k.iso"), "iso: k before l (got %s)", companion_core_browse_name(c, 3));
      CHECK(string_is_equal(companion_core_browse_name(c, 4), "l.iso"), "iso: then l (got %s)", companion_core_browse_name(c, 4));
      CHECK(string_is_equal(companion_core_browse_name(c, 9), "m.zip"), "zip last (got %s)", companion_core_browse_name(c, 9));
      companion_core_browse_sort(c, COMPANION_BROWSE_SORT_TYPE, false);
      CHECK(string_is_equal(companion_core_browse_name(c, 2), "m.zip"), "type desc: zip first (got %s)", companion_core_browse_name(c, 2));
      /* the four sorts give four different first files */
      {
         char first[4][64];
         companion_core_browse_sort(c, COMPANION_BROWSE_SORT_NAME, true);
         strlcpy(first[0], companion_core_browse_name(c, 2), 64);
         companion_core_browse_sort(c, COMPANION_BROWSE_SORT_SIZE, false);
         strlcpy(first[1], companion_core_browse_name(c, 2), 64);
         companion_core_browse_sort(c, COMPANION_BROWSE_SORT_TYPE, true);
         strlcpy(first[2], companion_core_browse_name(c, 2), 64);
         companion_core_browse_sort(c, COMPANION_BROWSE_SORT_DATE, true);
         strlcpy(first[3], companion_core_browse_name(c, 2), 64);
         CHECK(!string_is_equal(first[0], first[1]) && !string_is_equal(first[1], first[2])
               && !string_is_equal(first[2], first[3]) && !string_is_equal(first[0], first[2]),
               "each column orders differently: name=%s size=%s type=%s date=%s",
               first[0], first[1], first[2], first[3]);
      }
      /* clean up the extras for the tests after this one */
      for (i = 0; i < 4; i++) { fixture(p, sizeof(p), files[i]); remove(p); }
   }

   /* the order persists across a re-enumeration */
   companion_core_browse_sort(c, COMPANION_BROWSE_SORT_NAME, false);
   companion_core_browse_open(c, NULL);
   wait_browse(c);
   CHECK(companion_core_browse_sort_column(c) == COMPANION_BROWSE_SORT_NAME && !companion_core_browse_sort_ascending(c), "sort setting kept");
   CHECK(string_is_equal(companion_core_browse_name(c, 2), "zz_big.sfc"), "re-opened listing is in the chosen order (got %s)", companion_core_browse_name(c, 2));
   companion_core_free(c);
}

void companion_core_test_sort_listing(companion_core_t *core,
      struct string_list *list, uint64_t *size, int64_t *mtime,
      enum companion_browse_column column, bool ascending);

/* The crash: a listing with no metadata arrays (the Windows drive list
 * was built that way) sorted by Date or Size dereferenced NULL. Built
 * here directly, on every platform, so the comparator's NULL-safety is
 * tested where the drive list cannot be. */
static void test_sort_without_metadata(void)
{
   companion_core_t *c = make_core();
   struct string_list *l = string_list_new();
   union string_list_elem_attr attr;
   int col, asc;
   attr.i = RARCH_DIRECTORY;
   string_list_append(l, "C:\\", attr);
   string_list_append(l, "A:\\", attr);
   string_list_append(l, "D:\\", attr);
   for (col = COMPANION_BROWSE_SORT_NAME; col <= COMPANION_BROWSE_SORT_DATE; col++)
      for (asc = 0; asc < 2; asc++)
         companion_core_test_sort_listing(c, l, NULL, NULL,
               (enum companion_browse_column)col, asc != 0);
   companion_core_test_sort_listing(c, l, NULL, NULL, COMPANION_BROWSE_SORT_NAME, true);
   CHECK(string_is_equal(l->elems[0].data, "A:\\"), "sorted by name with no metadata: A first (got %s)", l->elems[0].data);
   string_list_free(l);
   companion_core_free(c);
}

/* Then the same shape through the real job path: sort by Date (or
 * Size), then open a listing made only of folders. */
static void test_sort_then_folders_only(void)
{
   companion_core_t *c = make_core();
   char p[512];
   int col;
   fixture(p, sizeof(p), "onlydirs"); mkdirp(p);
   fixture(p, sizeof(p), "onlydirs/b"); mkdirp(p);
   fixture(p, sizeof(p), "onlydirs/a"); mkdirp(p);
   fixture(p, sizeof(p), "onlydirs/c"); mkdirp(p);
   fixture(p, sizeof(p), "onlydirs");
   for (col = COMPANION_BROWSE_SORT_NAME; col <= COMPANION_BROWSE_SORT_DATE; col++)
   {
      int asc;
      for (asc = 0; asc < 2; asc++)
      {
         companion_core_browse_sort(c, (enum companion_browse_column)col, asc != 0);
         browse_changed = 0;
         CHECK(companion_core_browse_open(c, p), "open folders-only (col %d asc %d)", col, asc);
         CHECK(wait_browse(c), "lands (col %d asc %d)", col, asc);
         CHECK(companion_core_browse_count(c) == 4, "4 entries (.. a b c)");
         CHECK(companion_core_browse_dir_count(c) == 4, "all folders");
         /* and sorting the landed listing again, every column */
         companion_core_browse_sort(c, COMPANION_BROWSE_SORT_DATE, false);
         companion_core_browse_sort(c, COMPANION_BROWSE_SORT_SIZE, true);
         companion_core_browse_sort(c, COMPANION_BROWSE_SORT_TYPE, false);
         companion_core_browse_sort(c, COMPANION_BROWSE_SORT_NAME, true);
         CHECK(string_is_equal(companion_core_browse_name(c, 0), ".."), ".. first");
         CHECK(string_is_equal(companion_core_browse_name(c, 1), "a"), "then a (got %s)", companion_core_browse_name(c, 1));
      }
   }
   companion_core_free(c);
}

/* A view that, on every "listing changed", re-applies the sort it is
 * showing (Qt's table does this on a model reset). With a sort that
 * fired the callback even when nothing changed, that recursed until the
 * stack ran out. The listener counts its depth and stops itself at 3,
 * so the test reports the loop instead of crashing in it. */
static int reapply_depth, reapply_max;
static void on_browse_changed_reapply(void *ud)
{
   companion_core_t *c = (companion_core_t*)ud;
   reapply_depth++;
   if (reapply_depth > reapply_max)
      reapply_max = reapply_depth;
   if (reapply_depth <= 3)
      companion_core_browse_sort(c, companion_core_browse_sort_column(c),
            companion_core_browse_sort_ascending(c));
   reapply_depth--;
}

static void test_sort_callback_reentrancy(void)
{
   companion_callbacks_t cb;
   companion_core_t *c;
   char content[512];
   fixture(content, sizeof(content), "content");
   memset(&cb, 0, sizeof(cb));
   cb.on_browse_changed = on_browse_changed_reapply;
   c = companion_core_new(&cb, NULL);
   companion_core_set_ud(c, c);          /* the listener sorts this core */
   reapply_depth = reapply_max = 0;
   companion_core_browse_open(c, content);
   wait_browse(c);
   CHECK(reapply_max <= 1, "listing landed: callback depth %d (a loop would be 3+)", reapply_max);
   reapply_max = 0;
   companion_core_browse_sort(c, COMPANION_BROWSE_SORT_SIZE, false);
   CHECK(reapply_max == 1, "one change -> one callback, no recursion (depth %d)", reapply_max);
   reapply_max = 0;
   companion_core_browse_sort(c, COMPANION_BROWSE_SORT_SIZE, false);
   CHECK(reapply_max == 0, "same sort again -> no callback at all (depth %d)", reapply_max);
   companion_core_free(c);
}

/* Scenario: the click sequences the backends send, in order, the way
 * a user drives the browser - every step must complete and nothing may
 * fault. Run under ASan / TSan this is the regression net for the
 * core side of every companion. */
static void test_backend_scenario(void)
{
   companion_core_t *c = make_core();
   char content[512], pick[PATH_MAX_LENGTH], buf[64];
   bool needs_core = false;
   size_t i, n;
   fixture(content, sizeof(content), "content");

   /* tab -> File Browser: open default, listing lands */
   CHECK(companion_core_browse_open(c, NULL) && wait_browse(c), "enter browser");
   /* header clicks: every column, both ways, on the live listing */
   for (i = 0; i < 8; i++)
      companion_core_browse_sort(c, (enum companion_browse_column)(i % 4), (i & 1) != 0);
   /* select each row (the backends resolve name / path / type / size /
    * date / is-dir for the boxart pane and the table) */
   n = companion_core_browse_count(c);
   for (i = 0; i < n; i++)
   {
      CHECK(companion_core_browse_name(c, i) != NULL, "name %u", (unsigned)i);
      CHECK(companion_core_browse_path(c, i) != NULL, "path %u", (unsigned)i);
      companion_core_browse_size_str(c, i, buf, sizeof(buf));
      companion_core_browse_type_str(c, i, buf, sizeof(buf));
      companion_core_browse_date_str(c, i, buf, sizeof(buf));
      (void)companion_core_browse_is_dir(c, i);
      (void)companion_core_browse_size(c, i);
      (void)companion_core_browse_mtime(c, i);
   }
   /* double-click the folder, then a file, then Up, then the buttons */
   CHECK(companion_core_browse_activate(c, 1, NULL, &needs_core, pick, sizeof(pick)) == 0 && wait_browse(c), "descend");
   CHECK(companion_core_browse_activate(c, 2, NULL, &needs_core, pick, sizeof(pick)) < 0, "activate a file");
   CHECK(companion_core_browse_up(c) && wait_browse(c), "Up");
   CHECK(companion_core_browse_open(c, content) && wait_browse(c), "Start Directory");
   /* rapid navigation: several opens without waiting */
   companion_core_browse_open(c, content);
   companion_core_browse_up(c);
   companion_core_browse_open(c, content);
   companion_core_browse_up(c);
   CHECK(wait_browse(c), "rapid navigation settles");
   /* and the sort again on whatever landed */
   companion_core_browse_sort(c, COMPANION_BROWSE_SORT_DATE, false);
   companion_core_browse_sort(c, COMPANION_BROWSE_SORT_NAME, true);
   /* out of the browser and back into a playlist */
   companion_core_refresh_playlists(c);
   CHECK(companion_core_select_playlist(c, 2) && iterate_until_loaded(c), "back to a playlist");
   CHECK(companion_core_entry_count(c) == 3, "entries intact");
   companion_core_free(c);
}

/* Rename: only within the playlists directory; not over an existing
 * name; the list refreshes. Add files: files and directories (walked),
 * label from the file name, db_name from the playlist. Thumbnail
 * install: a PNG lands at the repository path, downscaled to the
 * setting. */
static void test_rename_add_install(void)
{
   companion_core_t *c = make_core();
   char gen[512], out[PATH_MAX_LENGTH], hist[512], p[512], sub[512];
   const char *paths[2];
   size_t n;
   companion_core_refresh_playlists(c);
   fixture(gen, sizeof(gen), "playlists/Sega - Mega Drive - Genesis.lpl");
   fixture(hist, sizeof(hist), "history.lpl");
   CHECK(!companion_core_playlist_rename(c, hist, "Other", out, sizeof(out)), "history (outside the dir) cannot be renamed");
   CHECK(!companion_core_playlist_rename(c, gen, "Nintendo - Nintendo Entertainment System", out, sizeof(out)), "not over an existing playlist");
   CHECK(!companion_core_playlist_rename(c, gen, "a/b", out, sizeof(out)), "no path separators");
   CHECK(companion_core_playlist_rename(c, gen, "Sega - Genesis", out, sizeof(out)), "rename");
   CHECK(string_ends_with(out, "playlists/Sega - Genesis.lpl"), "new path %s", out);
   CHECK(path_is_valid(out) && !path_is_valid(gen), "file moved");
   CHECK(string_is_equal(companion_core_playlist_name(c, 3), "Sega - Genesis"), "list refreshed (got %s)", companion_core_playlist_name(c, 3));
   /* add: one file and one directory (content/sub has c.gb) */
   fixture(p, sizeof(p), "content/a.nes");
   fixture(sub, sizeof(sub), "content/sub");
   paths[0] = p; paths[1] = sub;
   n = companion_core_playlist_add_files(c, out, paths, 2, NULL, NULL);
   CHECK(n == 2, "added a.nes and sub/c.gb (got %u)", (unsigned)n);
   CHECK(companion_core_select_playlist_path(c, out) && iterate_until_loaded(c), "reload the renamed playlist");
   CHECK(companion_core_entry_count(c) == 4, "2 + 2 entries (got %u)", (unsigned)companion_core_entry_count(c));
   {
      /* entries are shown in the file's order; the two added are last */
      const struct playlist_entry *e = NULL;
      size_t k;
      for (k = 0; k < companion_core_entry_count(c); k++)
         if (string_is_equal(companion_core_entry(c, k)->label, "a"))
            e = companion_core_entry(c, k);
      CHECK(e != NULL, "added entry 'a' present");
      CHECK(e && string_is_equal(e->label, "a"), "label is the file name without extension (got %s)", e ? e->label : "-");
      CHECK(e && string_is_equal(e->db_name, "Sega - Genesis.lpl"), "db_name is the playlist (got %s)", e ? e->db_name : "-");
      CHECK(e && string_is_equal(e->core_path, "DETECT"), "core DETECT");
   }
   CHECK(companion_core_playlist_add_files(c, COMPANION_ALL_PLAYLISTS_TOKEN, paths, 1, NULL, NULL) == 0, "All Playlists takes no files");
   /* The backends pass the core's own selected path back in: the
    * reload must not copy that buffer onto itself (an overlapping
    * strlcpy traps on macOS's fortified libc; this is what the harness
    * hit on Apple's runner). */
   {
      const char *own = companion_core_selected_playlist_path(c);
      size_t before   = companion_core_entry_count(c);
      char bsfc[512];
      const char *one[1];
      fixture(bsfc, sizeof(bsfc), "content/b.sfc");   /* not in the playlist yet */
      one[0] = bsfc;
      CHECK(own && *own, "a playlist is selected");
      n = companion_core_playlist_add_files(c, own, one, 1, NULL, NULL);
      CHECK(n == 1, "add through the selected-path alias (got %u)", (unsigned)n);
      CHECK(iterate_until_loaded(c), "reload landed");
      CHECK(companion_core_entry_count(c) == before + 1, "entries %u -> %u", (unsigned)before, (unsigned)companion_core_entry_count(c));
      CHECK(string_is_equal(companion_core_selected_playlist_path(c), own), "selected path intact");
   }
   /* thumbnail install from a TGA dropped on the boxart pane */
   {
      char img[512];
      fixture(img, sizeof(img), "drop.tga");
      write_tga_rgba(img, 64, 32, 0xff336699u);
      test_settings.uints.desktop_menu_thumbnail_max_size = 16;
      CHECK(companion_core_thumbnail_install(c, "Sega - Genesis", COMPANION_THUMB_BOXART,
               "a", img, out, sizeof(out)), "install");
      CHECK(string_ends_with(out, "Sega - Genesis/Named_Boxarts/a.png"), "at the repository path: %s", out);
      CHECK(path_is_valid(out), "png written");
      {
         struct texture_image ti;
         memset(&ti, 0, sizeof(ti));
         CHECK(image_texture_load(&ti, out) && ti.width == 16 && ti.height == 8, "downscaled to max size 16 (got %ux%u)", ti.width, ti.height);
         image_texture_free(&ti);
      }
      test_settings.uints.desktop_menu_thumbnail_max_size = 0;
   }
   companion_core_free(c);
}

/* Core options and shader parameters as the dialogs see them: from a
 * real core_option_manager built from v2 definitions, and the stub
 * menu shader. */
static void test_options_and_shader_params(void)
{
   companion_core_t *c = make_core();
   static struct retro_core_option_v2_definition defs[3];
   static const struct retro_core_option_v2_definition none = { 0 };
   struct retro_core_options_v2 v2;
   char cfg[512];
   float mn, mx, stp, ini;
   memset(defs, 0, sizeof(defs));
   defs[0].key = "test_speed"; defs[0].desc = "Speed"; defs[0].info = "How fast";
   defs[0].values[0].value = "slow"; defs[0].values[0].label = "Slow";
   defs[0].values[1].value = "fast"; defs[0].values[1].label = "Fast";
   defs[0].default_value = "fast";
   defs[1].key = "test_color"; defs[1].desc = "Colour";
   defs[1].values[0].value = "rgb"; defs[1].values[1].value = "mono";
   defs[1].default_value = "rgb";
   defs[2] = none;
   v2.categories = NULL;
   v2.definitions = defs;
   fixture(cfg, sizeof(cfg), "core.opt");
   test_runloop.core_options = core_option_manager_new(cfg, NULL, &v2, false);
   CHECK(test_runloop.core_options != NULL, "option manager built");

   CHECK(companion_core_option_count(c) == 2, "2 options (got %u)", (unsigned)companion_core_option_count(c));
   CHECK(string_is_equal(companion_core_option_desc(c, 0), "Speed"), "desc (got %s)", companion_core_option_desc(c, 0));
   CHECK(string_is_equal(companion_core_option_info(c, 0), "How fast"), "info");
   CHECK(companion_core_option_value_count(c, 0) == 2, "2 values");
   CHECK(string_is_equal(companion_core_option_value_label(c, 0, 1), "Fast"), "value label (got %s)", companion_core_option_value_label(c, 0, 1));
   CHECK(string_is_equal(companion_core_option_value_label(c, 1, 1), "mono"), "no label: the value (got %s)", companion_core_option_value_label(c, 1, 1));
   CHECK(companion_core_option_current(c, 0) == 1, "current is the default (fast)");
   companion_core_option_set(c, 0, 0);
   CHECK(companion_core_option_current(c, 0) == 0, "set to slow");
   companion_core_option_reset(c, 0);
   CHECK(companion_core_option_current(c, 0) == 1, "reset to the default");
   companion_core_option_set(c, 0, 5);
   CHECK(companion_core_option_current(c, 0) == 1, "out-of-range value ignored");
   companion_core_option_set(c, 1, 1);
   companion_core_option_reset_all(c);
   CHECK(companion_core_option_current(c, 1) == 0, "reset all");

   CHECK(companion_core_shader_param_count(c) == 2, "2 shader params");
   CHECK(string_is_equal(companion_core_shader_param_desc(c, 0), "Scanline strength"), "param desc");
   CHECK(string_is_equal(companion_core_shader_param_desc(c, 1), "CURV"), "no desc: the id (got %s)", companion_core_shader_param_desc(c, 1));
   CHECK(companion_core_shader_param_range(c, 0, &mn, &mx, &stp, &ini) && mn == 0.0f && mx == 1.0f && ini == 0.5f, "range");
   companion_core_shader_param_set(c, 0, 0.75f);
   CHECK(companion_core_shader_param_current(c, 0) == 0.75f, "set");
   companion_core_shader_param_set(c, 0, 9.0f);
   CHECK(companion_core_shader_param_current(c, 0) == 1.0f, "clamped to max");
   companion_core_shader_param_reset(c, 0);
   CHECK(companion_core_shader_param_current(c, 0) == 0.5f, "reset to initial");
   CHECK(string_is_equal(companion_core_shader_path(c), "/shaders/crt.slangp"), "shader path");
   {
      extern int stub_calls_shader_apply;
      int before = stub_calls_shader_apply;
      companion_core_shader_apply(c);
      CHECK(stub_calls_shader_apply == before + 1, "apply fires CMD_EVENT_SHADERS_APPLY_CHANGES");
   }
   core_option_manager_free(test_runloop.core_options);
   test_runloop.core_options = NULL;
   companion_core_free(c);
}

/* The Options table: every row reads back what it says, sets from
 * text with type checks, and lands in the settings. */
static void test_settings_table(void)
{
   companion_core_t *c = make_core();
   char buf[64];
   size_t i, n = companion_core_setting_count(c);
   CHECK(n == 13, "13 rows (got %u)", (unsigned)n);
   for (i = 0; i < n; i++)
      CHECK(*companion_core_setting_label(c, i) && *companion_core_setting_get(c, i, buf, sizeof(buf)) != 2, "row %u has a label", (unsigned)i);
   CHECK(companion_core_setting_kind(c, 2) == COMPANION_SETTING_CHOICE && companion_core_setting_choice_count(c, 2) == 3, "theme is a 3-way choice");
   CHECK(companion_core_setting_set(c, 2, "Dark") && test_settings.uints.desktop_menu_theme == 1, "theme by label");
   CHECK(companion_core_setting_set(c, 2, "2") && test_settings.uints.desktop_menu_theme == 2, "theme by index");
   CHECK(!companion_core_setting_set(c, 2, "purple"), "unknown theme refused");
   CHECK(string_is_equal(companion_core_setting_get(c, 2, buf, sizeof(buf)), "Custom"), "theme reads back (got %s)", buf);
   CHECK(companion_core_setting_set(c, 0, "true") && test_settings.bools.desktop_menu_save_geometry, "bool from 'true'");
   CHECK(!companion_core_setting_set(c, 0, "maybe"), "bad bool refused");
   CHECK(companion_core_setting_set(c, 7, "256") && test_settings.uints.desktop_menu_thumbnail_cache_limit == 256, "uint");
   CHECK(!companion_core_setting_set(c, 7, "12x"), "bad uint refused");
   CHECK(companion_core_setting_set(c, 4, "#ff8800") && string_is_equal(test_settings.arrays.desktop_menu_highlight_color, "#ff8800"), "string");
   CHECK(string_is_equal(companion_core_setting_get(c, 4, buf, sizeof(buf)), "#ff8800"), "string reads back");
   companion_core_free(c);
}

/* Hidden playlists: the comma-separated setting Qt's context menu
 * maintains, parsed and written in C (string_split drops empty parts on
 * every Qt back to 4, which Qt's own split() flag does not). */
static void test_hidden_playlists(void)
{
   companion_core_t *c = make_core();
   const char *nes = "/pl/Nintendo - Nintendo Entertainment System.lpl";
   const char *gen = "/pl/Sega - Mega Drive - Genesis.lpl";
   test_settings.arrays.desktop_menu_hidden_playlists[0] = '\0';
   CHECK(!companion_core_playlist_is_hidden(c, nes), "nothing hidden to start");
   companion_core_playlist_set_hidden(c, nes, true);
   CHECK(companion_core_playlist_is_hidden(c, nes), "hidden after setting");
   CHECK(string_is_equal(test_settings.arrays.desktop_menu_hidden_playlists,
            "Nintendo - Nintendo Entertainment System.lpl"), "the file name only (got %s)",
         test_settings.arrays.desktop_menu_hidden_playlists);
   companion_core_playlist_set_hidden(c, nes, true);
   CHECK(string_is_equal(test_settings.arrays.desktop_menu_hidden_playlists,
            "Nintendo - Nintendo Entertainment System.lpl"), "hiding twice adds one entry");
   companion_core_playlist_set_hidden(c, gen, true);
   CHECK(companion_core_playlist_is_hidden(c, gen) && companion_core_playlist_is_hidden(c, nes), "both hidden");
   companion_core_playlist_set_hidden(c, nes, false);
   CHECK(!companion_core_playlist_is_hidden(c, nes) && companion_core_playlist_is_hidden(c, gen), "one unhidden, the other kept");
   CHECK(string_is_equal(test_settings.arrays.desktop_menu_hidden_playlists,
            "Sega - Mega Drive - Genesis.lpl"), "list rewritten (got %s)",
         test_settings.arrays.desktop_menu_hidden_playlists);
   /* a stray comma must not survive, and must not match "" */
   strlcpy(test_settings.arrays.desktop_menu_hidden_playlists, "a.lpl,,b.lpl,",
         sizeof(test_settings.arrays.desktop_menu_hidden_playlists));
   CHECK(!companion_core_playlist_is_hidden(c, "/pl/"), "an empty name matches nothing");
   companion_core_playlist_set_hidden(c, "/pl/c.lpl", true);
   CHECK(string_is_equal(test_settings.arrays.desktop_menu_hidden_playlists, "a.lpl,b.lpl,c.lpl"),
         "empty parts dropped on the round trip (got %s)", test_settings.arrays.desktop_menu_hidden_playlists);
   companion_core_playlist_set_hidden(c, "/pl/nothere.lpl", false);
   CHECK(string_is_equal(test_settings.arrays.desktop_menu_hidden_playlists, "a.lpl,b.lpl,c.lpl"),
         "unhiding an absent name changes nothing");
   test_settings.arrays.desktop_menu_hidden_playlists[0] = '\0';
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

#ifndef COMPANION_TEST_NO_MAIN
int main(void)
{
   setup();
   test_playlist_listing();
   test_select_and_entries();
   test_all_playlists();
   test_thumbnail_path();
   test_browser();
   test_browser_sort();
   test_sort_without_metadata();
   test_sort_then_folders_only();
   test_backend_scenario();
   test_sort_callback_reentrancy();
   test_rename_add_install();
   test_options_and_shader_params();
   test_settings_table();
   test_hidden_playlists();
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
#endif /* COMPANION_TEST_NO_MAIN */
