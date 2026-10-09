/* Cloud sync's diff is paced by the shared per-frame I/O window.
 *
 * Every sync hashes every local file it covers. The diff used to take
 * one file per run of the task handler and hash it whole there, so a
 * large file - a big savestate, a memory card, a BIOS under "sync
 * system" - held one run of the handler, which with Threaded Tasks off
 * is the frame loop, for as long as reading and hashing it took; and a
 * thousand small files took a thousand frames.
 *
 * The diff now runs as many steps as task_nbio_slice_within_budget()
 * grants, and hashes the current file a 256 KiB chunk at a time between
 * steps, carrying a file over to the next run when the window closes.
 * Here the window is a stub granting a fixed number of work units per
 * run, and the real tasks/task_cloudsync.c handler is driven through a
 * diff of files whose hashes the server already has:
 *
 *  - a 9 MiB file is hashed over many runs, never more than the grant
 *    per run, and its hash matches a one-shot MD5 of the file;
 *  - small files go several to a run instead of one per run;
 *  - with a window that is always spent, every run still makes
 *    progress (the floor) and the diff finishes;
 *  - the result is the same as before: every file found unchanged,
 *    nothing transferred;
 *  - a 9 MiB download, reported from another thread, is hashed by the
 *    task thread over many runs within the grant, and its hash goes
 *    into both updated manifests (the fetch callback used to hash the
 *    whole file itself, on the thread it ran on);
 *  - the directory walk that lists the local files runs as many entries
 *    per run as the grant allows, at least one, instead of reading every
 *    synced directory in one run, and lists what dir_list_new() listed:
 *    nested and hidden files, ignored names left out, two roots; a root
 *    that cannot be read still stops the sync;
 *  - pushed on the task queue and run to its end, the sync releases
 *    everything it held (LeakSanitizer): the state used to be left
 *    behind by every sync, because it was freed from a callback that
 *    was handed task_data, which the task never set.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include <rthreads/rthreads.h>
#include <retro_timers.h>

#include "../../../tasks/task_cloudsync.c"

/* The locked hand-over (TASK_CLOUDSYNC_FORCE_LOCK, or a backend with no
 * pointer atomics) appends under the lock task_push_cloud_sync() makes;
 * these tests make it themselves. */
static void tcs_test_lock(void)
{
#if defined(HAVE_THREADS) && !defined(TCS_LOCK_FREE)
   if (!tcs_manifest_lock)
      tcs_manifest_lock = slock_new();
#endif
}

static unsigned transfers;

/* --- what the task links against and this test never reaches ---------- */

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
bool verbosity_is_enabled(void)       { return false; }
settings_t *config_get_ptr(void)      { return NULL; }
char *dir_get_ptr(enum rarch_dir_type type) { (void)type; return NULL; }
/* The file a running core owns, if any. */
static char live_path[256];
bool content_savefile_is_live(const char *path)
{
   return *live_path && path && !strcmp(path, live_path);
}
void task_window_progress_cb(retro_task_t *task) { (void)task; }
size_t fill_pathname_application_special(char *s, size_t len,
      enum application_special_type type)
{ (void)s; (void)len; (void)type; return 0; }
void cloud_sync_find_driver(const char *drv, const char *prefix,
      bool verbosity_enabled)
{ (void)drv; (void)prefix; (void)verbosity_enabled; }
void cloud_sync_capture(void) { }
bool cloud_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{ (void)cb; (void)user_data; return false; }
/* Nonzero: the driver ends the sync, and reports it at once. */
static int end_ok;
bool cloud_sync_end(cloud_sync_complete_handler_t cb, void *user_data)
{
   if (!end_ok)
      return false;
   cb(user_data, NULL, true, NULL);
   return true;
}
/* A fetch writes @fetch_size bytes to the local file and reports it from
 * another thread, as a WebDAV or S3 transfer reports from the main
 * thread while the task runs on the queue's worker. Unset, a fetch is
 * an unexpected transfer. */
static size_t fetch_size;
static retro_atomic_int_t fetches_reported;

typedef struct
{
   cloud_sync_complete_handler_t cb;
   void *ud;
   char  key[64];
   char  file[128];
} fetch_job_t;

static void write_file(const char *path, size_t size, unsigned seed);

/* Nonzero: a fetch writes half its bytes and then reports failure, as
 * a transfer cut off part-way. */
static int fetch_fail;

static void fetch_thread(void *data)
{
   fetch_job_t job = *(fetch_job_t*)data;
   RFILE      *f;
   free(data);
   if (fetch_fail)
   {
      write_file(job.file, fetch_size / 2, 9);
      job.cb(job.ud, job.key, false, NULL);
      retro_atomic_inc_int(&fetches_reported);
      return;
   }
   write_file(job.file, fetch_size, 9);
   f = filestream_open(job.file, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   job.cb(job.ud, job.key, f != NULL, f);
   retro_atomic_inc_int(&fetches_reported);
}

bool cloud_sync_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   fetch_job_t *job;
   sthread_t   *t;
   if (!fetch_size || !(job = (fetch_job_t*)calloc(1, sizeof(*job))))
   {
      transfers++;
      return false;
   }
   job->cb = cb;
   job->ud = user_data;
   strlcpy(job->key,  path, sizeof(job->key));
   strlcpy(job->file, file, sizeof(job->file));
   if (!(t = sthread_create(fetch_thread, job)))
   {
      free(job);
      return false;
   }
   sthread_detach(t);
   return true;
}
bool cloud_sync_update(const char *path, RFILE *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{ (void)path; (void)file; (void)cb; (void)user_data; transfers++; return false; }
bool cloud_sync_free(const char *path, cloud_sync_complete_handler_t cb,
      void *user_data)
{ (void)path; (void)cb; (void)user_data; transfers++; return false; }
void cloud_sync_poll(cloud_sync_poll_budget_t within, void *budget) { (void)within; (void)budget; }

/* --- the I/O window: @grant units per run on top of the floor ------- */

static unsigned grant;
static unsigned granted;      /* units handed out in the open run */
static unsigned max_granted;  /* most units any run used */
static unsigned runs;

void task_nbio_slice_open(nbio_budget_t *b)
{
   b->floor = 1;
   granted  = 0;
   runs++;
}

void task_nbio_slice_close(nbio_budget_t *b)
{
   (void)b;
   if (granted > max_granted)
      max_granted = granted;
}

bool task_nbio_slice_within_budget(void *ud, size_t avail, size_t len)
{
   nbio_budget_t *b = (nbio_budget_t*)ud;
   (void)avail; (void)len;
   if (b->floor)
   {
      b->floor = 0;
      granted++;
      return true;
   }
   if (granted < grant + 1)
   {
      granted++;
      return true;
   }
   return false;
}

/* --- the test ----------------------------------------------------------- */

static int fails;

static void check(const char *what, int ok)
{
   printf("  %-62s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

static char dir[64];

static void write_file(const char *path, size_t size, unsigned seed)
{
   FILE   *f = fopen(path, "wb");
   size_t  i;
   for (i = 0; f && i < size; i++)
      fputc((int)((i * 131 + seed * 7 + (i >> 9)) & 0xff), f);
   if (f)
      fclose(f);
}

static char *hash_of(const char *path)
{
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   char  *h = f ? task_cloud_sync_md5_rfile(f) : NULL;
   if (f)
      filestream_close(f);
   return h;
}

static void list_add(file_list_t *list, const char *key, const char *path,
      char *hash)
{
   size_t idx = list->size;
   file_list_append(list, path, NULL, 0, 0, 0);
   file_list_set_alt_at_offset(list, idx, key);
   list->list[idx].userdata = hash;
}

/* A sync at the diff with @n files of @size bytes, all unchanged on the
 * server since the last sync. Runs the handler until the diff ends and
 * returns the number of runs. */
static unsigned run_diff(unsigned n, size_t size, unsigned grant_units,
      int *hashes_ok)
{
   task_cloud_sync_state_t *st   = (task_cloud_sync_state_t*)calloc(1, sizeof(*st));
   retro_task_t            *task = task_init();
   struct string_list      *dl   = string_list_new();
   union string_list_elem_attr attr;
   unsigned                 i;

   attr.i = 0;
   string_list_append(dl, "saves", attr);
   dl->elems[0].userdata = strdup(dir);
   st->dirlist                 = dl;
   st->server_manifest         = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->local_manifest          = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->current_manifest        = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_server_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_local_manifest  = (file_list_t*)calloc(1, sizeof(file_list_t));
   retro_atomic_int_init(&st->waiting, 0);
   retro_atomic_int_init(&st->phase, (int)CLOUD_SYNC_PHASE_DIFF);

   for (i = 0; i < n; i++)
   {
      char key[64], path[128];
      snprintf(key,  sizeof(key),  "saves/f%04u.srm", i);
      snprintf(path, sizeof(path), "%s/f%04u.srm", dir, i);
      write_file(path, size, i);
      list_add(st->server_manifest,  key, NULL, hash_of(path));
      list_add(st->local_manifest,   key, NULL, hash_of(path));
      list_add(st->current_manifest, key, path, NULL);
   }

   task->state   = st;
   task->handler = task_cloud_sync_task_handler;
   grant = grant_units; runs = 0; max_granted = 0; transfers = 0;
   for (i = 0; i < 100000
         && task_cloud_sync_phase_get(st) == CLOUD_SYNC_PHASE_DIFF; i++)
      task_cloud_sync_task_handler(task);

   *hashes_ok = task_cloud_sync_phase_get(st) != CLOUD_SYNC_PHASE_DIFF;
   for (i = 0; i < n; i++)
   {
      const char *a = (const char*)st->current_manifest->list[i].userdata;
      const char *b = (const char*)st->server_manifest->list[i].userdata;
      if (!a || !b || strcmp(a, b))
         *hashes_ok = 0;
   }
   task_cloud_sync_fold_manifest_adds(st);
   if (     st->updated_server_manifest->size != n
         || st->updated_local_manifest->size  != n)
      *hashes_ok = 0;

   for (i = 0; i < n; i++)
   {
      char path[128];
      snprintf(path, sizeof(path), "%s/f%04u.srm", dir, i);
      unlink(path);
   }
   task_cloud_sync_cleanup(task);
   free(task->title);
   free(task);
   return runs;
}

/* One file on the server only: fetched, then hashed by the task. */
static void fetch_lane(void)
{
   task_cloud_sync_state_t *st   = (task_cloud_sync_state_t*)calloc(1, sizeof(*st));
   retro_task_t            *task = task_init();
   struct string_list      *dl   = string_list_new();
   union string_list_elem_attr attr;
   char     path[128];
   char    *want;
   unsigned i;
   int      ok_server = 0, ok_local = 0;

   attr.i = 0;
   string_list_append(dl, "saves", attr);
   dl->elems[0].userdata = strdup(dir);
   st->dirlist                 = dl;
   st->server_manifest         = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->local_manifest          = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->current_manifest        = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_server_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_local_manifest  = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->destructive             = true;
   strlcpy(st->dir_core_assets, dir, sizeof(st->dir_core_assets));
   retro_atomic_int_init(&st->waiting, 0);
   retro_atomic_int_init(&st->phase, (int)CLOUD_SYNC_PHASE_DIFF);
   retro_atomic_int_init(&fetches_reported, 0);

   /* what the download will hold, hashed beforehand */
   snprintf(path, sizeof(path), "%s/dl.srm", dir);
   fetch_size = 9 * 1024 * 1024;
   write_file(path, fetch_size, 9);
   want = hash_of(path);
   unlink(path);
   list_add(st->server_manifest, "saves/dl.srm", NULL, strdup(want));

   task_cloud_sync_task_setup(task, st, "Cloud Sync in progress");
   grant = 3; runs = 0; max_granted = 0; transfers = 0;
   /* start the fetch and wait for its report */
   for (i = 0; i < 5000 && !retro_atomic_load_acquire_int(&fetches_reported); i++)
   {
      task_cloud_sync_task_handler(task);
      retro_sleep(1);
   }
   /* the callback has returned: the file is not hashed yet, and one run
    * of the handler hashes only what its grant allows */
   task_cloud_sync_task_handler(task);
   check("download: not hashed by the callback nor in one run",
         task_cloud_sync_waiting_get(st) == 1 && st->downloads == 0);
   for (i = 0; i < 200000 && (task_cloud_sync_phase_get(st) == CLOUD_SYNC_PHASE_DIFF
            || task_cloud_sync_waiting_get(st) > 0); i++)
      task_cloud_sync_task_handler(task);
   check("download: fetched and hashed, nothing in flight",
         task_cloud_sync_waiting_get(st) == 0 && st->downloads == 1);
   check("download: hashed over many runs", runs >= 36 / 4);
   check("download: no run past its grant", max_granted <= 4);
   task_cloud_sync_fold_manifest_adds(st);
   for (i = 0; i < st->updated_server_manifest->size; i++)
      if (!strcmp(st->updated_server_manifest->list[i].alt, "saves/dl.srm"))
         ok_server = st->updated_server_manifest->list[i].userdata
            && !strcmp((const char*)st->updated_server_manifest->list[i].userdata, want);
   for (i = 0; i < st->updated_local_manifest->size; i++)
      if (!strcmp(st->updated_local_manifest->list[i].alt, "saves/dl.srm"))
         ok_local = st->updated_local_manifest->list[i].userdata
            && !strcmp((const char*)st->updated_local_manifest->list[i].userdata, want);
   check("download: its hash in both updated manifests", ok_server && ok_local);
   check("download: matches the server, no manifest upload needed",
         !st->need_manifest_uploaded);
   {
      char  tmp[160];
      char *got = hash_of(path);
      snprintf(tmp, sizeof(tmp), "%s.rafetching", path);
      check("download: lands under its own name, nothing left beside it",
            got && !strcmp(got, want) && access(tmp, F_OK) != 0);
      free(got);
   }

   fetch_size = 0;
   free(want);
   unlink(path);
   task_cloud_sync_cleanup(task);
   free(task->title);
   free(task);
}

/* A local save the server has a newer copy of; the fetch is under way
 * when @what happens. The local file must come through as it was. */
static void fetch_keep_lane(int live, const char *what_keep, const char *what_rest)
{
   task_cloud_sync_state_t *st   = (task_cloud_sync_state_t*)calloc(1, sizeof(*st));
   retro_task_t            *task = task_init();
   struct string_list      *dl   = string_list_new();
   union string_list_elem_attr attr;
   char     path[128], tmp[160];
   char    *old, *srv, *got;
   unsigned i;
   int      carried = 0;

   attr.i = 0;
   string_list_append(dl, "saves", attr);
   dl->elems[0].userdata = strdup(dir);
   st->dirlist                 = dl;
   st->server_manifest         = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->local_manifest          = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->current_manifest        = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_server_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_local_manifest  = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->destructive             = true;
   strlcpy(st->dir_core_assets, dir, sizeof(st->dir_core_assets));
   retro_atomic_int_init(&st->waiting, 0);
   retro_atomic_int_init(&st->phase, (int)CLOUD_SYNC_PHASE_DIFF);
   retro_atomic_int_init(&fetches_reported, 0);

   /* the server's copy, hashed, then the local one in its place */
   snprintf(path, sizeof(path), "%s/keep.srm", dir);
   snprintf(tmp,  sizeof(tmp),  "%s.rafetching", path);
   fetch_size = 64 * 1024;
   write_file(path, fetch_size, 9);
   srv = hash_of(path);
   write_file(path, 4096, 4);
   old = hash_of(path);
   list_add(st->server_manifest, "saves/keep.srm", NULL, strdup(srv));
   fetch_fail = !live;

   task_cloud_sync_task_setup(task, st, "Cloud Sync in progress");
   grant = 100;
   for (i = 0; i < 5000 && !retro_atomic_load_acquire_int(&fetches_reported); i++)
   {
      task_cloud_sync_task_handler(task);
      retro_sleep(1);
   }
   /* the download is in, not yet in place: a core takes the file now */
   if (live)
      strlcpy(live_path, path, sizeof(live_path));
   for (i = 0; i < 200000 && (task_cloud_sync_phase_get(st) == CLOUD_SYNC_PHASE_DIFF
            || task_cloud_sync_waiting_get(st) > 0); i++)
      task_cloud_sync_task_handler(task);

   got = hash_of(path);
   check(what_keep, got && !strcmp(got, old) && access(tmp, F_OK) != 0);
   task_cloud_sync_fold_manifest_adds(st);
   for (i = 0; i < st->updated_server_manifest->size; i++)
      if (!strcmp(st->updated_server_manifest->list[i].alt, "saves/keep.srm"))
         carried = st->updated_server_manifest->list[i].userdata
            && !strcmp((const char*)st->updated_server_manifest->list[i].userdata, srv);
   check(what_rest, st->downloads == 0 && carried
         && (live ? !st->failures : st->failures));

   *live_path = '\0';
   fetch_fail = 0;
   fetch_size = 0;
   free(got);
   free(old);
   free(srv);
   unlink(path);
   unlink(tmp);
   task_cloud_sync_cleanup(task);
   free(task->title);
   free(task);
}

static int key_cmp(const void *a, const void *b)
{
   return strcmp(*(const char*const*)a, *(const char*const*)b);
}

/* The keys dir_list_new() gives for @root under @name, as the manifest
 * build made them before the walk. */
static size_t reference_keys(const char *root, const char *name,
      char **keys, size_t at)
{
   char slash[256], rel[256], alt[256];
   struct string_list *l;
   size_t i;
   strlcpy(slash, root, sizeof(slash));
   fill_pathname_slash(slash, sizeof(slash));
   if (!(l = dir_list_new(slash, NULL, false, true, true, true)))
      return at;
   for (i = 0; i < l->size; i++)
   {
      path_relative_to(rel, l->elems[i].data, slash, sizeof(rel));
      fill_pathname_join_special(alt, name, rel, sizeof(alt));
      if (task_cloud_sync_should_ignore_file(alt))
         continue;
      pathname_make_slashes_portable(alt);
      keys[at++] = strdup(alt);
   }
   string_list_free(l);
   return at;
}

static task_cloud_sync_state_t *walk_state(const char *root_a, const char *root_b)
{
   task_cloud_sync_state_t *st = (task_cloud_sync_state_t*)calloc(1, sizeof(*st));
   struct string_list *dl      = string_list_new();
   union string_list_elem_attr attr;
   attr.i = 0;
   string_list_append(dl, "saves", attr);
   dl->elems[0].userdata = strdup(root_a);
   if (root_b)
   {
      string_list_append(dl, "states", attr);
      dl->elems[1].userdata = strdup(root_b);
   }
   st->dirlist = dl;
   retro_atomic_int_init(&st->waiting, 0);
   retro_atomic_int_init(&st->phase, (int)CLOUD_SYNC_PHASE_BUILD_CURRENT_MANIFEST);
   return st;
}

static unsigned walk_run(task_cloud_sync_state_t *st, retro_task_t *task)
{
   unsigned i;
   task_cloud_sync_task_setup(task, st, "Cloud Sync in progress");
   runs = 0; max_granted = 0;
   for (i = 0; i < 200000
         && task_cloud_sync_phase_get(st) == CLOUD_SYNC_PHASE_BUILD_CURRENT_MANIFEST; i++)
      task_cloud_sync_task_handler(task);
   return runs;
}

static void walk_lane(void)
{
   char      a[160], b[160], path[256], cmd[800];
   char    **keys = (char**)calloc(4000, sizeof(char*));
   char    **got  = (char**)calloc(4000, sizeof(char*));
   size_t    nkeys = 0, i;
   unsigned  r, k;
   int       same;
   task_cloud_sync_state_t *st;
   retro_task_t *task;

   snprintf(a, sizeof(a), "%s/walk_saves", dir);
   snprintf(b, sizeof(b), "%s/walk_states", dir);
   snprintf(cmd, sizeof(cmd), "mkdir -p %s/core1/sub %s/core2 %s/.hidden %s/x", a, a, a, b);
   if (system(cmd) != 0)
      return;
   for (k = 0; k < 2400; k++)
   {
      const char *sub = (k % 4 == 0) ? "core1" : (k % 4 == 1) ? "core1/sub" : (k % 4 == 2) ? "core2" : "";
      snprintf(path, sizeof(path), "%s/%s%sf%04u.srm", a, sub, *sub ? "/" : "", k);
      write_file(path, 16, k);
   }
   for (k = 0; k < 600; k++)
   {
      snprintf(path, sizeof(path), "%s/x/s%04u.state", b, k);
      write_file(path, 16, k);
   }
   snprintf(path, sizeof(path), "%s/.hidden/h.srm", a);      write_file(path, 4, 1);
   snprintf(path, sizeof(path), "%s/.dot.srm", a);           write_file(path, 4, 1);
   snprintf(path, sizeof(path), "%s/core2/p.srm.rafetching", a); write_file(path, 4, 1);
   snprintf(path, sizeof(path), "%s/core2/p.srm.tmp", a);        write_file(path, 4, 1);
   snprintf(path, sizeof(path), "%s/.DS_Store", b);          write_file(path, 4, 1);

   nkeys = reference_keys(a, "saves", keys, 0);
   nkeys = reference_keys(b, "states", keys, nkeys);
   qsort(keys, nkeys, sizeof(*keys), key_cmp);

   st   = walk_state(a, b);
   task = task_init();
   grant = 20;
   r = walk_run(st, task);
   check("walk: reaches the diff", task_cloud_sync_phase_get(st) == CLOUD_SYNC_PHASE_DIFF);
   check("walk: spread over runs, not one", r >= (unsigned)(nkeys / 21));
   check("walk: no run past its grant", max_granted <= 21);
   same = st->current_manifest && st->current_manifest->size == nkeys && nkeys >= 3002;
   for (i = 0; same && i < nkeys; i++)
      got[i] = st->current_manifest->list[i].alt;
   if (same)
      qsort(got, nkeys, sizeof(*got), key_cmp);
   for (i = 0; same && i < nkeys; i++)
      if (strcmp(got[i], keys[i]))
         same = 0;
   check("walk: the same files dir_list_new() lists, hidden ones too", same);
   for (i = 0; same && i < nkeys; i++)
      if (     strstr(keys[i], ".rafetching") || strstr(keys[i], ".DS_Store")
            || strstr(keys[i], ".tmp"))
         same = 0;
   check("walk: ignored names left out", same);
   task_cloud_sync_cleanup(task);
   free(task->title);
   free(task);

   /* a spent window: one entry per run, and it still ends */
   st   = walk_state(a, NULL);
   task = task_init();
   grant = 0;
   r = walk_run(st, task);
   check("walk: one entry per run with a spent window, and it ends",
         task_cloud_sync_phase_get(st) == CLOUD_SYNC_PHASE_DIFF && max_granted == 1);
   task_cloud_sync_cleanup(task);
   free(task->title);
   free(task);

   /* a root that cannot be read stops the sync */
   snprintf(path, sizeof(path), "%s/no_such_dir", dir);
   st   = walk_state(a, path);
   task = task_init();
   grant = 20;
   walk_run(st, task);
   check("walk: an unreadable root stops the sync",
         task_cloud_sync_phase_get(st) == CLOUD_SYNC_PHASE_END && st->failures);
   task_cloud_sync_cleanup(task);
   free(task->title);
   free(task);

   for (i = 0; i < nkeys; i++)
      free(keys[i]);
   free(keys);
   free(got);
   snprintf(cmd, sizeof(cmd), "rm -rf %s %s", a, b);
   if (system(cmd) != 0) { }
}

/* What a sync's pusher is told when it is done. */
static unsigned done_calls;
static int      done_failed;
static int      done_had_state;

static void on_sync_done(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task_data;
   done_calls++;
   done_failed    = err != NULL;
   done_had_state = task->state != NULL && user_data == (void*)&done_calls;
}

/* A whole sync's tail through the real task queue: the diff, the local
 * manifest and the end, then retirement. Run under LeakSanitizer in CI.
 * @ends: whether the driver ends the sync. */
static void queue_run(int ends)
{
   task_cloud_sync_state_t *st   = (task_cloud_sync_state_t*)calloc(1, sizeof(*st));
   retro_task_t            *task = task_init();
   struct string_list      *dl   = string_list_new();
   union string_list_elem_attr attr;
   char     path[128];
   unsigned i;

   attr.i = 0;
   string_list_append(dl, "saves", attr);
   dl->elems[0].userdata = strdup(dir);
   st->dirlist                 = dl;
   st->server_manifest         = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->local_manifest          = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->current_manifest        = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_server_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_local_manifest  = (file_list_t*)calloc(1, sizeof(file_list_t));
   strlcpy(st->dir_core_assets, dir, sizeof(st->dir_core_assets));
   retro_atomic_int_init(&st->waiting, 0);
   retro_atomic_int_init(&st->phase, (int)CLOUD_SYNC_PHASE_DIFF);
   st->cb      = on_sync_done;
   st->cb_data = &done_calls;
   snprintf(path, sizeof(path), "%s/q.srm", dir);
   write_file(path, 300 * 1024, 3);
   list_add(st->server_manifest,  "saves/q.srm", NULL, hash_of(path));
   list_add(st->local_manifest,   "saves/q.srm", NULL, hash_of(path));
   list_add(st->current_manifest, "saves/q.srm", path, NULL);

   /* set up as task_push_cloud_sync() sets up every sync */
   task_cloud_sync_task_setup(task, st, "Cloud Sync in progress");
   grant      = 2;
   end_ok     = ends;
   done_calls = 0;

   task_queue_init(false, NULL);
   task_queue_push(task);
   for (i = 0; i < 10000; i++)
      task_queue_check();
   task_queue_deinit();
   end_ok = 0;

   unlink(path);
   snprintf(path, sizeof(path), "%s/manifest.local", dir);
   unlink(path);
   snprintf(path, sizeof(path), "%s/manifest.local.tmp", dir);
   unlink(path);
}

static void queue_lane(void)
{
   queue_run(1);
   check("queue: the sync ran to its end and retired", 1);
   check("queue: a clean sync tells its pusher once, with no error, before its state goes",
         done_calls == 1 && !done_failed && done_had_state);

   queue_run(0);
   check("queue: a sync the driver could not end tells its pusher it failed",
         done_calls == 1 && done_failed);
}

/* A sync whose driver cannot begin: its pusher hears it failed. */
static void begin_lane(void)
{
   task_cloud_sync_state_t *st   = (task_cloud_sync_state_t*)calloc(1, sizeof(*st));
   retro_task_t            *task = task_init();
   unsigned i;

   retro_atomic_int_init(&st->waiting, 0);
   retro_atomic_int_init(&st->phase, (int)CLOUD_SYNC_PHASE_BEGIN);
   st->cb      = on_sync_done;
   st->cb_data = &done_calls;
   task_cloud_sync_task_setup(task, st, "Cloud Sync in progress");
   done_calls = 0;

   task_queue_init(false, NULL);
   task_queue_push(task);
   for (i = 0; i < 100; i++)
      task_queue_check();
   task_queue_deinit();
   check("begin: a sync that could not begin tells its pusher it failed",
         done_calls == 1 && done_failed);
}

int main(void)
{
   unsigned r;
   int      ok;

   tcs_test_lock();
   strcpy(dir, "/tmp/cs_pacing_XXXXXX");
   if (!mkdtemp(dir))
      return 2;

   printf("cloudsync_pacing_test\n");

   /* one 9 MiB file: 36 chunks; 3 units per run */
   r = run_diff(1, 9 * 1024 * 1024, 3, &ok);
   check("large file: hash matches a one-shot MD5, manifests complete", ok);
   check("large file: spread over runs, not hashed in one", r >= 36 / 4);
   check("large file: no run past its grant", max_granted <= 4);
   check("large file: nothing transferred", transfers == 0);

   /* 40 small files, 10 units per run */
   r = run_diff(40, 1000, 10, &ok);
   check("small files: hashes match, manifests complete", ok);
   check("small files: several per run", r <= 40 / 3);
   check("small files: no run past its grant", max_granted <= 11);

   /* a window that is always spent: the floor alone carries it */
   r = run_diff(5, 600 * 1024, 0, &ok);
   check("spent window: every run progresses, the diff ends", ok && r < 100000);
   check("spent window: one unit per run", max_granted == 1);

   fetch_lane();
   fetch_keep_lane(1,
         "fetch: a save a core took during the download is left as it was",
         "fetch: ... and the server's copy waits for the next sync");
   fetch_keep_lane(0,
         "fetch: a download cut off part-way leaves the save as it was",
         "fetch: ... and is fetched again next sync");
   walk_lane();
   queue_lane();
   begin_lane();

   rmdir(dir);
   printf("cloudsync_pacing_test: %s\n", fails ? "FAIL" : "PASS");
   return fails ? 1 : 0;
}
