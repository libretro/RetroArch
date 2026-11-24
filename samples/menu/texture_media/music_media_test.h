#ifdef MEDIA_TEST_MUSIC
#include "../../../gfx/gfx_music_player.h"
#include "../../../gfx/gfx_music_visualizer.h"
#include "../../../tasks/task_audio_mixer.h"

static retro_atomic_int_t music_feeds, music_observers, music_frees, music_bad_owner;
static uint32_t music_hash;
static unsigned music_changes;

void __real_gfx_music_visualizer_feed(gfx_music_visualizer_t*,
      const void*, size_t, bool, unsigned);
void __wrap_gfx_music_visualizer_feed(gfx_music_visualizer_t *v,
      const void *pcm, size_t frames, bool floating, unsigned rate)
{
   retro_atomic_fetch_add_int(&music_feeds, 1);
   __real_gfx_music_visualizer_feed(v, pcm, frames, floating, rate);
}
bool __real_gfx_music_visualizer_render(gfx_music_visualizer_t*, uint32_t*, size_t, bool);
bool __wrap_gfx_music_visualizer_render(gfx_music_visualizer_t *v,
      uint32_t *pixels, size_t pitch, bool rgba)
{
   bool result = __real_gfx_music_visualizer_render(v, pixels, pitch, rgba);
   if (result)
   {
      unsigned x, y;
      uint32_t hash = 2166136261u;
      for (y = 0; y < GFX_MUSIC_VISUALIZER_HEIGHT; y += 3)
      {
         const uint32_t *row = (const uint32_t*)((const uint8_t*)pixels + y * pitch);
         for (x = 0; x < GFX_MUSIC_VISUALIZER_WIDTH; x += 3)
            hash = (hash ^ row[x]) * 16777619u;
      }
      if (hash != music_hash) music_changes++;
      music_hash = hash;
   }
   return result;
}
void __real_gfx_music_visualizer_free(gfx_music_visualizer_t*);
void __wrap_gfx_music_visualizer_free(gfx_music_visualizer_t *v)
{
   if (v) retro_atomic_fetch_add_int(&music_frees, 1);
   __real_gfx_music_visualizer_free(v);
}
bool __real_audio_driver_mixer_add_stream_observed(audio_mixer_stream_params_t*,
      audio_mixer_voice_t**, size_t*, const audio_mixer_observer_t*);
bool __wrap_audio_driver_mixer_add_stream_observed(audio_mixer_stream_params_t *params,
      audio_mixer_voice_t **voice, size_t *serial, const audio_mixer_observer_t *observer)
{
   retro_atomic_fetch_add_int(&music_observers, 1);
   /* A borrowed WAV must publish its resident wall before the voice starts;
    * the audio thread may consume it before the first feeder tick. */
   if (!params->buf_owner || params->type != AUDIO_MIXER_TYPE_WAV_STREAM
         || !params->avail || params->avail > params->bufsize)
      retro_atomic_store_release_int(&music_bad_owner, 1);
   return __real_audio_driver_mixer_add_stream_observed(params, voice, serial, observer);
}

static void media_music_pump(struct menu_state *menu_st,
      void (*draw)(struct menu_state*), enum menu_action action)
{
   task_queue_check();
   menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(), config_get_ptr(),
         action, cpu_features_get_time_usec());
   audio_driver_menu_sample(false);
   draw(menu_st);
   gfx_music_player_reap();
   retro_sleep(1);
}

#define MUSIC_REQUIRE(condition) do { if (!(condition)) { \
   printf("music viewer failure at line %u\n", __LINE__); return false; } } while (0)

static bool media_test_music(struct menu_state *menu_st, const char *path,
      void (*draw)(struct menu_state*),
      bool (*target_at)(struct menu_state*, gfx_media_target_t*))
{
   unsigned i, changed;
   int feeds;
   size_t frees;
   gfx_media_target_t target;
   gfx_media_playback_state_t state, paused;
   gfx_music_player_t *canceled;
   enum rarch_core_type core = runloop_state_get_ptr()->current_core_type;
   printf("music audio flags=%d output=%u device=%s\n", AUDIO_FLAGS_GET(audio_state_get_ptr()),
         audio_state_get_ptr()->output_samples_buf != NULL, config_get_ptr()->arrays.audio_driver);
   printf("music audio actual=%s enabled=%u data=%u\n",
         audio_state_get_ptr()->current_audio ? audio_state_get_ptr()->current_audio->ident : "none",
         config_get_ptr()->bools.audio_enable, audio_state_get_ptr()->context_audio_data != NULL);
   retro_atomic_int_init(&music_feeds, 0);
   retro_atomic_int_init(&music_observers, 0);
   retro_atomic_int_init(&music_frees, 0);
   retro_atomic_int_init(&music_bad_owner, 0);
   MUSIC_REQUIRE(task_push_audio_mixer_load_and_play(path, NULL, NULL, false,
            AUDIO_MIXER_SLOT_SELECTION_MANUAL, 0));
   for (i = 0; i < 100; i++) media_music_pump(menu_st, draw, MENU_ACTION_NOOP);
   MUSIC_REQUIRE(!retro_atomic_load_acquire_int(&music_feeds));
   MUSIC_REQUIRE(!retro_atomic_load_acquire_int(&music_observers));
   audio_driver_mixer_remove_stream(0);
   MUSIC_REQUIRE(menu_driver_media_open(path));
   for (i = 0; i < 4000; i++)
   {
      media_music_pump(menu_st, draw, MENU_ACTION_NOOP);
      if (target_at(menu_st, &target) && target.state(target.user,
               menu_st->current_time_us, &state) && state.position_us > 0
            && music_changes > 1) break;
   }
   printf("music ready: ticks=%u feeds=%d observers=%d changes=%u startup=%d target=%u\n",
         i, retro_atomic_load_acquire_int(&music_feeds),
         retro_atomic_load_acquire_int(&music_observers), music_changes,
         menu_driver_media_startup_token(), target_at(menu_st, &target));
   if (target_at(menu_st, &target) && target.state(target.user, menu_st->current_time_us, &state))
      printf("music position=%.0f paused=%u\n", (double)state.position_us, state.paused);
   MUSIC_REQUIRE(i < 4000);
   MUSIC_REQUIRE(runloop_state_get_ptr()->current_core_type == core);
   MUSIC_REQUIRE(retro_atomic_load_acquire_int(&music_observers) == 1);
   MUSIC_REQUIRE(!retro_atomic_load_acquire_int(&music_bad_owner));
   MUSIC_REQUIRE(target.pause(target.user, true, menu_st->current_time_us));
   for (i = 0; i < 100; i++) media_music_pump(menu_st, draw, MENU_ACTION_NOOP);
   MUSIC_REQUIRE(target.state(target.user, menu_st->current_time_us, &paused) && paused.paused);
   feeds = retro_atomic_load_acquire_int(&music_feeds);
   changed = music_changes;
   for (i = 0; i < 100; i++) media_music_pump(menu_st, draw, MENU_ACTION_NOOP);
   MUSIC_REQUIRE(target.state(target.user, menu_st->current_time_us, &state));
   MUSIC_REQUIRE(state.position_us == paused.position_us);
   MUSIC_REQUIRE(retro_atomic_load_acquire_int(&music_feeds) == feeds && music_changes == changed);
   MUSIC_REQUIRE(target.pause(target.user, false, menu_st->current_time_us));
   for (i = 0; i < 100 && retro_atomic_load_acquire_int(&music_feeds) == feeds; i++)
      media_music_pump(menu_st, draw, MENU_ACTION_NOOP);
   MUSIC_REQUIRE(retro_atomic_load_acquire_int(&music_feeds) > feeds);
   media_music_pump(menu_st, draw, MENU_ACTION_CANCEL);
   for (i = 0; i < 1000 && !retro_atomic_load_acquire_int(&music_frees); i++)
      media_music_pump(menu_st, draw, MENU_ACTION_NOOP);
   MUSIC_REQUIRE(retro_atomic_load_acquire_int(&music_frees) == 1);
   feeds = retro_atomic_load_acquire_int(&music_feeds);
   frees = (size_t)retro_atomic_load_acquire_int(&music_frees);
   canceled = gfx_music_player_new(path);
   MUSIC_REQUIRE(canceled);
   gfx_music_player_close(canceled);
   for (i = 0; i < 1000 && (size_t)retro_atomic_load_acquire_int(&music_frees) == frees; i++)
      media_music_pump(menu_st, draw, MENU_ACTION_NOOP);
   MUSIC_REQUIRE((size_t)retro_atomic_load_acquire_int(&music_frees) == frees + 1);
   MUSIC_REQUIRE(retro_atomic_load_acquire_int(&music_feeds) == feeds);
   puts("PASS: menu music uses borrowed streaming audio, changing visualizer pixels, retained core, pause/resume, close and canceled load; background audio has no visualizer");
   return true;
}
#undef MUSIC_REQUIRE
#endif
