/* Oracle for the overlay loader's pacing (tasks/task_overlay.c),
 * compiled from the shipping translation unit.
 *
 * The loader used to chunk by a count derived from the workload
 * itself - half of an overlay's descriptors, a quarter of the
 * overlays - which is not pacing: a bigger overlay just means a
 * bigger chunk, so the whole job always landed in the same two or
 * four handler invocations however long each one took.  With
 * Threaded Tasks off those invocations run on the thread driving the
 * frame loop, and each descriptor item is an image load off disk.
 *
 * What these lanes pin:
 *
 *   completeness - the loader still produces exactly the same
 *                  overlays and descriptors it did before, whatever
 *                  the pacing does: every overlay parsed, every
 *                  descriptor loaded, in order.
 *   pacing       - with a clock that exhausts the shared window
 *                  quickly, the load spreads over many invocations
 *                  and no single invocation swallows the job.
 *   scaling      - the number of items handled per invocation does
 *                  NOT grow with the size of the overlay, which is
 *                  the defect being fixed: doubling the descriptor
 *                  count must not double what one invocation does.
 *   one-item     - a window that is already exhausted still makes
 *                  progress, so the loader cannot stall.
 *   one read     - a still is read once, by its decode; only a file
 *                  the decode found animated is read for its bytes.
 *   predecode    - with a threaded task queue a pack's distinct
 *                  images are decoded together across the cores before
 *                  the first overlay is parsed, each exactly once, and
 *                  the load produces the same overlays and descriptors.
 *
 * Time is a virtual clock advancing a fixed step per observation, so
 * the assertions are exact rather than scheduler-dependent.  Image
 * loading is faked - what matters here is how many items each
 * invocation takes on, not what an item does. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <features/features_cpu.h>
#include <queues/task_queue.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

#include "../../../input/input_overlay.h"
#include "../../../tasks/tasks_internal.h"

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

/* ------------------------------------------------------------------ */
/* Virtual clock                                                       */
/* ------------------------------------------------------------------ */

static retro_time_t clock_now;
static retro_time_t clock_step;

retro_time_t cpu_features_get_time_usec(void)
{
   clock_now += clock_step;
   return clock_now;
}

uint64_t cpu_features_get(void) { return 0; }
unsigned cpu_features_get_core_amount(void) { return 4; }

/* The set's own threads let their read pools go; nothing pooled here */
void data_transfer_pool_flush(void) { }

/* NBIO_XFER_TICK_USEC is 4000; a third of it per observation
 * exhausts the window after a couple of budget checks. */
#define VIRTUAL_CLOCK_STEP 1500

/* ------------------------------------------------------------------ */
/* Stubs                                                               */
/* ------------------------------------------------------------------ */

static unsigned images_loaded;
/* Threads the decodes ran on, for the predecode lane */
#define MAX_DECODE_THREADS 8
static uintptr_t decode_thread[MAX_DECODE_THREADS];
static unsigned  decode_threads;
static slock_t  *decode_lock;

/* The loader deduplicates images by path and then COPIES the
 * texture_image into each consumer, so one allocation is reachable
 * from several structs.  In production the video driver owns that
 * memory; here the test owns it, tracking each allocation once and
 * releasing it after the run.  Freeing per struct would double
 * free - which is exactly what the first version of this stub
 * did. */
#define MAX_TRACKED_IMAGES 64
static uint32_t *tracked_pixels[MAX_TRACKED_IMAGES];
static unsigned  tracked_count;

/* The decode, which also says whether the file is an animated PNG:
 * here, any file named for one */
bool image_texture_load_request_ex(struct texture_image *img,
      const char *path, const image_texture_request_t *req,
      bool (*should_abort)(void *ud), void *ud, int *png_probe)
{
   (void)req; (void)should_abort; (void)ud;
   if (png_probe)
      *png_probe = strstr(path, "anim") ? 1 : 0;
   if (decode_lock)
   {
      uintptr_t id = sthread_get_current_thread_id();
      unsigned i;
      slock_lock(decode_lock);
      for (i = 0; i < decode_threads; i++)
         if (decode_thread[i] == id)
            break;
      if (i == decode_threads && i < MAX_DECODE_THREADS)
         decode_thread[decode_threads++] = id;
      images_loaded++;
      slock_unlock(decode_lock);
      /* long enough that the set's other threads get a share */
      retro_sleep(2);
   }
   else
      images_loaded++;
   if (img)
   {
      img->width  = 4;
      img->height = 4;
      img->pixels = (uint32_t*)calloc(16, sizeof(uint32_t));
      if (!img->pixels)
         return false;
      if (decode_lock)
         slock_lock(decode_lock);
      if (tracked_count < MAX_TRACKED_IMAGES)
         tracked_pixels[tracked_count++] = img->pixels;
      if (decode_lock)
         slock_unlock(decode_lock);
   }
   return true;
}

/* image_texture_set.c's own file set, which the loader does not use */
bool image_texture_load_request(struct texture_image *img,
      const char *path, const image_texture_request_t *req,
      bool (*should_abort)(void *ud), void *ud)
{
   return image_texture_load_request_ex(img, path, req, should_abort,
         ud, NULL);
}

/* Deliberately a no-op on pixels: see above. */
void image_texture_free(struct texture_image *img)
{
   if (img)
      img->pixels = NULL;
}

/* The loader tiles only for a driver whose requirements ask for GX
 * tiles, which this build (no RARCH_INTERNAL, no driver) never does. */
bool image_texture_tile_gx(struct texture_image *img)
{
   (void)img;
   return true;
}

static void release_tracked_images(void)
{
   unsigned i;
   for (i = 0; i < tracked_count; i++)
      free(tracked_pixels[i]);
   tracked_count = 0;
}

unsigned input_config_translate_str_to_bind_id(const char *str)
{
   return 0;
}

unsigned input_config_translate_str_to_rk(const char *str, size_t len)
{
   return 0;
}

/* Mirrors input/input_driver.c's implementation exactly.  Note what
 * it does NOT do: a descriptor's image and the entry in
 * load_images are copies of the same texture_image, sharing one
 * pixels allocation, so freeing pixels here would double free.  The
 * image lifetime belongs to the video driver in production; the
 * test's image stub allocates the pixels, so the test frees them
 * once, through load_images. */
void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;

   if (!overlay)
      return;

   for (i = 0; i < overlay->size; i++)
   {
      if (overlay->descs[i].eightway_config)
         free(overlay->descs[i].eightway_config);
      overlay->descs[i].eightway_config = NULL;
   }

   if (overlay->load_images)
      free(overlay->load_images);
   overlay->load_images = NULL;
   if (overlay->descs)
      free(overlay->descs);
   overlay->descs       = NULL;
}

/* The loader asks the frontend where an eight-way area's slopes are
 * kept; zeroes are valid slopes for this test. */
float *input_driver_overlay_eightway_slopes(bool abxy)
{
   static float slopes[2][2];
   return slopes[abxy ? 1 : 0];
}

void ui_companion_driver_notify_refresh(void) { }

/* verbosity.c is not linked: the loader logs, the test does not
 * care what it says. */
void RARCH_LOG(const char *fmt, ...) { }
void RARCH_WARN(const char *fmt, ...) { }
void RARCH_ERR(const char *fmt, ...) { }
void RARCH_DBG(const char *fmt, ...) { }

/* ------------------------------------------------------------------ */
/* Fixture                                                             */
/* ------------------------------------------------------------------ */

static char fixture_dir[256];

/* Writes an overlay config with @overlays overlays of @descs
 * descriptors each, plus the dummy image files they reference. */
static bool write_fixture(const char *path, unsigned overlays,
      unsigned descs)
{
   unsigned o, d;
   FILE *f = fopen(path, "wb");
   if (!f)
      return false;

   fprintf(f, "overlays = %u\n", overlays);
   for (o = 0; o < overlays; o++)
   {
      fprintf(f, "overlay%u_name = ol%u\n", o, o);
      fprintf(f, "overlay%u_full_screen = true\n", o);
      fprintf(f, "overlay%u_rect = \"0.0,0.0,1.0,1.0\"\n", o);
      fprintf(f, "overlay%u_overlay = img.png\n", o);
      fprintf(f, "overlay%u_descs = %u\n", o, descs);
      for (d = 0; d < descs; d++)
      {
         fprintf(f, "overlay%u_desc%u = \"a,0.5,0.5,rect,0.1,0.1\"\n",
               o, d);
         fprintf(f, "overlay%u_desc%u_overlay = img.png\n", o, d);
      }
   }
   fclose(f);
   return true;
}

static bool write_dummy_image(void)
{
   char path[512];
   FILE *f;
   snprintf(path, sizeof(path), "%s/img.png", fixture_dir);
   if (!(f = fopen(path, "wb")))
      return false;
   fputc(0, f);
   fclose(f);
   return true;
}

/* ------------------------------------------------------------------ */
/* Driving the loader                                                  */
/* ------------------------------------------------------------------ */

static bool any_finder(retro_task_t *task, void *userdata)
{
   return true;
}

static bool queue_busy(void)
{
   task_finder_data_t find_data;
   find_data.func     = any_finder;
   find_data.userdata = NULL;
   return task_queue_find(&find_data);
}

static unsigned loaded_overlays;
static unsigned loaded_descs;

static void overlay_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   overlay_task_data_t *data = (overlay_task_data_t*)task_data;
   unsigned i;

   if (!data)
      return;

   loaded_overlays = (unsigned)data->size;
   loaded_descs    = 0;
   for (i = 0; i < data->size; i++)
      loaded_descs += (unsigned)data->overlays[i].size;

   for (i = 0; i < data->size; i++)
      input_overlay_free_overlay(&data->overlays[i]);
   free(data->overlays);
   free(data->overlay_path);
   /* The real consumer (input_overlay_loaded in
    * input/input_driver.c) releases the image list here: the
    * successful hand-off transfers it, like the overlays and the
    * path above.  The stub used to leave it alone, which is why
    * this oracle could not see a second release of the same list
    * in the loader's cleanup. */
   string_list_free(data->image_list);
   /* And the animated images' list with it (dac760d996): the loader
    * hands that over too and forgets it, so whoever takes the payload
    * frees the file bytes nobody moved into a pack, then the list. */
   if (data->anim_list)
   {
      for (i = 0; i < data->anim_list->size; i++)
      {
         overlay_anim_src_t *src =
            (overlay_anim_src_t*)data->anim_list->elems[i].attr.p;
         if (src)
            free(src->data);
         free(src);
      }
      string_list_free(data->anim_list);
   }
   free(data);
}

/* Runs a full load, returning how many handler invocations it took.
 * @step is the virtual clock step per observation. */
static unsigned run_load(unsigned overlays, unsigned descs,
      retro_time_t step)
{
   char cfg[512];
   unsigned ticks = 0;

   snprintf(cfg, sizeof(cfg), "%s/ol.cfg", fixture_dir);
   if (!write_fixture(cfg, overlays, descs))
   {
      CHECK(false, "fixture write failed");
      return 0;
   }

   loaded_overlays = 0;
   loaded_descs    = 0;
   images_loaded   = 0;

   task_queue_init(false, NULL);

   clock_now  = 0;
   clock_step = step;

   if (!task_push_overlay_load_default(overlay_cb, cfg, false, NULL))
   {
      CHECK(false, "overlay task push failed");
      task_queue_deinit();
      return 0;
   }

   while (queue_busy() && ticks < 100000)
   {
      task_queue_check();
      ticks++;
   }

   task_queue_deinit();
   release_tracked_images();
   return ticks;
}

/* Opens of the pack's image files, counted through the VFS: the decode
 * is faked, so any open of one is a read beyond it */
static unsigned still_opens, anim_opens;
static struct retro_vfs_file_handle *counting_open(const char *path,
      unsigned mode, unsigned hints)
{
   if (strstr(path, "still.png"))
      still_opens++;
   if (strstr(path, "anim.png"))
      anim_opens++;
   return (struct retro_vfs_file_handle*)
      retro_vfs_file_open_impl(path, mode, hints);
}

static unsigned anim_kept, anim_null;
static void anim_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   overlay_task_data_t *data = (overlay_task_data_t*)task_data;
   unsigned i;
   if (data && data->anim_list && data->image_list)
      for (i = 0; i < data->anim_list->size; i++)
      {
         overlay_anim_src_t *src =
            (overlay_anim_src_t*)data->anim_list->elems[i].attr.p;
         bool anim = strstr(data->image_list->elems[i].data, "anim") != NULL;
         if (anim && src && src->len == 4)
            anim_kept++;
         if (!anim && !src)
            anim_null++;
      }
   overlay_cb(task, task_data, user_data, err);
}

/* ------------------------------------------------------------------ */
/* Lanes                                                               */
/* ------------------------------------------------------------------ */

static void lane_still_read_once(void)
{
   static struct retro_vfs_interface iface;
   struct retro_vfs_interface_info info;
   char cfg[512], path[512];
   unsigned had = failures, ticks = 0;
   FILE *f;
   const char *names[2] = { "still.png", "anim.png" };
   unsigned i;

   for (i = 0; i < 2; i++)
   {
      snprintf(path, sizeof(path), "%s/%s", fixture_dir, names[i]);
      if ((f = fopen(path, "wb")))
      {
         fwrite("\x89PNG", 1, 4, f);
         fclose(f);
      }
   }
   snprintf(cfg, sizeof(cfg), "%s/anim.cfg", fixture_dir);
   if (!(f = fopen(cfg, "wb")))
   {
      CHECK(false, "fixture write failed");
      return;
   }
   fprintf(f, "overlays = 1\n");
   fprintf(f, "overlay0_name = ol0\n");
   fprintf(f, "overlay0_full_screen = true\n");
   fprintf(f, "overlay0_rect = \"0.0,0.0,1.0,1.0\"\n");
   fprintf(f, "overlay0_overlay = still.png\n");
   fprintf(f, "overlay0_descs = 2\n");
   fprintf(f, "overlay0_desc0 = \"a,0.5,0.5,rect,0.1,0.1\"\n");
   fprintf(f, "overlay0_desc0_overlay = anim.png\n");
   fprintf(f, "overlay0_desc1 = \"b,0.2,0.2,rect,0.1,0.1\"\n");
   fprintf(f, "overlay0_desc1_overlay = still.png\n");
   fclose(f);

   memset(&iface, 0, sizeof(iface));
   iface.open                       = counting_open;
   info.required_interface_version  = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                       = &iface;
   filestream_vfs_init(&info);
   still_opens = anim_opens = anim_kept = anim_null = 0;

   task_queue_init(false, NULL);
   clock_now  = 0;
   clock_step = 0;
   if (task_push_overlay_load_default(anim_cb, cfg, false, NULL))
      while (queue_busy() && ticks < 100000)
      {
         task_queue_check();
         ticks++;
      }
   task_queue_deinit();
   release_tracked_images();
   info.iface = NULL;
   filestream_vfs_init(&info);

   CHECK(still_opens == 0, "a still was read %u times beyond its decode",
         still_opens);
   CHECK(anim_opens == 1, "an animated file was read %u times for its "
         "bytes, wanted 1", anim_opens);
   CHECK(anim_kept == 1 && anim_null == 1,
         "the pack kept %u animations and %u stills as stills, wanted 1 "
         "and 1", anim_kept, anim_null);

   if (failures == had)
      fprintf(stderr, "[pass] one-read lane\n");
}

static void lane_completeness(void)
{
   unsigned had = failures;

   /* Step 0: a clock that never advances, so the window never
    * expires and the loader runs unpaced - the fast path. */
   run_load(2, 6, 0);

   CHECK(loaded_overlays == 2, "loaded %u overlays, wanted 2",
         loaded_overlays);
   CHECK(loaded_descs == 12, "loaded %u descs, wanted 12",
         loaded_descs);

   if (failures == had)
      fprintf(stderr, "[pass] completeness lane\n");
}

static void lane_paced_matches_unpaced(void)
{
   unsigned had = failures;
   unsigned unpaced_ticks, paced_ticks;
   unsigned unpaced_overlays, unpaced_descs;

   unpaced_ticks    = run_load(3, 8, 0);
   unpaced_overlays = loaded_overlays;
   unpaced_descs    = loaded_descs;

   paced_ticks      = run_load(3, 8, VIRTUAL_CLOCK_STEP);

   CHECK(loaded_overlays == unpaced_overlays
         && loaded_descs == unpaced_descs,
         "paced load produced %u/%u, unpaced produced %u/%u",
         loaded_overlays, loaded_descs,
         unpaced_overlays, unpaced_descs);
   CHECK(paced_ticks > unpaced_ticks,
         "pacing did not spread the work: %u ticks paced vs %u "
         "unpaced", paced_ticks, unpaced_ticks);

   if (failures == had)
      fprintf(stderr,
            "[pass] pacing lane (%u ticks paced vs %u unpaced)\n",
            paced_ticks, unpaced_ticks);
}

static void lane_work_per_tick_does_not_scale(void)
{
   unsigned had = failures;
   unsigned small_ticks, large_ticks;
   unsigned small_descs, large_descs;
   double small_per_tick, large_per_tick;

   small_ticks = run_load(1, 8, VIRTUAL_CLOCK_STEP);
   small_descs = loaded_descs;

   large_ticks = run_load(1, 32, VIRTUAL_CLOCK_STEP);
   large_descs = loaded_descs;

   CHECK(small_descs == 8 && large_descs == 32,
         "fixtures did not load fully (%u, %u)",
         small_descs, large_descs);
   if (!small_ticks || !large_ticks)
   {
      CHECK(false, "no invocations recorded");
      return;
   }

   small_per_tick = (double)small_descs / (double)small_ticks;
   large_per_tick = (double)large_descs / (double)large_ticks;

   /* The defect this replaces chunked by size/2, so descriptors per
    * invocation grew linearly with the overlay: quadrupling the
    * count quadrupled what one invocation did.  Budgeted pacing
    * keeps it flat, so allow only a small margin. */
   CHECK(large_per_tick < small_per_tick * 2.0,
         "work per invocation scales with size: %.2f descs/tick at "
         "8 descs vs %.2f at 32", small_per_tick, large_per_tick);

   if (failures == had)
      fprintf(stderr, "[pass] scaling lane (%.2f vs %.2f descs/tick)\n",
            small_per_tick, large_per_tick);
}

static void lane_exhausted_window_progresses(void)
{
   unsigned had = failures;
   unsigned ticks;

   /* A step far larger than the whole window: every budget check
    * fails immediately, so only the guaranteed item per phase is
    * made.  The load must still complete rather than stall. */
   ticks = run_load(1, 10, 100000);

   CHECK(loaded_descs == 10,
         "exhausted-window load produced %u descs, wanted 10",
         loaded_descs);
   CHECK(ticks > 0 && ticks < 100000, "load did not terminate");

   if (failures == had)
      fprintf(stderr,
            "[pass] exhausted-window lane (%u ticks)\n", ticks);
}

/* A pack of @distinct images over @overlays overlays: each overlay's
 * base and its descs cycle through them, so every image is named more
 * than once */
static void lane_predecode(void)
{
   char cfg[512], path[512];
   unsigned had = failures, ticks = 0, o, d, i;
   unsigned distinct = 6, overlays = 2, descs = 8, unthreaded_loaded;
   FILE *f;

   for (i = 0; i < distinct; i++)
   {
      snprintf(path, sizeof(path), "%s/p%u.png", fixture_dir, i);
      if ((f = fopen(path, "wb")))
      {
         fputc(0, f);
         fclose(f);
      }
   }
   snprintf(cfg, sizeof(cfg), "%s/pre.cfg", fixture_dir);
   if (!(f = fopen(cfg, "wb")))
   {
      CHECK(false, "fixture write failed");
      return;
   }
   fprintf(f, "overlays = %u\n", overlays);
   for (o = 0; o < overlays; o++)
   {
      fprintf(f, "overlay%u_name = ol%u\n", o, o);
      fprintf(f, "overlay%u_full_screen = true\n", o);
      fprintf(f, "overlay%u_rect = \"0.0,0.0,1.0,1.0\"\n", o);
      fprintf(f, "overlay%u_overlay = p%u.png\n", o, o % distinct);
      fprintf(f, "overlay%u_descs = %u\n", o, descs);
      for (d = 0; d < descs; d++)
      {
         fprintf(f, "overlay%u_desc%u = \"a,0.5,0.5,rect,0.1,0.1\"\n",
               o, d);
         fprintf(f, "overlay%u_desc%u_overlay = p%u.png\n", o, d,
               (o + d) % distinct);
      }
   }
   fclose(f);

   for (i = 0; i < 2; i++)
   {
      bool threaded = i == 1;
      loaded_overlays = loaded_descs = images_loaded = 0;
      decode_threads  = 0;
      ticks           = 0;
      task_queue_init(threaded, NULL);
      clock_now  = 0;
      clock_step = 0;
      if (task_push_overlay_load_default(overlay_cb, cfg, false, NULL))
         while (queue_busy() && ticks < 1000000)
         {
            task_queue_check();
            ticks++;
         }
      task_queue_deinit();
      release_tracked_images();
      CHECK(loaded_overlays == overlays && loaded_descs == overlays * descs,
            "%s load produced %u overlays and %u descs",
            threaded ? "a threaded" : "an unthreaded",
            loaded_overlays, loaded_descs);
      CHECK(images_loaded == distinct,
            "%s load decoded %u images for %u distinct",
            threaded ? "a threaded" : "an unthreaded",
            images_loaded, distinct);
      if (!threaded)
         unthreaded_loaded = images_loaded;
      else
         CHECK(decode_threads > 1,
               "a threaded load decoded its pack on %u thread(s)",
               decode_threads);
   }
   (void)unthreaded_loaded;

   if (failures == had)
      fprintf(stderr, "[pass] predecode lane (%u threads)\n",
            decode_threads);
}

int main(void)
{
   char cmd[600];

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/overlay_fixture_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
   {
      fprintf(stderr, "fixture mkdir failed\n");
      return 1;
   }
   if (!write_dummy_image())
   {
      fprintf(stderr, "dummy image write failed\n");
      return 1;
   }

   lane_completeness();
   lane_paced_matches_unpaced();
   lane_work_per_tick_does_not_scale();
   lane_exhausted_window_progresses();
   lane_still_read_once();
   decode_lock = slock_new();
   lane_predecode();
   slock_free(decode_lock);
   decode_lock = NULL;

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL overlay_budget_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS overlay_budget_test\n");
   return 0;
}
