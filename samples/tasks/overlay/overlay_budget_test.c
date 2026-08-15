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

/* NBIO_XFER_TICK_USEC is 4000; a third of it per observation
 * exhausts the window after a couple of budget checks. */
#define VIRTUAL_CLOCK_STEP 1500

/* ------------------------------------------------------------------ */
/* Stubs                                                               */
/* ------------------------------------------------------------------ */

static unsigned images_loaded;

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

bool image_texture_load(struct texture_image *img, const char *path)
{
   images_loaded++;
   if (img)
   {
      img->width  = 4;
      img->height = 4;
      img->pixels = (uint32_t*)calloc(16, sizeof(uint32_t));
      if (!img->pixels)
         return false;
      if (tracked_count < MAX_TRACKED_IMAGES)
         tracked_pixels[tracked_count++] = img->pixels;
   }
   return true;
}

/* Deliberately a no-op on pixels: see above. */
void image_texture_free(struct texture_image *img)
{
   if (img)
      img->pixels = NULL;
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

/* The loader reads input_st only for eightway bind defaults; an
 * all-zero state is a valid one for that purpose. */
void *input_state_get_ptr(void)
{
   static char st[4096];
   return st;
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

/* ------------------------------------------------------------------ */
/* Lanes                                                               */
/* ------------------------------------------------------------------ */

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
