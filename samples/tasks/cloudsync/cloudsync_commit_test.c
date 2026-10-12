/* The real cloud-sync task, queued with threaded tasks on and off.
 * Downloads arrive on another thread. Save ownership and file commits
 * must stay on the content thread, including a failed hand-over. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>

static int fail_alloc;
static void *test_malloc(size_t size)
{
   if (fail_alloc)
   {
      fail_alloc = 0;
      return NULL;
   }
   return malloc(size);
}

#define malloc test_malloc
#include "../../../tasks/task_cloudsync.c"
#undef malloc

static const char *directory = "cloudsync_commit_test_files";
static char local_path[128], fetching_path[160];
static retro_atomic_int_t off_main, ownership_checks;
static int live, retired, failures, expect_old, expect_failure;
static void *pending_fetch;

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
bool verbosity_is_enabled(void) { return false; }
settings_t *config_get_ptr(void) { return NULL; }
char *dir_get_ptr(enum rarch_dir_type type) { (void)type; return NULL; }
bool content_savefile_is_live(const char *path)
{
   retro_atomic_inc_int(&ownership_checks);
   if (!task_is_on_main_thread())
      retro_atomic_inc_int(&off_main);
   return live && !strcmp(path, local_path);
}
void task_window_progress_cb(retro_task_t *task) { (void)task; }
size_t fill_pathname_application_special(char *s, size_t len,
      enum application_special_type type)
{ (void)s; (void)len; (void)type; return 0; }
void cloud_sync_find_driver(const char *drv, const char *prefix, bool verbose)
{ (void)drv; (void)prefix; (void)verbose; }
void cloud_sync_capture(void) { }
bool cloud_sync_begin(cloud_sync_complete_handler_t cb, void *ud)
{ (void)cb; (void)ud; return false; }
bool cloud_sync_end(cloud_sync_complete_handler_t cb, void *ud)
{ cb(ud, NULL, true, NULL); return true; }
bool cloud_sync_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *ud)
{ (void)path; (void)file; (void)cb; pending_fetch = ud; return true; }
bool cloud_sync_update(const char *path, RFILE *file,
      cloud_sync_complete_handler_t cb, void *ud)
{ (void)path; (void)file; (void)cb; (void)ud; return false; }
bool cloud_sync_free(const char *path, cloud_sync_complete_handler_t cb, void *ud)
{ (void)path; (void)cb; (void)ud; return false; }
void cloud_sync_poll(cloud_sync_poll_budget_t within, void *budget)
{ (void)within; (void)budget; }
void task_nbio_slice_open(nbio_budget_t *b) { b->floor = 1; }
void task_nbio_slice_close(nbio_budget_t *b) { (void)b; }
bool task_nbio_slice_within_budget(void *ud, size_t avail, size_t len)
{ (void)ud; (void)avail; (void)len; return true; }

static void add(file_list_t *list, const char *path, const char *hash)
{
   size_t at = list->size;
   file_list_append(list, path, NULL, 0, 0, 0);
   file_list_set_alt_at_offset(list, at, "saves/save.srm");
   list->list[at].userdata = hash ? strdup(hash) : NULL;
}

static bool holds(const char *path, const char *text)
{
   char buf[16];
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   int64_t n = f ? filestream_read(f, buf, sizeof(buf)) : -1;
   if (f)
      filestream_close(f);
   return n == (int64_t)strlen(text) && !memcmp(buf, text, strlen(text));
}

static void done(retro_task_t *task, void *data, void *ud, const char *error)
{
   task_cloud_sync_state_t *st = (task_cloud_sync_state_t*)task->state;
   size_t at;
   (void)data; (void)ud;
   CHECK(task_is_on_main_thread());
   CHECK((error != NULL) == expect_failure);
   task_cloud_sync_fold_manifest_adds(st);
   if (expect_old)
   {
      CHECK(file_list_search(st->updated_local_manifest, "saves/save.srm", &at));
      if (file_list_search(st->updated_local_manifest, "saves/save.srm", &at))
         CHECK(string_is_equal(CS_FILE_HASH(&st->updated_local_manifest->list[at]), "old"));
   }
   retired++;
}

static void complete_fetch(void *data)
{
   task_cloud_sync_fetch_state_t *fetch = (task_cloud_sync_fetch_state_t*)data;
   RFILE *f = filestream_open(fetching_path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(f != NULL);
   task_cloud_sync_fetch_cb(fetch, "saves/save.srm", true, f);
}

static void lane(bool threaded, bool deletion, bool owned, bool no_memory,
      bool destructive)
{
   retro_task_t *task = task_init();
   task_cloud_sync_state_t *st = (task_cloud_sync_state_t*)calloc(1, sizeof(*st));
   unsigned i;
   char manifest[160], backup_dir[160];
   struct string_list *backups;
   union string_list_elem_attr attr;
   retired = 0;
   live = 0;
   expect_old = owned || no_memory;
   expect_failure = no_memory;
   retro_atomic_store_relaxed_int(&off_main, 0);
   retro_atomic_store_relaxed_int(&ownership_checks, 0);
   CHECK(filestream_write_file(local_path, "old", 3));
   st->server_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->local_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->current_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_server_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->updated_local_manifest = (file_list_t*)calloc(1, sizeof(file_list_t));
   st->destructive = destructive;
   st->dirlist = string_list_new();
   attr.i = 0;
   string_list_append(st->dirlist, "saves", attr);
   st->dirlist->elems[0].userdata = strdup(directory);
   strlcpy(st->dir_core_assets, directory, sizeof(st->dir_core_assets));
   add(st->server_manifest, NULL, deletion ? NULL : "new");
   add(st->local_manifest, NULL, "old");
   add(st->current_manifest, local_path, "old");
   retro_atomic_int_init(&st->waiting, 0);
   retro_atomic_int_init(&st->phase, deletion ? CLOUD_SYNC_PHASE_DIFF : CLOUD_SYNC_PHASE_END);
   st->cb = done;
   task_cloud_sync_task_setup(task, st, "Cloud Sync");
   task_queue_init(threaded, NULL);
   if (!deletion)
   {
      sthread_t *worker;
      pending_fetch = NULL;
      task_cloud_sync_fetch_server_file(st);
      CHECK(pending_fetch != NULL);
      CHECK(holds(local_path, "old"));
      CHECK(filestream_write_file(fetching_path, "new", 3));
      fail_alloc = no_memory;
      worker = sthread_create(complete_fetch, pending_fetch);
      CHECK(worker != NULL);
      if (worker)
         sthread_join(worker);
      CHECK(holds(local_path, "old"));
   }
   /* A core takes the save after the transfer, before its commit. */
   live = owned;
   task_queue_push(task);
   for (i = 0; i < 2000 && !retired; i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(retired == 1);
   CHECK(retro_atomic_load_relaxed_int(&off_main) == 0);
   if (!no_memory)
      CHECK(retro_atomic_load_relaxed_int(&ownership_checks) > 0);
   if (expect_old)
      CHECK(holds(local_path, "old"));
   else if (!deletion)
      CHECK(holds(local_path, "new"));
   else
      CHECK(!path_is_valid(local_path));
   CHECK(!path_is_valid(fetching_path));
   snprintf(backup_dir, sizeof(backup_dir), "%s/cloud_backups/saves", directory);
   backups = dir_list_new(backup_dir, NULL, false, false, true, true);
   if (!destructive && !expect_old)
   {
      CHECK(backups && backups->size == 1);
      if (backups && backups->size)
         CHECK(holds(backups->elems[0].data, "old"));
   }
   else
      CHECK(!backups || backups->size == 0);
   if (backups)
   {
      for (i = 0; i < backups->size; i++)
         filestream_delete(backups->elems[i].data);
      string_list_free(backups);
   }
   task_queue_deinit();
   filestream_delete(local_path);
   snprintf(manifest, sizeof(manifest), "%s/manifest.local", directory);
   filestream_delete(manifest);
}

int main(void)
{
   unsigned t;
   retro_atomic_int_init(&off_main, 0);
   retro_atomic_int_init(&ownership_checks, 0);
#if defined(HAVE_THREADS) && !defined(TCS_LOCK_FREE)
   tcs_manifest_lock = slock_new();
#endif
   CHECK(path_mkdir(directory));
   snprintf(local_path, sizeof(local_path), "%s/save.srm", directory);
   pathname_conform_slashes_to_os(local_path);
   snprintf(fetching_path, sizeof(fetching_path), "%s.rafetching", local_path);
   for (t = 0; t < 2; t++)
   {
      lane(t != 0, false, false, false, true);
      lane(t != 0, false, true, false, true);
      lane(t != 0, false, false, true, true);
      lane(t != 0, true, true, false, true);
      lane(t != 0, true, false, false, true);
      lane(t != 0, false, false, false, false);
      lane(t != 0, false, true, false, false);
      lane(t != 0, false, false, true, false);
      lane(t != 0, true, true, false, false);
      lane(t != 0, true, false, false, false);
   }
   puts(failures ? "FAIL cloudsync_commit_test" : "PASS cloudsync_commit_test");
   return failures ? 1 : 0;
}
