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
 *   skip     - an extraction that opts in writes only the members
 *              whose archive entry changed or whose file is missing
 *              or was modified, and keeps a manifest of what it wrote
 *              beside them; a cancelled or failed run keeps the
 *              members it finished; a caller that does not opt in,
 *              a manifest that cannot be used or saved, a folder
 *              without mtimes and a source whose members have no CRC
 *              (.zst) all get every member written.
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
#include <vfs/vfs_implementation.h>

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

/* Writes since reset_writes(): to a dot file (the manifest) or not */
static unsigned manifest_writes;
static unsigned member_writes;
static bool     fail_manifest_writes;

static void note_member_write(const char *path);

int64_t __real_retro_vfs_file_write_impl(void *stream, const void *s,
      uint64_t len);
int64_t __wrap_retro_vfs_file_write_impl(void *stream, const void *s,
      uint64_t len)
{
   const char *path = retro_vfs_file_get_path_impl(
         (libretro_vfs_implementation_file*)stream);

   if ((int64_t)len > max_write)
      max_write = (int64_t)len;
   if (path && path_basename(path)[0] == '.')
   {
      if (fail_manifest_writes)
         return -1;
      manifest_writes++;
   }
   else
   {
      member_writes++;
      note_member_write(path);
   }
   return __real_retro_vfs_file_write_impl(stream, s, len);
}

/* With fail_mtime, a folder that has no mtimes (SMB, NFS, SAF) */
static unsigned mtime_calls;
static bool     fail_mtime;

int __real_retro_vfs_get_mtime_impl(const char *path, int64_t *mtime);
int __wrap_retro_vfs_get_mtime_impl(const char *path, int64_t *mtime)
{
   mtime_calls++;
   if (fail_mtime)
      return -1;
   return __real_retro_vfs_get_mtime_impl(path, mtime);
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

/* A Zstandard frame of one raw block: a member that has no CRC */

#define FIXTURE_ZST   "decompress_window_fixture.zst"
#define ZST_MEMBER    "decompress_window_fixture"
#define ZST_SIZE      16

static bool write_zst(const uint8_t *data)
{
   static const uint8_t head[9] = {
      0x28, 0xB5, 0x2F, 0xFD,       /* magic */
      0x20, ZST_SIZE,               /* single segment, content size */
      (ZST_SIZE << 3) | 1, 0, 0     /* last block, raw */
   };
   FILE *f = fopen(FIXTURE_ZST, "wb");
   bool ok;

   if (!f)
      return false;
   ok =     fwrite(head, 1, sizeof(head), f) == sizeof(head)
         && fwrite(data, 1, ZST_SIZE, f) == ZST_SIZE;
   return fclose(f) == 0 && ok;
}

/* Output side */

static void out_path(char *s, size_t len, const char *rel)
{
   fill_pathname_join_special(s, FIXTURE_OUT, rel, len);
}

/* The members a run wrote, seen at their temporary files */
static bool written[N_SMALL + 1];

static void note_member_write(const char *path)
{
   unsigned i;
   const char *rel;
   size_t len;
   size_t dir_len = strlen(FIXTURE_OUT);

   if (     !path
         || strncmp(path, FIXTURE_OUT, dir_len)
         || !PATH_CHAR_IS_SLASH(path[dir_len]))
      return;
   rel = path + dir_len + 1;
   len = strlen(rel);
   if (len < 4 || strcmp(rel + len - 4, ".tmp"))
      return;
   len -= 4;

   for (i = 0; i < n_members; i++)
   {
      const char *name = members[i].name;
      /* The subdir lane writes a/ members with the prefix dropped. */
      if (     (strlen(name) == len && !strncmp(name, rel, len))
            || (!strncmp(name, "a/", 2) && strlen(name + 2) == len
               && !strncmp(name + 2, rel, len)))
         written[i] = true;
   }
}

static unsigned count_written(void)
{
   unsigned i, n = 0;

   for (i = 0; i < n_members; i++)
      if (written[i])
         n++;
   return n;
}

static void reset_writes(void)
{
   manifest_writes = 0;
   member_writes   = 0;
   memset(written, 0, sizeof(written));
}

/* The manifest */

#define MANIFEST_HEADER "decompress_manifest 1\n"

static void manifest_file(char *s, size_t len)
{
   out_path(s, len, "." FIXTURE_ZIP ".crc");
}

/* The manifest's bytes and a NUL; NULL with no file */
static char *manifest_text(int64_t *len)
{
   char path[PATH_MAX_LENGTH];
   void *buf = NULL;

   *len = 0;
   manifest_file(path, sizeof(path));
   if (!filestream_read_file(path, &buf, len))
      return NULL;
   return (char*)buf;
}

static bool manifest_has_header(void)
{
   int64_t len;
   char *text = manifest_text(&len);
   bool ok    = text && !strncmp(text, MANIFEST_HEADER,
         strlen(MANIFEST_HEADER));

   free(text);
   return ok;
}

/* Lines after the header; 0 with no file */
static unsigned manifest_lines(void)
{
   int64_t i, len;
   unsigned n = 0;
   char *text = manifest_text(&len);

   for (i = 0; i < len; i++)
      if (text[i] == '\n')
         n++;
   free(text);
   return n ? n - 1 : 0;
}

/* The line of @text for @name, or NULL */
static const char *manifest_line(const char *text, const char *name)
{
   char key[80];
   const char *hit;

   snprintf(key, sizeof(key), " %s\n", name);
   if (!text || !(hit = strstr(text, key)))
      return NULL;
   while (hit > text && hit[-1] != '\n')
      hit--;
   return hit;
}

static bool manifest_names(const char *name)
{
   int64_t len;
   char *text = manifest_text(&len);
   bool found = manifest_line(text, name) != NULL;

   free(text);
   return found;
}

/* The mtime recorded for @name */
static bool manifest_mtime(const char *name, int64_t *mtime)
{
   int64_t len;
   long long v      = 0;
   char *text       = manifest_text(&len);
   const char *line = manifest_line(text, name);
   bool ok          = line && sscanf(line, "%*x %*u %lld", &v) == 1;

   free(text);
   *mtime = (int64_t)v;
   return ok;
}

/* The CRC recorded for @name */
static bool manifest_crc(const char *name, uint32_t *crc)
{
   int64_t len;
   unsigned v       = 0;
   char *text       = manifest_text(&len);
   const char *line = manifest_line(text, name);
   bool ok          = line && sscanf(line, "%x", &v) == 1;

   free(text);
   *crc = (uint32_t)v;
   return ok;
}

static void clear_outputs(void)
{
   unsigned i;
   char path[PATH_MAX_LENGTH];
   char tmp[PATH_MAX_LENGTH + 8];

   manifest_file(path, sizeof(path));
   snprintf(tmp, sizeof(tmp), "%s.tmp", path);
   filestream_delete(path);
   filestream_delete(tmp);

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
static int  last_progress;
static bool opt_in;   /* extract through task_push_decompress_skip_unchanged */
static const char *fixture_src = FIXTURE_ZIP;

static void extract_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   done          = true;
   done_ok       = (!err && dec);
   last_progress = task_get_progress(task);

   if (dec)
   {
      free(dec->source_file);
      free(dec);
   }
}

/* Pushes an extraction and pumps the unthreaded queue until the
 * completion callback has run.  Returns the number of checks taken,
 * or 0 when it does not finish within @max_checks.  @cancel_after
 * cancels the task after that many checks (0 never).  The write and
 * mtime counters start from zero. */
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

   reset_writes();
   mtime_calls = 0;

   task_queue_init(false, NULL);

   if (opt_in)
      task = task_push_decompress_skip_unchanged(fixture_src, FIXTURE_OUT,
            NULL, subdir, NULL, extract_cb, NULL, NULL, true);
   else
      task = task_push_decompress(fixture_src, FIXTURE_OUT, NULL,
            subdir, NULL, extract_cb, NULL, NULL, true);
   if (!task)
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

static unsigned full_run(void)
{
   return run_extract(STEP_UNBOUNDED, NULL, 1000, 0);
}

/* skip: each case starts from empty outputs and a good fixture */
static void lane_skip(unsigned n_a)
{
   char mpath[PATH_MAX_LENGTH];
   char mtmp[PATH_MAX_LENGTH + 8];
   char path[PATH_MAX_LENGTH];
   unsigned i, k, checks;

   manifest_file(mpath, sizeof(mpath));
   snprintf(mtmp, sizeof(mtmp), "%s.tmp", mpath);
   opt_in = true;

   /* repeat: a second run writes nothing, bounded window or not */
   {
      int progress;

      clear_outputs();
      full_run();
      progress = last_progress;
      CHECK(done_ok, "skip_repeat: the first run reported an error");
      CHECK(count_written() == n_members,
            "skip_repeat: the first run did not write every member");
      CHECK(manifest_lines() == n_members,
            "skip_repeat: the manifest does not have a line per member");
      CHECK(manifest_writes > 0,
            "skip_repeat: the first run did not write the manifest");

      checks = full_run();
      printf("[skip]    repeat: %u member writes, %u manifest writes,"
            " %u checks\n", member_writes, manifest_writes, checks);
      CHECK(done_ok, "skip_repeat: the second run reported an error");
      CHECK(member_writes == 0, "skip_repeat: the second run wrote members");
      CHECK(manifest_writes == 0,
            "skip_repeat: the second run rewrote the manifest");
      CHECK(checks != 0 && checks <= 3,
            "skip_repeat: the second run was not windowed");
      CHECK(last_progress == progress,
            "skip_repeat: skipped members did not advance the progress");

      checks = run_extract(STEP_EXHAUSTED, NULL, 10 * n_members, 0);
      CHECK(done_ok,
            "skip_repeat: the exhausted-window run reported an error");
      CHECK(checks != 0,
            "skip_repeat: an exhausted window stalled on skipped members");
      CHECK(member_writes == 0,
            "skip_repeat: the exhausted-window run wrote members");
      CHECK(count_matching(false) == n_members,
            "skip_repeat: contents differ from the archive");
   }

   /* changed: new bytes, a new size, and the DEFLATE member */
   {
      uint8_t *big = members[N_SMALL].data;

      clear_outputs();
      full_run();
      for (k = 8; k < N_SMALL && members[k].size < 2; k++)
         ;
      members[7].data[0] ^= 0xff;
      members[k].size--;
      big[BIG_SIZE - 1]  ^= 0xff;
      CHECK(k < N_SMALL && write_zip(),
            "skip_changed: could not rewrite the fixture archive");

      full_run();
      printf("[skip]    changed: %u members written for 7, %u and %u\n",
            count_written(), k, (unsigned)N_SMALL);
      CHECK(done_ok, "skip_changed: the run reported an error");
      CHECK(count_written() == 3
            && written[7] && written[k] && written[N_SMALL],
            "skip_changed: not exactly the changed members were written");
      CHECK(count_matching(false) == n_members,
            "skip_changed: contents differ from the archive");

      members[7].data[0] ^= 0xff;
      members[k].size++;
      big[BIG_SIZE - 1]  ^= 0xff;
      CHECK(write_zip(),
            "skip_changed: could not restore the fixture archive");
   }

   /* healed: a deleted file and an edited one come back */
   {
      int64_t mtime     = 0;
      const member_t *m = &members[4];
      uint8_t *other    = (uint8_t*)malloc(m->size);

      clear_outputs();
      full_run();

      out_path(path, sizeof(path), members[3].name);
      CHECK(filestream_delete(path) == 0,
            "skip_healed: could not delete a member's file");

      out_path(path, sizeof(path), m->name);
      CHECK(manifest_mtime(m->name, &mtime),
            "skip_healed: the manifest has no mtime for the member");
      if (other)
      {
         memcpy(other, m->data, m->size);
         other[0] ^= 0xff;
         CHECK(filestream_write_file(path, other, m->size),
               "skip_healed: could not edit a member's file");
         free(other);
      }
      CHECK(path_set_mtime(path, mtime + 2),
            "skip_healed: path_set_mtime failed");

      full_run();
      printf("[skip]    healed: %u members written for a deleted and an"
            " edited file\n", count_written());
      CHECK(done_ok, "skip_healed: the run reported an error");
      CHECK(count_written() == 2 && written[3] && written[4],
            "skip_healed: not exactly the two files were written again");
      CHECK(count_matching(false) == n_members,
            "skip_healed: contents differ from the archive");
   }

   /* resume: a cancelled run keeps the members it finished */
   {
      unsigned present, lines;

      clear_outputs();
      checks  = run_extract(STEP_EXHAUSTED, NULL, 10 * n_members, 20);
      present = count_present();
      lines   = manifest_lines();
      CHECK(checks != 0, "skip_resume: the cancelled run never completed");
      CHECK(!done_ok, "skip_resume: the cancelled run reported success");
      CHECK(lines >= 1 && present <= lines + 1 && lines <= present
            && present < n_members,
            "skip_resume: the manifest does not hold the finished members");

      full_run();
      printf("[skip]    resume: %u lines for %u files at the cancel,"
            " %u members written after\n", lines, present, count_written());
      CHECK(done_ok, "skip_resume: the next run reported an error");
      CHECK(count_written() == n_members - lines,
            "skip_resume: the next run did not write exactly the rest");
      CHECK(count_matching(false) == n_members,
            "skip_resume: contents differ from the archive");
   }

   /* plain: a caller that does not opt in */
   {
      int64_t before_len = 0;
      int64_t after_len  = 0;
      char *before;
      char *after;

      clear_outputs();
      full_run();
      before = manifest_text(&before_len);

      opt_in = false;
      full_run();
      opt_in = true;
      after  = manifest_text(&after_len);
      printf("[skip]    plain: %u members written, %u manifest writes\n",
            count_written(), manifest_writes);
      CHECK(done_ok, "skip_plain: the run reported an error");
      CHECK(count_written() == n_members,
            "skip_plain: a caller that did not opt in had members skipped");
      CHECK(manifest_writes == 0, "skip_plain: the manifest was written");
      CHECK(before && after && before_len == after_len
            && !memcmp(before, after, (size_t)before_len),
            "skip_plain: the manifest's bytes changed");
      CHECK(count_matching(false) == n_members,
            "skip_plain: contents differ from the archive");
      free(before);
      free(after);
   }

   /* foreign: another file's header, noise, lines for other contents */
   clear_outputs();
   full_run();
   for (k = 0; k < 3; k++)
   {
      static const char other_header[] = "core_updater_crc 1";
      int64_t len  = 0;
      bool spoiled = false;
      char *text   = manifest_text(&len);
      char *nl     = text ? strchr(text, '\n') : NULL;

      if (k == 0 && nl)
      {
         size_t head = sizeof(other_header) - 1;
         size_t rest = (size_t)(text + len - nl);
         char *out   = (char*)malloc(head + rest);
         if (out)
         {
            memcpy(out, other_header, head);
            memcpy(out + head, nl, rest);
            spoiled = filestream_write_file(mpath, out,
                  (int64_t)(head + rest));
            free(out);
         }
      }
      else if (k == 1)
      {
         uint8_t noise[4096];
         for (i = 0; i < sizeof(noise); i++)
            noise[i] = (uint8_t)lcg();
         spoiled = filestream_write_file(mpath, noise, sizeof(noise));
      }
      else if (nl)
      {
         /* Each line's CRC starts with another digit */
         char *p = nl + 1;
         while (p < text + len)
         {
            *p = (*p == '0') ? '1' : '0';
            if (!(p = strchr(p, '\n')))
               break;
            p++;
         }
         spoiled = filestream_write_file(mpath, text, len);
      }
      free(text);
      CHECK(spoiled, "skip_foreign: could not spoil the manifest");

      full_run();
      printf("[skip]    foreign %u: %u members written, %u lines after\n",
            k, count_written(), manifest_lines());
      CHECK(done_ok, "skip_foreign: the run reported an error");
      CHECK(count_written() == n_members,
            "skip_foreign: members were skipped on a manifest not to trust");
      CHECK(manifest_has_header() && manifest_lines() == n_members,
            "skip_foreign: the manifest was not replaced by a good one");
   }
   CHECK(count_matching(false) == n_members,
         "skip_foreign: contents differ from the archive");

   /* filter: a member the caller filtered out before has no line */
   {
      int64_t len;
      char *text;
      unsigned cut_names = 0, full_names = 0;
      unsigned wrong     = 0, differ     = 0;

      clear_outputs();
      run_extract(STEP_UNBOUNDED, "a", 1000, 0);
      CHECK(done_ok && count_written() == n_a,
            "skip_filter: the first subdir run did not write its members");
      run_extract(STEP_UNBOUNDED, "a", 1000, 0);
      text = manifest_text(&len);
      for (i = 0; i < n_members; i++)
      {
         const char *name = members[i].name;
         if (!strncmp(name, "a/", 2) && manifest_line(text, name + 2))
            cut_names++;
         if (manifest_line(text, name))
            full_names++;
      }
      free(text);
      printf("[skip]    filter: %u member writes on a second subdir run,"
            " %u lines\n", member_writes, manifest_lines());
      CHECK(done_ok, "skip_filter: the second subdir run reported an error");
      CHECK(member_writes == 0,
            "skip_filter: the second subdir run wrote members");
      CHECK(manifest_lines() == n_a && cut_names == n_a && full_names == 0,
            "skip_filter: the manifest's names are not the files written");
      CHECK(count_matching(true) == n_a,
            "skip_filter: subdir contents differ from the archive");

      clear_outputs();
      run_extract(STEP_UNBOUNDED, "a|", 1000, 0);
      CHECK(done_ok && count_written() == n_a,
            "skip_filter: the first filtered run did not write its members");
      run_extract(STEP_UNBOUNDED, "a|b|", 1000, 0);
      for (i = 0; i < n_members; i++)
      {
         const char *name = members[i].name;
         bool is_b        = !strncmp(name, "b/", 2);

         if (written[i] != is_b)
            wrong++;
         if (is_b || !strncmp(name, "a/", 2))
         {
            out_path(path, sizeof(path), name);
            if (!file_matches(path, &members[i]))
               differ++;
         }
      }
      printf("[skip]    filter: %u members written once b/ is admitted\n",
            count_written());
      CHECK(done_ok, "skip_filter: the wider run reported an error");
      CHECK(wrong == 0,
            "skip_filter: not exactly the newly admitted members were written");
      CHECK(differ == 0, "skip_filter: contents differ from the archive");
   }

   /* failed: the members finished before a bad one keep their lines */
   {
      unsigned lines;
      bool named;

      corrupt_member = 100;
      CHECK(write_zip(), "skip_failed: could not rewrite the fixture archive");
      clear_outputs();
      checks = full_run();
      lines  = manifest_lines();
      named  = manifest_names(members[100].name);
      CHECK(checks != 0 && !done_ok,
            "skip_failed: a member failing its CRC was not reported");
      CHECK(lines >= 1 && lines < n_members,
            "skip_failed: the manifest does not hold the finished members");
      CHECK(!named, "skip_failed: the failed member has a line");

      corrupt_member = -1;
      CHECK(write_zip(), "skip_failed: could not rewrite the fixture archive");
      full_run();
      printf("[skip]    failed: %u lines at the failure, %u members"
            " written after\n", lines, count_written());
      CHECK(done_ok, "skip_failed: the next run reported an error");
      CHECK(count_written() == n_members - lines,
            "skip_failed: the next run did not write exactly the rest");
      CHECK(count_matching(false) == n_members,
            "skip_failed: contents differ from the archive");
   }

   /* parked: a member stopped part way, by a cancel or by its CRC,
    * with its older file still in place is not taken as written */
   clear_outputs();
   full_run();
   for (k = 0; k < 2; k++)
   {
      member_t *m       = &members[N_SMALL];
      uint32_t old_crc  = encoding_crc32(0, m->data, m->size);
      uint32_t line_crc = 0;

      m->data[BIG_SIZE - 1 - k] ^= 0xff;
      corrupt_member = (k == 0) ? -1 : (int)N_SMALL;
      CHECK(write_zip(), "skip_parked: could not rewrite the fixture archive");

      /* As in keep, the cancel lands once the big member has parked */
      if (k == 0)
         run_extract(STEP_EXHAUSTED, NULL, 10 * n_members, N_SMALL + 3);
      else
         full_run();
      out_path(path, sizeof(path), BIG_NAME);
      CHECK(!done_ok, "skip_parked: the stopped run reported success");
      CHECK(path_is_valid(path) && !file_matches(path, m),
            "skip_parked: the member was not stopped part way");
      CHECK(manifest_crc(BIG_NAME, &line_crc) && line_crc == old_crc,
            "skip_parked: the manifest took the unfinished member as written");

      corrupt_member = -1;
      CHECK(write_zip(), "skip_parked: could not rewrite the fixture archive");
      full_run();
      printf("[skip]    parked, %s: %u members written after\n",
            k == 0 ? "cancel" : "bad CRC", count_written());
      CHECK(done_ok, "skip_parked: the next run reported an error");
      CHECK(count_written() == 1 && written[N_SMALL],
            "skip_parked: not exactly the unfinished member was written");
      CHECK(file_matches(path, m),
            "skip_parked: the older file was left in place");
   }
   CHECK(count_matching(false) == n_members,
         "skip_parked: contents differ from the archive");

   /* no_mtime: nothing is skipped or recorded, for a stat or two a run */
   {
      unsigned calls[3];

      clear_outputs();
      fail_mtime = true;
      for (k = 0; k < 2; k++)
      {
         full_run();
         calls[k] = mtime_calls;
         CHECK(done_ok, "skip_no_mtime: the run reported an error");
         CHECK(count_written() == n_members,
               "skip_no_mtime: members were skipped without an mtime");
         CHECK(!path_is_valid(mpath),
               "skip_no_mtime: a manifest was written without mtimes");
         CHECK(mtime_calls <= 1,
               "skip_no_mtime: more than one stat without a manifest");
      }

      fail_mtime = false;
      full_run();
      CHECK(manifest_lines() == n_members,
            "skip_no_mtime: no manifest to distrust");
      fail_mtime = true;
      full_run();
      fail_mtime = false;
      calls[2]   = mtime_calls;
      printf("[skip]    no_mtime: %u, %u and %u stats a run\n",
            calls[0], calls[1], calls[2]);
      CHECK(done_ok, "skip_no_mtime: the run reported an error");
      CHECK(count_written() == n_members,
            "skip_no_mtime: a line was trusted without an mtime");
      CHECK(mtime_calls <= 2,
            "skip_no_mtime: more than two stats with a manifest");
      CHECK(count_matching(false) == n_members,
            "skip_no_mtime: contents differ from the archive");
   }

   /* unwritable: the extraction does not depend on the manifest */
   clear_outputs();
   fail_manifest_writes = true;
   for (k = 0; k < 2; k++)
   {
      full_run();
      printf("[skip]    unwritable %u: %u members written\n",
            k, count_written());
      CHECK(done_ok, "skip_unwritable: the run reported an error");
      CHECK(count_written() == n_members,
            "skip_unwritable: members were skipped without a manifest");
      CHECK(!path_is_valid(mpath) && !path_is_valid(mtmp),
            "skip_unwritable: a manifest or its temporary file was left");
   }
   fail_manifest_writes = false;
   CHECK(count_matching(false) == n_members,
         "skip_unwritable: contents differ from the archive");

   /* no_crc: without a CRC a changed member of the same size cannot
    * be told from an unchanged one, so nothing is skipped or recorded */
   {
      static const uint8_t zst[2][ZST_SIZE + 1] =
         { "the first bytes.", "the other bytes." };
      char zman[PATH_MAX_LENGTH];
      char ztmp[PATH_MAX_LENGTH + 8];
      member_t zm;

      clear_outputs();
      out_path(path, sizeof(path), ZST_MEMBER);
      out_path(zman, sizeof(zman), "." FIXTURE_ZST ".crc");
      snprintf(ztmp, sizeof(ztmp), "%s.tmp", zman);
      memset(&zm, 0, sizeof(zm));
      zm.size     = ZST_SIZE;
      fixture_src = FIXTURE_ZST;
      for (k = 0; k < 2; k++)
      {
         zm.data = (uint8_t*)zst[k];
         CHECK(write_zst(zst[k]),
               "skip_no_crc: could not write the fixture archive");
         checks = full_run();
         printf("[skip]    no_crc %u: %u member writes, %u manifest writes\n",
               k, member_writes, manifest_writes);
         CHECK(checks && done_ok, "skip_no_crc: the run reported an error");
         CHECK(member_writes > 0,
               "skip_no_crc: a member without a CRC was skipped");
         CHECK(file_matches(path, &zm),
               "skip_no_crc: contents differ from the archive");
         CHECK(manifest_writes == 0 && !path_is_valid(zman),
               "skip_no_crc: a manifest was kept for members without a CRC");
      }
      fixture_src = FIXTURE_ZIP;
      filestream_delete(path);
      filestream_delete(zman);
      filestream_delete(ztmp);
      filestream_delete(FIXTURE_ZST);
   }

   opt_in = false;
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

   lane_skip(n_a);

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
