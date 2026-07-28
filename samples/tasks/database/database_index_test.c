/* Regression test for the crc and serial index caches.
 *
 * The scanner keeps one index per database, keyed by the database's
 * position in its list, and promotes a database to the front of that
 * list whenever it matches.  Anything else keyed by the same position
 * has to be promoted with it.
 *
 * When the index caches were first added they were not, so after the
 * first match the index at a given slot described a different
 * database than the entry at that slot.  A lookup then answered with
 * records belonging to another system, and the scanner filed the
 * content under whichever database happened to be in the slot - a
 * a title recorded under a wholly unrelated system, with nothing
 * reporting an error.
 *
 * Two things are checked here:
 *
 *   - a lookup against the index built for that database returns the
 *     record the database actually holds;
 *   - a lookup against an index built for a *different* database is
 *     refused rather than answered, so that if the pairing is ever
 *     broken again the result is the slow path rather than a wrong
 *     one.
 *
 * Builds its own databases so it depends on nothing but the library.
 *
 * Run with: make database_index_test SANITIZER=address,undefined
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <queues/task_queue.h>
#include "../../../core_info.h"
#include "../../../tasks/tasks_internal.h"
#include <time.h>
#include <retro_timers.h>
#include "../../../manual_content_scan.h"
#include "../../../configuration.h"
#include "../../../verbosity.h"
#include "../../../database_info.h"

/* The scanner objects this links against expect these; lifted from
 * main.c in this directory, which provides the same set. */



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
 * without dragging in the world.  manual_content_scan calls this to
 * walk the scan directory; returning NULL causes the scan to bail
 * without producing results, which is fine for a standalone demo. */
void *dir_list_new_special(const char *input_dir, unsigned type,
      const char *filter, bool show_hidden_files)
{
   (void)input_dir; (void)type; (void)filter; (void)show_hidden_files;
   return NULL;
}

static int failures = 0;
static int checks   = 0;

static void check(int ok, const char *what, const char *detail)
{
   checks++;
   printf("  %-5s %-40s %s\n", ok ? "ok" : "FAIL", what,
         detail ? detail : "");
   if (!ok)
      failures++;
}

/* ------------------------------------------------------------------
 * Minimal MsgPack writer, so the test does not build its inputs with
 * the library it is testing.
 * ------------------------------------------------------------------ */

typedef struct { uint8_t *data; size_t len, cap; } buf_t;

static void bput(buf_t *b, const void *p, size_t n)
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
static void bbyte(buf_t *b, uint8_t v)    { bput(b, &v, 1); }
static void bfixmap(buf_t *b, unsigned n) { bbyte(b, (uint8_t)(0x80 | n)); }
static void bfixstr(buf_t *b, const char *s)
{
   size_t n = strlen(s);
   bbyte(b, (uint8_t)(0xa0 | n));
   bput(b, s, n);
}
static void bbin(buf_t *b, const void *s, uint8_t n)
{
   bbyte(b, 0xc4);
   bbyte(b, n);
   bput(b, s, n);
}
static void buint8(buf_t *b, uint8_t v) { bbyte(b, 0xcc); bbyte(b, v); }
static void buint16(buf_t *b, uint16_t v)
{ bbyte(b, 0xcd); bbyte(b, (uint8_t)(v >> 8)); bbyte(b, (uint8_t)v); }

/* One database: @count records named "<tag> NNN", carrying crcs from
 * @crc_base upwards and a serial built from @tag. */
static int write_db(const char *path, const char *tag, uint32_t crc_base,
      int count)
{
   buf_t    body, meta;
   FILE    *f;
   uint8_t  hdr[16];
   uint64_t off;
   int      i;

   memset(&body, 0, sizeof(body));
   memset(&meta, 0, sizeof(meta));

   for (i = 0; i < count; i++)
   {
      char     name[64], serial[64];
      uint8_t  crc[4];
      uint32_t c = crc_base + (uint32_t)i;

      sprintf(name,   "%s %03d", tag, i);
      sprintf(serial, "%s-%03d", tag, i);
      crc[0] = (uint8_t)(c >> 24); crc[1] = (uint8_t)(c >> 16);
      crc[2] = (uint8_t)(c >> 8);  crc[3] = (uint8_t)c;

      bfixmap(&body, 4);
      bfixstr(&body, "name");   bfixstr(&body, name);
      bfixstr(&body, "crc");    bbin(&body, crc, 4);
      bfixstr(&body, "serial"); bbin(&body, serial, (uint8_t)strlen(serial));
      bfixstr(&body, "size");   buint8(&body, (uint8_t)(16 + i));
   }
   bbyte(&body, 0xc0);

   bfixmap(&meta, 1);
   bfixstr(&meta, "count");
   buint8(&meta, 1);

   off = 16 + (uint64_t)body.len;
   memcpy(hdr, "RARCHDB", 7);
   hdr[7] = 0;
   for (i = 0; i < 8; i++)
      hdr[8 + i] = (uint8_t)(off >> (56 - 8 * i));

   if (!(f = fopen(path, "wb")))
      return 0;
   fwrite(hdr, 1, sizeof(hdr), f);
   fwrite(body.data, 1, body.len, f);
   fwrite(meta.data, 1, meta.len, f);
   fclose(f);

   free(body.data);
   free(meta.data);
   return 1;
}


/* A database shaped like the ones that broke the size range:
 * @zero_at gets a size of 0 (the value min()/max() used to read as
 * "nothing accumulated yet"), and @crcless_at carries a size but no
 * crc, so the index walk only sees it if the scan reports records
 * that lack the key it was asked for. Sizes are 100 + 10 * i
 * otherwise, so the true range is known. */
static int write_db_sizes(const char *path, int count, int zero_at,
      int crcless_at)
{
   buf_t    body, meta;
   FILE    *f;
   uint8_t  hdr[16];
   uint64_t off;
   int      i;

   memset(&body, 0, sizeof(body));
   memset(&meta, 0, sizeof(meta));

   for (i = 0; i < count; i++)
   {
      char     name[64];
      uint8_t  crc[4];
      uint32_t c    = 0x9000u + (uint32_t)i;
      int      size = (i == zero_at) ? 0 : 100 + 10 * i;

      sprintf(name, "Sized %03d", i);
      crc[0] = (uint8_t)(c >> 24); crc[1] = (uint8_t)(c >> 16);
      crc[2] = (uint8_t)(c >> 8);  crc[3] = (uint8_t)c;

      if (i == crcless_at)
      {
         bfixmap(&body, 2);
         bfixstr(&body, "name"); bfixstr(&body, name);
         bfixstr(&body, "size"); buint16(&body, (uint16_t)size);
      }
      else
      {
         bfixmap(&body, 3);
         bfixstr(&body, "name"); bfixstr(&body, name);
         bfixstr(&body, "crc");  bbin(&body, crc, 4);
         bfixstr(&body, "size"); buint16(&body, (uint16_t)size);
      }
   }
   bbyte(&body, 0xc0);

   bfixmap(&meta, 1);
   bfixstr(&meta, "count");
   buint8(&meta, 1);

   off = 16 + (uint64_t)body.len;
   memcpy(hdr, "RARCHDB", 7);
   hdr[7] = 0;
   for (i = 0; i < 8; i++)
      hdr[8 + i] = (uint8_t)(off >> (56 - 8 * i));

   if (!(f = fopen(path, "wb")))
      return 0;
   fwrite(hdr, 1, sizeof(hdr), f);
   fwrite(body.data, 1, body.len, f);
   fwrite(meta.data, 1, meta.len, f);
   fclose(f);

   free(body.data);
   free(meta.data);
   return 1;
}

/* Name of the first record a lookup returns, or "" for none. */
static void crc_lookup(const database_info_crc_index_t *idx,
      const char *path, uint32_t crc, char *out, size_t len)
{
   database_info_list_t *l = database_info_list_new_crc(idx, path, crc, 0,
         DB_EXTRACT_SCAN_FIELDS);
   out[0] = '\0';
   if (l)
   {
      if (l->count && l->list[0].name)
         strncpy(out, l->list[0].name, len - 1);
      database_info_list_free(l);
      free(l);
   }
   else
      strncpy(out, "(refused)", len - 1);
   out[len - 1] = '\0';
}

static void serial_lookup(const database_info_serial_index_t *idx,
      const char *path, const char *serial, char *out, size_t len)
{
   database_info_list_t *l = database_info_list_new_serial(idx, path,
         serial, DB_EXTRACT_SCAN_FIELDS);
   out[0] = '\0';
   if (l)
   {
      if (l->count && l->list[0].name)
         strncpy(out, l->list[0].name, len - 1);
      database_info_list_free(l);
      free(l);
   }
   else
      strncpy(out, "(refused)", len - 1);
   out[len - 1] = '\0';
}

int main(int argc, char **argv)
{
   const char *dir = (argc > 1) ? argv[1] : "/tmp";
   char  path_a[512], path_b[512];
   char  got[256];
   database_info_crc_index_t    *cia, *cib;
   database_info_serial_index_t *sia, *sib;

   setvbuf(stdout, NULL, _IONBF, 0);
   printf("database index pairing test (dir: %s)\n\n", dir);

   sprintf(path_a, "%s/index_pair_a.rdb", dir);
   sprintf(path_b, "%s/index_pair_b.rdb", dir);

   if (   !write_db(path_a, "Alpha", 0x1000, 40)
       || !write_db(path_b, "Beta",  0x2000, 40))
   {
      check(0, "test databases", "could not be written");
      return 1;
    }

   cia = database_info_crc_index_new(path_a, 0);
   cib = database_info_crc_index_new(path_b, 0);
   sia = database_info_serial_index_new(path_a, 0);
   sib = database_info_serial_index_new(path_b, 0);

   if (!cia || !cib || !sia || !sib)
   {
      check(0, "index construction", "failed");
      return 1;
   }

   check(   database_info_crc_index_count(cia) == 40
         && database_info_crc_index_count(cib) == 40,
         "indexes cover every record", "40 each");

   /* Correct pairing resolves. */
   crc_lookup(cia, path_a, 0x1005, got, sizeof(got));
   check(!strcmp(got, "Alpha 005"), "crc lookup, index matches database",
         got);
   crc_lookup(cib, path_b, 0x2007, got, sizeof(got));
   check(!strcmp(got, "Beta 007"), "crc lookup, other database", got);

   serial_lookup(sia, path_a, "Alpha-012", got, sizeof(got));
   check(!strcmp(got, "Alpha 012"), "serial lookup, index matches database",
         got);

   /* The scanner compares db_info->serial against the serial it read
    * off the disc, so that field has to survive extraction.  Every
    * shipped database stores serial as binary rather than string -
    * 27056 binary and 0 string in Sony - PlayStation 2 - and a type
    * check that accepted only RDT_STRING left it NULL, which silently
    * stopped every disc system matching while crc-based ones kept
    * working. */
   {
      database_info_list_t *l = database_info_list_new_serial(sia, path_a,
            "Alpha-012", DB_EXTRACT_SCAN_FIELDS);
      const char *got_serial = (l && l->count) ? l->list[0].serial : NULL;
      check(got_serial && !strcmp(got_serial, "Alpha-012"),
            "binary serial survives extraction",
            got_serial ? got_serial : "(NULL)");
      if (l) { database_info_list_free(l); free(l); }
   }

   /* Mismatched pairing must be refused, not answered.  Without the
    * check these return the other database's record, which is how the
    * scanner came to file content under the wrong system. */
   crc_lookup(cib, path_a, 0x2007, got, sizeof(got));
   check(!strcmp(got, "(refused)"),
         "crc lookup, index from another database", got);

   serial_lookup(sib, path_a, "Beta-012", got, sizeof(got));
   check(!strcmp(got, "(refused)"),
         "serial lookup, index from another database", got);

   /* A crc that only the other database holds must not resolve even
    * with the pairing correct. */
   crc_lookup(cia, path_a, 0x2007, got, sizeof(got));
   check(got[0] == '\0', "crc absent from this database",
         got[0] ? got : "no match");

   /* A budget too small to hold the table must yield nothing rather
    * than a short one: a partial index misses matches silently, which
    * is the failure the whole fallback design exists to avoid. */
   {
      database_info_crc_index_t *tiny =
         database_info_crc_index_new(path_a, 64);
      check(tiny == NULL, "index refused when over budget",
            tiny ? "built anyway" : "refused");
      database_info_crc_index_free(tiny);
   }
   {
      database_info_crc_index_t *ample =
         database_info_crc_index_new(path_a, 1024 * 1024);
      char tmp[256];
      check(ample != NULL, "index built when within budget",
            ample ? "built" : "refused");
      if (ample)
      {
         crc_lookup(ample, path_a, 0x1005, tmp, sizeof(tmp));
         check(!strcmp(tmp, "Alpha 005"), "budgeted index still complete",
               tmp);
         check(database_info_crc_index_bytes(ample) > 0,
               "index reports its size",
               database_info_crc_index_bytes(ample) ? "non-zero" : "zero");
      }
      database_info_crc_index_free(ample);
   }

   database_info_crc_index_free(cia);
   database_info_crc_index_free(cib);
   database_info_serial_index_free(sia);
   database_info_serial_index_free(sib);

   /* Both of these produced a size range narrower than the data, and a
    * range used to decide which databases to skip is a missed match
    * when it is too narrow rather than a slow scan. */
   {
      char  path[1024];
      int64_t lo = -1, hi = -1;
      database_info_crc_index_t *idx;
      char  detail[128];

      /* 20 records, sizes 100..290 by tens, except: index 7 has size 0,
       * and index 19 - which holds the largest size - has no crc. */
      sprintf(path, "%s/sizes.rdb", dir);
      if (!write_db_sizes(path, 20, 7, 19))
      {
         check(0, "size-range fixture written", "write failed");
      }
      else if (!(idx = database_info_crc_index_new(path, 0)))
      {
         check(0, "size-range index built", "no index");
      }
      else
      {
         int got = database_info_crc_index_size_range(idx, &lo, &hi);

         sprintf(detail, "[%ld, %ld]", (long)lo, (long)hi);
         check(got && hi == 290,
               "size range covers a record with no crc", detail);
         check(got && lo == 0,
               "a zero size does not truncate the range", detail);
         database_info_crc_index_free(idx);
      }
   }

   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
