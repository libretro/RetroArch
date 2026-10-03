/* The run-ahead core-copy protocol, driven directly.
 *
 * The fragment of runahead.c between its BEGIN/END markers is
 * extracted verbatim at build time and included here. The VFS copy
 * it runs through is a stub that records every destination path it
 * is asked to open and can be told to refuse the open, so the
 * temporary-name generation is observable, and that hands its bytes
 * out one bounded step at a time, so the per-pass stepping is too;
 * the task queue and the shared I/O window are the real ones, the
 * queue built without threads so a pushed task runs when this thread
 * calls task_queue_check(). */

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
#include "../../input/input_defines.h"
#include "../../runahead.h"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* --- the file system, stubbed ------------------------------------------ */

#define MAX_DST 64
/* Ten megabytes of source, so the 4MB step bound is reached more than
 * once and a copy cannot finish in a single step. */
#define COPY_SRC_BYTES  ((int64_t)10 * 1024 * 1024)

static char     *dst_paths[MAX_DST];
static unsigned  dst_count;
static unsigned  refuse_opens;       /* refuse the next N copy opens */
static unsigned  deleted;
static char      last_deleted[512];
static unsigned  copy_steps;         /* filestream_copy_step calls    */
static int64_t   copy_step_bound;    /* max_bytes of the last step    */
static unsigned  copy_closed_unfinished;

struct retro_vfs_copy_handle { int64_t left; bool done; };

struct retro_vfs_copy_handle *filestream_copy_begin(const char *src_path,
      const char *dst_path, unsigned flags)
{
   struct retro_vfs_copy_handle *c;
   (void)src_path;
   (void)flags;
   if (dst_count < MAX_DST)
      dst_paths[dst_count++] = strdup(dst_path);
   if (refuse_opens)
   {
      refuse_opens--;
      return NULL;
   }
   c       = (struct retro_vfs_copy_handle*)calloc(1, sizeof(*c));
   c->left = COPY_SRC_BYTES;
   return c;
}

int filestream_copy_step(struct retro_vfs_copy_handle *c, int64_t max_bytes,
      int64_t *bytes_done, int64_t *bytes_total)
{
   int64_t n = (max_bytes > 0 && max_bytes < c->left) ? max_bytes : c->left;
   copy_steps++;
   copy_step_bound = max_bytes;
   c->left        -= n;
   if (bytes_total)
      *bytes_total = COPY_SRC_BYTES;
   if (bytes_done)
      *bytes_done  = COPY_SRC_BYTES - c->left;
   if (c->left > 0)
      return RETRO_VFS_COPY_RUNNING;
   c->done = true;
   return RETRO_VFS_COPY_DONE;
}

/* Closing an unfinished copy removes the partial destination, which
 * the real VFS does itself rather than through filestream_delete -
 * counted separately here for the same reason. */
int filestream_copy_close(struct retro_vfs_copy_handle *c)
{
   int ret;
   if (!c)
      return -1;
   ret = c->done ? 0 : -1;
   if (!c->done)
      copy_closed_unfinished++;
   free(c);
   return ret;
}

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
/* The dense input cache and the preempt slab, verbatim from runahead.c. */
#include "dense_cache_fragment.c"
#include "preempt_slab_fragment.c"

/* The dense cache: every common tuple has a slot, distinct per tuple;
 * everything else is refused to the list; a set then get reads back. */
static void t_dense_cache(void)
{
   runahead_dense_cache_t *c = (runahead_dense_cache_t*)calloc(1, sizeof(*c));
   unsigned port, id, index;
   int16_t *slot;
   printf("   input_set_get: the dense cache's slots and its refusals\n");
   for (port = 0; port < MAX_USERS; port++)
   {
      for (id = 0; id < 16; id++)
      {
         slot = runahead_dense_slot(c, port, RETRO_DEVICE_JOYPAD, 0, id);
         CHECK(slot != NULL, "joypad port %u id %u has no slot", port, id);
         if (slot) *slot = (int16_t)(port * 100 + id);
      }
      slot = runahead_dense_slot(c, port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
      CHECK(slot != NULL, "joypad port %u mask has no slot", port);
      for (index = 0; index < 3; index++)
         for (id = 0; id < 16; id++)
            CHECK(runahead_dense_slot(c, port, RETRO_DEVICE_ANALOG, index, id) != NULL,
                  "analog port %u index %u id %u has no slot", port, index, id);
   }
   for (port = 0; port < MAX_USERS; port++)
      for (id = 0; id < 16; id++)
         CHECK(*runahead_dense_slot(c, port, RETRO_DEVICE_JOYPAD, 0, id) == (int16_t)(port * 100 + id),
               "joypad port %u id %u read back wrong", port, id);
   /* Distinct: the mask slot is not a button's, analog is not joypad's. */
   CHECK(runahead_dense_slot(c, 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK)
         != runahead_dense_slot(c, 0, RETRO_DEVICE_JOYPAD, 0, 15), "the mask shares a button's slot");
   CHECK(runahead_dense_slot(c, 0, RETRO_DEVICE_ANALOG, 0, 0)
         != runahead_dense_slot(c, 0, RETRO_DEVICE_JOYPAD, 0, 0), "analog shares joypad's slot");
   /* Refused to the list: subclassed devices, other indexes and ids,
    * ports past the table, keyboards and pointers. */
   CHECK(!runahead_dense_slot(c, 0, RETRO_DEVICE_JOYPAD, 1, 0), "joypad index 1 was given a slot");
   CHECK(!runahead_dense_slot(c, 0, RETRO_DEVICE_JOYPAD, 0, 16), "joypad id 16 was given a slot");
   CHECK(!runahead_dense_slot(c, 0, RETRO_DEVICE_JOYPAD, 0, 255), "joypad id 255 was given a slot");
   CHECK(!runahead_dense_slot(c, 0, RETRO_DEVICE_ANALOG, 3, 0), "analog index 3 was given a slot");
   CHECK(!runahead_dense_slot(c, 0, RETRO_DEVICE_ANALOG, 0, 16), "analog id 16 was given a slot");
   CHECK(!runahead_dense_slot(c, MAX_USERS, RETRO_DEVICE_JOYPAD, 0, 0), "port MAX_USERS was given a slot");
   CHECK(!runahead_dense_slot(c, 0, RETRO_DEVICE_KEYBOARD, 0, 0), "a keyboard was given a slot");
   CHECK(!runahead_dense_slot(c, 0, RETRO_DEVICE_POINTER, 0, 0), "a pointer was given a slot");
   CHECK(!runahead_dense_slot(c, 0, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1), 0, 0), "a subclassed joypad was given a slot");
   CHECK(!runahead_dense_slot(NULL, 0, RETRO_DEVICE_JOYPAD, 0, 0), "a NULL cache gave a slot");
   /* A frame of polls as a core makes them: informational timing. */
   {
      struct timespec t0, t1;
      unsigned rep, polls = 0;
      volatile int16_t sink = 0;
      clock_gettime(CLOCK_MONOTONIC, &t0);
      for (rep = 0; rep < 20000; rep++)
         for (port = 0; port < 2; port++)
            for (id = 0; id < 16; id++)
            {
               int16_t *sl = runahead_dense_slot(c, port, RETRO_DEVICE_JOYPAD, 0, id);
               sink += *sl; *sl = sink; polls++;
            }
      clock_gettime(CLOCK_MONOTONIC, &t1);
      printf("      %.1f ns per poll, two joypads, get then set (informational)\n",
            ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / polls);
   }
   free(c);
}

/* The slab: N buffers inside one allocation, contiguous at state_size
 * strides, freed by freeing the first; an impossible product refused. */
static void t_preempt_slab(void)
{
   void *buffer[MAX_RUNAHEAD_FRAMES];
   unsigned i;
   size_t ss = 1000;
   printf("   preempt_slab: one allocation for all frames\n");
   memset(buffer, 0, sizeof(buffer));
   CHECK(preempt_slab_alloc(buffer, 6, ss), "a slab of six was refused");
   for (i = 1; i < 6; i++)
      CHECK((uint8_t*)buffer[i] == (uint8_t*)buffer[0] + i * ss, "buffer %u is not at its stride", i);
   for (i = 0; i < 6; i++)
      memset(buffer[i], (int)i, ss);
   for (i = 0; i < 6; i++)
      CHECK(((uint8_t*)buffer[i])[ss - 1] == (uint8_t)i, "buffer %u's bytes were shared", i);
   free(buffer[0]);
   CHECK(!preempt_slab_alloc(buffer, 0, ss), "zero frames was accepted");
   CHECK(!preempt_slab_alloc(buffer, 6, 0), "a zero state size was accepted");
   CHECK(!preempt_slab_alloc(buffer, 6, ((size_t)-1) / 2), "a product past SIZE_MAX was accepted");
}

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
   dst_count = 0; refuse_opens = 0; deleted = 0; last_deleted[0] = '\0';
   copy_steps = 0; copy_step_bound = 0; copy_closed_unfinished = 0;
}

/* The handler opens on its first pass and only then moves bytes, so a
 * copy is never done in one pass; run the queue until the callback
 * that frees the pipeline has run. */
static void run_copy_to_completion(void)
{
   unsigned pass;
   for (pass = 0; pass < 16 && runahead_copy_task_pending; pass++)
      task_queue_check();
}

/* --- cases ------------------------------------------------------------ */

/* RA-02: successive attempts must try distinct names. Locked once the
 * generator advances; until then reported, not failed. */
static bool names_advance = true;

static void t_lcg_names(void)
{
   char *tmp = NULL;
   struct retro_vfs_copy_handle *copy;
   unsigned i, distinct = 0;
   printf("   lcg_collision_walk: the first four candidates refused, the fifth taken\n");
   reset_fs();
   refuse_opens = 4;
   tmp = strdup("core_libretro.so");
   copy = copy_begin_with_random_name(&tmp, "/tmpdir", "/cores/core_libretro.so");
   CHECK(copy != NULL, "no copy was opened once a candidate was free");
   CHECK(dst_count == 5, "%u candidates tried, expected 5", dst_count);
   CHECK(copy_steps == 0, "%u bytes-moving steps ran inside the open", copy_steps);
   for (i = 1; i < dst_count; i++)
      if (strcmp(dst_paths[i], dst_paths[0]) != 0) distinct++;
   if (names_advance)
      CHECK(distinct == dst_count - 1, "only %u of %u candidates differed from the first", distinct, dst_count - 1);
   else if (distinct != dst_count - 1)
      printf("      (candidates did not advance: %u of %u distinct - generator not yet fixed)\n", distinct + 1, dst_count);
   CHECK(tmp && strstr(tmp, "/tmpdir/tmp") && strstr(tmp, ".so"), "the chosen path is %s", tmp ? tmp : "(null)");
   if (copy)
      filestream_copy_close(copy);
   free(tmp);
}

static void t_lcg_all_fail(void)
{
   char *tmp = strdup("core_libretro.so");
   struct retro_vfs_copy_handle *copy;
   printf("   lcg_all_fail: every candidate refused\n");
   reset_fs();
   refuse_opens = 1000;
   copy = copy_begin_with_random_name(&tmp, "/tmpdir", "/cores/core_libretro.so");
   CHECK(!copy, "a copy was opened with every candidate refused");
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
   task_queue_check();                                     /* pass one: the destination is opened */
   CHECK(runahead_copy_task_pending, "the task finished in the pass that only opens the copy");
   CHECK(dst_count == 1, "%u copies opened for one poll sequence", dst_count);
   CHECK(copy_steps == 0, "%u bytes-moving steps ran in the opening pass", copy_steps);
   run_copy_to_completion();                               /* the copy runs, the callback publishes */
   CHECK(!runahead_copy_task_pending, "pending still set after the task ran");
   CHECK(dst_count == 1, "%u copies made for one poll sequence", dst_count);
   /* Every step is bounded: a 10MB source cannot be one 10MB call. */
   CHECK(copy_steps >= 3, "a %lldMB source was moved in %u step(s)",
         (long long)(COPY_SRC_BYTES >> 20), copy_steps);
   CHECK(copy_step_bound == RUNAHEAD_COPY_STEP_BYTES,
         "a step was offered %lld bytes, not the step bound", (long long)copy_step_bound);
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
   run_copy_to_completion();
   CHECK(!runahead_copy_slot_done, "a stale result was published after a reset");
   CHECK(deleted >= 1, "the stale temp file was not deleted");
   st = runahead_copy_poll("/cores/b_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_PENDING, "poll after a discarded result returned %d, not a fresh PENDING", (int)st);
   run_copy_to_completion();
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
   run_copy_to_completion();
   CHECK(runahead_copy_slot_done, "core A's copy did not publish");
   st = runahead_copy_poll("/cores/b_libretro.so", "/tmpdir", &out);
   CHECK(st == RUNAHEAD_COPY_PENDING, "polling core B returned %d, not PENDING", (int)st);
   CHECK(deleted >= 1, "core A's temp file was not deleted");
   run_copy_to_completion();
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
   t_dense_cache();
   t_preempt_slab();
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
