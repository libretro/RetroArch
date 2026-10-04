/* Cloud sync's updated manifests, added to from several threads.
 *
 * While the diff runs, transfers complete on whatever thread the
 * driver calls back on, and each completion adds an entry to the
 * server manifest, the local one, or both - as the task thread itself
 * does for the entries it settles without a transfer. Nothing takes a
 * lock for it: an entry is pushed on a list, and the task thread folds
 * the list into the manifests once every transfer has reported, which
 * is when they are first read.
 *
 * Here four threads and the main one add entries at once, and after
 * the fold every entry must be in its manifest exactly once, with the
 * hash it was added with. A push that lost another's entry leaves the
 * count short; built under ThreadSanitizer, anything the threads share
 * without ordering is a reported race, and under ASan an entry freed
 * twice or not at all is.
 *
 * The unit under test is the real tasks/task_cloudsync.c, included;
 * the driver and the frontend around it are stubs nothing here calls.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <rthreads/rthreads.h>

#include "../../../tasks/task_cloudsync.c"

#define THREADS 4
#define ADDS    4000

/* --- what the task links against and this test never reaches ---------- */

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
bool verbosity_is_enabled(void)       { return false; }
settings_t *config_get_ptr(void)      { return NULL; }
char *dir_get_ptr(enum rarch_dir_type type) { (void)type; return NULL; }
bool content_savefile_is_live(const char *path) { (void)path; return false; }
void task_window_progress_cb(retro_task_t *task) { (void)task; }
size_t fill_pathname_application_special(char *s, size_t len,
      enum application_special_type type)
{ (void)s; (void)len; (void)type; return 0; }
void cloud_sync_find_driver(const char *drv, const char *prefix,
      bool verbosity_enabled)
{ (void)drv; (void)prefix; (void)verbosity_enabled; }
bool cloud_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{ (void)cb; (void)user_data; return false; }
bool cloud_sync_end(cloud_sync_complete_handler_t cb, void *user_data)
{ (void)cb; (void)user_data; return false; }
bool cloud_sync_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{ (void)path; (void)file; (void)cb; (void)user_data; return false; }
bool cloud_sync_update(const char *path, RFILE *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{ (void)path; (void)file; (void)cb; (void)user_data; return false; }
bool cloud_sync_free(const char *path, cloud_sync_complete_handler_t cb,
      void *user_data)
{ (void)path; (void)cb; (void)user_data; return false; }

/* --- the test ----------------------------------------------------------- */

static task_cloud_sync_state_t *state;
static retro_atomic_int_t       go;
static int                      fails;

static void check(const char *what, int ok)
{
   printf("  %-60s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

/* Entry i of thread k: its key, and the hash that goes with it. Odd
 * entries go to the server manifest, even ones to the local one, and
 * every fourth - an even one - to both, as a fetched file does. */
static void add_entries(unsigned k)
{
   unsigned i;
   for (i = 0; i < ADDS; i++)
   {
      char key[64];
      char hash[64];
      snprintf(key,  sizeof(key),  "saves/t%u/file%05u.srm", k, i);
      snprintf(hash, sizeof(hash), "hash-of-t%u-%05u", k, i);
      task_cloud_sync_add_to_updated_manifest(state, key,
            strdup(hash), (i & 1) != 0);
      if (!(i & 3))
         task_cloud_sync_add_to_updated_manifest(state, key,
               strdup(hash), (i & 1) == 0);
   }
}

static void adder(void *arg)
{
   while (!retro_atomic_load_acquire_int(&go))
      sthread_yield();
   add_entries((unsigned)(size_t)arg);
}

/* Every entry of the manifest carries the hash made from its key, and
 * no key appears twice. */
static int manifest_sound(file_list_t *list)
{
   size_t i;
   file_list_sort_on_alt(list);
   for (i = 0; i < list->size; i++)
   {
      unsigned k = 0, n = 0;
      char hash[64];
      const char *key = list->list[i].alt;
      if (!key || sscanf(key, "saves/t%u/file%u.srm", &k, &n) != 2)
         return 0;
      snprintf(hash, sizeof(hash), "hash-of-t%u-%05u", k, n);
      if (     !list->list[i].userdata
            || strcmp((const char*)list->list[i].userdata, hash))
         return 0;
      if (i && !strcmp(key, list->list[i - 1].alt))
         return 0;
   }
   return 1;
}

int main(void)
{
   sthread_t *threads[THREADS];
   unsigned   k;
   /* per thread: half its entries to each manifest, and a quarter
    * more to the server one */
   size_t     local  = (size_t)(THREADS + 1) * (ADDS / 2);
   size_t     server = local + (size_t)(THREADS + 1) * (ADDS / 4);

   state = (task_cloud_sync_state_t*)calloc(1, sizeof(*state));
   if (!state)
      return 1;
   state->updated_server_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   state->updated_local_manifest  = (file_list_t*)calloc(1, sizeof(file_list_t));
   if (!state->updated_server_manifest || !state->updated_local_manifest)
      return 1;

   for (k = 0; k < THREADS; k++)
      if (!(threads[k] = sthread_create(adder, (void*)(size_t)k)))
         return 1;
   retro_atomic_store_release_int(&go, 1);
   add_entries(THREADS);              /* the task thread's own */
   for (k = 0; k < THREADS; k++)
      sthread_join(threads[k]);

   check("nothing is in the manifests until they are folded",
            state->updated_server_manifest->size == 0
         && state->updated_local_manifest->size == 0);
   task_cloud_sync_fold_manifest_adds(state);
   check("every entry added reached the server manifest",
         state->updated_server_manifest->size == server);
   check("every entry added reached the local manifest",
         state->updated_local_manifest->size == local);
   check("each server entry has its own hash, once",
         manifest_sound(state->updated_server_manifest));
   check("each local entry has its own hash, once",
         manifest_sound(state->updated_local_manifest));
   check("no entry was lost for want of memory", !state->failures);

   /* A sync that ends before its manifests are read: what was added
    * still goes into them, and is freed with them. */
   add_entries(0);
   task_cloud_sync_fold_manifest_adds(state);
   file_list_free(state->updated_server_manifest);
   file_list_free(state->updated_local_manifest);
   free(state);

   if (fails)
   {
      printf("cloudsync_manifest_adds_test: FAIL (%d)\n", fails);
      return 1;
   }
   printf("cloudsync_manifest_adds_test: PASS\n");
   return 0;
}
