#include <rthreads/retro_eventcount.h>
#include "../../../gfx/gfx_thumbnail.h"
#include "../../../gfx/gfx_anim_preview.h"
#include "../../../gfx/gfx_surface_player.h"

static retro_eventcount_t preview_test_wake;
static retro_atomic_int_t preview_test_entered;
static retro_atomic_int_t preview_test_released;

static bool preview_test_feed(void *user)
{
   retro_atomic_store_release_int(&preview_test_entered, 1);
   while (!retro_atomic_load_acquire_int(&preview_test_released))
   {
      int key = retro_eventcount_prepare_wait(&preview_test_wake);
      if (!retro_atomic_load_acquire_int(&preview_test_released))
         retro_eventcount_commit_wait(&preview_test_wake, key);
      else
         retro_eventcount_cancel_wait(&preview_test_wake);
   }
   return gfx_anim_preview_feed((gfx_anim_preview_t*)user);
}

static void preview_test_watchdog(void *unused)
{
   (void)unused;
   retro_sleep(500);
   retro_atomic_store_release_int(&preview_test_released, 1);
   retro_eventcount_notify(&preview_test_wake);
}

static void preview_test_context_destroy(void *unused)
{
   (void)unused;
}

static void preview_test_teardown(const char *path)
{
   gfx_thumbnail_t thumb;
   gfx_surface_player_t *player;
   struct menu_state *menu = menu_state_get_ptr();
   const menu_ctx_driver_t *inner = menu->driver_ctx;
   menu_ctx_driver_t replacement = *inner;
   bool owned = (menu->flags & MENU_ST_FLAG_DATA_OWN) != 0;
   sthread_t *watchdog;
   int64_t deadline, started;
   gfx_thumbnail_init_blank(&thumb);
   gfx_thumbnail_request_file(path, &thumb, 0);
   player = (gfx_surface_player_t*)thumb.anim_player;
   CHECK(player != NULL, "preview: teardown session did not open");
   if (!player)
      return;
   if (!retro_eventcount_init(&preview_test_wake))
   {
      CHECK(false, "preview: eventcount allocation");
      gfx_thumbnail_reset(&thumb);
      return;
   }
   retro_atomic_store_relaxed_int(&preview_test_entered, 0);
   retro_atomic_store_relaxed_int(&preview_test_released, 0);
   player->cb.feed = preview_test_feed;
   player->cb.feed_user = thumb.anim_sess;
   gfx_thumbnail_animate(&thumb, cpu_features_get_time_usec());
   deadline = cpu_features_get_time_usec() + 5000000;
   while (!retro_atomic_load_acquire_int(&preview_test_entered)
         && cpu_features_get_time_usec() < deadline)
      retro_sleep(1);
   CHECK(retro_atomic_load_acquire_int(&preview_test_entered),
         "preview: worker did not enter feeder");
   watchdog = sthread_create(preview_test_watchdog, NULL);
   CHECK(watchdog != NULL, "preview: watchdog allocation");
   if (!watchdog)
   {
      retro_atomic_store_release_int(&preview_test_released, 1);
      retro_eventcount_notify(&preview_test_wake);
   }
   started = cpu_features_get_time_usec();
   gfx_thumbnail_reset(&thumb);
   CHECK(cpu_features_get_time_usec() - started < 100000,
         "preview: reset waited for the decoder");
   video_thread_wait_idle();
   replacement.context_destroy = preview_test_context_destroy;
   menu->driver_ctx = &replacement;
   menu->flags |= MENU_ST_FLAG_DATA_OWN;
   CHECK(menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL), "preview: reused-menu teardown");
   CHECK(retro_atomic_load_acquire_int(&preview_test_released),
         "preview: device teardown returned with the feeder still held");
   menu->driver_ctx = inner;
   if (!owned)
      menu->flags &= ~MENU_ST_FLAG_DATA_OWN;
   retro_atomic_store_release_int(&preview_test_released, 1);
   retro_eventcount_notify(&preview_test_wake);
   if (watchdog)
      sthread_join(watchdog);
   gfx_thumbnail_anim_worker_deinit();
   retro_eventcount_free(&preview_test_wake);
   run_frames(3);
}

static void lane_preview_surface(const char *path)
{
   unsigned pass, before = failures;
   for (pass = 0; pass < 2; pass++)
   {
      gfx_thumbnail_t thumb;
      int64_t deadline;
      size_t first = 0, last = 0;
      set_threaded_via_setting(pass != 0);
      run_frames(3);
      gfx_thumbnail_init_blank(&thumb);
      gfx_thumbnail_request_file(path, &thumb, 0);
      deadline = cpu_features_get_time_usec() + 5000000;
      while (cpu_features_get_time_usec() < deadline && !thumb.texture)
      {
         gfx_thumbnail_animate(&thumb, cpu_features_get_time_usec());
         run_frames(1);
      }
      CHECK(thumb.texture && thumb.status == GFX_THUMBNAIL_STATUS_AVAILABLE,
            "preview: first texture did not arrive (pass %u)", pass);
      if (thumb.anim)
         first = image_transfer_anim_stream_consumed(thumb.anim,
               (enum image_type_enum)thumb.anim_type);
      last = first;
      deadline = cpu_features_get_time_usec() + 3000000;
      while (thumb.anim && cpu_features_get_time_usec() < deadline)
      {
         gfx_thumbnail_animate(&thumb, cpu_features_get_time_usec());
         run_frames(1);
         if (thumb.anim)
            last = image_transfer_anim_stream_consumed(thumb.anim,
                  (enum image_type_enum)thumb.anim_type);
      }
      CHECK(last > first || (!thumb.anim && thumb.texture),
            "preview: decoder stopped advancing (pass %u)", pass);
      gfx_thumbnail_reset(&thumb);
      run_frames(3);
      preview_test_teardown(path);
   }
   set_threaded_via_setting(false);
   if (failures == before)
      fprintf(stderr, "[pass] preview playback and reused-menu teardown (direct/threaded)\n");
}
