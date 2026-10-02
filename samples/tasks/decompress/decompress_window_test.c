/* Oracle for the archive extraction task (tasks/task_decompress.c),
 * compiled from the shipping translation unit against the real
 * unthreaded task queue, the real ZIP backend and the real shared
 * per-frame I/O window (tasks/task_nbio_slice.c).
 *
 * What these lanes pin:
 *
 *   window   - with a window that never expires, the whole archive
 *              is extracted in one handler call rather than one
 *              member per call.
 *   floor    - with a window exhausted on first observation, every
 *              call still extracts something, so the task finishes.
 *   resume   - a DEFLATE member larger than one input slice parks
 *              and resumes inside the window.
 *   subdir   - the subdir handler variant is windowed the same way.
 *   cancel   - cancelling part way releases everything (LSan).
 *   crc      - a member whose contents do not match its recorded CRC
 *              fails the task and is not written, whether it fails
 *              as it starts or after parking.
 *   stream   - a DEFLATE member larger than the backend's output
 *              window reaches the disk a window at a time, never as
 *              one write of the whole member (writes are measured at
 *              retro_vfs_file_write_impl), and no temporary file is
 *              left beside it.  Its back-references reach across
 *              window boundaries, so a decoder that loses its history
 *              when the window is rebound fails the member's CRC.
 *   keep     - a member that fails its CRC, or whose extraction is
 *              cancelled part way, leaves a file already at its path
 *              as it was, and no temporary file beside it.
 *
 * Every lane checks the extracted bytes against the fixture.
 *
 * Time is a virtual clock advancing a fixed step per observation, so
 * the pacing assertions are exact rather than dependent on the speed
 * of the machine running them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include <boolean.h>
#include <compat/strl.h>
#include <encodings/crc32.h>
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <streams/file_stream.h>

#include "../../../msg_hash.h"
#include "../../../tasks/tasks_internal.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* Largest single write reaching the VFS */
#define STREAM_WINDOW (256 * 1024)   /* archive_file_zlib.c */
static int64_t max_write;

int64_t __real_retro_vfs_file_write_impl(void *stream, const void *s,
      uint64_t len);
int64_t __wrap_retro_vfs_file_write_impl(void *stream, const void *s,
      uint64_t len)
{
   if ((int64_t)len > max_write)
      max_write = (int64_t)len;
   return __real_retro_vfs_file_write_impl(stream, s, len);
}

/* Virtual clock */

static retro_time_t clock_now;
static retro_time_t clock_step;

retro_time_t cpu_features_get_time_usec(void)
{
   clock_now += clock_step;
   return clock_now;
}

uint64_t cpu_features_get(void) { return 0; }
unsigned cpu_features_get_core_amount(void) { return 1; }

/* NBIO_XFER_TICK_USEC is 4000; one observation past it exhausts the
 * window before the first work item. */
#define STEP_UNBOUNDED 0
#define STEP_EXHAUSTED 5000

/* Stubs */

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   return "";
}

/* Fixture */

#define FIXTURE_ZIP   "decompress_window_fixture.zip"
#define FIXTURE_OUT   "decompress_window_out"
#define N_SMALL       200
#define BIG_SIZE      (1024 * 1024)
#define BIG_NAME      "big.bin"

static const char *small_dirs[3] = { "", "a/", "b/c/" };

typedef struct
{
   uint8_t *data;
   uint32_t size;
   char     name[64];
} member_t;

static member_t members[N_SMALL + 1];
static unsigned n_members;
static bool     big_opens_window;

static uint32_t lcg_state = 12345;

static uint32_t lcg(void)
{
   lcg_state = lcg_state * 1103515245u + 12345u;
   return lcg_state >> 8;
}

static void put16(FILE *f, unsigned v)
{
   fputc(v & 0xff, f);
   fputc((v >> 8) & 0xff, f);
}

static void put32(FILE *f, uint32_t v)
{
   put16(f, v & 0xffff);
   put16(f, v >> 16);
}

static void member_name(char *s, size_t len, unsigned i)
{
   char num[16];
   size_t _len = strlcpy(s, small_dirs[i % 3], len);
   sprintf(num, "m%03u.bin", i);
   strlcpy(s + _len, num, len - _len);
}

static bool deflate_raw(const uint8_t *in, uint32_t in_len,
      uint8_t **out, uint32_t *out_len);

/* @prefix random bytes, then runs that are random, copied from up to
 * 32 KiB back, or short repeats */
static void fill_big(uint8_t *d, uint32_t size, uint32_t prefix)
{
   uint32_t j;

   for (j = 0; j < prefix; j++)
      d[j] = (uint8_t)lcg();
   while (j < size)
   {
      uint32_t k;
      uint32_t run = 16 + lcg() % 512;
      unsigned op  = lcg() % 4;
      if (run > size - j)
         run = size - j;
      if (op < 2)
         for (k = 0; k < run; k++)
            d[j + k] = (uint8_t)lcg();
      else if (op == 2)
      {
         uint32_t dist = 1 + lcg() % (j < 32768 ? j : 32768);
         for (k = 0; k < run; k++)
            d[j + k] = d[j + k - dist];
      }
      else
      {
         uint32_t period = 1 + lcg() % 7;
         for (k = 0; k < run; k++)
            d[j + k] = (k < period) ? (uint8_t)lcg() : d[j + k - period];
      }
      j += run;
   }
}

/* Whether inflating @c in READ_SLICE pieces ends a piece within the
 * first 32 KiB of an output window other than the first: the next
 * piece then starts with back-references into the previous window,
 * which only the decoder's own history can still resolve. */
#define READ_SLICE (128 * 1024)   /* archive_file_zlib.c */
static bool slice_edge_opens_window(const uint8_t *c, uint32_t c_len,
      uint32_t size)
{
   z_stream z;
   uint8_t *out = (uint8_t*)malloc(size);
   uint32_t off = 0;
   bool hit     = false;

   memset(&z, 0, sizeof(z));
   if (!out || inflateInit2(&z, -MAX_WBITS) != Z_OK)
   {
      free(out);
      return false;
   }
   z.next_out  = out;
   z.avail_out = size;
   while (off < c_len && !hit)
   {
      uint32_t n  = c_len - off < READ_SLICE ? c_len - off : READ_SLICE;
      uint32_t at;
      z.next_in   = (Bytef*)(c + off);
      z.avail_in  = n;
      off        += n;
      if (inflate(&z, 0) < 0)
         break;
      at  = (uint32_t)z.total_out;
      hit = off < c_len && at > STREAM_WINDOW
            && at % STREAM_WINDOW >= 1024
            && at % STREAM_WINDOW <= 16384;
   }
   inflateEnd(&z);
   free(out);
   return hit;
}

/* The DEFLATE member spans several input slices, so it parks, and the
 * random prefix is sized so that one slice ends just inside an output
 * window: the stream lane then fails its CRC on a decoder that loses
 * its history when the window is rebound. */
static void build_big(member_t *m)
{
   uint32_t prefix;
   uint32_t seed = lcg_state;

   m->size = BIG_SIZE;
   m->data = (uint8_t*)malloc(m->size);
   for (prefix = 2 * READ_SLICE; prefix < BIG_SIZE / 2; prefix += 1024)
   {
      uint8_t *c = NULL;
      uint32_t c_len;
      bool hit;

      lcg_state = seed;
      fill_big(m->data, m->size, prefix);
      if (!deflate_raw(m->data, m->size, &c, &c_len))
         break;
      hit = slice_edge_opens_window(c, c_len, m->size);
      free(c);
      if (hit)
      {
         big_opens_window = true;
         return;
      }
   }
}

static void build_members(void)
{
   unsigned i;

   for (i = 0; i < N_SMALL; i++)
   {
      uint32_t j;
      member_t *m = &members[i];
      m->size     = 1 + lcg() % 2048;
      m->data     = (uint8_t*)malloc(m->size);
      for (j = 0; j < m->size; j++)
         m->data[j] = (uint8_t)lcg();
      member_name(m->name, sizeof(m->name), i);
   }

   build_big(&members[N_SMALL]);
   strlcpy(members[N_SMALL].name, BIG_NAME, sizeof(members[N_SMALL].name));

   n_members = N_SMALL + 1;
}

static bool deflate_raw(const uint8_t *in, uint32_t in_len,
      uint8_t **out, uint32_t *out_len)
{
   z_stream z;
   uLong    bound;

   memset(&z, 0, sizeof(z));
   if (deflateInit2(&z, 6, Z_DEFLATED, -MAX_WBITS, 8,
            Z_DEFAULT_STRATEGY) != Z_OK)
      return false;
   bound = deflateBound(&z, in_len);
   if (!(*out = (uint8_t*)malloc(bound)))
   {
      deflateEnd(&z);
      return false;
   }
   z.next_in   = (Bytef*)in;
   z.avail_in  = in_len;
   z.next_out  = *out;
   z.avail_out = (uInt)bound;
   if (deflate(&z, Z_FINISH) != Z_STREAM_END)
   {
      deflateEnd(&z);
      free(*out);
      return false;
   }
   *out_len = (uint32_t)z.total_out;
   deflateEnd(&z);
   return true;
}

static uint32_t big_csize;
static int      corrupt_member = -1;   /* member given a wrong CRC */

/* Stored members, then the DEFLATE one last. */
static bool write_zip(void)
{
   unsigned i;
   uint32_t *offsets;
   uint32_t *csizes;
   uint32_t *crcs;
   uint32_t cd_start, cd_end;
   uint8_t *big_c   = NULL;
   FILE *f          = fopen(FIXTURE_ZIP, "wb");

   if (!f)
      return false;

   offsets = (uint32_t*)calloc(n_members, sizeof(*offsets));
   csizes  = (uint32_t*)calloc(n_members, sizeof(*csizes));
   crcs    = (uint32_t*)calloc(n_members, sizeof(*crcs));

   if (!deflate_raw(members[N_SMALL].data, members[N_SMALL].size,
            &big_c, &big_csize))
   {
      fclose(f);
      return false;
   }

   for (i = 0; i < n_members; i++)
   {
      member_t *m   = &members[i];
      bool deflated = (i == N_SMALL);
      unsigned nl   = (unsigned)strlen(m->name);

      crcs[i]       = encoding_crc32(0, m->data, m->size);
      if ((int)i == corrupt_member)
         crcs[i]   ^= 1;
      csizes[i]     = deflated ? big_csize : m->size;
      offsets[i]    = (uint32_t)ftell(f);

      put32(f, 0x04034b50);
      put16(f, 20);
      put16(f, 0);
      put16(f, deflated ? 8 : 0);
      put16(f, 0);
      put16(f, 0);
      put32(f, crcs[i]);
      put32(f, csizes[i]);
      put32(f, m->size);
      put16(f, nl);
      put16(f, 0);
      fwrite(m->name, 1, nl, f);
      fwrite(deflated ? big_c : m->data, 1, csizes[i], f);
   }

   cd_start = (uint32_t)ftell(f);
   for (i = 0; i < n_members; i++)
   {
      member_t *m   = &members[i];
      bool deflated = (i == N_SMALL);
      unsigned nl   = (unsigned)strlen(m->name);

      put32(f, 0x02014b50);
      put16(f, 20);
      put16(f, 20);
      put16(f, 0);
      put16(f, deflated ? 8 : 0);
      put16(f, 0);
      put16(f, 0);
      put32(f, crcs[i]);
      put32(f, csizes[i]);
      put32(f, m->size);
      put16(f, nl);
      put16(f, 0);
      put16(f, 0);
      put16(f, 0);
      put16(f, 0);
      put32(f, 0);
      put32(f, offsets[i]);
      fwrite(m->name, 1, nl, f);
   }
   cd_end = (uint32_t)ftell(f);

   put32(f, 0x06054b50);
   put16(f, 0);
   put16(f, 0);
   put16(f, n_members);
   put16(f, n_members);
   put32(f, cd_end - cd_start);
   put32(f, cd_start);
   put16(f, 0);

   fclose(f);
   free(big_c);
   free(offsets);
   free(csizes);
   free(crcs);
   return true;
}

/* Output side */

static void out_path(char *s, size_t len, const char *rel)
{
   fill_pathname_join_special(s, FIXTURE_OUT, rel, len);
}

static void clear_outputs(void)
{
   unsigned i;
   char path[PATH_MAX_LENGTH];

   for (i = 0; i < n_members; i++)
   {
      const char *rel = members[i].name;
      out_path(path, sizeof(path), rel);
      filestream_delete(path);
      /* The subdir lane writes a/ members with the prefix dropped. */
      if (!strncmp(rel, "a/", 2))
      {
         out_path(path, sizeof(path), rel + 2);
         filestream_delete(path);
      }
   }
}

static bool file_matches(const char *path, const member_t *m)
{
   void   *buf = NULL;
   int64_t len = 0;
   bool    ok;

   if (!filestream_read_file(path, &buf, &len))
      return false;
   ok = (len == (int64_t)m->size && !memcmp(buf, m->data, m->size));
   free(buf);
   return ok;
}

/* Driving the task */

static bool done;
static bool done_ok;

static void extract_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   done    = true;
   done_ok = (!err && dec);

   if (dec)
   {
      free(dec->source_file);
      free(dec);
   }
}

/* Pushes an extraction and pumps the unthreaded queue until the
 * completion callback has run.  Returns the number of checks taken,
 * or 0 when it does not finish within @max_checks.  @cancel_after
 * cancels the task after that many checks (0 never). */
static unsigned run_extract(retro_time_t step, const char *subdir,
      unsigned max_checks, unsigned cancel_after)
{
   unsigned checks = 0;
   void *task;

   clock_step = step;
   /* A fresh window period: the window's state outlives a run. */
   clock_now += 1000000;
   done       = false;
   done_ok    = false;

   task_queue_init(false, NULL);

   if (!(task = task_push_decompress(FIXTURE_ZIP, FIXTURE_OUT, NULL,
               subdir, NULL, extract_cb, NULL, NULL, true)))
   {
      task_queue_deinit();
      return 0;
   }

   while (!done && checks < max_checks)
   {
      if (cancel_after && checks == cancel_after)
         task_queue_cancel_task(task);
      task_queue_check();
      checks++;
   }

   task_queue_deinit();
   return done ? checks : 0;
}

static unsigned count_matching(bool subdir_a)
{
   unsigned i, n = 0;
   char path[PATH_MAX_LENGTH];

   for (i = 0; i < n_members; i++)
   {
      const char *rel = members[i].name;
      if (subdir_a)
      {
         if (strncmp(rel, "a/", 2))
            continue;
         rel += 2;
      }
      out_path(path, sizeof(path), rel);
      if (file_matches(path, &members[i]))
         n++;
   }
   return n;
}

static unsigned count_present(void)
{
   unsigned i, n = 0;
   char path[PATH_MAX_LENGTH];

   for (i = 0; i < n_members; i++)
   {
      out_path(path, sizeof(path), members[i].name);
      if (path_is_valid(path))
         n++;
   }
   return n;
}

int main(void)
{
   unsigned i, checks;
   unsigned n_a = 0;

   build_members();
   for (i = 0; i < n_members; i++)
      if (!strncmp(members[i].name, "a/", 2))
         n_a++;

   if (!write_zip())
   {
      fprintf(stderr, "FAIL: could not write the fixture archive\n");
      return 1;
   }
   CHECK(big_csize > 2 * 128 * 1024,
         "DEFLATE member does not span several input slices");
   CHECK(big_opens_window,
         "no input slice of the DEFLATE member ends inside an output window");

   /* window */
   clear_outputs();
   checks = run_extract(STEP_UNBOUNDED, NULL, 1000, 0);
   printf("[window]  unbounded window: %u checks for %u members\n",
         checks, n_members);
   CHECK(checks != 0, "extraction did not finish");
   CHECK(checks <= 3,
         "extraction took one call per member instead of one window");
   CHECK(done_ok, "extraction reported an error");
   CHECK(count_matching(false) == n_members,
         "extracted contents differ from the archive");

   /* floor + resume */
   clear_outputs();
   checks = run_extract(STEP_EXHAUSTED, NULL, 10 * n_members, 0);
   printf("[floor]   exhausted window: %u checks for %u members\n",
         checks, n_members);
   CHECK(checks != 0, "exhausted window stalled the extraction");
   CHECK(checks > n_members,
         "exhausted window did more than one work item per call");
   CHECK(done_ok, "extraction reported an error");
   CHECK(count_matching(false) == n_members,
         "extracted contents differ from the archive");

   /* subdir */
   clear_outputs();
   checks = run_extract(STEP_UNBOUNDED, "a", 1000, 0);
   printf("[subdir]  unbounded window: %u checks, %u of %u a/ members\n",
         checks, count_matching(true), n_a);
   CHECK(checks != 0 && checks <= 3,
         "subdir extraction not windowed");
   CHECK(done_ok, "subdir extraction reported an error");
   CHECK(count_matching(true) == n_a,
         "subdir contents differ from the archive");

   /* cancel */
   clear_outputs();
   checks = run_extract(STEP_EXHAUSTED, NULL, 10 * n_members, 20);
   printf("[cancel]  cancelled after 20 checks: %u present\n",
         count_present());
   CHECK(checks != 0, "cancelled extraction never completed");
   CHECK(!done_ok, "cancelled extraction reported success");
   CHECK(count_present() < n_members,
         "cancel did not stop the extraction");

   /* crc: a small member, then the DEFLATE one that parks */
   {
      static const unsigned bad[2] = { 5, N_SMALL };
      unsigned k;

      for (k = 0; k < 2; k++)
      {
         char path[PATH_MAX_LENGTH];

         corrupt_member = (int)bad[k];
         if (!write_zip())
         {
            CHECK(false, "could not rewrite the fixture archive");
            break;
         }
         clear_outputs();
         checks = run_extract(STEP_UNBOUNDED, NULL, 1000, 0);
         out_path(path, sizeof(path), members[bad[k]].name);
         printf("[crc]     bad CRC on %s: %s\n", members[bad[k]].name,
               done_ok ? "accepted" : "rejected");
         CHECK(checks != 0, "extraction with a bad CRC never completed");
         CHECK(!done_ok, "a member failing its CRC was not reported");
         CHECK(!path_is_valid(path), "a member failing its CRC was written");
      }
      corrupt_member = -1;
   }

   /* stream */
   {
      char path[PATH_MAX_LENGTH];
      char tmp[PATH_MAX_LENGTH + 8];

      if (!write_zip())
         CHECK(false, "could not rewrite the fixture archive");
      clear_outputs();
      max_write = 0;
      checks    = run_extract(STEP_UNBOUNDED, NULL, 1000, 0);
      out_path(path, sizeof(path), BIG_NAME);
      snprintf(tmp, sizeof(tmp), "%s.tmp", path);
      printf("[stream]  largest write %ld bytes for a %u byte member\n",
            (long)max_write, (unsigned)BIG_SIZE);
      CHECK(done_ok, "extraction reported an error");
      CHECK(file_matches(path, &members[N_SMALL]),
            "streamed member differs from the archive");
      CHECK(max_write <= STREAM_WINDOW,
            "a member was written whole instead of a window at a time");
      CHECK(!path_is_valid(tmp), "a temporary file was left behind");
   }

   /* keep: an earlier file at the big member's path survives a CRC
    * failure and a cancel, each landing mid-member */
   {
      static const char old_contents[] = "the previous file";
      char path[PATH_MAX_LENGTH];
      char tmp[PATH_MAX_LENGTH + 8];
      unsigned k;

      out_path(path, sizeof(path), BIG_NAME);
      snprintf(tmp, sizeof(tmp), "%s.tmp", path);

      for (k = 0; k < 2; k++)
      {
         void *buf   = NULL;
         int64_t len = 0;
         bool kept;

         corrupt_member = (k == 0) ? (int)N_SMALL : -1;
         if (!write_zip())
         {
            CHECK(false, "could not rewrite the fixture archive");
            break;
         }
         clear_outputs();
         filestream_write_file(path, old_contents, sizeof(old_contents));

         /* The cancel lands once the small members are out and the
          * big one has parked */
         if (k == 0)
            run_extract(STEP_UNBOUNDED, NULL, 1000, 0);
         else
            run_extract(STEP_EXHAUSTED, NULL, 10 * n_members,
                  N_SMALL + 3);

         kept = filestream_read_file(path, &buf, &len)
               && len == (int64_t)sizeof(old_contents)
               && !memcmp(buf, old_contents, sizeof(old_contents));
         free(buf);
         printf("[keep]    %s mid-member: previous file %s\n",
               k == 0 ? "bad CRC" : "cancel",
               kept ? "kept" : "replaced");
         CHECK(!done_ok, "a failed or cancelled extraction reported success");
         CHECK(kept, "a member that did not complete replaced the previous file");
         CHECK(!path_is_valid(tmp), "a temporary file was left behind");
      }
      corrupt_member = -1;
   }

   clear_outputs();
   filestream_delete(FIXTURE_ZIP);
   for (i = 0; i < n_members; i++)
      free(members[i].data);

   if (failures)
   {
      fprintf(stderr, "%u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] decompress_window_test\n");
   return 0;
}
