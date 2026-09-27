/* Harness for the content-closing state, driving the real frame loop.
 *
 * Closing content tears the core down, and the save and load handlers
 * call into that core - retro_serialize via
 * content_get_serialized_data, retro_unserialize via
 * core_unserialize.  A worker still inside one of those when
 * runloop_event_deinit_core() unloads the library dispatches into
 * freed code, which is why the close waits for those tasks and why
 * that wait cannot simply be deleted.
 *
 * The plan for removing the freeze is to stop blocking and let the
 * frame loop keep running while the close finishes.  The guard that
 * makes that safe - core_run() refusing to enter retro_run() while
 * runloop_state.content_closing is set - is in place but cannot fire
 * yet, because the close is still synchronous and no frame runs
 * during it.
 *
 * Which means the guard is, right now, entirely unexercised: it will
 * go live in the same change that first lets frames run during a
 * close, and if it is wrong the symptom is a call into an unloaded
 * dylib.  This drives it directly instead - the real core_run(), in
 * a real frontend, with the flag set - so that when it does go live
 * it is not going live untested.
 *
 * Links the shipping objects with only main() replaced; nothing is
 * stubbed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>

#include "../../../runloop.h"
#include "../../../retroarch.h"
#include "../../../configuration.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../verbosity.h"

#include <time/rtime.h>
#include <file/config_file.h>

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
/* A core whose retro_run only counts                                 */
/* ------------------------------------------------------------------ */

static unsigned retro_run_calls;

static void counting_retro_run(void)
{
   retro_run_calls++;
}

/* ------------------------------------------------------------------ */

static void lane_closing_blocks_retro_run(void)
{
   unsigned had              = failures;
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   void (*saved_run)(void)   = runloop_st->current_core.retro_run;

   runloop_st->current_core.retro_run = counting_retro_run;

   /* Not closing: the core runs, which is what makes the next
    * assertion mean something rather than passing because core_run()
    * never reaches retro_run() at all in this environment. */
   runloop_st->content_closing = false;
   retro_run_calls             = 0;
   core_run();

   CHECK(retro_run_calls == 1,
         "core_run() did not reach retro_run() with the flag clear, "
         "so this harness cannot tell whether the guard works");

   /* Closing: the core must not be entered.  This is the property
    * that keeps the frame loop off a core whose library is going
    * away. */
   runloop_st->content_closing = true;
   retro_run_calls             = 0;
   core_run();

   CHECK(retro_run_calls == 0,
         "core_run() entered retro_run() while content was closing - "
         "with the teardown unloading that library, this is a call "
         "into freed code");

   /* And back: the guard must not be sticky, or the frame loop would
    * stay dead after a close finished. */
   runloop_st->content_closing = false;
   retro_run_calls             = 0;
   core_run();

   CHECK(retro_run_calls == 1,
         "the core did not resume once closing had finished");

   runloop_st->current_core.retro_run = saved_run;
   runloop_st->content_closing        = false;

   if (failures == had)
      fprintf(stderr, "[pass] closing-blocks-retro-run lane\n");
}

/* Frames must keep being produced while closing, or the window stops
 * updating and the close reads as a hang - the very complaint this
 * work exists to fix. */
static void lane_closing_still_presents(void)
{
   unsigned had                = failures;
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   void (*saved_run)(void)     = runloop_st->current_core.retro_run;
   unsigned i;

   runloop_st->current_core.retro_run = counting_retro_run;
   runloop_st->content_closing        = true;
   retro_run_calls                    = 0;

   /* Many frames, as a close on slow storage would take.  The point
    * is that core_run() returns cleanly every time rather than
    * faulting on a torn-down core or wedging. */
   for (i = 0; i < 240; i++)
      core_run();

   CHECK(retro_run_calls == 0,
         "the core was entered during a 240-frame close");

   runloop_st->current_core.retro_run = saved_run;
   runloop_st->content_closing        = false;

   if (failures == had)
      fprintf(stderr, "[pass] closing-still-presents lane (240 frames)\n");
}

/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char cmd[600];
   char dir[400];
   char *rarch_argv[8];
   int rarch_argc = 0;
   FILE *cfg;

   snprintf(dir, sizeof(dir), "/tmp/closing_harness_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", dir);
   if (system(cmd) != 0)
      return 1;

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if ((cfg = fopen(cfg_path, "wb")))
   {
      fprintf(cfg, "video_driver = \"null\"\n");
      fprintf(cfg, "audio_driver = \"null\"\n");
      fprintf(cfg, "input_driver = \"null\"\n");
      fprintf(cfg, "input_joypad_driver = \"null\"\n");
      fprintf(cfg, "menu_driver = \"rgui\"\n");
      fprintf(cfg, "video_threaded = \"false\"\n");
      fclose(cfg);
   }

   /* The prelude rarch_main() performs before main_init; skipping any
    * of it is a null dereference deep inside. */
   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   lane_closing_blocks_retro_run();
   lane_closing_still_presents();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL content_closing_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS content_closing_test\n");
   return 0;
}
