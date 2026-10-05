/* Blocking cloud sync drivers (SMB, NFS) run on a worker thread.
 *
 * Their calls block on the network until done, and the sync used to
 * make them from its task handler - the frame loop itself with Threaded
 * Tasks off - so a slow or unreachable server froze the frontend for as
 * long as each call took. network/cloud_sync_driver.c now queues those
 * calls for one worker per sync and hands each result back through
 * cloud_sync_poll() on the caller's thread. Checked here against a
 * driver that blocks until the test lets it go:
 *
 * - every call returns at once while the driver is still blocked, and
 *   nothing completes until cloud_sync_poll() is called;
 * - handlers run on the polling thread, once each, in call order, and
 *   the driver sees the calls in that order, never two at a time;
 * - path and file names are copied: the caller's buffers can change
 *   straight after the call;
 * - success, the path given back and a read's RFILE reach the handler;
 *   a call the driver refuses without calling back fails, an upload's
 *   file going back to the handler to close;
 * - a sync whose BEGIN fails, and one that ends, let the worker go, and
 *   the next sync starts a new one;
 * - on exit, cloud_sync_deinit() waits for the call under way, drops
 *   the ones not started (closing an upload's file) without running
 *   their handlers, and gives up on a call stuck past its bound;
 * - a driver without the blocking flag is still called directly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include <boolean.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include <features/features_cpu.h>
#include <streams/file_stream.h>

#include "../../../network/cloud_sync_driver.h"

/* what cloud_sync_driver.c links against, outside the test */
int driver_find_index(const char *label, const char *drv)
{ (void)label; (void)drv; return -1; }
char *char_list_new_special(int type, void *data)
{ (void)type; (void)data; return NULL; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_LOG_OUTPUT(const char *fmt, ...) { (void)fmt; }
cloud_sync_driver_t cloud_sync_webdav;

static unsigned failures;
#define CHECK(cond, msg) \
   do { if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, msg); failures++; } } while (0)

/* ---- the blocking driver ---- */

static slock_t *gate_lock;
static scond_t *gate_cond;
static unsigned gate_open;     /* calls allowed to finish */
static unsigned entered;       /* calls the driver has started */
static unsigned inside;        /* calls running right now */
static unsigned max_inside;
static char     seen[16][64];  /* calls in the order the driver saw them */
static bool     refuse;        /* return false without calling back */
static uintptr_t driver_thread;

static void drv_enter(const char *what, const char *path)
{
   slock_lock(gate_lock);
   snprintf(seen[entered & 15], sizeof(seen[0]), "%s:%s", what,
         path ? path : "-");
   entered++;
   inside++;
   if (inside > max_inside)
      max_inside = inside;
   driver_thread = sthread_get_current_thread_id();
   while (gate_open == 0)
      scond_wait(gate_cond, gate_lock);
   gate_open--;
   inside--;
   slock_unlock(gate_lock);
}

static void gate_release(unsigned n)
{
   slock_lock(gate_lock);
   gate_open += n;
   scond_broadcast(gate_cond);
   slock_unlock(gate_lock);
}

static void release_later(void *data)
{
   (void)data;
   retro_sleep(50);
   gate_release(1);
}

static unsigned entered_count(void)
{
   unsigned n;
   slock_lock(gate_lock);
   n = entered;
   slock_unlock(gate_lock);
   return n;
}

static bool drv_begin(cloud_sync_complete_handler_t cb, void *ud)
{
   drv_enter("begin", NULL);
   cb(ud, NULL, !refuse, NULL);
   return true;
}
static bool drv_end(cloud_sync_complete_handler_t cb, void *ud)
{
   drv_enter("end", NULL);
   cb(ud, NULL, true, NULL);
   return true;
}
static RFILE *const read_file = (RFILE*)(uintptr_t)0x1000;
static bool drv_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *ud)
{
   char what[48];
   snprintf(what, sizeof(what), "read(%s)", file);
   drv_enter(what, path);
   if (refuse)
      return false;
   cb(ud, path, true, strcmp(path, "missing") ? read_file : NULL);
   return true;
}
static bool drv_update(const char *path, RFILE *file,
      cloud_sync_complete_handler_t cb, void *ud)
{
   drv_enter("update", path);
   if (refuse)
      return false;
   cb(ud, path, true, file);
   return true;
}
static bool drv_free(const char *path, cloud_sync_complete_handler_t cb,
      void *ud)
{
   drv_enter("free", path);
   cb(ud, path, false, NULL);
   return true;
}

static cloud_sync_driver_t blocking = {
   drv_begin, drv_end, drv_read, drv_update, drv_free, "blocking",
   CLOUD_SYNC_DRIVER_FLG_BLOCKING
};

/* ---- results as the handlers see them ---- */

typedef struct
{
   char      path[64];
   RFILE    *file;
   uintptr_t thread;
   int       tag;
   bool      success;
   bool      has_path;
} result_t;

static result_t results[16];
static unsigned nresults;

static void on_done(void *ud, const char *path, bool success, RFILE *file)
{
   result_t *r = &results[nresults++ & 15];
   r->tag      = (int)(intptr_t)ud;
   r->success  = success;
   r->has_path = path != NULL;
   r->file     = file;
   r->thread   = sthread_get_current_thread_id();
   snprintf(r->path, sizeof(r->path), "%s", path ? path : "");
}

/* polls until @n results have arrived, or about five seconds pass */
static bool wait_results(unsigned n)
{
   int i;
   for (i = 0; i < 5000 && nresults < n; i++)
   {
      cloud_sync_poll();
      if (nresults < n)
         retro_sleep(1);
   }
   return nresults == n;
}

static void reset(void)
{
   memset(results, 0, sizeof(results));
   memset(seen, 0, sizeof(seen));
   nresults = entered = max_inside = 0;
   refuse   = false;
}

int main(void)
{
   uintptr_t me = sthread_get_current_thread_id();
   char      path[64], local[64];
   RFILE    *upload_file = (RFILE*)(uintptr_t)0x2000;
   sthread_t *releaser;
   int       i;

   gate_lock = slock_new();
   gate_cond = scond_new();
   cloud_sync_state_get_ptr()->driver = &blocking;

   /* a whole sync, every call returning while the driver blocks */
   reset();
   CHECK(cloud_sync_begin(on_done, (void*)1), "begin queued");
   strcpy(path, "saves/a.srm");
   strcpy(local, "/local/a.srm");
   CHECK(cloud_sync_read(path, local, on_done, (void*)2), "read queued");
   strcpy(path, "XXXXXXXXXXX");
   strcpy(local, "XXXXXXXXXXXX");
   CHECK(cloud_sync_read("missing", "/local/m", on_done, (void*)3), "read queued");
   CHECK(cloud_sync_update("saves/b.srm", upload_file, on_done, (void*)4), "update queued");
   CHECK(cloud_sync_free("saves/c.srm", on_done, (void*)5), "free queued");
   CHECK(cloud_sync_end(on_done, (void*)6), "end queued");

   for (i = 0; i < 2000 && entered_count() == 0; i++)
      retro_sleep(1);
   CHECK(entered_count() == 1, "driver blocked inside the first call");
   cloud_sync_poll();
   CHECK(nresults == 0, "nothing completes while the driver blocks");

   gate_release(6);
   CHECK(wait_results(6), "every call completes");
   for (i = 0; i < 6; i++)
   {
      CHECK(results[i].tag == i + 1, "handlers run in call order");
      CHECK(results[i].thread == me, "handlers run on the polling thread");
   }
   CHECK(driver_thread != me, "driver ran on another thread");
   CHECK(max_inside == 1, "one call at a time");
   CHECK(!strcmp(seen[0], "begin:-")
         && !strcmp(seen[1], "read(/local/a.srm):saves/a.srm")
         && !strcmp(seen[2], "read(/local/m):missing")
         && !strcmp(seen[3], "update:saves/b.srm")
         && !strcmp(seen[4], "free:saves/c.srm")
         && !strcmp(seen[5], "end:-"), "driver saw the calls in order, names copied");
   CHECK(results[0].success && !results[0].has_path, "begin result");
   CHECK(results[1].success && results[1].file == read_file
         && !strcmp(results[1].path, "saves/a.srm"), "read result");
   CHECK(results[2].success && !results[2].file, "missing file result");
   CHECK(results[3].success && results[3].file == upload_file, "update result");
   CHECK(!results[4].success && results[4].has_path, "failed free result");
   CHECK(results[5].success, "end result");
   cloud_sync_poll();
   CHECK(nresults == 6, "each handler runs once");

   /* refused calls fail; the upload's file goes back to be closed */
   reset();
   cloud_sync_begin(on_done, (void*)1);
   gate_release(1);
   CHECK(wait_results(1) && results[0].success, "second sync begins on a new worker");
   refuse = true;
   cloud_sync_read("saves/a.srm", "/l", on_done, (void*)2);
   cloud_sync_update("saves/b.srm", upload_file, on_done, (void*)3);
   gate_release(2);
   CHECK(wait_results(3), "refused calls complete");
   CHECK(!results[1].success && !results[1].file, "refused read fails");
   CHECK(!results[2].success && results[2].file == upload_file, "refused update hands its file back");
   refuse = false;
   cloud_sync_end(on_done, (void*)4);
   gate_release(1);
   CHECK(wait_results(4), "end after refusals");

   /* a failed BEGIN ends the sync; the next one still runs */
   reset();
   refuse = true;
   cloud_sync_begin(on_done, (void*)1);
   gate_release(1);
   CHECK(wait_results(1) && !results[0].success, "failed begin");
   refuse = false;
   cloud_sync_begin(on_done, (void*)2);
   cloud_sync_end(on_done, (void*)3);
   gate_release(2);
   CHECK(wait_results(3) && results[1].success && results[2].success,
         "a sync after a failed begin");

   /* exit: the call under way finishes, the rest are dropped */
   reset();
   {
      char   tmp[] = "/tmp/cloudsync_async_XXXXXX";
      int    fd    = mkstemp(tmp);
      RFILE *up;
      retro_time_t t0;
      if (fd >= 0)
         close(fd);
      up = filestream_open(tmp, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(up != NULL, "upload file opened");
      cloud_sync_begin(on_done, (void*)1);
      cloud_sync_read("saves/a.srm", "/l", on_done, (void*)2);
      cloud_sync_update("saves/b.srm", up, on_done, (void*)3);
      for (i = 0; i < 2000 && entered_count() == 0; i++)
         retro_sleep(1);
      /* the blocked call is let go only once deinit holds the queue */
      releaser = sthread_create(release_later, NULL);
      t0 = cpu_features_get_time_usec();
      cloud_sync_deinit(1000);
      sthread_join(releaser);
      CHECK(cpu_features_get_time_usec() - t0 < 900000, "deinit returned once the call finished");
      CHECK(entered_count() == 1, "calls not started were dropped");
      cloud_sync_poll();
      CHECK(nresults == 0, "no handler runs after deinit");
      /* LeakSanitizer reports the upload's RFILE if it was not closed */
      unlink(tmp);
   }

   /* exit with a call stuck: the wait gives up at its bound */
   reset();
   {
      retro_time_t t0, took;
      cloud_sync_begin(on_done, (void*)1);
      for (i = 0; i < 2000 && entered_count() == 0; i++)
         retro_sleep(1);
      t0   = cpu_features_get_time_usec();
      cloud_sync_deinit(100);
      took = cpu_features_get_time_usec() - t0;
      CHECK(took >= 90000 && took < 2000000, "deinit gives up at its bound");
      gate_release(1);
      /* the stuck call ends on its own, the worker leaves, and its
       * result is dropped with the rest */
      t0 = cpu_features_get_time_usec();
      cloud_sync_deinit(2000);
      CHECK(cpu_features_get_time_usec() - t0 < 1900000, "the stuck call ended after release");
      cloud_sync_poll();
      CHECK(nresults == 0, "a late result is dropped, not handed back");
   }

   /* a driver without the flag is called directly */
   reset();
   blocking.flags = 0;
   gate_release(1);
   CHECK(cloud_sync_free("direct", on_done, (void*)9), "direct call");
   CHECK(nresults == 1 && results[0].tag == 9 && results[0].thread == me
         && driver_thread == me, "direct call completes inline on this thread");

   /* let the last worker leave before the process does */
   retro_sleep(50);
   slock_free(gate_lock);
   scond_free(gate_cond);
   if (failures)
   {
      printf("[FAIL] cloudsync_async_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] cloudsync_async_test\n");
   return 0;
}
