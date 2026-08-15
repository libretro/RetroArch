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


/* ------------------------------------------------------------------ */
/* Budgeted lanes: the resumable API must produce byte-identical      */
/* playlists to the blocking path, yielding along the way.            */
/* ------------------------------------------------------------------ */

static bool budget_countdown(void *ud)
{
   int *k = (int*)ud;
   if (*k <= 0)
      return false;
   (*k)--;
   return true;
}

static char *big_fixture_doc(unsigned entries, const char *base_dir)
{
   size_t cap  = (size_t)entries * 160 + 512;
   char *doc   = (char*)malloc(cap);
   size_t _len = 0;
   unsigned i;

   if (!doc)
      return NULL;
   _len += (size_t)snprintf(doc + _len, cap - _len,
         "{\n  \"version\": \"1.5\",\n"
         "  \"base_content_directory\": \"%s\",\n"
         "  \"items\": [\n", base_dir);
   for (i = 0; i < entries; i++)
      _len += (size_t)snprintf(doc + _len, cap - _len,
            "    { \"path\": \"%s/dir%03u/game%05u.bin\","
            " \"label\": \"Game %05u\" }%s\n",
            base_dir, i % 251, i, i, (i + 1 < entries) ? "," : "");
   _len += (size_t)snprintf(doc + _len, cap - _len, "  ]\n}\n");
   return doc;
}

static void check_playlists_equal(playlist_t *a, playlist_t *b,
      const char *lane)
{
   size_t i, n = playlist_size(a);
   CHECK(n == playlist_size(b), "%s: sizes differ (%u vs %u)",
         lane, (unsigned)n, (unsigned)playlist_size(b));
   if (n != playlist_size(b))
      return;
   for (i = 0; i < n; i++)
   {
      const struct playlist_entry *ea = NULL;
      const struct playlist_entry *eb = NULL;
      playlist_get_index(a, i, &ea);
      playlist_get_index(b, i, &eb);
      if (!ea || !eb
            || !streq(ea->path, eb->path)
            || !streq(ea->label, eb->label))
      {
         CHECK(false, "%s: entry %u differs", lane, (unsigned)i);
         return;
      }
   }
   CHECK(streq(playlist_get_default_core_path(a),
               playlist_get_default_core_path(b)),
         "%s: default_core_path differs", lane);
}

static void lane_budgeted_json(void)
{
   char path[512];
   char *doc = NULL;
   playlist_config_t config;
   playlist_t *blocking = NULL;
   playlist_t *stepped  = NULL;
   playlist_parse_t *p  = NULL;
   unsigned had = failures;
   unsigned yields = 0;
   int r;

   if (!(doc = big_fixture_doc(5000, "/games")))
   {
      CHECK(false, "budgeted json: doc alloc");
      return;
   }
   snprintf(path, sizeof(path), "%s/big.lpl", fixture_dir);
   CHECK(write_whole(path, doc), "fixture write");
   free(doc);
   config_defaults(&config, path);
   config.capacity = 8192;

   blocking = playlist_init(&config);
   CHECK(blocking && playlist_size(blocking) == 5000,
         "budgeted json: blocking reference");

   p = playlist_parse_begin(&config);
   CHECK(p != NULL, "budgeted json: begin");
   for (;;)
   {
      int k = 4;   /* four event batches per slice */
      r = playlist_parse_step(p, budget_countdown, &k);
      if (r != 0)
         break;
      yields++;
   }
   CHECK(r == 1, "budgeted json: step result %d", r);
   stepped = playlist_parse_end(p);
   CHECK(stepped != NULL, "budgeted json: end");

   /* 5000 entries at ~14 events each vs 4*256-event slices: the
    * parse cannot possibly fit one slice. */
   CHECK(yields >= 10, "budgeted json: only %u yields", yields);

   if (blocking && stepped)
      check_playlists_equal(blocking, stepped, "budgeted json");

   playlist_free(blocking);
   playlist_free(stepped);
   if (failures == had)
      fprintf(stderr, "[pass] budgeted json lane (%u yields)\n", yields);
}

static void lane_budgeted_old_format(void)
{
   char path[512];
   playlist_config_t config;
   playlist_t *blocking = NULL;
   playlist_t *stepped  = NULL;
   playlist_parse_t *p  = NULL;
   unsigned had = failures;
   unsigned yields = 0;
   int r;

   snprintf(path, sizeof(path), "%s/old.lpl", fixture_dir);
   CHECK(write_whole(path, old_format), "fixture write");
   config_defaults(&config, path);

   blocking = playlist_init(&config);

   p = playlist_parse_begin(&config);
   CHECK(p != NULL, "budgeted old: begin");
   for (;;)
   {
      int k = 1;   /* one line group per slice */
      r = playlist_parse_step(p, budget_countdown, &k);
      if (r != 0)
         break;
      yields++;
   }
   CHECK(r == 1, "budgeted old: step result %d", r);
   stepped = playlist_parse_end(p);
   CHECK(stepped && blocking, "budgeted old: results");
   CHECK(yields >= 1, "budgeted old: no yields at group granularity");
   if (blocking && stepped)
      check_playlists_equal(blocking, stepped, "budgeted old");
   playlist_free(blocking);
   playlist_free(stepped);
   if (failures == had)
      fprintf(stderr, "[pass] budgeted old-format lane (%u yields)\n", yields);
}

static void lane_budgeted_autofix(void)
{
   char path[512];
   char *doc = NULL;
   playlist_config_t config;
   playlist_t *blocking = NULL;
   playlist_t *stepped  = NULL;
   playlist_parse_t *p  = NULL;
   const struct playlist_entry *e = NULL;
   unsigned had = failures;
   int r;

   if (!(doc = big_fixture_doc(600, "/games")))
   {
      CHECK(false, "autofix: doc alloc");
      return;
   }
   snprintf(path, sizeof(path), "%s/fix.lpl", fixture_dir);
   CHECK(write_whole(path, doc), "fixture write");

   config_defaults(&config, path);
   config.capacity      = 8192;
   config.autofix_paths = true;
   strlcpy(config.base_content_directory, "/mnt/newgames",
         sizeof(config.base_content_directory));

   blocking = playlist_init(&config);
   CHECK(blocking && playlist_size(blocking) == 600,
         "autofix: blocking reference");
   playlist_get_index(blocking, 0, &e);
   CHECK(e && e->path && strncmp(e->path, "/mnt/newgames/", 14) == 0,
         "autofix: blocking rewrote paths (got \"%s\")",
         (e && e->path) ? e->path : "(null)");

   /* The fixture write above rewrote the file (autofix saves), so
    * regenerate it for the stepped run to see identical input. */
   CHECK(write_whole(path, doc), "fixture rewrite");
   free(doc);

   p = playlist_parse_begin(&config);
   CHECK(p != NULL, "autofix: begin");
   for (;;)
   {
      int k = 2;
      r = playlist_parse_step(p, budget_countdown, &k);
      if (r != 0)
         break;
   }
   CHECK(r == 1, "autofix: step result %d", r);
   stepped = playlist_parse_end(p);
   CHECK(stepped != NULL, "autofix: end");
   if (blocking && stepped)
      check_playlists_equal(blocking, stepped, "autofix");
   playlist_free(blocking);
   playlist_free(stepped);
   if (failures == had)
      fprintf(stderr, "[pass] budgeted autofix lane\n");
}

static void lane_abort_midway(void)
{
   char path[512];
   playlist_config_t config;
   playlist_parse_t *p = NULL;
   unsigned had = failures;
   int k;

   /* Reuse the big fixture from the budgeted lane. */
   snprintf(path, sizeof(path), "%s/big.lpl", fixture_dir);
   config_defaults(&config, path);
   config.capacity = 8192;

   p = playlist_parse_begin(&config);
   CHECK(p != NULL, "abort: begin");
   k = 2;
   CHECK(playlist_parse_step(p, budget_countdown, &k) == 0,
         "abort: expected a yield mid-parse");
   /* Abandon with entries staged and the parser mid-document: LSan
    * verifies nothing leaks. */
   playlist_parse_abort(p);

   /* Abort immediately after begin, and end-after-abort misuse
    * guards. */
   p = playlist_parse_begin(&config);
   CHECK(p != NULL, "abort: second begin");
   playlist_parse_abort(p);
   CHECK(playlist_parse_end(NULL) == NULL, "end(NULL)");
   CHECK(playlist_parse_step(NULL, NULL, NULL) == -1, "step(NULL)");
   if (failures == had)
      fprintf(stderr, "[pass] abort lane\n");
}


/* ------------------------------------------------------------------ */
/* Cache-reuse invariant.                                             */
/*                                                                    */
/* playlist_cached_is_reusable() decides whether the cached playlist  */
/* can stand in for a request by comparing the size recorded at read  */
/* time against path_get_size() now.  Whatever the read path records  */
/* must therefore be the ON-DISK size - which is not the same number  */
/* the stream reports for a compressed playlist, where rzip yields    */
/* the uncompressed size from its header.  Recording the wrong one    */
/* costs no correctness but defeats the cache completely: every menu  */
/* navigation would re-read and re-parse the file.                    */
/*                                                                    */
/* Observable via pointer identity: a reused cache returns the same   */
/* playlist object, a defeated one a freshly parsed object.           */
/* ------------------------------------------------------------------ */

static void check_cache_reuse(const char *path, bool compressed,
      const char *base_content_dir, const char *lane)
{
   playlist_config_t config;
   playlist_t *first  = NULL;
   playlist_t *second = NULL;

   config_defaults(&config, path);
   config.capacity = 8192;
   /* Match the on-disk format so the cached init does not rewrite
    * the file underneath the comparison, and the recorded base
    * content directory so reuse is not rejected before the size
    * comparison is even reached. */
   config.compress = compressed;
   playlist_config_set_base_content_directory(&config, base_content_dir);

   playlist_free_cached();

   CHECK(playlist_init_cached(&config), "%s: first cached init", lane);
   if (!(first = playlist_get_cached()))
   {
      CHECK(false, "%s: first cached playlist", lane);
      return;
   }

   /* Mark the cached object in memory only.  Pointer identity is
    * not a sound observable here - a re-parse frees the old
    * playlist and the allocator hands the same address straight
    * back - but a marker cannot survive a re-read from disk. */
   playlist_set_default_core_name(first, "CACHE_REUSE_SENTINEL");

   CHECK(playlist_init_cached(&config), "%s: second cached init", lane);
   second = playlist_get_cached();

   CHECK(second && streq(playlist_get_default_core_name(second),
            "CACHE_REUSE_SENTINEL"),
         "%s: cache was not reused - the size recorded at read time "
         "does not match path_get_size(), so every request re-parses",
         lane);

   playlist_free_cached();
}

static void lane_cache_reuse(void)
{
   char path[512];
   unsigned had = failures;

   /* Uncompressed JSON */
   snprintf(path, sizeof(path), "%s/full.lpl", fixture_dir);
   CHECK(write_whole(path, json_full), "fixture write");
   check_cache_reuse(path, false, "/games", "cache reuse (json)");

   /* Old format, with the configuration asking for the new one: the
    * cached init converts the file on the spot.  That rewrite
    * changes the on-disk size out from under the stamp taken while
    * reading, so this case is the one that catches a stamp left
    * stale after a format or compression conversion - the state in
    * which the cache can never be reused again. */
   snprintf(path, sizeof(path), "%s/old.lpl", fixture_dir);
   CHECK(write_whole(path, old_format), "fixture write");
   check_cache_reuse(path, false, "", "cache reuse (old format converted)");

#if defined(HAVE_COMPRESSION)
   /* RZIP compressed: the case where the stream size and the
    * on-disk size differ. */
   {
      intfstream_t *stream = NULL;
      snprintf(path, sizeof(path), "%s/comp.lpl", fixture_dir);
      stream = intfstream_open_rzip_file(path,
            RETRO_VFS_FILE_ACCESS_WRITE);
      CHECK(stream != NULL, "cache reuse: rzip open");
      if (stream)
      {
         intfstream_write(stream, json_full, (int64_t)strlen(json_full));
         intfstream_close(stream);
         free(stream);
         check_cache_reuse(path, true, "/games", "cache reuse (compressed)");
      }
   }
#endif

   if (failures == had)
      fprintf(stderr, "[pass] cache-reuse lane\n");
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
   lane_budgeted_json();
   lane_budgeted_old_format();
   lane_budgeted_autofix();
   lane_abort_midway();
   lane_cache_reuse();

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
