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

/* Writes @body's records to @path as libretrodb lays a database out:
 * the header, the records, a nil, then the {count} map the header
 * points at.  Frees @body. */
static int write_rdb(const char *path, buf_t *body)
{
   buf_t    meta;
   FILE    *f;
   uint8_t  hdr[16];
   uint64_t off;
   int      i, ok = 0;

   memset(&meta, 0, sizeof(meta));
   bbyte(body, 0xc0);
   bfixmap(&meta, 1);
   bfixstr(&meta, "count");
   buint8(&meta, 1);

   off = 16 + (uint64_t)body->len;
   memcpy(hdr, "RARCHDB", 7);
   hdr[7] = 0;
   for (i = 0; i < 8; i++)
      hdr[8 + i] = (uint8_t)(off >> (56 - 8 * i));

   if ((f = fopen(path, "wb")))
   {
      fwrite(hdr, 1, sizeof(hdr), f);
      fwrite(body->data, 1, body->len, f);
      fwrite(meta.data, 1, meta.len, f);
      fclose(f);
      ok = 1;
   }

   free(body->data);
   free(meta.data);
   return ok;
}

/* One database: @count records named "<tag> NNN", carrying crcs from
 * @crc_base upwards and a serial built from @tag. */
static int write_db(const char *path, const char *tag, uint32_t crc_base,
      int count)
{
   buf_t body;
   int   i;

   memset(&body, 0, sizeof(body));

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
   return write_rdb(path, &body);
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
   buf_t body;
   int   i;

   memset(&body, 0, sizeof(body));

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
   return write_rdb(path, &body);
}

/* Records named for archives: set.zip of 300 bytes, set.7z of 400 and
 * a ROM's name of 500; with @unsized, an arc.zip with no size too. */
static int write_db_archives(const char *path, bool unsized)
{
   static const char *const roms[3] = { "set.zip", "set.7z", "game.bin" };
   buf_t body;
   int   i;

   memset(&body, 0, sizeof(body));
   for (i = 0; i < 3; i++)
   {
      char    name[32];
      uint8_t crc[4];

      crc[0] = 0xA0; crc[1] = 0; crc[2] = 0; crc[3] = (uint8_t)i;
      sprintf(name, "Archive %d", i);
      bfixmap(&body, 4);
      bfixstr(&body, "name");     bfixstr(&body, name);
      bfixstr(&body, "rom_name"); bfixstr(&body, roms[i]);
      bfixstr(&body, "crc");      bbin(&body, crc, 4);
      bfixstr(&body, "size");     buint16(&body, (uint16_t)(300 + 100 * i));
   }
   if (unsized)
   {
      uint8_t crc[4];

      crc[0] = 0xA0; crc[1] = 0; crc[2] = 0; crc[3] = 9;
      bfixmap(&body, 3);
      bfixstr(&body, "name");     bfixstr(&body, "Unsized");
      bfixstr(&body, "rom_name"); bfixstr(&body, "arc.zip");
      bfixstr(&body, "crc");      bbin(&body, crc, 4);
   }
   return write_rdb(path, &body);
}

/* @count records, each named for a zip of its own, sizes from 1000 up. */
static int write_db_zips(const char *path, int count)
{
   buf_t body;
   int   i;

   memset(&body, 0, sizeof(body));
   for (i = 0; i < count; i++)
   {
      char    name[16], rom[16];
      uint8_t crc[4];

      crc[0] = 0xB0; crc[1] = 0;
      crc[2] = (uint8_t)(i >> 8); crc[3] = (uint8_t)i;
      sprintf(name, "Zip %04d", i);
      sprintf(rom,  "z%04d.zip", i);
      bfixmap(&body, 4);
      bfixstr(&body, "name");     bfixstr(&body, name);
      bfixstr(&body, "rom_name"); bfixstr(&body, rom);
      bfixstr(&body, "crc");      bbin(&body, crc, 4);
      bfixstr(&body, "size");     buint16(&body, (uint16_t)(1000 + i));
   }
   return write_rdb(path, &body);
}

/* The crc of record @i of write_db_many(): scattered, so that the
 * index has to sort them, and every hundredth repeats the one before
 * it. */
static uint32_t many_crc(int i)
{
   if (i % 100 == 99)
      i--;
   return (uint32_t)i * 2654435761u;
}

/* @count records named "Many NNNN", with many_crc()'s crcs, sizes from
 * 1000 up, and a serial "Many-NNNN" on the even ones. */
static int write_db_many(const char *path, int count)
{
   buf_t body;
   int   i;

   memset(&body, 0, sizeof(body));

   for (i = 0; i < count; i++)
   {
      char     name[16], serial[16];
      uint8_t  crc[4];
      uint32_t c = many_crc(i);

      sprintf(name,   "Many %04d", i);
      sprintf(serial, "Many-%04d", i);
      crc[0] = (uint8_t)(c >> 24); crc[1] = (uint8_t)(c >> 16);
      crc[2] = (uint8_t)(c >> 8);  crc[3] = (uint8_t)c;

      bfixmap(&body, (i % 2) ? 3 : 4);
      bfixstr(&body, "name"); bfixstr(&body, name);
      bfixstr(&body, "crc");  bbin(&body, crc, 4);
      if (!(i % 2))
      {
         bfixstr(&body, "serial");
         bbin(&body, serial, (uint8_t)strlen(serial));
      }
      bfixstr(&body, "size"); buint16(&body, (uint16_t)(1000 + i));
   }
   return write_rdb(path, &body);
}

/* Name of the first record a lookup returns, or "" for none. */
static void crc_lookup(database_info_crc_index_t *idx,
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

static void serial_lookup(database_info_serial_index_t *idx,
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

/* A job stepped until it is done: the one-shot build the stepped jobs
 * below are held to. */
static void index_new(const char *path, size_t max_bytes,
      database_info_crc_index_t **crc,
      database_info_serial_index_t **serial, bool *has_serial)
{
   database_info_index_job_t *job;
   unsigned                   want = 0;

   if (crc)
      want |= DATABASE_INFO_INDEX_CRC;
   if (serial)
      want |= DATABASE_INFO_INDEX_SERIAL;
   if (has_serial)
      want |= DATABASE_INFO_INDEX_HAS_SERIAL;
   job = database_info_index_job_new(path, max_bytes, want);
   while (job && !database_info_index_job_step(job, 4096, 65536))
      ;
   database_info_index_job_finish(job, crc, serial, has_serial);
}

static database_info_crc_index_t *crc_index_new(const char *path,
      size_t max_bytes)
{
   database_info_crc_index_t *idx;
   index_new(path, max_bytes, &idx, NULL, NULL);
   return idx;
}

static database_info_serial_index_t *serial_index_new(const char *path,
      size_t max_bytes)
{
   database_info_serial_index_t *idx;
   index_new(path, max_bytes, NULL, &idx, NULL);
   return idx;
}

static database_info_crc_index_t *crc_index_want(const char *path,
      size_t max_bytes, unsigned want)
{
   database_info_crc_index_t *idx = NULL;
   database_info_index_job_t *job = database_info_index_job_new(path,
         max_bytes, DATABASE_INFO_INDEX_CRC | want);

   while (job && !database_info_index_job_step(job, 4096, 65536))
      ;
   database_info_index_job_finish(job, &idx, NULL, NULL);
   return idx;
}

/* Builds @path's indexes in one walk under @max and as two builds, the
 * crc index first and the serial index on what it leaves.  False when
 * the two disagree on which come back; *@crc_only says the one walk
 * held the crc index alone. */
static int split_agrees(const char *path, size_t max, int *crc_only)
{
   database_info_crc_index_t    *ci = NULL, *ci1;
   database_info_serial_index_t *si = NULL, *si1;
   size_t left = max;
   int    agree;

   ci1 = crc_index_new(path, max);
   if (ci1)
      left = (database_info_crc_index_bytes(ci1) < max)
         ? max - database_info_crc_index_bytes(ci1) : 0;
   si1 = left ? serial_index_new(path, left) : NULL;
   index_new(path, max, &ci, &si, NULL);
   agree     = (!ci == !ci1) && (!si == !si1);
   *crc_only = ci && !si;
   database_info_crc_index_free(ci);
   database_info_serial_index_free(si);
   database_info_crc_index_free(ci1);
   database_info_serial_index_free(si1);
   return agree;
}

/* index_new() builds in one walk what the two builders
 * build in two, so its indexes have to answer as theirs do, and a
 * budget has to be spent as when the crc index was built first and
 * the serial index got the rest. */
static void check_one_walk(const char *dir, const char *path_a)
{
   database_info_crc_index_t    *ci = NULL, *ci1;
   database_info_serial_index_t *si = NULL, *si1;
   bool   has_serial = false;
   int    i, same    = 1;
   char   a[256], b[256];
   char  *path       = (char*)malloc(1024);

   if (!path)
   {
      check(0, "one walk", "out of memory");
      return;
   }

   index_new(path_a, 0, &ci, &si, &has_serial);
   ci1 = crc_index_new(path_a, 0);
   si1 = serial_index_new(path_a, 0);
   check(   ci && si && ci1 && si1 && has_serial
         && database_info_crc_index_count(ci)
            == database_info_crc_index_count(ci1)
         && database_info_serial_index_count(si)
            == database_info_serial_index_count(si1),
         "one walk builds both indexes", "crc and serial, 40 each");

   for (i = 0; ci && si && ci1 && si1 && i < 40; i++)
   {
      char serial[32];

      crc_lookup(ci,  path_a, 0x1000 + (uint32_t)i, a, sizeof(a));
      crc_lookup(ci1, path_a, 0x1000 + (uint32_t)i, b, sizeof(b));
      if (!a[0] || strcmp(a, b))
         same = 0;
      sprintf(serial, "Alpha-%03d", i);
      serial_lookup(si,  path_a, serial, a, sizeof(a));
      serial_lookup(si1, path_a, serial, b, sizeof(b));
      if (!a[0] || strcmp(a, b))
         same = 0;
   }
   {
      int64_t lo = 0, hi = 0, lo1 = 1, hi1 = 1;

      if (   !database_info_crc_index_size_range(ci,  &lo,  &hi)
          || !database_info_crc_index_size_range(ci1, &lo1, &hi1)
          || lo != lo1 || hi != hi1)
         same = 0;
   }
   check(same, "one walk answers as the two builds",
         "every crc, every serial, the size range");
   database_info_crc_index_free(ci);
   database_info_serial_index_free(si);
   database_info_crc_index_free(ci1);
   database_info_serial_index_free(si1);

   {
      size_t   max, edge;
      unsigned tight = 0;
      int      agree = 1, crc_only;
      char     detail[64];

      for (max = 1024; max <= 32768; max += 256)
      {
         if (!split_agrees(path_a, max, &crc_only))
            agree = 0;
         if (crc_only)
            tight++;
      }

      /* The serial build's first table, 1024 entries of 8 bytes, fits
       * what the crc index leaves of @edge and not a byte less. */
      ci1  = crc_index_new(path_a, 0);
      edge = database_info_crc_index_bytes(ci1) + 8192;
      database_info_crc_index_free(ci1);
      if (   !split_agrees(path_a, edge - 1, &crc_only) || !crc_only
          || !split_agrees(path_a, edge, &crc_only)     ||  crc_only)
         agree = 0;
      sprintf(detail, "%u budgets hold the crc index only", tight);
      check(agree && tight, "budget split as the two builds split it",
            detail);
   }

   /* The crc lookup asks only whether serials are there. */
   sprintf(path, "%s/sizes.rdb", dir);
   index_new(path_a, 0, &ci, NULL, &has_serial);
   i = ci && has_serial;
   database_info_crc_index_free(ci);
   index_new(path, 0, &ci, NULL, &has_serial);
   check(i && ci && !has_serial, "serials seen without a serial index",
         "Alpha has them, sizes.rdb not");
   database_info_crc_index_free(ci);

   index_new(path, 0, &ci, &si, &has_serial);
   check(   ci && si && !has_serial
         && database_info_serial_index_count(si) == 0,
         "no serials: an empty serial index", "sizes.rdb");
   database_info_crc_index_free(ci);
   database_info_serial_index_free(si);
   free(path);
}

/* Records in the index job's database: hundreds of steps at the
 * sizes below, and a dozen merge passes. */
#define JOB_RECORDS 3000

/* database_info_index_job_*() spreads a build over calls of a few
 * records or entries each.  Its indexes have to answer every crc and
 * serial of a database whose crcs come in no order, as index_new()'s
 * do, and a job freed part way must not leak. */
static void check_index_job(const char *dir)
{
   static const unsigned stop_at[] = { 100, 3000, 9000 };
   database_info_crc_index_t    *ci  = NULL, *ci1 = NULL;
   database_info_serial_index_t *si  = NULL, *si1 = NULL;
   database_info_index_job_t    *job;
   bool     has_serial = false;
   unsigned steps      = 0;
   int      i, ok      = 1;
   char     want[32], got[64], detail[64];
   char    *path       = (char*)malloc(1024);

   if (!path)
   {
      check(0, "index job", "out of memory");
      return;
   }
   sprintf(path, "%s/index_job.rdb", dir);
   if (!write_db_many(path, JOB_RECORDS))
   {
      check(0, "index job", "could not write database");
      free(path);
      return;
   }

   job = database_info_index_job_new(path, 0,
         DATABASE_INFO_INDEX_CRC | DATABASE_INFO_INDEX_SERIAL);
   while (job && !database_info_index_job_step(job, 7, 5))
      steps++;
   database_info_index_job_finish(job, &ci, &si, &has_serial);
   index_new(path, 0, &ci1, &si1, NULL);
   printf("  info  index job steps for %d records: %u\n", JOB_RECORDS,
         steps);
   sprintf(detail, "%u steps of 7 records or 5 entries", steps);
   check(   ci && si && has_serial
         && steps > JOB_RECORDS / 7
         && database_info_crc_index_count(ci) == JOB_RECORDS
         && database_info_serial_index_count(si) == JOB_RECORDS / 2,
         "index job built in steps", detail);

   for (i = 0; ci && si && ci1 && si1 && i < JOB_RECORDS; i++)
   {
      char serial[16];

      sprintf(want, "Many %04d", (i % 100 == 99) ? i - 1 : i);
      crc_lookup(ci,  path, many_crc(i), got, sizeof(got));
      if (strcmp(got, want))
         ok = 0;
      crc_lookup(ci1, path, many_crc(i), got, sizeof(got));
      if (strcmp(got, want))
         ok = 0;
      if (i % 2)
         continue;
      sprintf(want,   "Many %04d", i);
      sprintf(serial, "Many-%04d", i);
      serial_lookup(si,  path, serial, got, sizeof(got));
      if (strcmp(got, want))
         ok = 0;
      serial_lookup(si1, path, serial, got, sizeof(got));
      if (strcmp(got, want))
         ok = 0;
   }
   {
      int64_t lo = 0, hi = 0, lo1 = 1, hi1 = 1;

      if (   !ci || !ci1
          || !database_info_crc_index_size_range(ci,  &lo,  &hi)
          || !database_info_crc_index_size_range(ci1, &lo1, &hi1)
          || lo != 1000 || hi != 1000 + JOB_RECORDS - 1
          || lo != lo1  || hi != hi1)
         ok = 0;
   }
   check(ok && si1, "index job answers every crc and serial",
         "stepped and one-shot, and the size range");
   database_info_crc_index_free(ci);
   database_info_serial_index_free(si);
   database_info_crc_index_free(ci1);
   database_info_serial_index_free(si1);

   /* Freed in the walk, the crc sort and the serial sort; a leak
    * shows under ASan. */
   for (i = 0; i < 3; i++)
   {
      job = database_info_index_job_new(path, 0,
            DATABASE_INFO_INDEX_CRC | DATABASE_INFO_INDEX_SERIAL);
      for (steps = 0; job && steps < stop_at[i]; steps++)
         if (database_info_index_job_step(job, 7, 5))
            break;
      ok = (job != NULL) && steps == stop_at[i];
      database_info_index_job_free(job);
      if (!ok)
         break;
   }
   check(ok, "index job freed part way", "in the walk and in both sorts");
   free(path);
}

/* A run of records that ends on the last one still finishes the walk,
 * and a record that cannot be read fails the job. */
static void check_index_job_edges(const char *dir)
{
   database_info_crc_index_t    *ci = NULL;
   database_info_serial_index_t *si = NULL;
   database_info_index_job_t    *job;
   bool     has_serial = false;
   unsigned steps      = 0;
   long     len, i;
   int      ok         = 0;
   int      made;
   char     detail[64];
   char    *path       = (char*)malloc(1024);
   uint8_t *data       = NULL;
   FILE    *f;

   if (!path)
   {
      check(0, "index job edges", "out of memory");
      return;
   }
   sprintf(path, "%s/index_edges.rdb", dir);
   if (!write_db_many(path, JOB_RECORDS))
   {
      check(0, "index job edges", "could not write database");
      free(path);
      return;
   }

   job = database_info_index_job_new(path, 0,
         DATABASE_INFO_INDEX_CRC | DATABASE_INFO_INDEX_SERIAL);
   while (job && !database_info_index_job_step(job, 5, 4096))
      steps++;
   database_info_index_job_finish(job, &ci, &si, &has_serial);
   sprintf(detail, "%u steps of 5 records", steps);
   check(   ci && si && has_serial
         && database_info_crc_index_count(ci) == JOB_RECORDS
         && database_info_serial_index_count(si) == JOB_RECORDS / 2,
         "index job whose last run ends on the last record", detail);
   database_info_crc_index_free(ci);
   database_info_serial_index_free(si);
   ci = NULL;
   si = NULL;

   /* An unused msgpack type where record 1500's map begins. */
   if (   (f = fopen(path, "r+b"))
       && !fseek(f, 0, SEEK_END)
       && (len = ftell(f)) > 0
       && (data = (uint8_t*)malloc((size_t)len))
       && !fseek(f, 0, SEEK_SET)
       && fread(data, 1, (size_t)len, f) == (size_t)len)
   {
      for (i = 7; i + 9 <= len; i++)
         if (!memcmp(data + i, "Many 1500", 9))
            break;
      if (   i + 9 <= len
          && !fseek(f, i - 7, SEEK_SET)
          && fputc(0xc1, f) != EOF)
         ok = 1;
   }
   if (f)
      fclose(f);
   free(data);
   if (!ok)
   {
      check(0, "index job edges", "could not damage the database");
      free(path);
      return;
   }
   job = database_info_index_job_new(path, 0,
         DATABASE_INFO_INDEX_CRC | DATABASE_INFO_INDEX_SERIAL);
   made = (job != NULL);
   for (steps = 0; job && steps < JOB_RECORDS; steps++)
      if (database_info_index_job_step(job, 7, 5))
         break;
   database_info_index_job_finish(job, &ci, &si, &has_serial);
   check(   made && steps < JOB_RECORDS && !ci && !si,
         "index job fails at a record it cannot read", "no index");
   database_info_crc_index_free(ci);
   database_info_serial_index_free(si);
   free(path);
}

/* The sizes of the records named for an archive, beside the crc index:
 * an archive of a type and size no such record has is not held. */
static void check_archive_sizes(const char *dir)
{
   char path[512], path_u[512], detail[64];
   database_info_crc_index_t *idx;
   int  i;

   sprintf(path,   "%s/index_archives.rdb",         dir);
   sprintf(path_u, "%s/index_archives_unsized.rdb", dir);
   if (!write_db_archives(path, false) || !write_db_archives(path_u, true))
   {
      check(0, "archive size databases", "could not be written");
      return;
   }

   if (!(idx = crc_index_want(path, 0, DATABASE_INFO_INDEX_ARCHIVE_SIZES)))
      check(0, "archive size index built", "no index");
   else
   {
      check(database_info_crc_index_may_hold_archive(idx, "zip", 300),
            "archive sizes: a zip record's size is held", "set.zip 300");
      check(database_info_crc_index_may_hold_archive(idx, "ZIP", 300),
            "archive sizes: the type in any case", "ZIP 300");
      check(!database_info_crc_index_may_hold_archive(idx, "zip", 301),
            "archive sizes: another size is not", "zip 301");
      check(!database_info_crc_index_may_hold_archive(idx, "zip", 400),
            "archive sizes: another type's size is not", "zip 400");
      check(database_info_crc_index_may_hold_archive(idx, "7z", 400),
            "archive sizes: 7z kept apart", "set.7z 400");
      check(!database_info_crc_index_may_hold_archive(idx, "zip", 500),
            "archive sizes: a ROM's name is no archive", "game.bin 500");
      check(database_info_crc_index_may_hold_archive(idx, "chd", 12345),
            "archive sizes: a type not kept may be held", "chd");
      check(database_info_crc_index_may_hold_archive(idx, "zip", 0),
            "archive sizes: an unknown size may be held", "zip 0");
      database_info_crc_index_free(idx);
   }

   if (!(idx = crc_index_want(path, 0, 0)))
      check(0, "index without archive sizes built", "no index");
   else
   {
      check(database_info_crc_index_may_hold_archive(idx, "zip", 301),
            "archive sizes: an index without them may hold any",
            "zip 301");
      database_info_crc_index_free(idx);
   }

   if (!(idx = crc_index_want(path_u, 0, DATABASE_INFO_INDEX_ARCHIVE_SIZES)))
      check(0, "unsized archive index built", "no index");
   else
   {
      check(database_info_crc_index_may_hold_archive(idx, "zip", 999),
            "archive sizes: an unsized zip record may hold any zip",
            "arc.zip");
      check(!database_info_crc_index_may_hold_archive(idx, "7z", 999),
            "archive sizes: only for its own type", "7z 999");
      database_info_crc_index_free(idx);
   }

   check(database_info_crc_index_may_hold_archive(NULL, "zip", 1),
         "archive sizes: no index may hold any", "NULL");

   /* 1024 records fill the crc table, so a budget of the index without
    * sizes holds the table and the sizes' build, but not both kept. */
   sprintf(path_u, "%s/index_archives_many.rdb", dir);
   if (!write_db_zips(path_u, 1024))
   {
      check(0, "archive size budget database", "could not be written");
      return;
   }
   for (i = 0; i < 2; i++)
   {
      database_info_crc_index_t *full = crc_index_want(path_u, 0,
            i ? DATABASE_INFO_INDEX_ARCHIVE_SIZES : 0);
      size_t max = database_info_crc_index_bytes(full);

      database_info_crc_index_free(full);
      idx = crc_index_want(path_u, max, DATABASE_INFO_INDEX_ARCHIVE_SIZES);
      sprintf(detail, "zip 999 under %u bytes", (unsigned)max);
      if (i)
         check(idx && !database_info_crc_index_may_hold_archive(idx, "zip",
                  999), "archive sizes: kept within the budget", detail);
      else
         check(idx && database_info_crc_index_may_hold_archive(idx, "zip",
                  999), "archive sizes: dropped over the budget", detail);
      database_info_crc_index_free(idx);
   }
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

   cia = crc_index_new(path_a, 0);
   cib = crc_index_new(path_b, 0);
   sia = serial_index_new(path_a, 0);
   sib = serial_index_new(path_b, 0);

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
         crc_index_new(path_a, 64);
      check(tiny == NULL, "index refused when over budget",
            tiny ? "built anyway" : "refused");
      database_info_crc_index_free(tiny);
   }
   {
      database_info_crc_index_t *ample =
         crc_index_new(path_a, 1024 * 1024);
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
      else if (!(idx = crc_index_new(path, 0)))
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

   check_one_walk(dir, path_a);
   check_index_job(dir);
   check_index_job_edges(dir);
   check_archive_sizes(dir);

   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
