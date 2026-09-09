/* The run-ahead core-copy protocol, driven directly.
 *
 * The fragment of runahead.c between its BEGIN/END markers is
 * extracted verbatim at build time and included here. The file
 * system it copies through is a stub that records every destination
 * path it is asked to open and can be told to refuse writes, so the
 * temporary-name generation is observable; the task queue is the
 * real one built without threads, so a pushed task runs when this
 * thread calls task_queue_check(). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <boolean.h>
#include <streams/file_stream.h>
#include <queues/task_queue.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include "../../runloop.h"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* --- the file system, stubbed ------------------------------------------ */

#define MAX_DST 64
static char     *dst_paths[MAX_DST];
static unsigned  dst_count;
static unsigned  refuse_writes;      /* refuse the next N write opens */
static unsigned  deleted;
static char      last_deleted[512];

struct RFILE { int is_src; size_t pos; };

RFILE *filestream_open(const char *path, unsigned mode, unsigned hints)
{
   RFILE *f;
   (void)hints;
   if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
   {
      if (dst_count < MAX_DST)
         dst_paths[dst_count++] = strdup(path);
      if (refuse_writes)
      {
         refuse_writes--;
         return NULL;
      }
      f = (RFILE*)calloc(1, sizeof(*f));
      f->is_src = 0;
      return f;
   }
   f = (RFILE*)calloc(1, sizeof(*f));
   f->is_src = 1;
   return f;
}
int64_t filestream_read(RFILE *f, void *buf, int64_t len)
{
   /* a 1000-byte source */
   int64_t left = 1000 - (int64_t)f->pos;
   if (left <= 0) return 0;
   if (len > left) len = left;
   memset(buf, 0x5A, (size_t)len);
   f->pos += (size_t)len;
   return len;
}
int64_t filestream_write(RFILE *f, const void *buf, int64_t len) { (void)f; (void)buf; return len; }
int filestream_close(RFILE *f) { free(f); return 0; }
int filestream_delete(const char *path) { deleted++; strlcpy(last_deleted, path, sizeof(last_deleted)); return 0; }
bool path_mkdir(const char *dir) { (void)dir; return true; }

/* The two static helpers the fragment leans on from above it. The
 * temp dir is the one the harness names. */
static void strcat_alloc(char **dst, const char *s)
{
   size_t dl = *dst ? strlen(*dst) : 0, sl = s ? strlen(s) : 0;
   char *tmp = (char*)realloc(*dst, dl + sl + 1);
   if (!tmp) return;
   memcpy(tmp + dl, s ? s : "", sl + 1);
   *dst = tmp;
}
static char *get_tmpdir_alloc(const char *override_dir)
{
   return strdup(override_dir ? override_dir : "/tmpdir");
}

/* The fragment, verbatim from runahead.c. */
#include "runahead_copy_fragment.c"
/* The preempt analog-mask bit rule, verbatim from runahead.c. */
#include "preempt_mask_fragment.c"

/* RA-01: an (index, id) with no bit is rejected, never folded onto a
 * bit that exists; the valid ones land where the reader looks. */
static void t_analog_mask_bit(void)
{
   unsigned bit;
   printf("   shift_reject: analog (index, id) pairs with no bit are refused\n");
   CHECK(preempt_analog_mask_bit(0, 0, &bit) && bit == 0, "left X");
   CHECK(preempt_analog_mask_bit(0, 1, &bit) && bit == 1, "left Y");
   CHECK(preempt_analog_mask_bit(1, 0, &bit) && bit == 2, "right X");
   CHECK(preempt_analog_mask_bit(1, 1, &bit) && bit == 3, "right Y");
   CHECK(preempt_analog_mask_bit(2, 0, &bit) && bit == 4, "button 0");
   CHECK(preempt_analog_mask_bit(2, 15, &bit) && bit == 19, "button 15");
   CHECK(!preempt_analog_mask_bit(2, 16, &bit), "button 16 has no bit");
   CHECK(!preempt_analog_mask_bit(0, 2, &bit), "stick id 2 has no bit");
   CHECK(!preempt_analog_mask_bit(3, 0, &bit), "index 3 has no bit");
   CHECK(!preempt_analog_mask_bit(~0u, 0, &bit), "index ~0 has no bit");
   CHECK(!preempt_analog_mask_bit(0, ~0u, &bit), "id ~0 has no bit");
   /* The old expression: id + index * 2 with index 16 and id 0 is a
    * shift of 32, undefined; with index 15, id 1, bit 31 aliased a
    * button. Neither is a bit now. */
   CHECK(!preempt_analog_mask_bit(16, 0, &bit), "(16, 0) has no bit");
   CHECK(!preempt_analog_mask_bit(15, 1, &bit), "(15, 1) has no bit");
}

static void reset_fs(void)
{
   unsigned i;
   for (i = 0; i < dst_count; i++) free(dst_paths[i]);
   dst_count = 0; refuse_writes = 0; deleted = 0; last_deleted[0] = '\0';
}

/* --- cases ------------------------------------------------------------ */

/* RA-02: successive attempts must try distinct names. Locked once the
 * generator advances; until then reported, not failed. */
static bool names_advance = true;

static void t_lcg_names(void)
{
   char *tmp = NULL;
   bool ok;
   unsigned i, distinct = 0;
   printf("   lcg_collision_walk: the first four candidates refused, the fifth taken\n");
   reset_fs();
   refuse_writes = 4;
   tmp = strdup("core_libretro.so");
   ok = copy_file_with_random_name(&tmp, "/tmpdir", "/cores/core_libretro.so");
   CHECK(ok, "the copy did not succeed once a candidate was free");
   CHECK(dst_count == 5, "%u candidates tried, expected 5", dst_count);
   for (i = 1; i < dst_count; i++)
      if (strcmp(dst_paths[i], dst_paths[0]) != 0) distinct++;
   if (names_advance)
      CHECK(distinct == dst_count - 1, "only %u of %u candidates differed from the first", distinct, dst_count - 1);
   else if (distinct != dst_count - 1)
      printf("      (candidates did not advance: %u of %u distinct - generator not yet fixed)\n", distinct + 1, dst_count);
   CHECK(tmp && strstr(tmp, "/tmpdir/tmp") && strstr(tmp, ".so"), "the chosen path is %s", tmp ? tmp : "(null)");
   free(tmp);
}

static void t_lcg_all_fail(void)
{
   char *tmp = strdup("core_libretro.so");
   bool ok;
   printf("   lcg_all_fail: every candidate refused\n");
   reset_fs();
   refuse_writes = 1000;
   ok = copy_file_with_random_name(&tmp, "/tmpdir", "/cores/core_libretro.so");
   CHECK(!ok, "the copy reported success with every write refused");
   CHECK(dst_count == 30, "%u attempts, expected 30", dst_count);
   free(tmp);
}

/* The protocol: poll pushes a task; the task runs in task_queue_check;
 * the callback publishes; the next poll returns the path. */
static void t_copy_protocol(void)
{
   char *out = NULL;
   enum runahead_copy_status st;
   printf("   copy protocol: pending, then ready, one task at a time\n");
   reset_fs();
   task_queue_init(false, NULL);
   st = runahead_copy_poll("/cores/a_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_PENDING, "first poll returned %d, not PENDING", (int)st);
   CHECK(runahead_copy_task_pending, "a task was not marked pending");
   st = runahead_copy_poll("/cores/a_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_PENDING, "second poll before the task ran returned %d", (int)st);
   task_queue_check();                                     /* the task runs, the callback publishes */
   CHECK(!runahead_copy_task_pending, "pending still set after the task ran");
   CHECK(dst_count == 1, "%u copies made for one poll sequence", dst_count);
   st = runahead_copy_poll("/cores/a_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_READY && out != NULL, "poll after completion returned %d", (int)st);
   if (out) free(out);
   task_queue_deinit();
}

static void t_generation_discard(void)
{
   char *out = NULL;
   enum runahead_copy_status st;
   printf("   copy_generation_discard: a reset while the task is in flight\n");
   reset_fs();
   task_queue_init(false, NULL);
   st = runahead_copy_poll("/cores/b_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_PENDING, "poll returned %d", (int)st);
   runahead_copy_reset(true);          /* generation bumps; the in-flight result must be dropped */
   task_queue_check();
   CHECK(!runahead_copy_slot_done, "a stale result was published after a reset");
   CHECK(deleted >= 1, "the stale temp file was not deleted");
   st = runahead_copy_poll("/cores/b_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_PENDING, "poll after a discarded result returned %d, not a fresh PENDING", (int)st);
   task_queue_check();
   st = runahead_copy_poll("/cores/b_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_READY, "the fresh copy did not complete: %d", (int)st);
   if (out) free(out);
   task_queue_deinit();
}

static void t_stale_src(void)
{
   char *out = NULL;
   enum runahead_copy_status st;
   printf("   stale_src_reset: a result for core A is dropped when core B is polled\n");
   reset_fs();
   task_queue_init(false, NULL);
   runahead_copy_poll("/cores/a_libretro.so", "/tmpdir", &out);
   task_queue_check();
   CHECK(runahead_copy_slot_done, "core A's copy did not publish");
   st = runahead_copy_poll("/cores/b_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_PENDING, "polling core B returned %d, not PENDING", (int)st);
   CHECK(deleted >= 1, "core A's temp file was not deleted");
   task_queue_check();
   st = runahead_copy_poll("/cores/b_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_READY, "core B's copy did not complete: %d", (int)st);
   if (out) free(out);
   task_queue_deinit();
}

int main(void)
{
   /* A stray 'return false' from a status-returning function must
    * read as UNAVAILABLE. */
   { typedef char enum_false_is_unavailable[(RUNAHEAD_COPY_UNAVAILABLE == 0) ? 1 : -1]; (void)sizeof(enum_false_is_unavailable); }

   printf("runahead copy:\n");
   t_analog_mask_bit();
   t_lcg_names();
   t_lcg_all_fail();
   t_copy_protocol();
   t_generation_discard();
   t_stale_src();
   reset_fs();
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("runahead copy: names advance, one task at a time, stale results dropped\n");
   return 0;
}
