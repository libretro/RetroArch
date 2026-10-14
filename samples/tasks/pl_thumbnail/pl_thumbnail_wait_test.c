/* The playlist thumbnail tasks waiting on their transfers, on the
 * threaded task queue.
 *
 * The suite includes tasks/task_pl_thumbnail_download.c so it can run
 * the per-entry handler on a handle it builds, against the real queue.
 * task_push_http_download_file() is stubbed with a task of its own that
 * stands in for the transfer: it finishes when the lane lets it, and its
 * callback is the thumbnail task's own.
 *
 * Lanes:
 *  - waiting: while a transfer runs the handler parks, and the
 *    transfer's callback wakes it - a few passes per thumbnail, not one
 *    per worker spin;
 *  - cancel: a task cancelled with a transfer in flight finishes only
 *    after the transfer's callback, which writes into the task's handle.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include <features/features_cpu.h>

#include "../../../tasks/task_pl_thumbnail_download.c"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

/* ---- the transfer stand-in ---- */

static retro_atomic_int_t transfers_pushed;
static retro_atomic_int_t transfers_go;     /* 1: transfers may finish */
static retro_atomic_int_t transfer_callbacks;
static retro_atomic_int_t parent_retired_after;  /* callbacks seen at retire */
static retro_atomic_int_t parent_retired;

struct transfer
{
   retro_task_callback_t cb;
   void                 *user_data;
   retro_time_t          due;
};

static void transfer_handler(retro_task_t *task)
{
   struct transfer *t = (struct transfer*)task->state;
   if (     retro_atomic_load_acquire_int(&transfers_go)
         && cpu_features_get_time_usec() >= t->due)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   else
      retro_sleep(1);
}

static void transfer_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   struct transfer *t = (struct transfer*)task->state;
   (void)user_data;
   t->cb(task, task_data, t->user_data, err);
   retro_atomic_fetch_add_int(&transfer_callbacks, 1);
}

static void transfer_cleanup(retro_task_t *task)
{
   free(task->state);
   task->state = NULL;
}

void *task_push_http_download_file(const char *url, const char *path,
      bool mute, const char *type, retro_task_callback_t cb,
      void *user_data)
{
   retro_task_t    *task = task_init();
   struct transfer *t    = (struct transfer*)calloc(1, sizeof(*t));
   (void)url; (void)path; (void)mute; (void)type;
   if (!task || !t)
   {
      free(task);
      free(t);
      return NULL;
   }
   t->cb           = cb;
   t->user_data    = user_data;
   t->due          = cpu_features_get_time_usec() + 15000;
   task->state     = t;
   task->handler   = transfer_handler;
   task->callback  = transfer_callback;
   task->cleanup   = transfer_cleanup;
   task->flags    |= RETRO_TASK_FLG_MUTE;
   retro_atomic_fetch_add_int(&transfers_pushed, 1);
   task_queue_push(task);
   return task;
}

/* ---- the rest of the frontend, stubbed ---- */

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return ""; }
bool path_is_valid(const char *path) { (void)path; return false; }
bool path_mkdir(const char *dir) { (void)dir; return true; }
void net_http_urlencode_full(char *dst, const char *src, size_t len)
{ strlcpy(dst, src, len); }
void task_window_progress_cb(retro_task_t *task) { (void)task; }

/* ---- counting the handler's passes ---- */

static retro_task_t      *watched;
static retro_atomic_int_t passes;

uint8_t __real_task_get_flags(retro_task_t *task);
uint8_t __wrap_task_get_flags(retro_task_t *task)
{
   if (task == watched)
      retro_atomic_fetch_add_int(&passes, 1);
   return __real_task_get_flags(task);
}

/* ---- driving it ---- */

static void parent_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   retro_atomic_store_release_int(&parent_retired_after,
         retro_atomic_load_acquire_int(&transfer_callbacks));
   retro_atomic_store_release_int(&parent_retired, 1);
}

static void parent_cleanup(retro_task_t *task)
{
   pl_thumb_handle_t *pl_thumb = (pl_thumb_handle_t*)task->state;
   if (pl_thumb)
      free_pl_thumb_handle(pl_thumb);
   task->state = NULL;
}

static retro_task_t *push_entry(void)
{
   retro_task_t      *task     = task_init();
   pl_thumb_handle_t *pl_thumb = (pl_thumb_handle_t*)calloc(1,
         sizeof(*pl_thumb));
   gfx_thumbnail_path_data_t *pd = (gfx_thumbnail_path_data_t*)calloc(1,
         sizeof(*pd));

   strlcpy(pd->system,           "Lane System",  sizeof(pd->system));
   strlcpy(pd->content_img_full, "entry.png",    sizeof(pd->content_img_full));
   pl_thumb->thumbnail_path_data = pd;
   pl_thumb->system              = strdup("Lane System");
   pl_thumb->dir_thumbnails      = strdup("/tmp/pl_thumbnail_wait_test");
   pl_thumb->type_idx            = 1;
   pl_thumb->name_flags          = PLAYLIST_THUMBNAIL_FLAG_FULL_NAME;
   pl_thumb->status              = PL_THUMB_ITERATE_TYPE;
   pl_thumb->ident               = task->ident;
   retro_atomic_int_init(&pl_thumb->http_done, 0);

   task->handler  = task_pl_entry_thumbnail_download_handler;
   task->callback = parent_callback;
   task->cleanup  = parent_cleanup;
   task->state    = pl_thumb;
   task->flags   |= RETRO_TASK_FLG_MUTE;

   retro_atomic_store_release_int(&transfers_pushed, 0);
   retro_atomic_store_release_int(&transfer_callbacks, 0);
   retro_atomic_store_release_int(&parent_retired, 0);
   retro_atomic_store_release_int(&parent_retired_after, -1);
   retro_atomic_store_release_int(&passes, 0);
   watched = task;
   task_queue_push(task);
   return task;
}

static void pump_until_retired(void)
{
   retro_time_t start = cpu_features_get_time_usec();
   while (!retro_atomic_load_acquire_int(&parent_retired)
         && cpu_features_get_time_usec() - start < 5000000)
   {
      task_queue_check();
      retro_sleep(1);
   }
   /* the transfers retire too */
   task_queue_wait(NULL, NULL);
}

static void lane_waiting(void)
{
   unsigned had = failures;

   retro_atomic_store_release_int(&transfers_go, 1);
   push_entry();
   pump_until_retired();

   CHECK(retro_atomic_load_acquire_int(&parent_retired) == 1,
         "the entry task did not retire");
   CHECK(retro_atomic_load_acquire_int(&transfers_pushed) == 3,
         "%d transfers, wanted one per thumbnail type",
         retro_atomic_load_acquire_int(&transfers_pushed));
   /* Per type: the pass that pushes, the one the callback wakes, and a
    * watchdog or a wake that lands mid-pass now and then. */
   CHECK(retro_atomic_load_acquire_int(&passes) <= 15,
         "the handler ran %d times over three transfers",
         retro_atomic_load_acquire_int(&passes));
   if (failures == had)
      fprintf(stderr, "[pass] waiting: %d passes over %d transfers\n",
            retro_atomic_load_acquire_int(&passes),
            retro_atomic_load_acquire_int(&transfers_pushed));
}

static void lane_cancel(void)
{
   unsigned      had = failures;
   retro_task_t *task;
   retro_time_t  start;

   retro_atomic_store_release_int(&transfers_go, 0);
   task  = push_entry();
   start = cpu_features_get_time_usec();
   while (!retro_atomic_load_acquire_int(&transfers_pushed)
         && cpu_features_get_time_usec() - start < 2000000)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(retro_atomic_load_acquire_int(&transfers_pushed) == 1,
         "no transfer started");

   task_queue_cancel_task(task);
   for (start = 0; start < 30; start++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(!retro_atomic_load_acquire_int(&parent_retired),
         "a cancelled task retired with its transfer still running");

   retro_atomic_store_release_int(&transfers_go, 1);
   pump_until_retired();

   CHECK(retro_atomic_load_acquire_int(&parent_retired) == 1,
         "the cancelled task did not retire");
   CHECK(retro_atomic_load_acquire_int(&parent_retired_after) >= 1,
         "the cancelled task retired before its transfer called back");
   if (failures == had)
      fprintf(stderr, "[pass] cancel: the task outlives its transfer's callback\n");
}

int main(void)
{
   task_queue_init(true, NULL);
   lane_waiting();
   lane_cancel();
   task_queue_deinit();

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed\n");
   return 0;
}
