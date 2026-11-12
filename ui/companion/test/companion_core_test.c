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
#include "../../../gfx/gfx_thumbnail_results.h"
#include "../companion_core.h"

extern char stub_match_push_path[];
extern void stub_core_info_find_set(const char *path, const char *databases);
#ifdef HAVE_NETWORKING
#include "../../../tasks/tasks_internal.h"
extern char stub_http_url[PATH_MAX_LENGTH];
extern retro_task_callback_t stub_http_cb;
extern void *stub_http_ud;
#endif
extern settings_t test_settings;
extern runloop_state_t test_runloop;
extern int  stub_calls_load_with_new_core;
extern int  stub_calls_load_with_current_core;
extern char stub_last_content[PATH_MAX_LENGTH];
extern char stub_last_core[PATH_MAX_LENGTH];

static int fails;
/* A companion_rect_t, read back per axis. */
#define RX(r) VIDEO_POS_X((r).pos)
#define RY(r) VIDEO_POS_Y((r).pos)
#define RW(r) ((int)VIDEO_SCALE_W((r).dims))
#define RH(r) ((int)VIDEO_SCALE_H((r).dims))
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
static int  thumbnails_changed;

static void on_playlists_changed(void *ud) { (void)ud; playlists_changed++; }
static void on_playlist_changed(void *ud)  { (void)ud; playlist_changed++; }
static void on_browse_changed(void *ud)    { (void)ud; browse_changed++; }
static void on_thumbnails_changed(void *ud) { (void)ud; thumbnails_changed++; }

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

static bool file_is(const char *path, const char *text)
{
   char buf[64];
   size_t n;
   FILE *f = fopen(path, "rb");
   if (!f)
      return false;
   n = fread(buf, 1, sizeof(buf), f);
   fclose(f);
   return n == strlen(text) && !memcmp(buf, text, n);
}

#include <formats/image.h>
#include <streams/image_texture_file.h>
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
   cb.on_thumbnails_changed = on_thumbnails_changed;
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
   /* '"' is replaced as the downloader replaces it */
   fixture(want, sizeof(want),
         "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Say _Hi_ (USA).png");
   writef(want, "png");
   n = companion_core_thumbnail_path(c, "Nintendo - Nintendo Entertainment System",
         COMPANION_THUMB_BOXART, "Say \"Hi\" (USA)", "/x", out, sizeof(out));
   CHECK(n > 0 && string_is_equal(out, want), "\" sanitised to _: %s", out);
   /* then the content file name */
   fixture(want, sizeof(want),
         "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Kid Icarus (USA).png");
   writef(want, "png");
   n = companion_core_thumbnail_path(c, "Nintendo - Nintendo Entertainment System",
         COMPANION_THUMB_BOXART, "kid icarus", "/roms/nes/Kid Icarus (USA).nes",
         out, sizeof(out));
   CHECK(n > 0 && string_is_equal(out, want), "the file name after the label: %s", out);
   /* an entry without a label, as Qt passes it */
   n = companion_core_thumbnail_path(c, "Nintendo - Nintendo Entertainment System",
         COMPANION_THUMB_BOXART, "/roms/nes/Kid Icarus (USA).nes",
         "/roms/nes/Kid Icarus (USA).nes", out, sizeof(out));
   CHECK(n > 0 && string_is_equal(out, want), "no label: the file stem: %s", out);
   /* then the match the thumbnail task stored, if the menu shows it */
   {
      size_t rec;
      gfx_thumbnail_results_t *t = gfx_thumbnail_results_new();
      rec = gfx_thumbnail_results_add(t, "/roms/nes/m.nes", "Metroid (U) [!]", "",
            "Nintendo - Nintendo Entertainment System");
      gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
            "Metroid (Europe).png");
      rec = gfx_thumbnail_results_add(t, "/roms/nes/k.nes", "Kirby (U)", "",
            "Nintendo - Nintendo Entertainment System");
      gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
            "Kirby (Europe).jpg");
      gfx_thumbnail_results_sort(t);
      gfx_thumbnail_results_open("/pl/companion.lpl");
      gfx_thumbnail_results_install("/pl/companion.lpl", t, true);
   }
   fixture(want, sizeof(want),
         "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Metroid (Europe).png");
   writef(want, "png");
   n = companion_core_thumbnail_path(c, "Nintendo - Nintendo Entertainment System",
         COMPANION_THUMB_BOXART, "Metroid (U) [!]", "/roms/nes/m.nes", out, sizeof(out));
   CHECK(n > 0 && string_is_equal(out, want), "then the stored match: %s", out);
   fixture(want, sizeof(want),
         "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Kirby (Europe).jpg");
   writef(want, "jpg");
   n = companion_core_thumbnail_path(c, "Nintendo - Nintendo Entertainment System",
         COMPANION_THUMB_BOXART, "Kirby (U)", "/roms/nes/k.nes", out, sizeof(out));
   CHECK(n > 0 && string_ends_with(out, "Named_Boxarts/Kirby (U).png"),
         "a non-PNG match only with playlist_allow_non_png: %s", out);
   test_settings.bools.playlist_allow_non_png = true;
   n = companion_core_thumbnail_path(c, "Nintendo - Nintendo Entertainment System",
         COMPANION_THUMB_BOXART, "Kirby (U)", "/roms/nes/k.nes", out, sizeof(out));
   CHECK(n > 0 && string_is_equal(out, want), "a non-PNG match when allowed: %s", out);
   test_settings.bools.playlist_allow_non_png = false;
   gfx_thumbnail_results_deinit();
   companion_core_free(c);
}

/* A one-entry playlist without a db_name. */
static void write_row_lpl(const char *rel, const char *path,
      const char *label, const char *core_path, const char *core_name)
{
   char p[512];
   char buf[1024];
   fixture(p, sizeof(p), rel);
   snprintf(buf, sizeof(buf),
         "{\n  \"version\": \"1.5\",\n  \"items\": [\n"
         "    {\n      \"path\": \"%s\",\n      \"label\": \"%s\",\n"
         "      \"core_path\": \"%s\",\n      \"core_name\": \"%s\",\n"
         "      \"crc32\": \"\",\n      \"db_name\": \"\"\n    }\n  ]\n}\n",
         path, label, core_path, core_name);
   writef(p, buf);
}

/* Entry @i's art as a companion looks it up, with the system the
 * entry resolves to. */
static size_t entry_boxart(companion_core_t *c, size_t i,
      char *sys, size_t sys_len, char *out, size_t len)
{
   const struct playlist_entry *e = companion_core_entry(c, i);
   companion_core_entry_thumbnail_system(c, i, sys, sys_len);
   if (!e)
      return 0;
   return companion_core_thumbnail_path(c, sys, COMPANION_THUMB_BOXART,
         e->label, e->path, out, len);
}

/* History and favourites rows without a db_name find the match the
 * thumbnail task stored, under the system it stored it: the core's
 * database, else the content's folder (as the menu). */
static void test_entry_thumbnail_system(void)
{
   static const char *const nes = "Nintendo - Nintendo Entertainment System";
   companion_core_t *c = make_core();
   char p[512], sys[NAME_MAX_LENGTH];
   char out[512], want[512];
   gfx_thumbnail_results_t *t;
   size_t rec, n;

   fixture(p, sizeof(p), "hist"); mkdirp(p);
   write_row_lpl("hist/content_history.lpl", "/roms/nes/zelda.nes",
         "Legend of Zelda, The (U) [!]", "/cores/fceumm_libretro.so", "FCEUmm");
   write_row_lpl("hist/content_favorites.lpl",
         "/roms/Nintendo - Nintendo Entertainment System/Tetris (U) [!].nes",
         "Tetris (U) [!]", "/cores/nodb_libretro.so", "NoDB");
   fixture(p, sizeof(p), "fav2"); mkdirp(p);
   write_row_lpl("fav2/content_favorites.lpl", "/roms/My.Roms/Rygar.nes",
         "rygar_dump", "/cores/nodb_libretro.so", "NoDB");
   write_row_lpl("hist/Collection.lpl", "/roms/nes/smb.nes",
         "super mario bros", "DETECT", "DETECT");
   stub_core_info_find_set("/cores/fceumm_libretro.so",
         "Nintendo - Nintendo Entertainment System|Nintendo - Family Computer Disk System");

   t   = gfx_thumbnail_results_new();
   rec = gfx_thumbnail_results_add(t, "/roms/nes/zelda.nes",
         "Legend of Zelda, The (U) [!]", "", nes);
   gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
         "Legend of Zelda, The (USA).png");
   gfx_thumbnail_results_sort(t);
   fixture(p, sizeof(p), "hist/content_history.lpl");
   gfx_thumbnail_results_open(p);
   gfx_thumbnail_results_install(p, t, true);
   t   = gfx_thumbnail_results_new();
   rec = gfx_thumbnail_results_add(t,
         "/roms/Nintendo - Nintendo Entertainment System/Tetris (U) [!].nes",
         "Tetris (U) [!]", "", nes);
   gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
         "Tetris (USA).png");
   gfx_thumbnail_results_sort(t);
   fixture(p, sizeof(p), "hist/content_favorites.lpl");
   gfx_thumbnail_results_open(p);
   gfx_thumbnail_results_install(p, t, true);
   t   = gfx_thumbnail_results_new();
   rec = gfx_thumbnail_results_add(t, "/roms/My.Roms/Rygar.nes",
         "rygar_dump", "", "My.Roms");
   gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
         "Rygar (USA).png");
   gfx_thumbnail_results_sort(t);
   fixture(p, sizeof(p), "fav2/content_favorites.lpl");
   gfx_thumbnail_results_open(p);
   gfx_thumbnail_results_install(p, t, true);

   fixture(want, sizeof(want), "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Legend of Zelda, The (USA).png");
   writef(want, "png");
   fixture(want, sizeof(want), "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Tetris (USA).png");
   writef(want, "png");
   fixture(p, sizeof(p), "thumbnails/My.Roms"); mkdirp(p);
   fixture(p, sizeof(p), "thumbnails/My.Roms/Named_Boxarts"); mkdirp(p);
   fixture(want, sizeof(want), "thumbnails/My.Roms/Named_Boxarts/Rygar (USA).png");
   writef(want, "png");

   /* History: the core's first database */
   fixture(p, sizeof(p), "hist/content_history.lpl");
   CHECK(companion_core_select_playlist_path(c, p) && iterate_until_loaded(c),
         "select %s", p);
   entry_boxart(c, 0, sys, sizeof(sys), out, sizeof(out));
   fixture(want, sizeof(want), "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Legend of Zelda, The (USA).png");
   CHECK(string_is_equal(sys, nes), "history row: its core's database (got '%s')", sys);
   CHECK(string_is_equal(out, want), "history row: the stored match: %s", out);

   /* Favourites, a core without a database: the content's folder */
   fixture(p, sizeof(p), "hist/content_favorites.lpl");
   CHECK(companion_core_select_playlist_path(c, p) && iterate_until_loaded(c),
         "select %s", p);
   entry_boxart(c, 0, sys, sizeof(sys), out, sizeof(out));
   fixture(want, sizeof(want), "thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Tetris (USA).png");
   CHECK(string_is_equal(sys, nes), "favourites row: the content's folder (got '%s')", sys);
   CHECK(string_is_equal(out, want), "favourites row: the stored match: %s", out);
   n = companion_core_entry_thumbnail_system(c, 0, sys, sizeof(sys));
   CHECK(n == strlen(sys), "content folder: returns the length written (%u, wrote %u)",
         (unsigned)n, (unsigned)strlen(sys));

   /* A folder with a dot: looked up under the name as it is */
   fixture(p, sizeof(p), "fav2/content_favorites.lpl");
   CHECK(companion_core_select_playlist_path(c, p) && iterate_until_loaded(c),
         "select %s", p);
   entry_boxart(c, 0, sys, sizeof(sys), out, sizeof(out));
   fixture(want, sizeof(want), "thumbnails/My.Roms/Named_Boxarts/Rygar (USA).png");
   CHECK(string_is_equal(sys, "My.Roms"), "dotted folder kept whole (got '%s')", sys);
   CHECK(string_is_equal(out, want), "dotted folder: the stored match: %s", out);

   /* A collection row: the playlist's name */
   fixture(p, sizeof(p), "hist/Collection.lpl");
   CHECK(companion_core_select_playlist_path(c, p) && iterate_until_loaded(c),
         "select %s", p);
   companion_core_entry_thumbnail_system(c, 0, sys, sizeof(sys));
   CHECK(string_is_equal(sys, "Collection"), "collection row: its playlist (got '%s')", sys);

   stub_core_info_find_set(NULL, NULL);
   gfx_thumbnail_results_deinit();
   companion_core_free(c);
}

static void test_thumbnail_match_push(void)
{
   char pl[PATH_MAX_LENGTH];
   companion_core_t *c = make_core();

   fixture(pl, sizeof(pl), "Match.lpl");
   writef(pl, "{\"version\": \"1.5\", \"items\": []}\n");
   stub_match_push_path[0] = '\0';
   CHECK(companion_core_select_playlist_path(c, pl) && iterate_until_loaded(c),
         "select %s", pl);
   CHECK(string_is_equal(stub_match_push_path, pl),
         "a loaded playlist asks for its thumbnail matches: %s",
         stub_match_push_path);
   thumbnails_changed = 0;
   companion_core_notify_thumbnails(c, "/elsewhere/Other.lpl");
   CHECK(thumbnails_changed == 0, "another playlist's matches: no notice");
   companion_core_notify_thumbnails(c, pl);
   CHECK(thumbnails_changed == 1, "the shown playlist's matches: %d notices",
         thumbnails_changed);
   companion_core_free(c);
}

#ifdef HAVE_NETWORKING
static bool file_holds(const char *path, const char *text)
{
   char buf[64];
   size_t n;
   FILE *f = fopen(path, "rb");
   if (!f)
      return false;
   n = fread(buf, 1, sizeof(buf) - 1, f);
   fclose(f);
   buf[n] = '\0';
   return string_is_equal(buf, text);
}

static bool download(companion_core_t *c, const char *label,
      const char *content_path)
{
   http_transfer_data_t data;
   if (!companion_core_thumbnail_download(c, "DL", label, content_path, "Named_Boxarts"))
      return false;
   memset(&data, 0, sizeof(data));
   data.data   = (char*)"png";
   data.len    = 3;
   data.status = 200;
   stub_http_cb(NULL, &data, stub_http_ud, NULL);
   return true;
}

/* A download lands where the display looks first, by the name the
 * server is asked for, and never on another file. */
static void test_thumbnail_download(void)
{
   char p[PATH_MAX_LENGTH];
   const char *rom      = "/roms/Foo (USA).nes";
   companion_core_t *c = make_core();

   fixture(p, sizeof(p), "thumbnails/DL");
   mkdirp(p);
   fixture(p, sizeof(p), "thumbnails/DL/Named_Boxarts");
   mkdirp(p);
   fixture(p, sizeof(p), "thumbnails/DL/Named_Boxarts/X.jpg");
   writef(p, "jpg");
   CHECK(download(c, "X", "/roms/x.nes"), "labelled: started");
   CHECK(string_ends_with(stub_http_url, "/DL/Named_Boxarts/X.png"),
         "labelled: url %s", stub_http_url);
   CHECK(file_holds(p, "jpg"), "the .jpg beside it keeps its bytes");
   fixture(p, sizeof(p), "thumbnails/DL/Named_Boxarts/X.png");
   CHECK(file_holds(p, "png"), "saved as X.png");

   /* The .partial suffix is cut at the end only */
   fixture(p, sizeof(p), "thumbnails/DL/Named_Boxarts/Bar");
   writef(p, "keep");
   CHECK(download(c, "Bar.partial", NULL), "a name holding .partial");
   CHECK(file_holds(p, "keep"), "a file named by its head is not replaced");
   fixture(p, sizeof(p), "thumbnails/DL/Named_Boxarts/Bar.partial.png");
   CHECK(file_holds(p, "png"), "saved as Bar.partial.png");

   /* Qt's label-less form: the label is the path */
   CHECK(download(c, rom, rom), "label-less: started");
   CHECK(string_ends_with(stub_http_url, "/DL/Named_Boxarts/Foo (USA).png"),
         "label-less: the file stem is asked for: %s", stub_http_url);
   companion_core_thumbnail_path(c, "DL", "Named_Boxarts", rom, rom,
         p, sizeof(p));
   CHECK(string_ends_with(p, "/DL/Named_Boxarts/Foo (USA).png")
         && file_holds(p, "png"), "saved where it is shown: %s", p);
   companion_core_free(c);
}
#endif

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
   char gen[512], out[512], hist[512], p[512], sub[512];
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
               "a", "/roms/gen/a.md", img, out, sizeof(out)), "install");
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

static void test_rename_cache(void)
{
   companion_core_t *c = make_core();
   char out[512];
   companion_core_refresh_playlists(c);
   fixture(out, sizeof(out), "playlists/Sega - Genesis.lpl");
   /* A cache follows its playlist, there and back */
   {
      char from[512];
      char to[512];
      char back[512];
      char again[512];
      strlcpy(from, out, sizeof(from));
      strlcat(from, ".thumbs", sizeof(from));
      writef(from, "{}");
      CHECK(companion_core_playlist_rename(c, out, "Sega - Genesis 2", to, sizeof(to)),
            "rename with a cache");
      strlcpy(back, to, sizeof(back));
      strlcat(back, ".thumbs", sizeof(back));
      CHECK(path_is_valid(back) && !path_is_valid(from), "its .lpl.thumbs moves with it");
      CHECK(companion_core_playlist_rename(c, to, "Sega - Genesis", again, sizeof(again))
            && string_is_equal(again, out), "and back: %s", again);
      CHECK(path_is_valid(from) && !path_is_valid(back), "and its cache with it");
      remove(from);
   }
   companion_core_free(c);
}

static void test_install_names(void)
{
   companion_core_t *c = make_core();
   char out[512], img[512], jpg[512], shown[512];
   fixture(img, sizeof(img), "drop.tga");
   write_tga_rgba(img, 64, 32, 0xff336699u);
   /* an existing X.jpg keeps its bytes: the drop goes to X.png, shown
    * first from then on */
   fixture(jpg, sizeof(jpg), "thumbnails/Sega - Genesis/Named_Boxarts/X.jpg");
   writef(jpg, "not a png");
   CHECK(companion_core_thumbnail_install(c, "Sega - Genesis", COMPANION_THUMB_BOXART,
            "X", "/roms/gen/other.md", img, out, sizeof(out)),
         "install beside an X.jpg");
   CHECK(string_ends_with(out, "Named_Boxarts/X.png"), "saved as X.png: %s", out);
   CHECK(file_is(jpg, "not a png"), "X.jpg unchanged");
   companion_core_thumbnail_path(c, "Sega - Genesis", COMPANION_THUMB_BOXART,
         "X", "/roms/gen/other.md", shown, sizeof(shown));
   CHECK(string_is_equal(shown, out), "then shown: %s", shown);
   /* a label-less entry: saved under its file stem, where it is looked up */
   CHECK(companion_core_thumbnail_install(c, "Sega - Genesis", COMPANION_THUMB_BOXART,
            "Yoshi.md", "/roms/gen/Yoshi.md", img, out, sizeof(out)),
         "install, no label");
   CHECK(string_ends_with(out, "Named_Boxarts/Yoshi.png"), "saved under the stem: %s", out);
   companion_core_thumbnail_path(c, "Sega - Genesis", COMPANION_THUMB_BOXART,
         "Yoshi.md", "/roms/gen/Yoshi.md", shown, sizeof(shown));
   CHECK(string_is_equal(shown, out) && path_is_valid(shown), "then shown: %s", shown);
   /* Qt passes the path as the label */
   companion_core_thumbnail_save_path(c, "Sega - Genesis", COMPANION_THUMB_BOXART,
         "/roms/gen/Yoshi.md", "/roms/gen/Yoshi.md", shown, sizeof(shown));
   CHECK(string_is_equal(shown, out), "Qt's save path: %s", shown);
   companion_core_free(c);
}

/* Fills @buf with 'Z', then asks for the path in a buffer of @len. */
static size_t bounds_path(companion_core_t *c, bool display, char *buf,
      size_t size, size_t len)
{
   memset(buf, 'Z', size);
   if (display)
      return companion_core_thumbnail_path(c, "Bounds",
            COMPANION_THUMB_BOXART, "bounds", "/roms/gen/bounds.md", buf, len);
   return companion_core_thumbnail_save_path(c, "Bounds",
         COMPANION_THUMB_BOXART, "bounds", "/roms/gen/bounds.md", buf, len);
}

static bool bounds_untouched(const char *buf, size_t from, size_t size)
{
   for (; from < size; from++)
      if (buf[from] != 'Z')
         return false;
   return true;
}

/* A thumbnail path that does not fit, ".png" included, is no path:
 * nothing past the buffer, never a name cut short of its .png, and
 * install writes nothing. */
static void test_thumbnail_path_bounds(void)
{
   static char buf[PATH_MAX_LENGTH], full[PATH_MAX_LENGTH];
   companion_core_t *c = make_core();
   char img[512];
   size_t n, d, t, i;

   n = bounds_path(c, false, full, sizeof(full), sizeof(full));
   d = n - (sizeof("/bounds.png") - 1);
   CHECK(n > 16 && string_ends_with(full, "/Bounds/Named_Boxarts/bounds.png"),
         "full save path: %s", full);

   CHECK(!bounds_path(c, false, buf, sizeof(buf), d + 3) && !*buf
         && bounds_untouched(buf, d + 3, sizeof(buf)),
         "save path, folder + name past the buffer: none");
   CHECK(!bounds_path(c, false, buf, sizeof(buf), n) && !*buf
         && bounds_untouched(buf, n, sizeof(buf)),
         "save path, no room for the .png's NUL: none (got %s)", buf);
   CHECK(!bounds_path(c, false, buf, sizeof(buf), n - 2) && !*buf,
         "save path, .png cut short: none (got %s)", buf);
   CHECK(bounds_path(c, false, buf, sizeof(buf), n + 1) == n
         && string_is_equal(buf, full), "save path, exact fit: %s", buf);
   CHECK(!bounds_path(c, true, buf, sizeof(buf), d - 4) && !*buf
         && bounds_untouched(buf, d - 4, sizeof(buf)),
         "display path, folder past the buffer: none");
   CHECK(!bounds_path(c, true, buf, sizeof(buf), d + 3) && !*buf
         && bounds_untouched(buf, d + 3, sizeof(buf)),
         "display path, folder + name past the buffer: none");
   CHECK(!bounds_path(c, true, buf, sizeof(buf), n) && !*buf,
         "display path, no room for the .png: none (got %s)", buf);

   /* A playlist's database name (folders and all) that leaves
    * bounds.png two bytes short of a PATH_MAX_LENGTH buffer: install
    * creates and writes nothing. */
   t = PATH_MAX_LENGTH - 2 - (n - 4 - STRLEN_CONST("Bounds"));
   strlcpy(buf, "Bounds", sizeof(buf));
   for (i = STRLEN_CONST("Bounds"); i < t; i++)
      buf[i] = ((i - STRLEN_CONST("Bounds")) % 100) ? 'd' : '/';
   buf[t - 1] = 'd';
   buf[t]     = '\0';
   CHECK(companion_core_thumbnail_dir(c, buf, COMPANION_THUMB_BOXART,
            full, sizeof(full)) + (n - d) - 4 == PATH_MAX_LENGTH - 2,
         "long database name sized as meant");
   fixture(img, sizeof(img), "bounds.tga");
   write_tga_rgba(img, 8, 8, 0xff336699u);
   CHECK(!companion_core_thumbnail_install(c, buf,
            COMPANION_THUMB_BOXART, "bounds", "/roms/gen/bounds.md", img,
            full, sizeof(full)), "install with no room for .png: refused");
   fixture(full, sizeof(full), "thumbnails/Bounds");
   CHECK(!path_is_directory(full), "install created nothing");
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
   /* Tooltip help: every row has its own message, none shared (the
    * stub returns a distinct string per enum, so two rows on one
    * enum would collide). */
   {
      char help[32][64];
      size_t j;
      CHECK(n <= 32, "settings table fits the help check");
      for (i = 0; i < n && i < 32; i++)
      {
         const char *h = companion_core_setting_sublabel(c, i);
         CHECK(h && *h, "row %u has tooltip help", (unsigned)i);
         strlcpy(help[i], h ? h : "", sizeof(help[i]));
      }
      for (i = 0; i < n && i < 32; i++)
         for (j = 0; j < i; j++)
            CHECK(!string_is_equal(help[i], help[j]),
                  "rows %u and %u share help", (unsigned)i, (unsigned)j);
   }
   CHECK(!*companion_core_setting_sublabel(c, n), "out-of-range row has no help");
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

/* Controller shortcuts: the hotkeys list's entries and macros as rows of
 * text, an empty row of each kind to add the next. */
extern char stub_entry_text[256][64];
extern char stub_macro_text[256][64];

static void test_shortcuts(void)
{
   companion_core_t *c = make_core();
   char buf[256];
   size_t n;
   unsigned i;

   memset(stub_entry_text, 0, sizeof(stub_entry_text));
   memset(stub_macro_text, 0, sizeof(stub_macro_text));
   n = companion_core_shortcut_count(c);
   CHECK(n == 2, "with none, one empty row of each kind (got %u)", (unsigned)n);
   CHECK(!companion_core_shortcut_is_macro(c, 0) && companion_core_shortcut_is_macro(c, 1),
         "combinations come first, then macros");
   CHECK(string_is_equal(companion_core_shortcut_label(c, 0, buf, sizeof(buf)), "New combination"),
         "the empty combination row (got %s)", buf);
   CHECK(!*companion_core_shortcut_get(c, 0, buf, sizeof(buf)), "an empty row has no text");

   /* added on the empty row, under the lowest number free */
   CHECK(companion_core_shortcut_set(c, 0, "l3+r3 : menu_toggle"), "a combination is added");
   CHECK(string_is_equal(stub_entry_text[1], "l3+r3 : menu_toggle"), "it is number 1");
   n = companion_core_shortcut_count(c);
   CHECK(n == 3 && string_is_equal(companion_core_shortcut_label(c, 0, buf, sizeof(buf)), "Combination 1"),
         "the row is Combination 1 and a new empty row follows");
   CHECK(string_is_equal(companion_core_shortcut_get(c, 0, buf, sizeof(buf)), "l3+r3 : menu_toggle"),
         "the row reads back as written");
   CHECK(!companion_core_shortcut_set(c, 0, "nonsense"), "text that does not read is refused");
   CHECK(string_is_equal(stub_entry_text[1], "l3+r3 : menu_toggle"), "and the entry is left as it was");

   /* a macro, on its empty row */
   CHECK(companion_core_shortcut_set(c, 2, "down 2, a 4"), "a macro is added");
   n = companion_core_shortcut_count(c);
   CHECK(n == 4 && companion_core_shortcut_is_macro(c, 2)
         && string_is_equal(companion_core_shortcut_label(c, 2, buf, sizeof(buf)), "Macro 1"),
         "the macro is a row of its own after the combinations");
   CHECK(!companion_core_shortcut_set(c, 3, "no steps"), "a macro that does not read is refused");

   /* the lowest number free is taken */
   strlcpy(stub_entry_text[3], "a : quit", sizeof(stub_entry_text[3]));
   companion_core_shortcut_count(c);
   CHECK(companion_core_shortcut_set(c, 2, "b : quit") && string_is_equal(stub_entry_text[2], "b : quit"),
         "the empty row adds number 2, the lowest free");

   /* an empty text takes the number out */
   n = companion_core_shortcut_count(c);
   CHECK(companion_core_shortcut_set(c, 0, "  ") && !stub_entry_text[1][0], "an empty text takes it out");
   CHECK(companion_core_shortcut_count(c) == n - 1, "and its row goes");

   /* full: no empty row to add a seventeenth */
   for (i = 1; i <= 16; i++)
      snprintf(stub_entry_text[i], sizeof(stub_entry_text[i]), "a : quit");
   n = companion_core_shortcut_count(c);
   CHECK(n == 16 + 2, "sixteen combinations, no empty row for them (got %u)", (unsigned)n);
   CHECK(!companion_core_shortcut_set(c, 99, "a : quit"), "a row past the table is refused");

   CHECK(strstr(companion_core_shortcut_help(c, false, buf, sizeof(buf)), "menu_toggle|quit") != NULL,
         "a combination's help lists what it can do");
   CHECK(*companion_core_shortcut_help(c, true, buf, sizeof(buf)), "a macro has help");
   memset(stub_entry_text, 0, sizeof(stub_entry_text));
   memset(stub_macro_text, 0, sizeof(stub_macro_text));
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

/* Hide / unhide, and New / Delete: the listing is the same for every
 * backend, so the core filters it rather than each toolkit hiding rows
 * of its own. */
static void test_hidden_new_delete(void)
{
   companion_core_t *c = make_core();
   char gen[512], out[PATH_MAX_LENGTH], gen_name[128];
   size_t n0;
   companion_core_refresh_playlists(c);
   n0 = companion_core_playlist_count(c);
   /* the last row's own path and name: earlier tests may have renamed it */
   strlcpy(gen, companion_core_playlist_path(c, n0 - 1), sizeof(gen));
   strlcpy(gen_name, companion_core_playlist_name(c, n0 - 1), sizeof(gen_name));
   CHECK(n0 == 4, "4 rows to start (got %u)", (unsigned)n0);
   CHECK(companion_core_hidden_count(c) == 0, "none hidden");

   companion_core_playlist_set_hidden(c, gen, true);
   CHECK(companion_core_playlist_is_hidden(c, gen), "marked hidden");
   CHECK(companion_core_playlist_count(c) == n0 - 1, "listing drops it (%u)", (unsigned)companion_core_playlist_count(c));
   CHECK(companion_core_playlist_name(c, 3) == NULL
         || !string_is_equal(companion_core_playlist_name(c, 3), gen_name), "row 3 is no longer it");
   CHECK(companion_core_hidden_count(c) == 1, "one hidden");
   CHECK(string_is_equal(companion_core_hidden_name(c, 0), gen_name), "hidden name (got %s)", companion_core_hidden_name(c, 0));
   CHECK(string_is_equal(companion_core_hidden_path(c, 0), gen), "hidden path");

   companion_core_playlist_set_hidden(c, gen, false);
   CHECK(companion_core_playlist_count(c) == n0, "unhide restores the row");
   CHECK(string_is_equal(companion_core_playlist_name(c, 3), gen_name), "and in its sorted place (got %s)", companion_core_playlist_name(c, 3));
   CHECK(companion_core_hidden_count(c) == 0, "none hidden again");

   /* a special can be hidden too (Qt hides any row) */
   {
      const char *hist = companion_core_playlist_path(c, 1);
      companion_core_playlist_set_hidden(c, hist, true);
      CHECK(companion_core_playlist_count(c) == n0 - 1, "special hidden too");
      CHECK(companion_core_hidden_count(c) == 1, "and listed as hidden");
      companion_core_playlist_set_hidden(c, hist, false);
   }

   /* New / Delete */
   CHECK(companion_core_playlist_new(c, "My Games", out, sizeof(out)), "new playlist");
   CHECK(string_ends_with(out, "playlists/My Games.lpl") && path_is_valid(out), "created at %s", out);
   CHECK(companion_core_playlist_count(c) == n0 + 1, "listing has it (%u)", (unsigned)companion_core_playlist_count(c));
   CHECK(!companion_core_playlist_new(c, "My Games", NULL, 0), "not over an existing one");
   CHECK(!companion_core_playlist_new(c, "a/b", NULL, 0), "no path separators");
   /* it parses as an empty playlist */
   CHECK(companion_core_select_playlist_path(c, out) && iterate_until_loaded(c), "the new playlist parses");
   CHECK(companion_core_entry_count(c) == 0, "and is empty (%u)", (unsigned)companion_core_entry_count(c));
   {
      char thumbs[PATH_MAX_LENGTH];
      strlcpy(thumbs, out, sizeof(thumbs));
      strlcat(thumbs, ".thumbs", sizeof(thumbs));
      writef(thumbs, "{}");
      CHECK(companion_core_playlist_delete(c, out), "delete");
      CHECK(!path_is_valid(thumbs), "its .lpl.thumbs goes with it");
   }
   CHECK(!path_is_valid(out), "file gone");
   CHECK(companion_core_playlist_count(c) == n0, "listing back to %u", (unsigned)n0);
   {
      char hist[512];
      fixture(hist, sizeof(hist), "history.lpl");
      CHECK(!companion_core_playlist_delete(c, hist), "a special (outside the directory) is not deleted");
      CHECK(path_is_valid(hist), "and still there");
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

#ifndef COMPANION_TEST_NO_MAIN
/* Dock layout rows (desktop_menu_dock_*): the plain-text replacement
 * for the old Qt saveState blob. Regression for the "Save Dock
 * Positions" reports: every dock has its own row, tab partners and the
 * raised tab survive a round trip, old short rows still parse, and
 * garbage is rejected rather than placing a dock somewhere odd. */
static void test_dock_rows(void)
{
   companion_dock_state_t st, back;
   char row[64];

   memset(&st, 0, sizeof(st));
   st.area   = COMPANION_DOCK_RIGHT;
   st.shown  = true;
   st.dims   = VIDEO_SCALE_PACK(320, 400);
   strlcpy(st.tabbed_with, "boxart", sizeof(st.tabbed_with));
   st.raised = true;
   companion_dock_row_format(row, sizeof(row), &st);
   CHECK(strcmp(row, "right,1,320,400,boxart,1,0") == 0, "format: %s", row);
   CHECK(companion_dock_row_parse(row, &back), "parse formatted row");
   CHECK(back.area == COMPANION_DOCK_RIGHT && back.shown
         && back.dims == VIDEO_SCALE_PACK(320, 400) && strcmp(back.tabbed_with, "boxart") == 0
         && back.raised && back.order == 0, "round trip keeps every field");

   /* Standalone hidden dock at the bottom: "-" partner, not raised. */
   memset(&st, 0, sizeof(st));
   st.area = COMPANION_DOCK_BOTTOM; st.dims = VIDEO_SCALE_PACK(0, 160);
   companion_dock_row_format(row, sizeof(row), &st);
   CHECK(strcmp(row, "bottom,0,0,160,-,0,0") == 0, "hidden standalone: %s", row);
   CHECK(companion_dock_row_parse(row, &back) && !back.shown
         && back.tabbed_with[0] == '\0' && VIDEO_SCALE_H(back.dims) == 160, "hidden round trip");

   /* Floating: the only kind of row that carries a screen position. */
   memset(&st, 0, sizeof(st));
   st.area = COMPANION_DOCK_FLOAT; st.shown = true;
   st.dims = VIDEO_SCALE_PACK(200, 300);
   st.pos  = VIDEO_POS_PACK(640, 120);
   companion_dock_row_format(row, sizeof(row), &st);
   CHECK(strcmp(row, "float,1,200,300,-,0,0,640,120") == 0, "floating row: %s", row);
   CHECK(companion_dock_row_parse(row, &back) && back.area == COMPANION_DOCK_FLOAT
         && VIDEO_SCALE_W(back.dims) == 200 && back.pos == VIDEO_POS_PACK(640, 120),
         "floating round trip keeps x,y: %s", row);
   /* Docked rows never grow the position fields, and a floating row
    * from before positions were saved parses with x = y = 0. */
   st.area = COMPANION_DOCK_RIGHT;
   companion_dock_row_format(row, sizeof(row), &st);
   CHECK(strcmp(row, "right,1,200,300,-,0,0") == 0, "docked row has no x,y: %s", row);
   CHECK(companion_dock_row_parse("float,1,200,300,-,0", &back)
         && back.area == COMPANION_DOCK_FLOAT && back.pos == VIDEO_POS_PACK(0, 0),
         "old floating row parses");
   CHECK(companion_dock_row_parse("float,1,200,300,-,0,-5,99999", &back)
         && back.pos == VIDEO_POS_PACK(0, 0), "off-screen position becomes 0");

   /* Sizes a dock reports before it is laid out (0/1) and absurd ones
    * are written and read as 0 = default. */
   memset(&st, 0, sizeof(st));
   st.area = COMPANION_DOCK_LEFT; st.shown = true;
   st.dims = VIDEO_SCALE_PACK(1, 0xffff);
   companion_dock_row_format(row, sizeof(row), &st);
   CHECK(strcmp(row, "left,1,0,0,-,0,0") == 0, "bogus sizes become default: %s", row);
   CHECK(companion_dock_row_parse("left,1,1,40000,-,0", &back)
         && back.dims == 0, "bogus sizes rejected on parse");

   /* The slot order on a side: two docks stacked on the right keep
    * which is above which. Written for docked and floating rows alike,
    * and the field count tells a row with it from one without: a
    * six-field docked row and an eight-field floating row predate it
    * (order 0, and the floating row's x,y still land where they were),
    * an out-of-range order becomes 0. */
   memset(&st, 0, sizeof(st));
   st.area = COMPANION_DOCK_RIGHT; st.shown = true;
   st.dims = VIDEO_SCALE_PACK(0, 500); st.order = 1;
   companion_dock_row_format(row, sizeof(row), &st);
   CHECK(strcmp(row, "right,1,0,500,-,0,1") == 0, "order written: %s", row);
   CHECK(companion_dock_row_parse(row, &back) && back.order == 1
         && VIDEO_SCALE_H(back.dims) == 500 && VIDEO_POS_X(back.pos) == 0, "order round trip");
   st.area = COMPANION_DOCK_FLOAT; st.pos = VIDEO_POS_PACK(33, 44);
   companion_dock_row_format(row, sizeof(row), &st);
   CHECK(strcmp(row, "float,1,0,500,-,0,1,33,44") == 0, "floating row with order: %s", row);
   CHECK(companion_dock_row_parse(row, &back) && back.order == 1
         && back.pos == VIDEO_POS_PACK(33, 44), "floating order round trip");
   CHECK(companion_dock_row_parse("right,1,320,400,boxart,1", &back)
         && back.order == 0 && VIDEO_SCALE_W(back.dims) == 320 && back.raised,
         "six-field row from before order parses with order 0");
   CHECK(companion_dock_row_parse("float,1,300,200,-,0,100,60", &back)
         && back.order == 0 && back.pos == VIDEO_POS_PACK(100, 60),
         "eight-field floating row keeps its x,y");
   CHECK(companion_dock_row_parse("right,1,0,0,-,0,42", &back) && back.order == 0,
         "out-of-range order becomes 0");
   CHECK(companion_dock_row_parse("right,1,0,0,-,0,-1", &back) && back.order == 0,
         "negative order becomes 0");
   CHECK(strcmp(companion_dock_key(COMPANION_DOCK_CORE_INFO), "core_info") == 0
         && strcmp(companion_dock_key(COMPANION_DOCK_LOG), "log") == 0
         && companion_dock_key(COMPANION_DOCK_COUNT)[0] == '\0', "dock keys");

   /* The first shipped format (area,shown,size[,tab]) still parses:
    * size is the width, everything else defaults. */
   CHECK(companion_dock_row_parse("right,1,320,2", &back)
         && back.area == COMPANION_DOCK_RIGHT && back.shown && VIDEO_SCALE_W(back.dims) == 320
         && back.tabbed_with[0] == '\0' && !back.raised, "old four-field row");
   CHECK(companion_dock_row_parse("left,0,280", &back)
         && !back.shown && VIDEO_SCALE_W(back.dims) == 280, "old three-field row");

   /* Not ours: empty, a lone word, unknown area. */
   CHECK(!companion_dock_row_parse("", &back), "empty row rejected");
   CHECK(!companion_dock_row_parse(NULL, &back), "NULL row rejected");
   CHECK(!companion_dock_row_parse("left", &back), "one field rejected");
   CHECK(!companion_dock_row_parse("sideways,1,10,10,-,0", &back), "unknown area rejected");
}

/* The Load Core picker's placement: it takes the size that shows every
 * row without a scrollbar when the screen allows, is clamped to the
 * work area otherwise (Qt opened taller than the screen), never
 * shrinks below its minimum (Win32 opened at 420x400 whatever the
 * list), and is centred over the companion window but kept on
 * screen. */
static void test_place_window(void)
{
   companion_rect_t avail, owner, r;
   avail.pos  = VIDEO_POS_PACK(0, 0);
   avail.dims = VIDEO_SCALE_PACK(1920, 1040);
   owner.pos  = VIDEO_POS_PACK(320, 160);
   owner.dims = VIDEO_SCALE_PACK(1280, 720);

   /* Fits: the needed size, centred over the owner. */
   companion_place_window(&avail, &owner, VIDEO_SCALE_PACK(520, 640), VIDEO_SCALE_PACK(420, 400), &r);
   CHECK(RW(r) == 520 && RH(r) == 640 && RX(r) == 320 + (1280 - 520) / 2
         && RY(r) == 160 + (720 - 640) / 2, "fits: needed size centred (%d,%d %dx%d)", RX(r), RY(r), RW(r), RH(r));
   /* Taller than the screen: clamped to the work area less the margin,
    * pulled back on screen. */
   companion_place_window(&avail, &owner, VIDEO_SCALE_PACK(520, 3000), VIDEO_SCALE_PACK(420, 400), &r);
   CHECK(RH(r) == 1040 - 32 && RY(r) == 16, "taller than the screen: clamped and on screen (%d %d)", RH(r), RY(r));
   /* Smaller than the minimum grows to it. */
   companion_place_window(&avail, &owner, VIDEO_SCALE_PACK(200, 100), VIDEO_SCALE_PACK(420, 400), &r);
   CHECK(RW(r) == 420 && RH(r) == 400, "below minimum grows to it (%dx%d)", RW(r), RH(r));
   /* Owner half off the screen: the window still lands inside it. */
   owner.pos = VIDEO_POS_PACK(1500, 900);
   companion_place_window(&avail, &owner, VIDEO_SCALE_PACK(520, 640), VIDEO_SCALE_PACK(420, 400), &r);
   CHECK(RX(r) + RW(r) <= 1920 - 16 && RY(r) + RH(r) <= 1040 - 16 && RX(r) >= 16 && RY(r) >= 16,
         "owner off the edge: window kept on screen (%d,%d %dx%d)", RX(r), RY(r), RW(r), RH(r));
   /* A work area with an origin (second monitor / dock on the left). */
   avail.pos = VIDEO_POS_PACK(1920, 40); owner.pos = VIDEO_POS_PACK(2000, 100);
   companion_place_window(&avail, &owner, VIDEO_SCALE_PACK(520, 640), VIDEO_SCALE_PACK(420, 400), &r);
   CHECK(RX(r) >= 1936 && RY(r) >= 56 && RX(r) + RW(r) <= 1920 + 1920 - 16,
         "offset work area respected (%d,%d)", RX(r), RY(r));
   /* A screen smaller than the minimum: the minimum still wins so the
    * controls stay usable. */
   avail.pos = VIDEO_POS_PACK(0, 0); avail.dims = VIDEO_SCALE_PACK(400, 300);
   owner.pos = VIDEO_POS_PACK(0, 0);
   companion_place_window(&avail, &owner, VIDEO_SCALE_PACK(520, 640), VIDEO_SCALE_PACK(420, 400), &r);
   CHECK(RW(r) == 420 && RH(r) == 400, "tiny screen: minimum kept (%dx%d)", RW(r), RH(r));
}

/* The dock layout model (ui/companion/companion_dock.c): the same
 * placement, sizing and hit-testing Qt's QMainWindow gives the Qt
 * companion, for the native companions to render. */
#include "../companion_dock.h"

static int dock_slot_of(const companion_dock_layout_t *l, enum companion_dock_id p)
{
   enum companion_dock_area side;
   int idx;
   return companion_dock_find(l, p, &side, &idx) ? idx : -1;
}

static void test_dock_model(void)
{
   companion_dock_layout_t l, back;
   companion_dock_geometry_t g;
   companion_dock_metrics_t m;
   companion_rect_t client;
   companion_dock_hit_t hit;
   companion_dock_drop_t drop;
   settings_t *settings = config_get_ptr();
   enum companion_dock_area side;
   int idx, i;

   m.gap = 5; m.strip_h = 20; m.tab_h = 22; m.min_pane = 60;
   m.def_side = 280; m.def_bar = 150; m.def_slot = 200;
   client.pos  = VIDEO_POS_PACK(0, 0);
   client.dims = VIDEO_SCALE_PACK(1280, 720);

   /* The default is Qt's: three left, a four-tab group above Core Info
    * on the right, the log hidden below. */
   companion_dock_default(&l);
   CHECK(l.nslots[COMPANION_DOCK_LEFT] == 3 && l.nslots[COMPANION_DOCK_RIGHT] == 2
         && l.nslots[COMPANION_DOCK_BOTTOM] == 1 && !l.shown[COMPANION_DOCK_LOG]
         && l.slots[COMPANION_DOCK_RIGHT][0].n == 4
         && companion_dock_raised(&l, COMPANION_DOCK_TITLE) == COMPANION_DOCK_BOXART,
         "dock: default layout");

   /* Layout: every shown pane gets a rect, tabs share their slot,
    * hidden log takes no room, the content sits between the sides. */
   companion_dock_layout(&l, &m, &client, &g);
   CHECK(g.laid_out[COMPANION_DOCK_SEARCH] && g.laid_out[COMPANION_DOCK_BOXART]
         && !g.laid_out[COMPANION_DOCK_TITLE] && !g.laid_out[COMPANION_DOCK_LOG]
         && g.laid_out[COMPANION_DOCK_CORE_INFO], "dock: laid-out set");
   CHECK(RX(g.content) == 285 && RW(g.content) == 1280 - 2 * 285 && RH(g.content) == 720,
         "dock: content between the sides (%d,%d %dx%d)", RX(g.content), RY(g.content), RW(g.content), RH(g.content));
   CHECK(RH(g.tabbar[COMPANION_DOCK_BOXART]) == 22 && RH(g.tabbar[COMPANION_DOCK_CORE_INFO]) == 0
         && RH(g.strip[COMPANION_DOCK_BOXART]) == 20
         && RY(g.pane[COMPANION_DOCK_CORE_INFO]) > RY(g.pane[COMPANION_DOCK_BOXART]),
         "dock: tab bar on the group only, Core Info below the thumbnails");
   CHECK(RY(g.pane[COMPANION_DOCK_CORE_INFO]) + RH(g.pane[COMPANION_DOCK_CORE_INFO]) == 720,
         "dock: the last slot reaches the bottom (%d)", RY(g.pane[COMPANION_DOCK_CORE_INFO]) + RH(g.pane[COMPANION_DOCK_CORE_INFO]));
   CHECK(l.slots[COMPANION_DOCK_RIGHT][0].size + l.slots[COMPANION_DOCK_RIGHT][1].size + 5 == 720,
         "dock: sizes written back (%d + %d)", l.slots[COMPANION_DOCK_RIGHT][0].size, l.slots[COMPANION_DOCK_RIGHT][1].size);
   CHECK(g.ngaps == 2 + 2 + 1, "dock: two side gaps, two between the left slots, one on the right (%d)", g.ngaps);

   /* Hit testing: the strip, a tab, a gap. */
   companion_dock_hit_test(&l, &g, &m, RX(g.strip[COMPANION_DOCK_BOXART]) + 4, RY(g.strip[COMPANION_DOCK_BOXART]) + 4, &hit);
   CHECK(hit.kind == COMPANION_DOCK_HIT_STRIP && hit.pane == COMPANION_DOCK_BOXART, "dock: hit the thumbnail strip");
   companion_dock_hit_test(&l, &g, &m, RX(g.tabbar[COMPANION_DOCK_BOXART]) + RW(g.tabbar[COMPANION_DOCK_BOXART]) * 3 / 8,
         RY(g.tabbar[COMPANION_DOCK_BOXART]) + 4, &hit);
   CHECK(hit.kind == COMPANION_DOCK_HIT_TAB && hit.pane == COMPANION_DOCK_TITLE, "dock: hit the second tab (%d)", (int)hit.pane);
   companion_dock_hit_test(&l, &g, &m, 282, 300, &hit);
   CHECK(hit.kind == COMPANION_DOCK_HIT_GAP && g.gaps[hit.gap].side == COMPANION_DOCK_LEFT
         && g.gaps[hit.gap].index == -1, "dock: hit the left edge gap");

   /* Dragging that gap widens the left side; the next layout honours it. */
   companion_dock_drag_gap(&l, &g, &m, &client, hit.gap, 340);
   companion_dock_layout(&l, &m, &client, &g);
   CHECK(l.side_size[COMPANION_DOCK_LEFT] == 340 && RX(g.content) == 345, "dock: left side dragged to 340 (%d)", RX(g.content));
   /* And the gap between the right slots moves the split. */
   for (i = 0; i < g.ngaps; i++)
      if (g.gaps[i].side == COMPANION_DOCK_RIGHT && g.gaps[i].index == 0)
         break;
   CHECK(i < g.ngaps, "dock: gap between the right slots found");
   companion_dock_drag_gap(&l, &g, &m, &client, i, 500);
   companion_dock_layout(&l, &m, &client, &g);
   CHECK(l.slots[COMPANION_DOCK_RIGHT][0].size >= 495 && l.slots[COMPANION_DOCK_RIGHT][0].size <= 500
         && RY(g.pane[COMPANION_DOCK_CORE_INFO]) + RH(g.pane[COMPANION_DOCK_CORE_INFO]) == 720,
         "dock: right split dragged (%d / %d)", l.slots[COMPANION_DOCK_RIGHT][0].size, l.slots[COMPANION_DOCK_RIGHT][1].size);

   /* Tatsuya79's layout by edits: Screenshots out of the group into a
    * slot above Title Screen, which comes out too; Boxart and Logo stay
    * grouped; Core Info hidden. */
   companion_dock_place(&l, COMPANION_DOCK_SCREENSHOT, COMPANION_DOCK_RIGHT, 0);
   companion_dock_place(&l, COMPANION_DOCK_TITLE, COMPANION_DOCK_RIGHT, 1);
   companion_dock_set_shown(&l, COMPANION_DOCK_CORE_INFO, false);
   CHECK(l.nslots[COMPANION_DOCK_RIGHT] == 4 && dock_slot_of(&l, COMPANION_DOCK_SCREENSHOT) == 0
         && dock_slot_of(&l, COMPANION_DOCK_TITLE) == 1 && dock_slot_of(&l, COMPANION_DOCK_BOXART) == 2
         && l.slots[COMPANION_DOCK_RIGHT][2].n == 2 && dock_slot_of(&l, COMPANION_DOCK_CORE_INFO) == 3,
         "dock: split out and stacked (%d slots)", l.nslots[COMPANION_DOCK_RIGHT]);
   companion_dock_layout(&l, &m, &client, &g);
   CHECK(g.laid_out[COMPANION_DOCK_SCREENSHOT] && g.laid_out[COMPANION_DOCK_TITLE]
         && RY(g.pane[COMPANION_DOCK_SCREENSHOT]) < RY(g.pane[COMPANION_DOCK_TITLE])
         && !g.laid_out[COMPANION_DOCK_CORE_INFO], "dock: Screenshots above Title Screen, Core Info gone");

   /* Rows: what the Qt companion writes, read back identical. */
   settings->bools.desktop_menu_save_dock_positions = true;
   companion_dock_to_rows(settings, &l);
   CHECK(strncmp(settings->arrays.desktop_menu_dock_screenshot, "right,1,", 8) == 0
         && strstr(settings->arrays.desktop_menu_dock_screenshot, ",-,0,0") != NULL
         && strstr(settings->arrays.desktop_menu_dock_title, ",-,0,1") != NULL
         && strstr(settings->arrays.desktop_menu_dock_logo, ",boxart,0,2") != NULL
         && strncmp(settings->arrays.desktop_menu_dock_core_info, "right,0,", 8) == 0
         && strstr(settings->arrays.desktop_menu_dock_core_info, ",-,0,3") != NULL,
         "dock: rows (%s / %s / %s / %s)", settings->arrays.desktop_menu_dock_screenshot,
         settings->arrays.desktop_menu_dock_title, settings->arrays.desktop_menu_dock_logo,
         settings->arrays.desktop_menu_dock_core_info);
   CHECK(companion_dock_from_rows(settings, &back), "dock: rows read back");
   CHECK(memcmp(&back.nslots, &l.nslots, sizeof(l.nslots)) == 0
         && memcmp(back.area, l.area, sizeof(l.area)) == 0
         && memcmp(back.shown, l.shown, sizeof(l.shown)) == 0
         && back.side_size[COMPANION_DOCK_LEFT] == 340
         && back.slots[COMPANION_DOCK_RIGHT][0].size == l.slots[COMPANION_DOCK_RIGHT][0].size
         && back.slots[COMPANION_DOCK_RIGHT][2].n == 2
         && companion_dock_raised(&back, COMPANION_DOCK_LOGO) == COMPANION_DOCK_BOXART,
         "dock: rows round trip is a fixed point");
   /* The rows Qt itself writes for that layout (from the persist test)
    * read into the same shape. */
   strlcpy(settings->arrays.desktop_menu_dock_search,     "left,1,289,59,-,0,0", 64);
   strlcpy(settings->arrays.desktop_menu_dock_playlists,  "left,1,289,531,-,0,1", 64);
   strlcpy(settings->arrays.desktop_menu_dock_core,       "left,1,289,77,-,0,2", 64);
   strlcpy(settings->arrays.desktop_menu_dock_boxart,     "right,0,0,0,-,0,2", 64);
   strlcpy(settings->arrays.desktop_menu_dock_title,      "right,1,186,224,-,0,1", 64);
   strlcpy(settings->arrays.desktop_menu_dock_screenshot, "right,1,186,449,-,0,0", 64);
   strlcpy(settings->arrays.desktop_menu_dock_logo,       "right,0,0,0,-,0,2", 64);
   strlcpy(settings->arrays.desktop_menu_dock_core_info,  "right,0,0,0,-,0,2", 64);
   strlcpy(settings->arrays.desktop_menu_dock_log,        "bottom,0,0,0,-,0,0", 64);
   CHECK(companion_dock_from_rows(settings, &back) && dock_slot_of(&back, COMPANION_DOCK_SCREENSHOT) == 0
         && dock_slot_of(&back, COMPANION_DOCK_TITLE) == 1 && !back.shown[COMPANION_DOCK_BOXART]
         && back.side_size[COMPANION_DOCK_RIGHT] == 186 && back.slots[COMPANION_DOCK_RIGHT][0].size == 449,
         "dock: Qt's rows for the layout read into it");

   /* Drops: the middle of a pane tabs onto it, its thirds slot before
    * and after, the client's edge band a side's end, elsewhere floats. */
   companion_dock_default(&l);
   companion_dock_layout(&l, &m, &client, &g);
   companion_dock_drop_target(&l, &g, &m, &client, COMPANION_DOCK_LOG,
         RX(g.pane[COMPANION_DOCK_CORE_INFO]) + 10,
         RY(g.pane[COMPANION_DOCK_CORE_INFO]) + RH(g.pane[COMPANION_DOCK_CORE_INFO]) / 2, &drop);
   CHECK(drop.kind == COMPANION_DOCK_DROP_TAB && drop.onto == COMPANION_DOCK_CORE_INFO, "dock: drop mid-pane tabs");
   companion_dock_drop_target(&l, &g, &m, &client, COMPANION_DOCK_LOG,
         RX(g.strip[COMPANION_DOCK_CORE_INFO]) + 10, RY(g.strip[COMPANION_DOCK_CORE_INFO]) + 2, &drop);
   CHECK(drop.kind == COMPANION_DOCK_DROP_SLOT && drop.side == COMPANION_DOCK_RIGHT && drop.index == 1,
         "dock: drop on the top third slots before (%d)", drop.index);
   companion_dock_drop_target(&l, &g, &m, &client, COMPANION_DOCK_LOG, 640, 715, &drop);
   CHECK(drop.kind == COMPANION_DOCK_DROP_SLOT && drop.side == COMPANION_DOCK_BOTTOM, "dock: drop on the bottom band");
   companion_dock_drop_target(&l, &g, &m, &client, COMPANION_DOCK_LOG, 640, 360, &drop);
   CHECK(drop.kind == COMPANION_DOCK_DROP_FLOAT, "dock: drop mid-content floats");
   {
      companion_rect_t fr;
      fr.pos  = VIDEO_POS_PACK(100, 100);
      fr.dims = VIDEO_SCALE_PACK(300, 200);
      companion_dock_apply_drop(&l, COMPANION_DOCK_LOG, &drop, &fr);
      CHECK(l.area[COMPANION_DOCK_LOG] == COMPANION_DOCK_FLOAT && l.shown[COMPANION_DOCK_LOG]
            && RW(l.floats[COMPANION_DOCK_LOG]) == 300 && l.nslots[COMPANION_DOCK_BOTTOM] == 0,
            "dock: floated and shown, bottom side emptied");
      companion_dock_to_rows(settings, &l);
      CHECK(strcmp(settings->arrays.desktop_menu_dock_log, "float,1,300,200,-,0,0,100,100") == 0,
            "dock: floating row (%s)", settings->arrays.desktop_menu_dock_log);
   }
   /* Tabifying onto a standalone makes a group; raising picks the tab;
    * placing back out shrinks it; detaching the raised member keeps
    * the group's raised index valid. */
   companion_dock_tabify(&l, COMPANION_DOCK_LOG, COMPANION_DOCK_CORE_INFO);
   CHECK(companion_dock_is_tabbed(&l, COMPANION_DOCK_CORE_INFO)
         && companion_dock_raised(&l, COMPANION_DOCK_CORE_INFO) == COMPANION_DOCK_LOG, "dock: tabified and raised");
   companion_dock_raise(&l, COMPANION_DOCK_CORE_INFO);
   CHECK(companion_dock_raised(&l, COMPANION_DOCK_LOG) == COMPANION_DOCK_CORE_INFO, "dock: raise");
   companion_dock_place(&l, COMPANION_DOCK_CORE_INFO, COMPANION_DOCK_LEFT, 99);
   CHECK(!companion_dock_is_tabbed(&l, COMPANION_DOCK_LOG) && dock_slot_of(&l, COMPANION_DOCK_CORE_INFO) == 3
         && companion_dock_find(&l, COMPANION_DOCK_CORE_INFO, &side, &idx) && side == COMPANION_DOCK_LEFT
         && companion_dock_raised(&l, COMPANION_DOCK_LOG) == COMPANION_DOCK_LOG,
         "dock: placed out of the group to the left's end");
   /* Moving a slot down its own side lands where asked. */
   companion_dock_place(&l, COMPANION_DOCK_SEARCH, COMPANION_DOCK_LEFT, 3);
   CHECK(dock_slot_of(&l, COMPANION_DOCK_SEARCH) == 2 && dock_slot_of(&l, COMPANION_DOCK_CORE_INFO) == 3
         && dock_slot_of(&l, COMPANION_DOCK_PLAYLISTS) == 0, "dock: moved down its own side (%d)", dock_slot_of(&l, COMPANION_DOCK_SEARCH));

   /* A side whose panes are all hidden takes no room. */
   companion_dock_default(&l);
   for (i = COMPANION_DOCK_BOXART; i <= COMPANION_DOCK_CORE_INFO; i++)
      companion_dock_set_shown(&l, (enum companion_dock_id)i, false);
   companion_dock_layout(&l, &m, &client, &g);
   CHECK(RX(g.content) + RW(g.content) == 1280, "dock: hidden right side takes no room (%d)", RX(g.content) + RW(g.content));
   /* A tiny client keeps the content's minimum. */
   client.dims = VIDEO_SCALE_PACK(200, 100);
   companion_dock_default(&l);
   companion_dock_layout(&l, &m, &client, &g);
   CHECK(RW(g.content) >= 60 && RH(g.content) >= 60, "dock: tiny client keeps the content (%dx%d)", RW(g.content), RH(g.content));
   settings->bools.desktop_menu_save_dock_positions = false;
}

/* A bulk add pushes what it has not seen without a search of every
 * entry; what it has seen takes the full push. Both together must do
 * exactly what the full push does for each file: the same playlist from
 * new files, files already there for the same core (moved to the top)
 * and for another (added beside), a file twice in one add, and a folder
 * added twice. */
static void bulk_seed(const char *rel, const char *many)
{
   char path[512], buf[4096];
   int  off = 0, i;
   static const int   ids[4]   = { 5, 10, 15, 20 };
   static const char *cores[4] = { "DETECT", "DETECT", "DETECT", "/cores/other_libretro.so" };
   fixture(path, sizeof(path), rel);
   off += snprintf(buf + off, sizeof(buf) - off,
         "{\n  \"version\": \"1.5\",\n  \"items\": [\n");
   for (i = 0; i < 4; i++)
      off += snprintf(buf + off, sizeof(buf) - off,
            "    { \"path\": \"%s/f%02d.bin\", \"label\": \"f%02d\", \"core_path\": \"%s\","
            " \"core_name\": \"DETECT\", \"crc32\": \"00000000|crc\", \"db_name\": \"Bulk.lpl\" }%s\n",
            many, ids[i], ids[i], cores[i], i < 3 ? "," : "");
   snprintf(buf + off, sizeof(buf) - off, "  ]\n}\n");
   writef(path, buf);
}

static playlist_t *bulk_open(const char *rel)
{
   char path[512];
   playlist_config_t config;
   memset(&config, 0, sizeof(config));
   config.capacity = 1000;
   fixture(path, sizeof(path), rel);
   playlist_config_set_path(&config, path);
   return playlist_init(&config);
}

static void test_bulk_add_matches_pushes(void)
{
   companion_core_t *c = make_core();
   char many[512], p[512], f03[512], f10[512], f20[512], pa[512];
   const char *paths[6];
   size_t i, na, nb = 0;
   playlist_t *a, *b;
   fixture(p, sizeof(p), "bulk"); mkdirp(p);
   fixture(p, sizeof(p), "bulk/a"); mkdirp(p);
   fixture(p, sizeof(p), "bulk/b"); mkdirp(p);
   fixture(many, sizeof(many), "bulk/many"); mkdirp(many);
   for (i = 0; i < 40; i++)
   {
      snprintf(p, sizeof(p), "%s/f%02u.bin", many, (unsigned)i);
      writef(p, "x");
   }
   bulk_seed("bulk/a/Bulk.lpl", many);
   bulk_seed("bulk/b/Bulk.lpl", many);
   snprintf(f03, sizeof(f03), "%s/f03.bin", many);
   snprintf(f10, sizeof(f10), "%s/f10.bin", many);
   snprintf(f20, sizeof(f20), "%s/f20.bin", many);
   paths[0] = many; paths[1] = f10; paths[2] = f20;
   paths[3] = f03;  paths[4] = f03; paths[5] = many;

   fixture(pa, sizeof(pa), "bulk/a/Bulk.lpl");
   na = companion_core_playlist_add_files(c, pa, paths, 6, NULL, NULL);

   /* the same, a full push per file, folders walked as the add walks them */
   if ((b = bulk_open("bulk/b/Bulk.lpl")))
   {
      for (i = 0; i < 6; i++)
      {
         struct string_list *list = NULL;
         size_t k, cnt = 1;
         if (path_is_directory(paths[i]))
         {
            list = dir_list_new(paths[i], NULL, true, false, false, false);
            cnt  = list ? list->size : 0;
         }
         for (k = 0; k < cnt; k++)
         {
            const char *one = list ? list->elems[k].data : paths[i];
            char label[NAME_MAX_LENGTH];
            fill_pathname(label, path_basename(one), "", sizeof(label));
            if (companion_core_playlist_push(c, b, one, label, "DETECT",
                     "DETECT", "Bulk.lpl"))
               nb++;
         }
         string_list_free(list);
      }
      playlist_write_file(b);
      playlist_free(b);
   }
   CHECK(na == nb, "bulk add counted %u, full pushes %u", (unsigned)na, (unsigned)nb);

   a = bulk_open("bulk/a/Bulk.lpl");
   b = bulk_open("bulk/b/Bulk.lpl");
   CHECK(a && b, "both playlists load");
   if (a && b)
   {
      CHECK(playlist_size(a) == playlist_size(b), "bulk add %u entries, full pushes %u",
            (unsigned)playlist_size(a), (unsigned)playlist_size(b));
      CHECK(playlist_size(a) == 41, "the 40 files, and f20 for another core besides (got %u)",
            (unsigned)playlist_size(a));
      for (i = 0; i < playlist_size(a) && i < playlist_size(b); i++)
      {
         const struct playlist_entry *ea = NULL, *eb = NULL;
         playlist_get_index(a, i, &ea);
         playlist_get_index(b, i, &eb);
         if (!ea || !eb
               || !string_is_equal(ea->path, eb->path)
               || !string_is_equal(ea->label, eb->label)
               || !string_is_equal(ea->core_path, eb->core_path)
               || !string_is_equal(ea->db_name, eb->db_name))
         {
            CHECK(false, "entry %u: bulk add %s (%s), full pushes %s (%s)", (unsigned)i,
                  ea ? ea->path : "-", ea ? ea->core_path : "-",
                  eb ? eb->path : "-", eb ? eb->core_path : "-");
            break;
         }
      }
   }
   playlist_free(a);
   playlist_free(b);
   companion_core_free(c);
}

int main(void)
{
   setup();
   test_playlist_listing();
   test_select_and_entries();
   test_all_playlists();
   test_thumbnail_path();
   test_entry_thumbnail_system();
   test_thumbnail_match_push();
#ifdef HAVE_NETWORKING
   test_thumbnail_download();
#endif
   test_browser();
   test_browser_sort();
   test_sort_without_metadata();
   test_sort_then_folders_only();
   test_backend_scenario();
   test_sort_callback_reentrancy();
   test_rename_add_install();
   test_bulk_add_matches_pushes();
   test_rename_cache();
   test_install_names();
   test_thumbnail_path_bounds();
   test_options_and_shader_params();
   test_settings_table();
   test_shortcuts();
   test_hidden_new_delete();
   test_hidden_playlists();
   test_run_paths();
   test_launch_options();
   test_dock_rows();
   test_place_window();
   test_dock_model();
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
