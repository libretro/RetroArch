/* The Emscripten HTTP task waiting on its fetch, on the threaded task
 * queue.
 *
 * tasks/task_http_emscripten.c builds on the host against the stand-in
 * headers in include/, which carry the fetch API's types; the suite
 * includes the shipping translation unit and plays the browser: its
 * emscripten_fetch() keeps the request, and the lane settles it from
 * this thread, which is where the fetch's callbacks run.
 *
 * Lane: while the fetch is in flight the task parks, and the settle
 * wakes it - a few passes, not one per worker spin, and the task
 * retires well inside its one-second watchdog.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <retro_miscellaneous.h>
#include <features/features_cpu.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

/* The unit refuses to build outside Emscripten.  Its headers are in
 * already, the host's view of them; only the unit sees the define. */
#define __EMSCRIPTEN__ 1
#include "../../../tasks/task_http_emscripten.c"
#undef __EMSCRIPTEN__

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

/* ---- the browser ---- */

static emscripten_fetch_t *in_flight = NULL;

void emscripten_fetch_attr_init(emscripten_fetch_attr_t *fetch_attr)
{
   memset(fetch_attr, 0, sizeof(*fetch_attr));
}

emscripten_fetch_t *emscripten_fetch(emscripten_fetch_attr_t *fetch_attr,
      const char *url)
{
   emscripten_fetch_t *fetch = (emscripten_fetch_t*)calloc(1,
         sizeof(*fetch));
   (void)url;
   if (!fetch)
      return NULL;
   fetch->userData     = fetch_attr->userData;
   fetch->__attributes = *fetch_attr;
   in_flight           = fetch;
   return fetch;
}

EMSCRIPTEN_RESULT emscripten_fetch_close(emscripten_fetch_t *fetch)
{
   if (fetch == in_flight)
      in_flight = NULL;
   free(fetch);
   return 0;
}

size_t emscripten_fetch_get_response_headers_length(emscripten_fetch_t *fetch)
{ (void)fetch; return 0; }
size_t emscripten_fetch_get_response_headers(emscripten_fetch_t *fetch,
      char *dst, size_t dstSizeBytes)
{ (void)fetch; (void)dst; (void)dstSizeBytes; return 0; }

/* ---- the rest of the frontend, stubbed ---- */

void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return ""; }
void task_window_progress_cb(retro_task_t *task) { (void)task; }
bool filestream_write_file(const char *path, const void *data, int64_t size)
{ (void)path; (void)data; (void)size; return true; }
int filestream_delete(const char *path) { (void)path; return 0; }
size_t net_http_headers_compact(char *raw)
{ return raw ? strlen(raw) : 0; }

/* ---- counting the handler's passes ---- */

static retro_atomic_int_t passes;

/* The HTTP task is the only one queued; its handler is set before the
 * push, so the worker can tell it by that. */
uint8_t __real_task_get_flags(retro_task_t *task);
uint8_t __wrap_task_get_flags(retro_task_t *task)
{
   if (task->handler == task_http_transfer_handler)
      retro_atomic_fetch_add_int(&passes, 1);
   return __real_task_get_flags(task);
}

/* ---- the lane ---- */

static retro_atomic_int_t retired;
static int                retired_status;

static void on_done(retro_task_t *task, void *task_data, void *user_data,
      const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   (void)task; (void)user_data; (void)err;
   retired_status = data ? data->status : -1;
   retro_atomic_store_release_int(&retired, 1);
}

static void lane_parks_until_settled(void)
{
   static char  body[] = "hello";
   unsigned     had    = failures;
   retro_time_t start;
   retro_time_t settled_at;
   int          i;

   retro_atomic_store_release_int(&passes, 0);
   retro_atomic_store_release_int(&retired, 0);
   CHECK(task_push_http_transfer("http://example.invalid/x", true, NULL,
            on_done, NULL) != NULL && in_flight != NULL,
         "the fetch did not start");
   if (!in_flight)
      return;

   /* In flight for a while: the handler parks. */
   start = cpu_features_get_time_usec();
   while (cpu_features_get_time_usec() - start < 50000)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(retro_atomic_load_acquire_int(&passes) <= 3,
         "the handler ran %d times with the fetch in flight",
         retro_atomic_load_acquire_int(&passes));

   /* The browser settles it, on this thread. */
   in_flight->status   = 200;
   in_flight->data     = body;
   in_flight->numBytes = sizeof(body) - 1;
   settled_at          = cpu_features_get_time_usec();
   in_flight->__attributes.onsuccess(in_flight);

   for (i = 0; i < 3000 && !retro_atomic_load_acquire_int(&retired); i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(retro_atomic_load_acquire_int(&retired) == 1, "the task did not retire");
   CHECK(retired_status == 200, "the response was %d", retired_status);
   CHECK(cpu_features_get_time_usec() - settled_at < 500000,
         "the settle did not wake the task (%lld us)",
         (long long)(cpu_features_get_time_usec() - settled_at));

   if (failures == had)
      fprintf(stderr, "[pass] parks until settled: %d passes\n",
            retro_atomic_load_acquire_int(&passes));
}

int main(void)
{
   task_queue_init(true, NULL);
   lane_parks_until_settled();
   task_queue_deinit();

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed\n");
   return 0;
}
