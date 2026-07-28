/* End-to-end regression test for the content scanner.
 *
 * Drives task_push_dbscan() over content that is built to match, and
 * checks the playlists it produces name the right games under the
 * right databases.
 *
 * This is the only test here that reaches the scan as a whole: the
 * task lifecycle, the database iteration, the crc lookup, the
 * promotion of a matched database to the front of the list, the
 * playlist write and the teardown.  Two defects in this tree went
 * undetected because nothing exercised that:
 *
 *   - the index caches were keyed by a database's position in the
 *     list but not moved when a match promoted it, so content ended
 *     up filed under whichever database took the vacated slot;
 *   - playlist_init() left scan_record.overwrite_playlist
 *     uninitialised, and it is written into the playlist and read
 *     back on the next scan to decide whether to overwrite.
 *
 * The second content file therefore matches the *second* database on
 * purpose.  A match in the first would never trigger the promotion,
 * and the pairing bug would pass unnoticed again.
 *
 * Everything is built here - databases, content, core info - so the
 * test needs no fixture beyond a writable directory.  Content is made
 * to match by forcing its crc32: four bytes appended to a buffer can
 * bring the checksum to any value, which is cheaper and more precise
 * than searching for content that happens to collide.
 *
 * Run with: make scan_test SANITIZER=address,undefined
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <queues/task_queue.h>
#include <lists/dir_list.h>
#include <retro_timers.h>
#include <streams/file_stream.h>

#include "../../../core_info.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../manual_content_scan.h"
#include "../../../list_special.h"
#include "../../../configuration.h"
#include "../../../verbosity.h"

#define SCAN_TIMEOUT_SECONDS 120


/* Stubs for symbols referenced by the retroarch-tree sources we pull
 * in.  The real definitions live in intl/msg_hash_us.c and
 * configuration.c, but those files transitively require RARCH_INTERNAL
 * which drags in the entire frontend subsystem.  This sample only
 * exercises task_push_dbscan; none of these symbols are actually
 * invoked on the path through task_push_dbscan / task_queue_check.
 *
 * These take enum msg_hash_enums now that configuration.h - included
 * for settings_t - brings msg_hash.h with it.  They used to be
 * declared with int, which matched at the link level but conflicts
 * once the real prototypes are visible. */
int msg_hash_get_help_us_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   (void)msg;
   if (s && len)
      s[0] = '\0';
   return 0;
}

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
   (void)msg;
   return "";
}

/* The string-table index builder (added by the msg_hash strtab refactor).
 * With msg_hash_to_str_us() stubbed above, the index is never consulted, so
 * this can be an empty stub rather than linking intl/msg_hash_us.c. */
void msg_hash_us_index_init(void)
{
}

settings_t *config_get_ptr(void)
{
   /* A real settings_t, not a zeroed blob.
    *
    * The scan reads settings->paths.directory_playlist before it does
    * anything else and refuses to start if it is empty - so a stub that
    * returned zeros meant no task was ever created, the completion
    * callback never fired, and this sample sat in its loop until the CI
    * runner killed it.  The playlist directory argument it accepts on
    * the command line went nowhere.
    *
    * Static so it is zero-initialised; main() fills in the one field
    * the scan actually consults. */
   static settings_t settings;
   return &settings;
}

/* Additional stubs for retroarch-core symbols referenced transitively.
 * None of these are exercised on the dbscan path; they're link-time
 * stubs to avoid pulling in retroarch.c, runloop.c, frontend drivers,
 * and the UI/video subsystems. */
void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration,
      bool flush, char *title, unsigned icon, unsigned category)
{
   (void)msg; (void)len; (void)prio; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
}

bool retroarch_override_setting_is_set(unsigned enum_idx, void *data)
{
   (void)enum_idx; (void)data;
   return false;
}

void ui_companion_driver_notify_refresh(void)
{
}

void video_display_server_set_window_progress(int progress, bool finished)
{
   (void)progress; (void)finished;
}

/* task_database.c's progress_cb is now the shared task_window_progress_cb,
 * whose definition lives in tasks/task_file_transfer.c.  Pulling that
 * file in would drag in nbio, the audio mixer, and the image-task
 * machinery, none of which the dbscan path exercises.  Stub it here
 * to satisfy the linker; the function is never called on this code
 * path because no progress_cb is invoked unless a worker thread
 * publishes progress, and this sample never reaches that state. */
void task_window_progress_cb(retro_task_t *task)
{
   (void)task;
}

/* dir_list_new_special lives in retroarch.c, which we cannot link
 * without dragging in the world.
 *
 * Returning NULL here meant the scan never enumerated its databases,
 * so it could not finish, and everything downstream of a completed
 * scan - notably the task teardown - went unexercised.  The scanner
 * only asks for DIR_LIST_DATABASES, which retroarch.c answers with a
 * plain "rdb" listing, so that much is worth providing. */
struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter, bool show_hidden_files)
{
   (void)filter;

   if (type == DIR_LIST_DATABASES)
      return dir_list_new(input_dir, "rdb", false, show_hidden_files,
            false, false);

   return NULL;
}

static int failures = 0;
static int checks   = 0;

static void check(int ok, const char *what, const char *detail)
{
   checks++;
   printf("  %-5s %-42s %s\n", ok ? "ok" : "FAIL", what,
         detail ? detail : "");
   if (!ok)
      failures++;
}

/* ------------------------------------------------------------------
 * crc32, and forcing a buffer to a chosen one
 * ------------------------------------------------------------------ */

static uint32_t crc_table[256];
static uint8_t  crc_top[256];          /* table index by top byte */

static void crc_init(void)
{
   unsigned i, j;
   for (i = 0; i < 256; i++)
   {
      uint32_t c = i;
      for (j = 0; j < 8; j++)
         c = (c >> 1) ^ ((c & 1) ? 0xEDB88320u : 0u);
      crc_table[i] = c;
   }
   for (i = 0; i < 256; i++)
      crc_top[crc_table[i] >> 24] = (uint8_t)i;
}

static uint32_t crc_of(const uint8_t *data, size_t len)
{
   uint32_t reg = 0xFFFFFFFFu;
   size_t   i;
   for (i = 0; i < len; i++)
      reg = (reg >> 8) ^ crc_table[(reg ^ data[i]) & 0xFF];
   return reg ^ 0xFFFFFFFFu;
}

/* Four bytes that, appended to @data, bring its crc32 to @target.
 *
 * Each step of the checksum is reversible: the top byte of the
 * register after a step identifies the table entry used, which gives
 * both the register before it and the byte that was fed in.  Walking
 * four steps back from the wanted value and then forward again from
 * the current one yields the bytes. */
static void crc_force(const uint8_t *data, size_t len, uint32_t target,
      uint8_t out[4])
{
   uint32_t reg = crc_of(data, len) ^ 0xFFFFFFFFu;
   uint32_t r   = target ^ 0xFFFFFFFFu;
   uint8_t  idx[4];
   int      i;

   for (i = 0; i < 4; i++)
   {
      idx[i] = crc_top[r >> 24];
      r      = (r ^ crc_table[idx[i]]) << 8;
   }
   for (i = 3; i >= 0; i--)
   {
      out[3 - i] = (uint8_t)((reg & 0xFF) ^ idx[i]);
      reg        = (reg >> 8) ^ crc_table[idx[i]];
   }
}

/* ------------------------------------------------------------------
 * A minimal .rdb, so the test does not depend on shipped data
 * ------------------------------------------------------------------ */

typedef struct { uint8_t *d; size_t len, cap; } buf_t;

static void bput(buf_t *b, const void *p, size_t n)
{
   if (b->len + n > b->cap)
   {
      size_t want = b->cap ? b->cap * 2 : 256;
      while (want < b->len + n)
         want *= 2;
      b->d   = (uint8_t*)realloc(b->d, want);
      b->cap = want;
   }
   memcpy(b->d + b->len, p, n);
   b->len += n;
}
static void bbyte(buf_t *b, uint8_t v)     { bput(b, &v, 1); }
static void bfixmap(buf_t *b, unsigned n)  { bbyte(b, (uint8_t)(0x80 | n)); }
static void bfixstr(buf_t *b, const char *s)
{
   size_t n = strlen(s);
   bbyte(b, (uint8_t)(0xa0 | n));
   bput(b, s, n);
}
static void bbin4(buf_t *b, uint32_t v)
{
   uint8_t c[4];
   c[0] = (uint8_t)(v >> 24); c[1] = (uint8_t)(v >> 16);
   c[2] = (uint8_t)(v >> 8);  c[3] = (uint8_t)v;
   bbyte(b, 0xc4); bbyte(b, 4); bput(b, c, 4);
}
static void buint32(buf_t *b, uint32_t v)
{
   uint8_t c[4];
   c[0] = (uint8_t)(v >> 24); c[1] = (uint8_t)(v >> 16);
   c[2] = (uint8_t)(v >> 8);  c[3] = (uint8_t)v;
   bbyte(b, 0xce); bput(b, c, 4);
}

/* One database holding a single named record. */
static int write_db(const char *path, const char *game, uint32_t crc,
      uint32_t size)
{
   buf_t    body, meta;
   FILE    *f;
   uint8_t  hdr[16];
   uint64_t off;
   int      i;

   memset(&body, 0, sizeof(body));
   memset(&meta, 0, sizeof(meta));

   bfixmap(&body, 3);
   bfixstr(&body, "name"); bfixstr(&body, game);
   bfixstr(&body, "crc");  bbin4(&body, crc);
   bfixstr(&body, "size"); buint32(&body, size);
   bbyte(&body, 0xc0);

   bfixmap(&meta, 1);
   bfixstr(&meta, "count");
   bbyte(&meta, 0x01);

   off = 16 + (uint64_t)body.len;
   memcpy(hdr, "RARCHDB", 7);
   hdr[7] = 0;
   for (i = 0; i < 8; i++)
      hdr[8 + i] = (uint8_t)(off >> (56 - 8 * i));

   if (!(f = fopen(path, "wb")))
      return 0;
   fwrite(hdr, 1, sizeof(hdr), f);
   fwrite(body.d, 1, body.len, f);
   fwrite(meta.d, 1, meta.len, f);
   fclose(f);
   free(body.d);
   free(meta.d);
   return 1;
}

/* Content of @size whose crc32 is @crc. */
static int write_content(const char *path, uint32_t crc, uint32_t size)
{
   uint8_t *d = (uint8_t*)malloc(size);
   uint32_t i;
   FILE    *f;

   if (!d || size < 8)
   {
      free(d);
      return 0;
   }
   for (i = 0; i < size - 4; i++)
      d[i] = (uint8_t)((i * 37 + 11) & 0xFF);
   crc_force(d, size - 4, crc, d + size - 4);

   if (crc_of(d, size) != crc)
   {
      free(d);
      return 0;
   }
   if (!(f = fopen(path, "wb")))
   {
      free(d);
      return 0;
   }
   fwrite(d, 1, size, f);
   fclose(f);
   free(d);
   return 1;
}

/* A one-entry zip using the stored method, so no compressor is needed
 * and the bytes are entirely predictable.  Written by hand because
 * the point is to exercise the scanner's archive handling, not to
 * depend on a fixture checked into the tree. */
static int write_zip(const char *path, const char *member,
      const uint8_t *data, uint32_t len, uint32_t crc)
{
   FILE    *f = fopen(path, "wb");
   uint16_t nlen = (uint16_t)strlen(member);
   uint8_t  h[46];
   uint32_t local_off = 0;

   if (!f)
      return 0;

#define PUT16(p, v) do { (p)[0] = (uint8_t)(v); (p)[1] = (uint8_t)((v) >> 8); } while (0)
#define PUT32(p, v) do { (p)[0] = (uint8_t)(v);        (p)[1] = (uint8_t)((v) >> 8);                          (p)[2] = (uint8_t)((v) >> 16); (p)[3] = (uint8_t)((v) >> 24); } while (0)

   /* local file header */
   memset(h, 0, 30);
   PUT32(h,      0x04034b50); PUT16(h + 4,  20); PUT16(h + 6, 0);
   PUT16(h + 8,  0);          /* stored */
   PUT16(h + 10, 0); PUT16(h + 12, 0);
   PUT32(h + 14, crc); PUT32(h + 18, len); PUT32(h + 22, len);
   PUT16(h + 26, nlen); PUT16(h + 28, 0);
   fwrite(h, 1, 30, f);
   fwrite(member, 1, nlen, f);
   fwrite(data, 1, len, f);

   /* central directory */
   local_off = 0;
   memset(h, 0, 46);
   PUT32(h,      0x02014b50); PUT16(h + 4, 20); PUT16(h + 6, 20);
   PUT16(h + 8,  0); PUT16(h + 10, 0);
   PUT16(h + 12, 0); PUT16(h + 14, 0);
   PUT32(h + 16, crc); PUT32(h + 20, len); PUT32(h + 24, len);
   PUT16(h + 28, nlen); PUT16(h + 30, 0); PUT16(h + 32, 0);
   PUT16(h + 34, 0); PUT16(h + 36, 0); PUT32(h + 38, 0);
   PUT32(h + 42, local_off);
   fwrite(h, 1, 46, f);
   fwrite(member, 1, nlen, f);

   /* end of central directory */
   memset(h, 0, 22);
   PUT32(h,      0x06054b50);
   PUT16(h + 8,  1); PUT16(h + 10, 1);
   PUT32(h + 12, 46 + nlen);
   PUT32(h + 16, 30 + nlen + len);
   fwrite(h, 1, 22, f);
   fclose(f);
#undef PUT16
#undef PUT32
   return 1;
}

static int file_contains(const char *path, const char *needle)
{
   int64_t  len  = 0;
   char    *data = NULL;
   int      hit;

   if (!filestream_read_file(path, (void**)&data, &len) || !data)
      return 0;
   hit = (strstr(data, needle) != NULL);
   free(data);
   return hit;
}

/* ------------------------------------------------------------------ */

static bool loop_active   = true;
static bool scan_completed = false;

static void scan_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data;
   if (err && *err)
      fprintf(stderr, "  scan reported an error: %s\n", err);
   scan_completed = true;
   loop_active    = false;
}

static void msgq_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   (void)task; (void)msg; (void)prio; (void)duration; (void)flush;
}

int main(int argc, char **argv)
{
   const char *root = (argc > 1) ? argv[1] : "/tmp";
   char db_dir[512], core_dir[512], info_dir[512];
   char in_dir[512], pl_dir[512], p[1024];
   bool cache_supported = false;
   /* Alpha lives in the first database, Beta in the second, so the
    * second match promotes a database that was not already at the
    * front. */
   const uint32_t crc_a = 0xA1B2C3D4u, crc_b = 0x5E6F7A8Bu;
   const uint32_t sz_a  = 4096,        sz_b  = 8192;
   /* A cue sheet does not carry the checksum itself; the scanner
    * parses it, finds the track it names and checksums that.  This
    * is the only coverage of that path. */
   const uint32_t crc_c = 0x0C0FFEE0u;
   const uint32_t sz_c  = 6144;
   /* Inside an archive, so the scanner has to expand it and build a
    * "archive#member" path for the entry. */
   const uint32_t crc_d = 0xD15CD15Cu;
   const uint32_t sz_d  = 2048;

   setvbuf(stdout, NULL, _IONBF, 0);
   crc_init();
   verbosity_disable();

   printf("scanner end-to-end test (dir: %s)\n\n", root);

   sprintf(db_dir,   "%s/scan_db",       root);
   sprintf(core_dir, "%s/scan_cores",    root);
   sprintf(info_dir, "%s/scan_info",     root);
   sprintf(in_dir,   "%s/scan_content",  root);
   sprintf(pl_dir,   "%s/scan_playlist", root);
   path_mkdir(db_dir); path_mkdir(core_dir); path_mkdir(info_dir);
   path_mkdir(in_dir); path_mkdir(pl_dir);

   sprintf(p, "%s/Test Alpha.rdb", db_dir);
   if (!write_db(p, "Alpha The Game", crc_a, sz_a))
   { check(0, "fixture", "could not write database"); return 1; }
   sprintf(p, "%s/Test Beta.rdb", db_dir);
   if (!write_db(p, "Beta The Game", crc_b, sz_b))
   { check(0, "fixture", "could not write database"); return 1; }
   sprintf(p, "%s/Test Disc.rdb", db_dir);
   if (!write_db(p, "Disc The Game", crc_c, sz_c))
   { check(0, "fixture", "could not write database"); return 1; }
   sprintf(p, "%s/Test Zip.rdb", db_dir);
   if (!write_db(p, "Zipped The Game", crc_d, sz_d))
   { check(0, "fixture", "could not write database"); return 1; }

   sprintf(p, "%s/02_alpha.bin", in_dir);
   if (!write_content(p, crc_a, sz_a))
   { check(0, "fixture", "crc forcing failed"); return 1; }
   sprintf(p, "%s/01_beta.bin", in_dir);
   if (!write_content(p, crc_b, sz_b))
   { check(0, "fixture", "crc forcing failed"); return 1; }
   /* The track the cue names, and the sheet itself.  The scanner
    * reads the sheet, resolves the track beside it and checksums
    * that, so only the track carries the matching crc. */
   sprintf(p, "%s/03_disc.bin", in_dir);
   if (!write_content(p, crc_c, sz_c))
   { check(0, "fixture", "crc forcing failed"); return 1; }
   sprintf(p, "%s/03_disc.cue", in_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write cue"); return 1; }
      fprintf(f,
            "FILE \"03_disc.bin\" BINARY\n"
            "  TRACK 01 MODE1/2352\n"
            "    INDEX 01 00:00:00\n");
      fclose(f);
   }

   /* The archive itself checksums to something else entirely, so a
    * match can only come from expanding it. */
   {
      uint8_t *d = (uint8_t*)malloc(sz_d);
      uint32_t i;
      for (i = 0; i < sz_d - 4; i++)
         d[i] = (uint8_t)((i * 53 + 7) & 0xFF);
      crc_force(d, sz_d - 4, crc_d, d + sz_d - 4);
      sprintf(p, "%s/04_bundle.zip", in_dir);
      if (!write_zip(p, "inner.bin", d, sz_d, crc_d))
      { free(d); check(0, "fixture", "could not write zip"); return 1; }
      free(d);
   }

   check(1, "content forced to the databases' crcs",
         "alpha, beta, a cue track and an archive member");

   /* The scanner skips any database no installed core claims, so the
    * core info has to name both databases as well as the extension -
    * see core_info_database_supports_content_path().  Without the
    * database line the scan reports no match for content whose crc is
    * certainly present, which is easy to mistake for a lookup bug. */
   sprintf(p, "%s/test_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Scan Test\"\n"
            "corename = \"ScanTest\"\n"
            "supported_extensions = \"bin|cue|zip\"\n"
            "database = \"Test Alpha|Test Beta|Test Disc|Test Zip\"\n");
      fclose(f);
   }
   sprintf(p, "%s/test_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }

#ifdef HAVE_THREADS
   task_queue_init(true, msgq_push);
#else
   task_queue_init(false, msgq_push);
#endif
   core_info_init_list(info_dir, core_dir, "so", true, false,
         &cache_supported);

   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
         sizeof(config_get_ptr()->paths.path_content_database));

   /* Without this the scanner never opens an archive, and the entry
    * inside is invisible. */
   {
      bool *search_archives = manual_content_scan_get_search_archives_ptr();
      if (search_archives)
         *search_archives = true;
   }

   if (!manual_content_scan_set_menu_system_name(
            MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR, NULL))
      check(0, "system name", "could not be set");

   if (!task_push_dbscan(pl_dir, db_dir, in_dir, true, false, scan_cb))
   {
      check(0, "scan started", "task_push_dbscan refused");
      goto done;
   }

   {
      time_t started = time(NULL);
      while (loop_active)
      {
         task_queue_check();
         if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
            break;
         retro_sleep(1);
      }
   }
   check(scan_completed, "scan ran to completion",
         scan_completed ? "callback fired" : "timed out");

   /* Each game must appear in its own database's playlist.  Getting
    * this wrong is not a crash - the entry is a real record from a
    * real database, just the wrong one. */
   sprintf(p, "%s/Test Alpha.lpl", pl_dir);
   check(file_contains(p, "Alpha The Game"),
         "first database's game in its playlist", "Alpha The Game");
   check(!file_contains(p, "Beta The Game"),
         "and not the other database's game", "no Beta in Alpha");

   sprintf(p, "%s/Test Beta.lpl", pl_dir);
   check(file_contains(p, "Beta The Game"),
         "promoted database's game in its playlist", "Beta The Game");
   check(!file_contains(p, "Alpha The Game"),
         "and not the other database's game", "no Alpha in Beta");

   /* The cue is the entry that lands in the playlist, not the track
    * it names: the scanner checksums the track but records the sheet,
    * which is what a frontend would load. */
   sprintf(p, "%s/Test Zip.lpl", pl_dir);
   check(file_contains(p, "Zipped The Game"),
         "archive member matched", "Zipped The Game");
   /* The member is what gets checksummed and matched, but the entry
    * records the archive: that is the path a frontend loads.  Assert
    * it rather than the "archive#member" form the scan builds
    * internally, which does not reach the playlist. */
   check(   file_contains(p, "04_bundle.zip")
         && !file_contains(p, "#inner.bin"),
         "entry records the archive, not the member", "04_bundle.zip");

   sprintf(p, "%s/Test Disc.lpl", pl_dir);
   check(file_contains(p, "Disc The Game"),
         "cue resolved through its track", "Disc The Game");
   check(file_contains(p, ".cue"),
         "playlist records the sheet, not the track", "path ends .cue");

done:
   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
