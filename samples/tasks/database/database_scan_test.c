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
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <retro_timers.h>
#include <streams/file_stream.h>
#include <compat/strl.h>

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
 * exercises task_push_dbscan, and msg_hash_to_str_us() is the one stub
 * a check depends on: it names the invalid content message.
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
   if (msg == MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT)
      return "invalid content";
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

static bool invalid_content_pushed = false;

/* Additional stubs for retroarch-core symbols referenced transitively,
 * to avoid pulling in retroarch.c, runloop.c, frontend drivers, and the
 * UI/video subsystems.  runloop_msg_queue_push() also records the
 * invalid content message. */
void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration,
      bool flush, char *title, unsigned icon, unsigned category)
{
   (void)len; (void)prio; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
   if (msg && !strcmp(msg, "invalid content"))
      invalid_content_pushed = true;
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

static void bbin(buf_t *b, const char *s)
{
   size_t n = strlen(s);
   bbyte(b, 0xc4);
   bbyte(b, (uint8_t)n);
   bput(b, s, n);
}

/* A database record.  Real databases store the serial as binary. */
typedef struct
{
   const char *name;
   const char *serial;
   uint32_t    crc;
   uint32_t    size;
} db_rec_t;

/* One database holding @n records; a NULL serial leaves the key out. */
static int write_db_set(const char *path, const db_rec_t *recs,
      unsigned n)
{
   buf_t    body, meta;
   FILE    *f;
   uint8_t  hdr[16];
   uint64_t off;
   unsigned r;
   int      i;

   memset(&body, 0, sizeof(body));
   memset(&meta, 0, sizeof(meta));

   for (r = 0; r < n; r++)
   {
      bfixmap(&body, recs[r].serial ? 4 : 3);
      bfixstr(&body, "name"); bfixstr(&body, recs[r].name);
      if (recs[r].serial)
      {
         bfixstr(&body, "serial");
         bbin(&body, recs[r].serial);
      }
      bfixstr(&body, "crc");  bbin4(&body, recs[r].crc);
      bfixstr(&body, "size"); buint32(&body, recs[r].size);
   }
   bbyte(&body, 0xc0);

   bfixmap(&meta, 1);
   bfixstr(&meta, "count");
   bbyte(&meta, (uint8_t)n);

   off = 16 + (uint64_t)body.len;
   memcpy(hdr, "RARCHDB", 7);
   hdr[7] = 0;
   for (i = 0; i < 8; i++)
      hdr[8 + i] = (uint8_t)(off >> (56 - 8 * i));

   if (!(f = fopen(path, "wb")))
   {
      free(body.d);
      free(meta.d);
      return 0;
   }
   fwrite(hdr, 1, sizeof(hdr), f);
   fwrite(body.d, 1, body.len, f);
   fwrite(meta.d, 1, meta.len, f);
   fclose(f);
   free(body.d);
   free(meta.d);
   return 1;
}

/* One database holding a single named record. */
static int write_db(const char *path, const char *game, uint32_t crc,
      uint32_t size)
{
   db_rec_t rec;
   rec.name   = game;
   rec.serial = NULL;
   rec.crc    = crc;
   rec.size   = size;
   return write_db_set(path, &rec, 1);
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

/* Brings @d's crc32 to @crc through its last four bytes, then writes
 * it out. */
static int write_forced(const char *path, uint8_t *d, uint32_t size,
      uint32_t crc)
{
   FILE *f;

   crc_force(d, size - 4, crc, d + size - 4);
   if (crc_of(d, size) != crc || !(f = fopen(path, "wb")))
      return 0;
   fwrite(d, 1, size, f);
   fclose(f);
   return 1;
}

#define PS1_SECTOR     2352
#define PS1_TRACK_SIZE (26 * PS1_SECTOR)
/* Shorter than the 60000 bytes detect_ps1_game() reads. */
#define PS1_SHORT_SIZE (24 * PS1_SECTOR)

/* A raw MODE2/2352 PS1 data track of @size bytes: the license string
 * detect_system() looks for, and @n strings at the offsets in @at. */
static int write_ps1_track_sized(const char *path, uint32_t size,
      const char *const *str, const unsigned *at, unsigned n, uint32_t crc)
{
   uint8_t *d = (uint8_t*)calloc(1, size);
   unsigned i;
   int      ok;

   if (!d)
      return 0;
   memcpy(d + 0x24f8, "Sony Computer ", 14);
   for (i = 0; i < n; i++)
      memcpy(d + at[i], str[i], strlen(str[i]));
   ok = write_forced(path, d, size, crc);
   free(d);
   return ok;
}

/* A full track: the forced crc lands past the bytes
 * detect_ps1_game() reads. */
static int write_ps1_track(const char *path, const char *const *str,
      const unsigned *at, unsigned n, uint32_t crc)
{
   return write_ps1_track_sized(path, PS1_TRACK_SIZE, str, at, n, crc);
}

static int write_cue(const char *path, const char *track)
{
   FILE *f = fopen(path, "w");

   if (!f)
      return 0;
   fprintf(f,
         "FILE \"%s\" BINARY\n"
         "  TRACK 01 MODE2/2352\n"
         "    INDEX 01 00:00:00\n", track);
   fclose(f);
   return 1;
}

#define CTR_IMAGE_SIZE 0x8000
/* Room for an NCCH at 1 << 25, where flags[6] = 16 would put one. */
#define CTR_BIG_SIZE   0x2000200
/* Ends 0x180 bytes into the NCCH header at 0x4000. */
#define CTR_CUT_SIZE   0x4180

static void put_le32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* A 3DS cart image (NCSD) whose partition 0 starts @part0 media units
 * in, or a bare NCCH when @part0 is 0, with @code as its product code.
 * A zero @ncch_magic leaves the partition without its magic. */
static int write_ctr(const char *path, uint32_t part0, int ncch_magic,
      const char *code, uint32_t crc)
{
   uint8_t *d = (uint8_t*)calloc(1, CTR_IMAGE_SIZE);
   uint8_t *ncch;
   int      ok;

   if (!d)
      return 0;
   ncch = d + part0 * 0x200;
   if (part0)
   {
      memcpy(d + 0x100, "NCSD", 4);
      put_le32(d + 0x104, CTR_IMAGE_SIZE / 0x200);
      put_le32(d + 0x120, part0);
      put_le32(d + 0x124, CTR_IMAGE_SIZE / 0x200 - part0);
   }
   if (ncch_magic)
      memcpy(ncch + 0x100, "NCCH", 4);
   memcpy(ncch + 0x150, code, strlen(code));
   ok = write_forced(path, d, CTR_IMAGE_SIZE, crc);
   free(d);
   return ok;
}

/* A cart image @size bytes long whose table puts partition 0 at @part0
 * units of 0x200 << @shift, with an NCCH header at @ncch_at (where it
 * fits) holding @len bytes of @code. */
static int write_ctr_ncsd(const char *path, uint32_t size, uint8_t shift,
      uint32_t part0, uint32_t ncch_at, const char *code, size_t len,
      uint32_t crc)
{
   uint8_t *d = (uint8_t*)calloc(1, size);
   int      ok;

   if (!d)
      return 0;
   memcpy(d + 0x100, "NCSD", 4);
   put_le32(d + 0x104, size / 0x200);
   d[0x18E] = shift;
   put_le32(d + 0x120, part0);
   if (ncch_at + 0x104 <= size)
      memcpy(d + ncch_at + 0x100, "NCCH", 4);
   if (ncch_at + 0x150 + len <= size)
      memcpy(d + ncch_at + 0x150, code, len);
   ok = write_forced(path, d, size, crc);
   free(d);
   return ok;
}

/* A CIA: its header size word, and neither NCSD nor NCCH magic. */
static int write_cia(const char *path, uint32_t crc)
{
   uint8_t *d = (uint8_t*)calloc(1, CTR_IMAGE_SIZE);
   int      ok;

   if (!d)
      return 0;
   put_le32(d, 0x2020);
   ok = write_forced(path, d, CTR_IMAGE_SIZE, crc);
   free(d);
   return ok;
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

/* Returns false if the scan could not be started; otherwise waits for
 * it, and scan_completed says whether it finished in time. */
static bool run_scan(const char *dir)
{
   time_t started;

   loop_active    = true;
   scan_completed = false;
   if (!task_push_dbscan(dir, scan_cb))
      return false;

   started = time(NULL);
   while (loop_active)
   {
      task_queue_check();
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         break;
      retro_sleep(1);
   }
   return true;
}

/* As run_scan(), but counts the checks the scan took.  Meaningful on
 * the regular queue, where every check runs the handler once. */
static unsigned run_scan_counted(const char *dir)
{
   unsigned gathers = 0;
   time_t   started;

   loop_active    = true;
   scan_completed = false;
   if (!task_push_dbscan(dir, scan_cb))
      return 0;

   started = time(NULL);
   while (loop_active)
   {
      task_queue_check();
      gathers++;
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         break;
   }
   return gathers;
}

/* Databases no core claims, and files that match nothing, in the
 * lane below. */
#define UNCLAIMED_DBS   48
#define UNMATCHED_FILES 4

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
   /* Claimed by a second core that supports only ".gam", so the
    * databases' claims differ and have to stay paired with their
    * database when a match moves it to the front of the list. */
   const uint32_t crc_g = 0x6A3A6A3Au;
   const uint32_t sz_g  = 3072;

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
   sprintf(p, "%s/Test Gamma.rdb", db_dir);
   if (!write_db(p, "Gamma The Game", crc_g, sz_g))
   { check(0, "fixture", "could not write database"); return 1; }

   /* A ".gam" file carrying the Gamma record, and one carrying the
    * Alpha record: no core claiming Test Alpha supports ".gam", so
    * the second must not land anywhere. */
   sprintf(p, "%s/05_gamma.gam", in_dir);
   if (!write_content(p, crc_g, sz_g))
   { check(0, "fixture", "crc forcing failed"); return 1; }
   sprintf(p, "%s/06_cross.gam", in_dir);
   if (!write_content(p, crc_a, sz_a))
   { check(0, "fixture", "crc forcing failed"); return 1; }

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
    * see core_info_database_claim().  Without the
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
            "supported_extensions = \"bin|cue|gdi|zip\"\n"
            "database = \"Test Alpha|Test Beta|Test Disc|Test Zip\"\n");
      fclose(f);
   }
   sprintf(p, "%s/test_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/gamma_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Gamma Test\"\n"
            "corename = \"GammaTest\"\n"
            "supported_extensions = \"gam\"\n"
            "database = \"Test Gamma\"\n");
      fclose(f);
   }
   sprintf(p, "%s/gamma_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/ps1_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"PS1 Test\"\n"
            "corename = \"PS1Test\"\n"
            "supported_extensions = \"cue\"\n"
            "database = \"Test PS1\"\n");
      fclose(f);
   }
   sprintf(p, "%s/ps1_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/ctr_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"3DS Test\"\n"
            "corename = \"CtrTest\"\n"
            "supported_extensions = \"3ds|cci|cxi\"\n"
            "database = \"Test 3DS|Test 3DS Strict\"\n");
      fclose(f);
   }
   sprintf(p, "%s/ctr_libretro.so", core_dir);
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

   if (!run_scan(in_dir))
   {
      check(0, "scan started", "task_push_dbscan refused");
      goto done;
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

   /* Test Gamma is visited after four databases have been moved to
    * the front of the list; its claim has to have travelled with it. */
   sprintf(p, "%s/Test Gamma.lpl", pl_dir);
   check(file_contains(p, "05_gamma.gam"),
         "database claimed by a second core matched", "05_gamma.gam");
   sprintf(p, "%s/Test Alpha.lpl", pl_dir);
   check(!file_contains(p, "06_cross.gam"),
         "extension of a core not claiming the database refused",
         "no 06_cross.gam in Alpha");

   /* PS1 discs match on the serial their data track carries.  A root
    * directory listing PSX.EXE ahead of the executable the disc boots
    * must not end the search.  A disc that boots PSX.EXE, as the
    * SYSTEM.CNF BOOT target or for want of a SYSTEM.CNF, has no serial
    * whatever else the track holds, so the crc decides. */
   {
      static const db_rec_t ps1_recs[] = {
         { "Serial The Game",   "SLUS-00152", 0x5E1A0152u, PS1_TRACK_SIZE },
         { "Short Track Game",  "SLUS-00777", 0x5E1A0777u, PS1_SHORT_SIZE },
         { "PSX Boot The Game", NULL,         0x0B0071E5u, PS1_TRACK_SIZE },
         { "Decoy Boot Game",   "SLUS-00998", 0xDEC0DEC3u, PS1_TRACK_SIZE },
         { "PSX Boot Two",      NULL,         0x0B0071E6u, PS1_TRACK_SIZE },
         { "Decoy Serial Game", "SLUS-00999", 0xDEC0DEC0u, PS1_TRACK_SIZE },
         { "PSX No CNF Game",   NULL,         0x0B0071E7u, PS1_TRACK_SIZE },
         { "Decoy No CNF Game", "SLUS-01010", 0xDEC0DEC1u, PS1_TRACK_SIZE },
         { "PSX Lower Game",    NULL,         0x0B0071E8u, PS1_TRACK_SIZE },
         { "Decoy Lower Game",  "SLUS-01111", 0xDEC0DEC2u, PS1_TRACK_SIZE }
      };
      /* Where Tomb Raider (USA) has them: the root directory in sector
       * 22, SYSTEM.CNF in sector 23. */
      static const char *const serial_str[] = {
         "PSX.EXE;1", "SLUS_001.52;1", "SYSTEM.CNF;1",
         "BOOT = cdrom:\\SLUS_001.52;1"
      };
      static const unsigned serial_at[] = { 52407, 52517, 52577, 54120 };
      /* The same, on a track that ends inside the probe window. */
      static const char *const short_str[] = {
         "PSX.EXE;1", "SLUS_007.77;1", "SYSTEM.CNF;1",
         "BOOT = cdrom:\\SLUS_007.77;1"
      };
      /* No backslash after "cdrom:"; a serial-like record listed. */
      static const char *const boot_str[] = {
         "PSX.EXE;1", "SLUS_009.98;1", "SYSTEM.CNF;1",
         "BOOT=cdrom:PSX.EXE;1"
      };
      static const unsigned boot_at[]   = { 52407, 52517, 52577, 54120 };
      /* A serial-like record listed between PSX.EXE and SYSTEM.CNF. */
      static const char *const decoy_str[] = {
         "PSX.EXE;1", "SLUS_009.99;1", "SYSTEM.CNF;1",
         "BOOT = cdrom:\\PSX.EXE;1"
      };
      static const unsigned decoy_at[]  = { 52407, 52517, 52577, 54120 };
      /* No SYSTEM.CNF, so the BIOS boots PSX.EXE. */
      static const char *const nocnf_str[] = {
         "PSX.EXE;1", "SLUS_010.10;1"
      };
      static const unsigned nocnf_at[]  = { 52407, 52517 };
      static const char *const lower_str[] = {
         "PSX.EXE;1", "SLUS_011.11;1", "SYSTEM.CNF;1",
         "BOOT = cdrom:\\psx.exe;1"
      };
      static const unsigned lower_at[]  = { 52407, 52517, 52577, 54120 };
      char ps1_db[512], ps1_in[512];

      sprintf(ps1_db, "%s/scan_db_ps1", root);
      sprintf(ps1_in, "%s/scan_ps1",    root);
      path_mkdir(ps1_db);
      path_mkdir(ps1_in);

      sprintf(p, "%s/Test PS1.rdb", ps1_db);
      if (!write_db_set(p, ps1_recs, ARRAY_SIZE(ps1_recs)))
      { check(0, "fixture", "could not write database"); goto done; }
      sprintf(p, "%s/07_serial.bin", ps1_in);
      if (!write_ps1_track(p, serial_str, serial_at,
            ARRAY_SIZE(serial_str), 0x07070707u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/07_serial.cue", ps1_in);
      if (!write_cue(p, "07_serial.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/12_ps1_short.bin", ps1_in);
      if (!write_ps1_track_sized(p, PS1_SHORT_SIZE, short_str, serial_at,
            ARRAY_SIZE(short_str), 0x12121212u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/12_ps1_short.cue", ps1_in);
      if (!write_cue(p, "12_ps1_short.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/08_psx_boot.bin", ps1_in);
      if (!write_ps1_track(p, boot_str, boot_at,
            ARRAY_SIZE(boot_str), 0x0B0071E5u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/08_psx_boot.cue", ps1_in);
      if (!write_cue(p, "08_psx_boot.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/09_psx_decoy.bin", ps1_in);
      if (!write_ps1_track(p, decoy_str, decoy_at,
            ARRAY_SIZE(decoy_str), 0x0B0071E6u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/09_psx_decoy.cue", ps1_in);
      if (!write_cue(p, "09_psx_decoy.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/10_psx_nocnf.bin", ps1_in);
      if (!write_ps1_track(p, nocnf_str, nocnf_at,
            ARRAY_SIZE(nocnf_str), 0x0B0071E7u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/10_psx_nocnf.cue", ps1_in);
      if (!write_cue(p, "10_psx_nocnf.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/11_psx_lower.bin", ps1_in);
      if (!write_ps1_track(p, lower_str, lower_at,
            ARRAY_SIZE(lower_str), 0x0B0071E8u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/11_psx_lower.cue", ps1_in);
      if (!write_cue(p, "11_psx_lower.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }

      strlcpy(config_get_ptr()->paths.path_content_database, ps1_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      if (!run_scan(ps1_in))
      {
         check(0, "PS1 scan started", "task_push_dbscan refused");
         goto done;
      }
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(scan_completed, "PS1 scan completed",
            scan_completed ? "callback fired" : "timed out");

      sprintf(p, "%s/Test PS1.lpl", pl_dir);
      check(   file_contains(p, "Serial The Game")
            && file_contains(p, "SLUS-00152|serial"),
            "PS1 serial found past a listed PSX.EXE", "SLUS-00152|serial");
      check(   file_contains(p, "Short Track Game")
            && file_contains(p, "SLUS-00777|serial"),
            "PS1 serial found on a track shorter than the probe",
            "SLUS-00777|serial");
      check(   file_contains(p, "PSX Boot The Game")
            && file_contains(p, "0B0071E5|crc")
            && !file_contains(p, "Decoy Boot Game"),
            "PSX.EXE as BOOT target falls to the crc", "0B0071E5|crc");
      check(   file_contains(p, "PSX Boot Two")
            && file_contains(p, "0B0071E6|crc")
            && !file_contains(p, "Decoy Serial Game"),
            "listed serial ignored when PSX.EXE boots", "0B0071E6|crc");
      check(   file_contains(p, "PSX No CNF Game")
            && file_contains(p, "0B0071E7|crc")
            && !file_contains(p, "Decoy No CNF Game"),
            "no SYSTEM.CNF: PSX.EXE falls to the crc", "0B0071E7|crc");
      check(   file_contains(p, "PSX Lower Game")
            && file_contains(p, "0B0071E8|crc")
            && !file_contains(p, "Decoy Lower Game"),
            "lowercase PSX.EXE BOOT falls to the crc", "0B0071E8|crc");
   }

   /* 3DS images match on the product code in their NCCH header.  A
    * code several records share goes to the crc, and the first of them
    * stands in when the crc matches none.  A code no record carries, a
    * header without its magic, or a CIA, takes the crc path. */
   {
      static const db_rec_t ctr_recs[] = {
         { "Unique The Game",    "CTR-P-UNIQ", 0x0BAD0001u, CTR_IMAGE_SIZE },
         { "Shared Rev 0",       "CTR-P-SHRD", 0x5AD00000u, CTR_IMAGE_SIZE },
         { "Shared Rev 1",       "CTR-P-SHRD", 0x5AD00001u, CTR_IMAGE_SIZE },
         { "Fallback First",     "CTR-P-FALL", 0xFA110000u, CTR_IMAGE_SIZE },
         { "Fallback Second",    "CTR-P-FALL", 0xFA110001u, CTR_IMAGE_SIZE },
         { "Cia The Game",       NULL,         0xC1A00000u, CTR_IMAGE_SIZE },
         { "Bad Magic The Game", NULL,         0xBAD0BAD0u, CTR_IMAGE_SIZE },
         { "Bad Magic Decoy",    "CTR-P-BADM", 0xBAD00000u, CTR_IMAGE_SIZE },
         { "Bare The Game",      "CTR-P-BARE", 0x0BAD0002u, CTR_IMAGE_SIZE },
         /* LEGO Batman 2 (Canada) has its serial without the prefix. */
         { "Miss The Game",      "MISS",       0x3155C0DEu, CTR_IMAGE_SIZE },
         { "Unit Shift The Game", "CTR-P-UNIT", 0x0BAD0003u, CTR_IMAGE_SIZE },
         { "Big Shift Owner",    NULL,         0xB16B0000u, CTR_BIG_SIZE },
         { "Big Shift Decoy",    "CTR-P-BIGU", 0xB16B0001u, CTR_BIG_SIZE },
         { "Cut Header Owner",   NULL,         0xC0700000u, CTR_CUT_SIZE },
         { "Cut Header Decoy",   "CTR-P-CUTH", 0xC0700001u, CTR_CUT_SIZE },
         { "Sixteen The Game",   "CTR-P-SIXTEEN-16", 0x0BAD0004u,
                                                           CTR_IMAGE_SIZE },
         { "Del Code Owner",     NULL,         0xDE100000u, CTR_IMAGE_SIZE },
         { "Del Code Decoy",     "CTR-P-DEL\x7f", 0xDE100001u, CTR_IMAGE_SIZE }
      };
      char ctr_db[512], ctr_in[512];

      sprintf(ctr_db, "%s/scan_db_3ds", root);
      sprintf(ctr_in, "%s/scan_3ds",    root);
      path_mkdir(ctr_db);
      path_mkdir(ctr_in);

      sprintf(p, "%s/Test 3DS.rdb", ctr_db);
      if (!write_db_set(p, ctr_recs, ARRAY_SIZE(ctr_recs)))
      { check(0, "fixture", "could not write database"); goto done; }
      /* Partition 0 where carts have it, 0x4000. */
      sprintf(p, "%s/10_unique.3ds", ctr_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-UNIQ", 0x10101010u))
      { check(0, "fixture", "could not write image"); goto done; }
      /* Elsewhere, so the offset has to come from the table. */
      sprintf(p, "%s/11_shared.3ds", ctr_in);
      if (!write_ctr(p, 0x28, 1, "CTR-P-SHRD", 0x5AD00001u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/12_fallback.3ds", ctr_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-FALL", 0x12121212u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/13_cia.cci", ctr_in);
      if (!write_cia(p, 0xC1A00000u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/14_bad_magic.3ds", ctr_in);
      if (!write_ctr(p, 0x20, 0, "CTR-P-BADM", 0xBAD0BAD0u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/15_bare.cxi", ctr_in);
      if (!write_ctr(p, 0, 1, "CTR-P-BARE", 0x15151515u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/16_miss.3ds", ctr_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-MISS", 0x3155C0DEu))
      { check(0, "fixture", "could not write image"); goto done; }
      /* Units of 0x400. */
      sprintf(p, "%s/17_unit.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_IMAGE_SIZE, 1, 0x10, 0x4000,
               "CTR-P-UNIT", 10, 0x17171717u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/18_big_shift.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_BIG_SIZE, 16, 1, 0x2000000,
               "CTR-P-BIGU", 10, 0xB16B0000u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/19_cut.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_CUT_SIZE, 0, 0x20, 0x4000,
               "CTR-P-CUTH", 10, 0xC0700000u))
      { check(0, "fixture", "could not write image"); goto done; }
      /* No NUL: the next field follows. */
      sprintf(p, "%s/1a_sixteen.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_IMAGE_SIZE, 0, 0x20, 0x4000,
               "CTR-P-SIXTEEN-16XYZ", 19, 0x1A1A1A1Au))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/1b_del.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_IMAGE_SIZE, 0, 0x20, 0x4000,
               "CTR-P-DEL\x7f", 10, 0xDE100000u))
      { check(0, "fixture", "could not write image"); goto done; }

      strlcpy(config_get_ptr()->paths.path_content_database, ctr_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      if (!run_scan(ctr_in))
      {
         check(0, "3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(scan_completed, "3DS scan completed",
            scan_completed ? "callback fired" : "timed out");

      sprintf(p, "%s/Test 3DS.lpl", pl_dir);
      check(   file_contains(p, "Unique The Game")
            && file_contains(p, "CTR-P-UNIQ|serial"),
            "unique 3DS serial matched without the crc", "CTR-P-UNIQ|serial");
      check(   file_contains(p, "Shared Rev 1")
            && !file_contains(p, "Shared Rev 0")
            && file_contains(p, "CTR-P-SHRD|serial"),
            "shared 3DS serial resolved by the crc", "Shared Rev 1");
      check(   file_contains(p, "Fallback First")
            && !file_contains(p, "Fallback Second"),
            "shared serial, no crc match: first record", "Fallback First");
      check(   file_contains(p, "Cia The Game")
            && file_contains(p, "C1A00000|crc"),
            "CIA header in a .cci keeps the crc path", "C1A00000|crc");
      check(   file_contains(p, "Bad Magic The Game")
            && !file_contains(p, "Bad Magic Decoy"),
            "partition without NCCH magic: crc path", "BAD0BAD0|crc");
      check(   file_contains(p, "Bare The Game")
            && file_contains(p, "CTR-P-BARE|serial"),
            "bare .cxi matched on its serial", "CTR-P-BARE|serial");
      check(   file_contains(p, "Miss The Game")
            && file_contains(p, "3155C0DE|crc"),
            "serial in no record falls back to the crc", "3155C0DE|crc");
      check(   file_contains(p, "Unit Shift The Game")
            && file_contains(p, "CTR-P-UNIT|serial"),
            "partition offset in units of 0x200 << flags[6]",
            "CTR-P-UNIT|serial");
      check(   file_contains(p, "Big Shift Owner")
            && !file_contains(p, "Big Shift Decoy"),
            "flags[6] over 15: crc path", "B16B0000|crc");
      check(   file_contains(p, "Cut Header Owner")
            && !file_contains(p, "Cut Header Decoy"),
            "NCCH header cut short by the end: crc path", "C0700000|crc");
      check(   file_contains(p, "Sixteen The Game")
            && file_contains(p, "CTR-P-SIXTEEN-16|serial"),
            "16-byte code with no NUL ends at 16 bytes",
            "CTR-P-SIXTEEN-16|serial");
      check(   file_contains(p, "Del Code Owner")
            && !file_contains(p, "Del Code Decoy"),
            "code with a byte past '~': crc path", "DE100000|crc");
   }

   /* A 3DS database no installed core claims is passed over, as the crc
    * lookup passes it over, unless Scan Without Core Match is on.  Its
    * name must not start with "Test 3DS ": string_list_find_elem()
    * takes a space for the end of a string, so the test core would
    * claim it. */
   {
      static const db_rec_t uncl_recs[] = {
         { "Unclaimed The Game", "CTR-P-UNCL", 0x0C1A0001u, CTR_IMAGE_SIZE }
      };
      char uncl_db[512], uncl_in[512];

      sprintf(uncl_db, "%s/scan_db_3ds_unclaimed", root);
      sprintf(uncl_in, "%s/scan_3ds_unclaimed",    root);
      path_mkdir(uncl_db);
      path_mkdir(uncl_in);

      sprintf(p, "%s/Unclaimed 3DS.rdb", uncl_db);
      if (!write_db_set(p, uncl_recs, ARRAY_SIZE(uncl_recs)))
      { check(0, "fixture", "could not write database"); goto done; }
      sprintf(p, "%s/40_unclaimed.3ds", uncl_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-UNCL", 0x0C1A00FFu))
      { check(0, "fixture", "could not write image"); goto done; }

      strlcpy(config_get_ptr()->paths.path_content_database, uncl_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      if (!run_scan(uncl_in))
      {
         check(0, "unclaimed 3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      sprintf(p, "%s/Unclaimed 3DS.lpl", pl_dir);
      check(scan_completed && !path_is_valid(p),
            "3DS database no core claims passed over", "no playlist");

      filestream_delete(p);
      config_get_ptr()->bools.scan_without_core_match = true;
      if (!run_scan(uncl_in))
      {
         check(0, "unclaimed 3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      config_get_ptr()->bools.scan_without_core_match = false;
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(   scan_completed
            && file_contains(p, "Unclaimed The Game")
            && file_contains(p, "CTR-P-UNCL|serial"),
            "Scan Without Core Match: unclaimed 3DS database",
            "CTR-P-UNCL|serial");
   }

   /* scan_serial_and_crc makes the crc a requirement after a 3DS
    * serial hit, as it is for PSP and Dreamcast.  A hit the crc turns
    * down goes on to the crc lookup. */
   {
      static const db_rec_t strict_recs[] = {
         { "Strict Hit",            "CTR-P-STHT", 0x57A1C001u, CTR_IMAGE_SIZE },
         { "Strict Miss",           "CTR-P-STMS", 0x57A1C002u, CTR_IMAGE_SIZE },
         { "Strict Code Elsewhere", "CTR-P-SCEL", 0x57A1C010u, CTR_IMAGE_SIZE },
         { "Strict Crc Owner",      NULL,         0x57A1C011u, CTR_IMAGE_SIZE },
         { "Strict Shared Rev 0",   "CTR-P-SSHR", 0x57A1C020u, CTR_IMAGE_SIZE },
         { "Strict Shared Rev 1",   "CTR-P-SSHR", 0x57A1C021u, CTR_IMAGE_SIZE },
         { "Strict Shared Owner",   "CTR-P-SOWN", 0x57A1C022u, CTR_IMAGE_SIZE },
         { "Strict Nowhere Rev 0",  "CTR-P-SNOW", 0x57A1C030u, CTR_IMAGE_SIZE },
         { "Strict Nowhere Rev 1",  "CTR-P-SNOW", 0x57A1C031u, CTR_IMAGE_SIZE }
      };
      char strict_db[512], strict_in[512];

      sprintf(strict_db, "%s/scan_db_3ds_strict", root);
      sprintf(strict_in, "%s/scan_3ds_strict",    root);
      path_mkdir(strict_db);
      path_mkdir(strict_in);

      sprintf(p, "%s/Test 3DS Strict.rdb", strict_db);
      if (!write_db_set(p, strict_recs, ARRAY_SIZE(strict_recs)))
      { check(0, "fixture", "could not write database"); goto done; }
      sprintf(p, "%s/20_strict_hit.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-STHT", 0x57A1C001u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/21_strict_miss.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-STMS", 0x57A1C0FFu))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/22_strict_elsewhere.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-SCEL", 0x57A1C011u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/23_strict_shared_elsewhere.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-SSHR", 0x57A1C022u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/24_strict_nowhere.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-SNOW", 0x57A1C0EEu))
      { check(0, "fixture", "could not write image"); goto done; }

      config_get_ptr()->bools.scan_serial_and_crc = true;
      strlcpy(config_get_ptr()->paths.path_content_database, strict_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      if (!run_scan(strict_in))
      {
         check(0, "strict 3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      config_get_ptr()->bools.scan_serial_and_crc = false;
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(scan_completed, "strict 3DS scan completed",
            scan_completed ? "callback fired" : "timed out");

      sprintf(p, "%s/Test 3DS Strict.lpl", pl_dir);
      check(   file_contains(p, "Strict Hit")
            && file_contains(p, "CTR-P-STHT|serial"),
            "scan_serial_and_crc: serial and crc match", "CTR-P-STHT|serial");
      check(!file_contains(p, "Strict Miss"),
            "scan_serial_and_crc: crc mismatch refused", "no Strict Miss");
      check(   file_contains(p, "Strict Crc Owner")
            && file_contains(p, "57A1C011|crc")
            && !file_contains(p, "Strict Code Elsewhere"),
            "scan_serial_and_crc: crc on another record", "57A1C011|crc");
      check(   file_contains(p, "Strict Shared Owner")
            && file_contains(p, "57A1C022|crc")
            && !file_contains(p, "Strict Shared Rev"),
            "scan_serial_and_crc: shared, crc elsewhere", "57A1C022|crc");
      check(!file_contains(p, "24_strict_nowhere"),
            "scan_serial_and_crc: shared, crc nowhere", "no 24_");
   }

   /* First-match selection moves a matched database to the front and
    * stops the search there.  A 3DS code no record carries must still
    * reach the crc: the first miss matches by crc and moves Test 3DS to
    * the front, and the others meet it there.  The database list is in
    * readdir order, so this bites only when Test 3DS is not listed
    * first; the pads make that so in creation, reverse creation and
    * name order.  A code the crc turns down reaches the crc too: of the
    * two such files, whichever comes second meets Test 3DS there. */
   {
      static const db_rec_t first_recs[] = {
         { "First Miss The Game", "FMIS", 0xF1157000u, CTR_IMAGE_SIZE },
         { "First Strict Code",   "CTR-P-FSTR", 0xF1157001u, CTR_IMAGE_SIZE },
         { "First Strict Owner A", NULL,  0xF1157002u, CTR_IMAGE_SIZE },
         { "First Strict Owner B", NULL,  0xF1157003u, CTR_IMAGE_SIZE }
      };
      static const char *const first_files[] = {
         "30_first_miss_a.3ds", "31_first_miss_b.3ds", "32_first_miss_c.3ds"
      };
      static const char *const strict_files[] = {
         "33_first_strict_a.3ds", "34_first_strict_b.3ds"
      };
      char     first_db[512], first_in[512];
      unsigned i;
      int      all_matched = 1;

      sprintf(first_db, "%s/scan_db_3ds_first", root);
      sprintf(first_in, "%s/scan_3ds_first",    root);
      path_mkdir(first_db);
      path_mkdir(first_in);

      for (i = 0; i < 6; i++)
      {
         if (i == 3)
         {
            sprintf(p, "%s/Test 3DS.rdb", first_db);
            if (!write_db_set(p, first_recs, ARRAY_SIZE(first_recs)))
            { check(0, "fixture", "could not write database"); goto done; }
         }
         sprintf(p, "%s/Test 3DS Pad %u.rdb", first_db, i);
         if (!write_db(p, "Pad The Game", 0x9AD00000u + i, CTR_IMAGE_SIZE))
         { check(0, "fixture", "could not write database"); goto done; }
      }
      for (i = 0; i < ARRAY_SIZE(first_files); i++)
      {
         sprintf(p, "%s/%s", first_in, first_files[i]);
         if (!write_ctr(p, 0x20, 1, "CTR-P-FMIS", 0xF1157000u))
         { check(0, "fixture", "could not write image"); goto done; }
      }
      for (i = 0; i < ARRAY_SIZE(strict_files); i++)
      {
         sprintf(p, "%s/%s", first_in, strict_files[i]);
         if (!write_ctr(p, 0x20, 1, "CTR-P-FSTR", 0xF1157002u + i))
         { check(0, "fixture", "could not write image"); goto done; }
      }
      {
         struct string_list *dbs = dir_list_new(first_db, "rdb", false,
               false, false, false);
         if (dbs && dbs->size
               && !strcmp(path_basename(dbs->elems[0].data),
                  "Test 3DS.rdb"))
            printf("  note  first match: Test 3DS lists first, so this "
                  "lane cannot bite\n");
         string_list_free(dbs);
      }

      manual_content_scan_set_menu_scan_method(
            MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
      manual_content_scan_set_menu_scan_db_select(
            MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH, NULL);
      strlcpy(config_get_ptr()->paths.path_content_database, first_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      config_get_ptr()->bools.scan_serial_and_crc = true;
      if (!run_scan(first_in))
      {
         check(0, "first-match 3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      config_get_ptr()->bools.scan_serial_and_crc = false;
      manual_content_scan_set_menu_scan_db_select(
            MANUAL_CONTENT_SCAN_SELECT_DB_AUTO, NULL);
      manual_content_scan_set_menu_scan_method(
            MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(scan_completed, "first-match 3DS scan completed",
            scan_completed ? "callback fired" : "timed out");

      sprintf(p, "%s/Test 3DS.lpl", pl_dir);
      for (i = 0; i < ARRAY_SIZE(first_files); i++)
         if (!file_contains(p, first_files[i]))
            all_matched = 0;
      check(all_matched && file_contains(p, "F1157000|crc"),
            "first match: serial miss falls back to crc",
            "30_, 31_ and 32_first_miss");
      check(   file_contains(p, "33_first_strict_a.3ds")
            && file_contains(p, "34_first_strict_b.3ds")
            && file_contains(p, "F1157002|crc")
            && file_contains(p, "F1157003|crc")
            && !file_contains(p, "First Strict Code"),
            "first match: strict rejection falls back to crc",
            "33_ and 34_first_strict");
   }

   /* A strict scan hands every file to DATABASE_SCAN_ITERATE_NEXT.  32
    * files fill string_list_new()'s initial capacity, so ASan catches a
    * read past the last entry. */
   {
      char pad_dir[512];
      uint32_t i;

      sprintf(pad_dir, "%s/scan_pad", root);
      path_mkdir(pad_dir);
      for (i = 0; i < 32; i++)
      {
         sprintf(p, "%s/pad_%02u.bin", pad_dir, (unsigned)i);
         if (!write_content(p, 0x7AD00000u + i, 1024))
         { check(0, "fixture", "crc forcing failed"); goto done; }
      }

      if (!run_scan(pad_dir))
      {
         check(0, "full list scan started", "task_push_dbscan refused");
         goto done;
      }
      check(scan_completed, "scan of a full content list completed",
            scan_completed ? "callback fired" : "timed out");
   }

   /* An empty folder has no entry to start iterating from. */
   {
      char empty_dir[512];

      sprintf(empty_dir, "%s/scan_empty", root);
      path_mkdir(empty_dir);

      invalid_content_pushed = false;
      if (!run_scan(empty_dir))
      {
         check(0, "empty folder scan started", "task_push_dbscan refused");
         goto done;
      }
      check(scan_completed && invalid_content_pushed,
            "empty folder reported as invalid content",
            !scan_completed ? "timed out"
            : invalid_content_pushed ? "message pushed" : "no message");
   }

   /* A sheet that names itself is still the entry being scanned, so
    * pruning its tracks must not free it.  Only ASan sees the
    * use-after-free. */
   {
      char self_dir[512];
      FILE *f;

      sprintf(self_dir, "%s/scan_self", root);
      path_mkdir(self_dir);

      sprintf(p, "%s/self.cue", self_dir);
      if (!(f = fopen(p, "w")))
      { check(0, "fixture", "could not write cue"); goto done; }
      fprintf(f,
            "FILE \"self.cue\" BINARY\n"
            "  TRACK 01 MODE1/2352\n"
            "    INDEX 01 00:00:00\n");
      fclose(f);

      sprintf(p, "%s/self.gdi", self_dir);
      if (!(f = fopen(p, "w")))
      { check(0, "fixture", "could not write gdi"); goto done; }
      fprintf(f, "1\n1 0 4 2352 self.gdi 0\n");
      fclose(f);

      if (!run_scan(self_dir))
      {
         check(0, "self-naming sheet scan started", "task_push_dbscan refused");
         goto done;
      }
      check(scan_completed, "self-naming cue and gdi scanned",
            scan_completed ? "callback fired" : "timed out");
   }

   /* A database no core claims costs nothing per file: the step that
    * refuses one passes over every refused database, so on the
    * regular queue, which runs the handler once per check, a file
    * takes a check per database it can match plus a fixed few - not
    * one per database in the directory. */
   {
      char many_db[512], many_in[512];
      unsigned i, gathers;

      sprintf(many_db, "%s/scan_db_many", root);
      sprintf(many_in, "%s/scan_unmatched", root);
      path_mkdir(many_db);
      path_mkdir(many_in);

      sprintf(p, "%s/Test Alpha.rdb", many_db);
      if (!write_db(p, "Alpha The Game", crc_a, sz_a))
      { check(0, "fixture", "could not write database"); goto done; }
      /* Never opened: the gate refuses them before any read. */
      for (i = 0; i < UNCLAIMED_DBS; i++)
      {
         FILE *f;
         sprintf(p, "%s/Unclaimed %02u.rdb", many_db, i);
         if (!(f = fopen(p, "wb")))
         { check(0, "fixture", "could not write database"); goto done; }
         fclose(f);
      }
      for (i = 0; i < UNMATCHED_FILES; i++)
      {
         sprintf(p, "%s/none_%02u.bin", many_in, i);
         if (!write_content(p, 0x3C000000u + i, 1024))
         { check(0, "fixture", "crc forcing failed"); goto done; }
      }

      /* The scanner reads its database directory from the settings. */
      strlcpy(config_get_ptr()->paths.path_content_database, many_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      task_queue_deinit();
      task_queue_init(false, msgq_push);
      gathers = run_scan_counted(many_in);
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      printf("  info  checks for %u files over %u databases: %u\n",
            UNMATCHED_FILES, UNCLAIMED_DBS + 1, gathers);
      check(scan_completed, "scan over unclaimed databases completed",
            scan_completed ? "callback fired" : "timed out");
      check(gathers && gathers < UNMATCHED_FILES * 8 + 32,
            "unclaimed databases passed over in one step",
            "checks bounded by files, not databases");
   }

done:
   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
