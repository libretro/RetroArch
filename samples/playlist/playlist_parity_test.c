/* Parity oracle for playlist.c's read path (playlist_init /
 * playlist_read_file), compiled from the shipping translation unit.
 *
 * Purpose: pin the reader's observable behaviour BEFORE the
 * follow-up commits restructure it (callback driver to pull loop,
 * then a budgeted resumable parse).  Every lane asserts
 * hand-derived expectations against fixtures this test writes, so
 * a later behavioural drift fails here rather than in someone's
 * library:
 *
 *   json          - the full schema: every header meta field, the
 *                   scan record, and entries carrying every member
 *                   the handler accepts, including subsystem rom
 *                   arrays and runtime/last-played values; unknown
 *                   members (flat and nested) are skipped.
 *   json partial  - malformed JSON WARNS and keeps everything
 *                   parsed up to the error; playlist_init still
 *                   succeeds.  Only allocation failure fails the
 *                   read.  This surprising contract is exactly why
 *                   it is pinned.
 *   old format    - six-line entry groups followed by the metadata
 *                   block (default core, label display mode,
 *                   thumbnail modes, sort mode), Windows and Unix
 *                   line endings.
 *   compressed    - the same JSON document behind the RZIP
 *                   interface (transparent for uncompressed input,
 *                   decompressing for rzip files).
 *   missing file  - an absent playlist reads as an empty playlist,
 *                   successfully.
 *   capacity      - entries beyond config.capacity are dropped,
 *                   entries within it survive.
 *
 * No threads anywhere on this path, so the sanitizer sweep is
 * ASan+UBSan+LSan. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <streams/interface_stream.h>
#include <streams/rzip_stream.h>
#include <vfs/vfs.h>
#include <libretro.h>

#include "../../playlist.h"
#include "../../verbosity.h"

/* ------------------------------------------------------------------ */
/* Stubs: the three externs playlist.c references outside the read    */
/* path (dedup matching and core association resolution; none run    */
/* during playlist_init).                                             */
/* ------------------------------------------------------------------ */

#include "../../core_info.h"

bool core_info_find(const char *core_path, core_info_t **core_info)
{
   (void)core_path;
   if (core_info)
      *core_info = NULL;
   return false;
}

bool core_info_core_file_id_is_equal(const char *core_path_a,
      const char *core_path_b)
{
   (void)core_path_a;
   (void)core_path_b;
   return false;
}

bool play_feature_delivery_enabled(void)
{
   return false;
}

/* verbosity.c console hooks (frontend driver not linked) */
void frontend_driver_attach_console(void) { }
void frontend_driver_detach_console(void) { }

/* ------------------------------------------------------------------ */

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

static void config_defaults(playlist_config_t *config, const char *path)
{
   memset(config, 0, sizeof(*config));
   config->capacity            = 512;
   config->old_format          = false;
   config->compress            = false;
   config->fuzzy_archive_match = false;
   config->autofix_paths       = false;
   playlist_config_set_path(config, path);
}

static bool streq(const char *a, const char *b)
{
   if (!a && !b)
      return true;
   if (!a || !b)
      return false;
   return strcmp(a, b) == 0;
}

/* ------------------------------------------------------------------ */
/* Fixtures                                                           */
/* ------------------------------------------------------------------ */

/* Every member the handler accepts, plus unknown members it must
 * skip (one flat, one nested object, one nested array). */
static const char json_full[] =
"{\n"
"  \"version\": \"1.5\",\n"
"  \"default_core_path\": \"/cores/core_a.so\",\n"
"  \"default_core_name\": \"Core A\",\n"
"  \"label_display_mode\": 2,\n"
"  \"right_thumbnail_mode\": 3,\n"
"  \"left_thumbnail_mode\": 2,\n"
"  \"thumbnail_match_mode\": 1,\n"
"  \"sort_mode\": 1,\n"
"  \"base_content_directory\": \"/games\",\n"
"  \"scan_content_dir\": \"/games/snes\",\n"
"  \"scan_file_exts\": \"sfc|smc\",\n"
"  \"scan_dat_file_path\": \"/dats/snes.dat\",\n"
"  \"scan_database_name\": \"Nintendo - SNES\",\n"
"  \"scan_search_recursively\": true,\n"
"  \"scan_search_archives\": false,\n"
"  \"scan_filter_dat_content\": true,\n"
"  \"scan_overwrite_playlist\": false,\n"
"  \"future_flat_member\": \"ignored\",\n"
"  \"future_object\": { \"nested\": [1, 2, {\"deep\": true}] },\n"
"  \"items\": [\n"
"    {\n"
"      \"path\": \"/games/snes/Game One.sfc\",\n"
"      \"label\": \"Game One\",\n"
"      \"core_path\": \"/cores/core_a.so\",\n"
"      \"core_name\": \"Core A\",\n"
"      \"crc32\": \"AABBCCDD|crc\",\n"
"      \"db_name\": \"Nintendo - SNES.lpl\",\n"
"      \"subsystem_ident\": \"sgb\",\n"
"      \"subsystem_name\": \"Super Game Boy\",\n"
"      \"subsystem_roms\": [ \"/games/snes/SGB.sfc\", \"/games/gb/Game.gb\" ],\n"
"      \"runtime_hours\": 12,\n"
"      \"runtime_minutes\": 34,\n"
"      \"runtime_seconds\": 56,\n"
"      \"last_played_year\": 2026,\n"
"      \"last_played_month\": 8,\n"
"      \"last_played_day\": 15,\n"
"      \"last_played_hour\": 1,\n"
"      \"last_played_minute\": 2,\n"
"      \"last_played_second\": 3,\n"
"      \"future_entry_member\": [ \"ignored\", { \"x\": 1 } ]\n"
"    },\n"
"    {\n"
"      \"path\": \"/games/snes/Game Two.sfc\",\n"
"      \"label\": \"\"\n"
"    },\n"
"    {\n"
"      \"label\": \"Pathless Entry\",\n"
"      \"core_name\": \"Core B\"\n"
"    }\n"
"  ]\n"
"}\n";

/* The same document, truncated mid-entry: everything before the
 * cut must survive, and playlist_init must still succeed. */
static const char json_truncated[] =
"{\n"
"  \"version\": \"1.5\",\n"
"  \"default_core_path\": \"/cores/core_a.so\",\n"
"  \"default_core_name\": \"Core A\",\n"
"  \"items\": [\n"
"    {\n"
"      \"path\": \"/games/snes/Game One.sfc\",\n"
"      \"label\": \"Game One\"\n"
"    },\n"
"    {\n"
"      \"path\": \"/games/snes/Game Two.sfc\",\n"
"      \"lab";

/* Old format: six lines per entry, then the metadata block the
 * old writer appends.  Mixed line endings on purpose. */
static const char old_format[] =
"/games/md/Alpha.md\r\n"
"Alpha\r\n"
"/cores/core_md.so\r\n"
"Core MD\r\n"
"11223344|crc\r\n"
"Sega - Mega Drive.lpl\r\n"
"/games/md/Beta.md\n"
"Beta\n"
"DETECT\n"
"DETECT\n"
"00000000|crc\n"
"Sega - Mega Drive.lpl\n"
"default_core_path = \"/cores/core_md.so\"\n"
"default_core_name = \"Core MD\"\n"
"label_display_mode = \"1\"\n"
"thumbnail_mode = \"2|3\"\n"
"sort_mode = \"1\"\n";

/* ------------------------------------------------------------------ */
/* Lanes                                                              */
/* ------------------------------------------------------------------ */

static void check_json_full_contents(playlist_t *pl, const char *lane)
{
   const struct playlist_entry *e = NULL;

   CHECK(playlist_size(pl) == 3, "%s: size %u, wanted 3",
         lane, (unsigned)playlist_size(pl));

   CHECK(streq(playlist_get_default_core_path(pl), "/cores/core_a.so"),
         "%s: default_core_path", lane);
   CHECK(streq(playlist_get_default_core_name(pl), "Core A"),
         "%s: default_core_name", lane);
   CHECK(playlist_get_label_display_mode(pl) == LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
         "%s: label_display_mode", lane);
   CHECK(playlist_get_thumbnail_mode(pl, PLAYLIST_THUMBNAIL_RIGHT)
            == PLAYLIST_THUMBNAIL_MODE_TITLE_SCREENS,
         "%s: right_thumbnail_mode", lane);
   CHECK(playlist_get_thumbnail_mode(pl, PLAYLIST_THUMBNAIL_LEFT)
            == PLAYLIST_THUMBNAIL_MODE_SCREENSHOTS,
         "%s: left_thumbnail_mode", lane);
   CHECK(playlist_get_sort_mode(pl) == PLAYLIST_SORT_MODE_ALPHABETICAL,
         "%s: sort_mode", lane);
   CHECK(streq(playlist_get_scan_content_dir(pl), "/games/snes"),
         "%s: scan_content_dir", lane);
   CHECK(streq(playlist_get_scan_file_exts(pl), "sfc|smc"),
         "%s: scan_file_exts", lane);
   CHECK(streq(playlist_get_scan_dat_file_path(pl), "/dats/snes.dat"),
         "%s: scan_dat_file_path", lane);

   playlist_get_index(pl, 0, &e);
   CHECK(e != NULL, "%s: entry 0 missing", lane);
   if (e)
   {
      CHECK(streq(e->path, "/games/snes/Game One.sfc"), "%s: e0 path", lane);
      CHECK(streq(e->label, "Game One"), "%s: e0 label", lane);
      CHECK(streq(e->core_path, "/cores/core_a.so"), "%s: e0 core_path", lane);
      CHECK(streq(e->core_name, "Core A"), "%s: e0 core_name", lane);
      CHECK(streq(e->crc32, "AABBCCDD|crc"), "%s: e0 crc32", lane);
      CHECK(streq(e->db_name, "Nintendo - SNES.lpl"), "%s: e0 db_name", lane);
      CHECK(streq(e->subsystem_ident, "sgb"), "%s: e0 subsystem_ident", lane);
      CHECK(streq(e->subsystem_name, "Super Game Boy"), "%s: e0 subsystem_name", lane);
      CHECK(e->subsystem_roms && e->subsystem_roms->size == 2
            && streq(e->subsystem_roms->elems[0].data, "/games/snes/SGB.sfc")
            && streq(e->subsystem_roms->elems[1].data, "/games/gb/Game.gb"),
            "%s: e0 subsystem_roms", lane);
      CHECK(e->runtime_hours == 12 && e->runtime_minutes == 34
            && e->runtime_seconds == 56, "%s: e0 runtime", lane);
      CHECK(e->last_played_year == 2026 && e->last_played_month == 8
            && e->last_played_day == 15 && e->last_played_hour == 1
            && e->last_played_minute == 2 && e->last_played_second == 3,
            "%s: e0 last_played", lane);
   }

   e = NULL;
   playlist_get_index(pl, 1, &e);
   CHECK(e && streq(e->path, "/games/snes/Game Two.sfc"),
         "%s: e1 path", lane);
   /* An empty JSON string value is treated as absent: no
    * allocation, member stays NULL.  Pinned as-is. */
   CHECK(e && !e->label, "%s: e1 empty label is NULL", lane);
   CHECK(e && !e->core_path && !e->crc32, "%s: e1 absent members", lane);

   e = NULL;
   playlist_get_index(pl, 2, &e);
   CHECK(e && !e->path && streq(e->label, "Pathless Entry")
         && streq(e->core_name, "Core B"),
         "%s: e2 pathless entry survives", lane);
}

static void lane_json_full(void)
{
   unsigned had = failures;
   char path[512];
   playlist_config_t config;
   playlist_t *pl = NULL;

   snprintf(path, sizeof(path), "%s/full.lpl", fixture_dir);
   CHECK(write_whole(path, json_full), "fixture write");
   config_defaults(&config, path);

   pl = playlist_init(&config);
   CHECK(pl != NULL, "json full: init failed");
   if (!pl)
      return;
   check_json_full_contents(pl, "json full");
   playlist_free(pl);
   if (failures == had)
      fprintf(stderr, "[pass] json full-schema lane\n");
}

static void lane_json_truncated(void)
{
   unsigned had = failures;
   char path[512];
   playlist_config_t config;
   playlist_t *pl = NULL;
   const struct playlist_entry *e = NULL;

   snprintf(path, sizeof(path), "%s/trunc.lpl", fixture_dir);
   CHECK(write_whole(path, json_truncated), "fixture write");
   config_defaults(&config, path);

   pl = playlist_init(&config);
   CHECK(pl != NULL,
         "truncated JSON must warn, keep partial data and succeed");
   if (!pl)
      return;
   CHECK(streq(playlist_get_default_core_path(pl), "/cores/core_a.so"),
         "truncated: header meta before the cut survives");
   CHECK(playlist_size(pl) >= 1, "truncated: first entry survives");
   playlist_get_index(pl, 0, &e);
   CHECK(e && streq(e->path, "/games/snes/Game One.sfc")
         && streq(e->label, "Game One"),
         "truncated: entry 0 contents");
   playlist_free(pl);
   if (failures == had)
      fprintf(stderr, "[pass] json truncated lane\n");
}

static void lane_old_format(void)
{
   unsigned had = failures;
   char path[512];
   playlist_config_t config;
   playlist_t *pl = NULL;
   const struct playlist_entry *e = NULL;

   snprintf(path, sizeof(path), "%s/old.lpl", fixture_dir);
   CHECK(write_whole(path, old_format), "fixture write");
   config_defaults(&config, path);

   pl = playlist_init(&config);
   CHECK(pl != NULL, "old format: init failed");
   if (!pl)
      return;
   CHECK(playlist_size(pl) == 2, "old format: size %u, wanted 2",
         (unsigned)playlist_size(pl));

   playlist_get_index(pl, 0, &e);
   CHECK(e && streq(e->path, "/games/md/Alpha.md")
         && streq(e->label, "Alpha")
         && streq(e->core_path, "/cores/core_md.so")
         && streq(e->core_name, "Core MD")
         && streq(e->crc32, "11223344|crc")
         && streq(e->db_name, "Sega - Mega Drive.lpl"),
         "old format: entry 0 (CRLF endings)");

   e = NULL;
   playlist_get_index(pl, 1, &e);
   CHECK(e && streq(e->path, "/games/md/Beta.md")
         && streq(e->core_path, "DETECT"),
         "old format: entry 1 (LF endings)");

   CHECK(streq(playlist_get_default_core_path(pl), "/cores/core_md.so")
         && streq(playlist_get_default_core_name(pl), "Core MD"),
         "old format: default core metadata");
   CHECK(playlist_get_label_display_mode(pl)
            == LABEL_DISPLAY_MODE_REMOVE_PARENTHESES,
         "old format: label_display_mode 1");
   CHECK(playlist_get_thumbnail_mode(pl, PLAYLIST_THUMBNAIL_RIGHT)
            == PLAYLIST_THUMBNAIL_MODE_SCREENSHOTS
         && playlist_get_thumbnail_mode(pl, PLAYLIST_THUMBNAIL_LEFT)
            == PLAYLIST_THUMBNAIL_MODE_TITLE_SCREENS,
         "old format: thumbnail_mode \"2|3\" is right|left");
   CHECK(playlist_get_sort_mode(pl) == PLAYLIST_SORT_MODE_ALPHABETICAL,
         "old format: sort_mode");
   playlist_free(pl);
   if (failures == had)
      fprintf(stderr, "[pass] old-format lane\n");
}

static void lane_compressed(void)
{
#if defined(HAVE_COMPRESSION)
   unsigned had = failures;
   char path[512];
   playlist_config_t config;
   playlist_t *pl       = NULL;
   intfstream_t *stream = NULL;

   snprintf(path, sizeof(path), "%s/comp.lpl", fixture_dir);

   /* Write the full-schema document through the RZIP writer, so the
    * on-disk bytes are an actual rzip container. */
   stream = intfstream_open_rzip_file(path, RETRO_VFS_FILE_ACCESS_WRITE);
   CHECK(stream != NULL, "rzip open for write");
   if (!stream)
      return;
   CHECK(intfstream_write(stream, json_full,
         (int64_t)strlen(json_full)) == (int64_t)strlen(json_full),
         "rzip write");
   intfstream_close(stream);
   free(stream);

   config_defaults(&config, path);
   pl = playlist_init(&config);
   CHECK(pl != NULL, "compressed: init failed");
   if (!pl)
      return;
   check_json_full_contents(pl, "compressed");
   playlist_free(pl);
   if (failures == had)
      fprintf(stderr, "[pass] compressed lane\n");
#else
   fprintf(stderr, "[skip] compressed lane (no HAVE_COMPRESSION)\n");
#endif
}

static void lane_missing(void)
{
   unsigned had = failures;
   char path[512];
   playlist_config_t config;
   playlist_t *pl = NULL;

   snprintf(path, sizeof(path), "%s/does_not_exist.lpl", fixture_dir);
   config_defaults(&config, path);

   pl = playlist_init(&config);
   CHECK(pl != NULL, "missing file must init an empty playlist");
   CHECK(pl && playlist_size(pl) == 0, "missing file: size 0");
   playlist_free(pl);
   if (failures == had)
      fprintf(stderr, "[pass] missing-file lane\n");
}

static void lane_capacity(void)
{
   unsigned had = failures;
   char path[512];
   char *doc = NULL;
   size_t doc_cap = 64 * 1024;
   size_t _len = 0;
   unsigned i;
   playlist_config_t config;
   playlist_t *pl = NULL;
   const struct playlist_entry *e = NULL;

   if (!(doc = (char*)malloc(doc_cap)))
   {
      CHECK(false, "capacity: doc alloc");
      return;
   }

   _len += (size_t)snprintf(doc + _len, doc_cap - _len,
         "{\n  \"version\": \"1.5\",\n  \"items\": [\n");
   for (i = 0; i < 8; i++)
      _len += (size_t)snprintf(doc + _len, doc_cap - _len,
            "    { \"path\": \"/games/cap/%02u.bin\", \"label\": \"L%02u\" }%s\n",
            i, i, (i < 7) ? "," : "");
   _len += (size_t)snprintf(doc + _len, doc_cap - _len, "  ]\n}\n");

   snprintf(path, sizeof(path), "%s/cap.lpl", fixture_dir);
   CHECK(write_whole(path, doc), "fixture write");
   free(doc);

   config_defaults(&config, path);
   config.capacity = 5;

   pl = playlist_init(&config);
   CHECK(pl != NULL, "capacity: init failed");
   if (!pl)
      return;
   CHECK(playlist_size(pl) == 5, "capacity: size %u, wanted 5",
         (unsigned)playlist_size(pl));
   playlist_get_index(pl, 4, &e);
   CHECK(e && streq(e->path, "/games/cap/04.bin"),
         "capacity: last surviving entry is the fifth");
   playlist_free(pl);
   if (failures == had)
      fprintf(stderr, "[pass] capacity lane\n");
}

int main(int argc, char *argv[])
{
   char cmd[600];

   (void)argc;
   (void)argv;

   verbosity_enable();

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/playlist_fixture_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
   {
      fprintf(stderr, "fixture mkdir failed\n");
      return 1;
   }

   lane_json_full();
   lane_json_truncated();
   lane_old_format();
   lane_compressed();
   lane_missing();
   lane_capacity();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL playlist_parity_test: %u failures\n", failures);
      return 1;
   }
   fprintf(stderr, "PASS playlist_parity_test\n");
   return 0;
}
