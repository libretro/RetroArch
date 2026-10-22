/* Actual frontend teardown, with recording and history callbacks observed. */
#include <stdio.h>
#include "../../../record/record_driver.h"
#include "../../../configuration.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_thread_wrapper.h"
#include "../../../defaults.h"
#include "../../../playlist.h"
#include "../../../verbosity.h"
static recording_state_t recording_state;

static settings_t test_settings;
static unsigned freed, histories, gpu_cleanup;
static bool finalize_result, gpu_start_pending;
struct defaults g_defaults;
/* The native GPU encoders, which this test does not build: only their
 * addresses are compared */
const record_driver_t record_nvenc;
const record_driver_t record_metal;
/* A native GPU recording still starting is cancelled, not finalized */
bool recording_gpu_cancel_start(void) { return gpu_start_pending; }


settings_t *config_get_ptr(void) { return &test_settings; }
void video_thread_wait_idle(void) { }
void video_driver_gpu_record_deinit(void) { gpu_cleanup++; }
void command_playlist_push_write(playlist_t *playlist,
      const struct playlist_entry *entry)
{
   (void)playlist; (void)entry;
   histories++;
}
static bool finish(void *data)
{
   (void)data;
   return finalize_result;
}
static void release(void *data) { (void)data; freed++; }

#include "record_finalize_fragment.c"

int record_finalize_test_run(void)
{
   record_driver_t driver = {0};
   unsigned lane;
   driver.free = release;
   driver.ident = "test";
   test_settings.bools.history_list_enable = true;
   for (lane = 0; lane < 3; lane++)
   {
      bool expected = lane != 0;
      freed = histories = gpu_cleanup = 0;
      finalize_result = expected;
      driver.finalize = lane == 2 ? NULL : finish;
      recording_state.driver = &driver;
      recording_state.data = &driver;
      recording_state.path[0] = 'x';
      recording_state.path[1] = '\0';
      if (recording_deinit() != expected || freed != 1 || gpu_cleanup != 1
            || histories != (unsigned)expected || recording_state.data
            || recording_state.driver || recording_state.path[0])
      {
         fprintf(stderr, "frontend finalize lane %u failed\n", lane);
         return 1;
      }
   }
   /* Stopped while a native GPU recording was still starting: the start
    * is cancelled, nothing finalized, freed or pushed to history */
   freed = histories = gpu_cleanup = 0;
   gpu_start_pending = true;
   driver.finalize = finish;
   recording_state.driver = &driver;
   recording_state.data = &driver;
   recording_state.path[0] = 'x';
   if (     !recording_deinit() || freed || gpu_cleanup || histories
         || recording_state.data || recording_state.driver
         || recording_state.path[0])
   {
      fprintf(stderr, "frontend finalize: a pending GPU start was not cancelled\n");
      return 1;
   }
   gpu_start_pending = false;
   puts("[pass] frontend finalization propagates failures, cleans up, and gates history");
   return 0;
}
